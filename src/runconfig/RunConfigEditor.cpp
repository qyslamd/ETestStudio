#include "RunConfigEditor.h"

#include <QAction>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRectF>
#include <QSet>
#include <QSizePolicy>
#include <QToolBar>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "MonitorPropertyWidget.h"
#include "ProgramChecklistWidget.h"
#include "VisualizationArea.h"
#include "VisualizerPaletteWidget.h"
#include "libui/EtDockWidget.h"
#include "logger/Logger.h"
#include "project/ProjectManager.h"
#include "visualizers/SignalVisualizer.h"
#include "visualizers/VisualizerFactory.h"
#include "visualizers/VisualizerProxy.h"

using etest::core_ui::AppIconProvider;

namespace {
// dock 关闭按钮 → 同步取消工具栏 toggle 勾选（与三编辑器统一）
void syncDockCloseAction(QAction* action) {
  action->blockSignals(true);
  action->setChecked(false);
  action->blockSignals(false);
}
}  // namespace

namespace etest::runconfig {

// ══════════════════════════════════════════════════════════════════════════════
// RunConfigEditor
// ══════════════════════════════════════════════════════════════════════════════

RunConfigEditor::RunConfigEditor(const QString& id, QWidget* parent)
    : QMainWindow(parent), file_path_(id) {
  initUi();
}

RunConfigEditor::~RunConfigEditor() {
  // Qt 析构顺序：先删 children 后断开连接，而成员（config_/snapshots_）先于
  // ~QObject 销毁。children 析构期间若触发访问成员的槽：
  //   1) vis_area_ 析构 clearAll() → selectionChanged → 访问 toolbar
  //   按钮（已删） 2) vis_area_ 析构 clearAll() → visualizerRemoved →
  //   removeMonitorById
  //      访问 config_/snapshots_（成员已销毁）
  // 均在 children 析构前显式断开。
  if (vis_area_ && vis_area_->scene()) {
    disconnect(vis_area_->scene(), &QGraphicsScene::selectionChanged, this,
               nullptr);
  }
  if (vis_area_) {
    disconnect(vis_area_, &VisualizationArea::visualizerRemoved, this, nullptr);
  }
}

// ── IEditor ──

QString RunConfigEditor::displayName() const {
  if (file_path_.isEmpty()) {
    return QStringLiteral("运行配置");
  }
  return QFileInfo(file_path_).fileName();
}

QString RunConfigEditor::editorId() const {
  return file_path_;
}

QString RunConfigEditor::editorType() const {
  return QStringLiteral("runconfig");
}

QString RunConfigEditor::filePath() const {
  return file_path_;
}

QWidget* RunConfigEditor::widget() {
  return this;
}

QObject* RunConfigEditor::signalObject() {
  return this;
}

bool RunConfigEditor::isModified() const {
  return modified_;
}

bool RunConfigEditor::save() {
  collectLayout();  // 收集当前可视化区布局（scene 坐标，与视图缩放无关）

  QString path = file_path_;
  if (path.isEmpty()) {
    path = QFileDialog::getSaveFileName(this, QStringLiteral("保存运行配置"),
                                        QString(),
                                        QStringLiteral("运行配置 (*.erun)"));
    if (path.isEmpty()) {
      return false;
    }
    file_path_ = path;
  }
  // 未绑定卡软提醒（决策 32 / 终审 🔵11：非阻塞，统计未绑定数可继续保存）
  int unbound = 0;
  for (const auto& m : config_.monitors) {
    if (m.connectionId.isEmpty()) {
      ++unbound;
    }
  }
  if (unbound > 0) {
    // 非阻塞软提醒（决策 32：不硬拦，可继续保存）
    auto* box = new QMessageBox(
        QMessageBox::Information, QStringLiteral("保存运行配置"),
        QStringLiteral("有 %1 个监听器未绑定连线，运行态将显示为未绑定。")
            .arg(unbound),
        QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->show();
  }
  if (saveToFile(path)) {
    // 保存到当前项目 run/ 下 → 设为当前运行配置（.etproj
    // settings.runConfigFile）
    syncRunConfigRef(path);
    modified_ = false;
    emit modificationChanged(false);
    return true;
  }
  return false;
}

// 将 .erun 设为当前运行配置：保存路径落在当前项目 run/ 下时写 settings
void RunConfigEditor::syncRunConfigRef(const QString& path) {
  auto& proj_mgr = etest::core::project::ProjectManager::instance();
  if (!proj_mgr.isProjectOpen()) {
    return;
  }
  const QString root = proj_mgr.currentProjectRoot();
  if (root.isEmpty()) {
    return;
  }
  // 统一用相对路径判断是否在项目内：QDir::separator 与 absoluteFilePath 在
  // Windows 上分隔符不一致，startsWith 前缀判断不可靠
  const QString abs = QFileInfo(path).absoluteFilePath();
  const QString rel = QDir(root).relativeFilePath(abs);
  if (rel.startsWith(QLatin1String("../"))) {
    return;  // 不在当前项目内（脱离项目打开的 .erun 不写）
  }
  proj_mgr.setSetting(QStringLiteral("runConfigFile"), rel);
}

bool RunConfigEditor::saveAs(const QString& path) {
  file_path_ = path;
  return save();
}

bool RunConfigEditor::canUndo() const {
  return snapshot_index_ > 0;
}

bool RunConfigEditor::canRedo() const {
  return snapshot_index_ < snapshots_.size() - 1;
}

void RunConfigEditor::undo() {
  if (!canUndo()) {
    return;
  }
  --snapshot_index_;
  restoreSnapshot(snapshots_[snapshot_index_]);
}

void RunConfigEditor::redo() {
  if (!canRedo()) {
    return;
  }
  ++snapshot_index_;
  restoreSnapshot(snapshots_[snapshot_index_]);
}

void RunConfigEditor::openFile(const QString& filePath) {
  file_path_ = filePath;
  loadFromFile(file_path_);
  refreshUi();
  // 压基线快照：首次修改后 canUndo() 才能为 true（对齐 TestProgram/Protocol）
  snapshots_.clear();
  snapshot_index_ = -1;
  saveSnapshot();
}

// ── 嵌入模式 / 主题图标 ──

void RunConfigEditor::setEmbeddedMode(bool embedded) {
  embedded_ = embedded;
  if (embedded) {
    menuBar()->hide();
    toolbar_->hide();
  } else {
    menuBar()->show();
    toolbar_->show();
  }
}

// ── IEditorCommandSource ────────────────────────────────────────

QList<etest::app::EditorCommand> RunConfigEditor::editorCommands() {
  QList<etest::app::EditorCommand> cmds;
  auto addCmd = [](const QString& group, const QString& title,
                   const QString& iconName, bool large, QAction* action) {
    etest::app::EditorCommand c;
    c.group = group;
    c.title = title;
    c.iconName = iconName;
    c.large = large;
    c.trigger = [action]() { action->trigger(); };
    c.isEnabled = [action]() { return action->isEnabled(); };
    return c;
  };
  auto addCheckableCmd = [&](const QString& group, const QString& title,
                             const QString& iconName, bool large,
                             QAction* action) {
    etest::app::EditorCommand c = addCmd(group, title, iconName, large, action);
    c.checkable = true;
    c.isChecked = [action]() { return action->isChecked(); };
    return c;
  };
  // 编辑
  cmds.append(addCmd(QStringLiteral("编辑"), QStringLiteral("撤销"),
                     QStringLiteral("undo"), true, undo_action_));
  cmds.append(addCmd(QStringLiteral("编辑"), QStringLiteral("重做"),
                     QStringLiteral("redo"), true, redo_action_));
  // 文件
  cmds.append(addCmd(QStringLiteral("文件"), QStringLiteral("新建"),
                     QStringLiteral("file_new"), false, new_action_));
  cmds.append(addCmd(QStringLiteral("文件"), QStringLiteral("保存"),
                     QStringLiteral("file_save"), false, save_action_));
  // 布局
  {
    etest::app::EditorCommand c;
    c.group = QStringLiteral("布局");
    c.title = QStringLiteral("排列");
    c.iconName = QStringLiteral("topo_align");
    c.large = true;
    c.isEnabled = [this]() { return align_btn_->isEnabled(); };
    c.menu = [this]() { return align_menu_; };
    cmds.append(c);
  }
  {
    etest::app::EditorCommand c;
    c.group = QStringLiteral("布局");
    c.title = QStringLiteral("分布");
    c.iconName = QStringLiteral("topo_distribute");
    c.large = true;
    c.isEnabled = [this]() { return dist_btn_->isEnabled(); };
    c.menu = [this]() { return dist_menu_; };
    cmds.append(c);
  }
  // 视图
  cmds.append(addCheckableCmd(
      QStringLiteral("视图"), QStringLiteral("测试程序"),
      QStringLiteral("testprogram"), false, test_program_toggle_action_));
  cmds.append(addCheckableCmd(
      QStringLiteral("视图"), QStringLiteral("可视化组件"),
      QStringLiteral("monitor"), false, palette_toggle_action_));
  cmds.append(addCheckableCmd(QStringLiteral("视图"), QStringLiteral("属性"),
                              QStringLiteral("file_json"), false,
                              property_toggle_action_));
  return cmds;
}

QObject* RunConfigEditor::commandStateObject() {
  return this;
}

void RunConfigEditor::reloadToolbarIcons() {
  auto& provider = AppIconProvider::instance();
  provider.clearCache();
  if (undo_action_) {
    undo_action_->setIcon(provider.icon(QStringLiteral("undo")));
  }
  if (redo_action_) {
    redo_action_->setIcon(provider.icon(QStringLiteral("redo")));
  }
  if (new_action_) {
    new_action_->setIcon(provider.icon(QStringLiteral("file_new")));
  }
  if (save_action_) {
    save_action_->setIcon(provider.icon(QStringLiteral("file_save")));
  }
  if (align_btn_) {
    align_btn_->setIcon(provider.icon(QStringLiteral("topo_align")));
  }
  if (dist_btn_) {
    dist_btn_->setIcon(provider.icon(QStringLiteral("topo_distribute")));
  }
  if (test_program_toggle_action_) {
    // 图标固定，不随 checked 切换（与三编辑器统一）
    test_program_toggle_action_->setIcon(
        provider.icon(QStringLiteral("testprogram")));
  }
  if (palette_toggle_action_) {
    palette_toggle_action_->setIcon(provider.icon(QStringLiteral("monitor")));
  }
  if (property_toggle_action_) {
    property_toggle_action_->setIcon(
        provider.icon(QStringLiteral("file_json")));
  }
}

bool RunConfigEditor::eventFilter(QObject* obj, QEvent* event) {
  // 用户点 dock 关闭按钮 → 同步取消工具栏 toggle 勾选（与三编辑器统一）
  if (event->type() == QEvent::Close) {
    if (obj == test_program_dock_) {
      syncDockCloseAction(test_program_toggle_action_);
    } else if (obj == palette_dock_) {
      syncDockCloseAction(palette_toggle_action_);
    } else if (obj == property_dock_) {
      syncDockCloseAction(property_toggle_action_);
    }
    // 面板开关勾选态变化 → 刷新 Ribbon 上下文命令（3.6 状态同步）
    emit commandsChanged();
  }
  return QMainWindow::eventFilter(obj, event);
}

// ── UI ──

void RunConfigEditor::initUi() {
  // 工具栏：QMainWindow 原生工具栏
  toolbar_ = addToolBar(QStringLiteral("运行配置"));
  toolbar_->setObjectName(QStringLiteral("runConfigToolbar"));
  toolbar_->setMovable(false);

  // 撤销/重做（快照式，与三编辑器统一）
  undo_action_ = toolbar_->addAction(QStringLiteral("撤销"));
  undo_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("undo")));
  undo_action_->setShortcut(QKeySequence::Undo);
  undo_action_->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
  undo_action_->setEnabled(false);
  connect(undo_action_, &QAction::triggered, this, &RunConfigEditor::undo);
  redo_action_ = toolbar_->addAction(QStringLiteral("重做"));
  redo_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("redo")));
  redo_action_->setShortcut(QKeySequence::Redo);
  redo_action_->setToolTip(QStringLiteral("重做 (Ctrl+Y)"));
  redo_action_->setEnabled(false);
  connect(redo_action_, &QAction::triggered, this, &RunConfigEditor::redo);
  toolbar_->addSeparator();

  new_action_ = toolbar_->addAction(QStringLiteral("新建"));
  new_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("file_new")));
  new_action_->setToolTip(QStringLiteral("新建运行配置"));
  connect(new_action_, &QAction::triggered, this, [this]() {
    // 未保存改动先确认，避免清空路径后改动无法找回
    if (modified_) {
      const auto ret = QMessageBox::warning(
          this, QStringLiteral("新建运行配置"),
          QStringLiteral("当前运行配置有未保存的修改，确定要新建吗？"),
          QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
      if (ret != QMessageBox::Yes) {
        return;
      }
    }
    saveSnapshot();  // 新建前压快照，撤销可恢复
    config_ = RunConfig();
    file_path_.clear();
    refreshUi();
    markModified();
  });
  save_action_ = toolbar_->addAction(QStringLiteral("保存"));
  save_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("file_save")));
  save_action_->setToolTip(QStringLiteral("保存运行配置 (.erun)"));
  connect(save_action_, &QAction::triggered, this, [this]() { save(); });
  // 排列按钮（QToolButton + QMenu，参考拓扑 TopologyEditorWidget）
  align_btn_ = new QToolButton(this);
  align_btn_->setText(QStringLiteral("排列"));
  align_btn_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("topo_align")));
  align_btn_->setPopupMode(QToolButton::InstantPopup);
  align_menu_ = new QMenu(align_btn_);
  auto addAlign = [this](const QString& text, VisualizationArea::AlignType t) {
    auto* act = align_menu_->addAction(text);
    connect(act, &QAction::triggered, this,
            [this, t]() { vis_area_->alignVisualizers(t); });
  };
  addAlign(QStringLiteral("左对齐"), VisualizationArea::AlignType::Left);
  addAlign(QStringLiteral("水平居中"), VisualizationArea::AlignType::HCenter);
  addAlign(QStringLiteral("右对齐"), VisualizationArea::AlignType::Right);
  addAlign(QStringLiteral("顶端对齐"), VisualizationArea::AlignType::Top);
  addAlign(QStringLiteral("垂直居中"), VisualizationArea::AlignType::VCenter);
  addAlign(QStringLiteral("底端对齐"), VisualizationArea::AlignType::Bottom);
  align_btn_->setMenu(align_menu_);
  align_btn_->setEnabled(false);  // 初始无选中，禁用直到选中 >=2
  toolbar_->addWidget(align_btn_);

  // 分布按钮
  dist_btn_ = new QToolButton(this);
  dist_btn_->setText(QStringLiteral("分布"));
  dist_btn_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("topo_distribute")));
  dist_btn_->setPopupMode(QToolButton::InstantPopup);
  dist_menu_ = new QMenu(dist_btn_);
  auto addDist = [this](const QString& text,
                        VisualizationArea::DistributeType t) {
    auto* act = dist_menu_->addAction(text);
    connect(act, &QAction::triggered, this,
            [this, t]() { vis_area_->distributeVisualizers(t); });
  };
  addDist(QStringLiteral("水平分布"),
          VisualizationArea::DistributeType::Horizontal);
  addDist(QStringLiteral("垂直分布"),
          VisualizationArea::DistributeType::Vertical);
  dist_btn_->setMenu(dist_menu_);
  dist_btn_->setEnabled(false);  // 初始无选中，禁用直到选中 >=2
  toolbar_->addWidget(dist_btn_);

  // 弹簧：面板开关右对齐（参考拓扑 TopologyEditorWidget）
  auto* spacer = new QWidget(toolbar_);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar_->addWidget(spacer);

  // 测试程序面板显示/隐藏 toggle（与三编辑器统一：dock 可关，工具栏 toggle
  // 重开）
  test_program_toggle_action_ = toolbar_->addAction(QStringLiteral("测试程序"));
  test_program_toggle_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("testprogram")));
  test_program_toggle_action_->setCheckable(true);
  test_program_toggle_action_->setChecked(true);
  test_program_toggle_action_->setToolTip(
      QStringLiteral("显示/隐藏测试程序面板"));

  // 测试程序多选 dock（Left 停靠，同套 dock 管理）
  program_list_ = new ProgramChecklistWidget(this);
  test_program_dock_ =
      new etest::ui::EtDockWidget(QStringLiteral("测试程序"), this);
  test_program_dock_->setObjectName(QStringLiteral("runConfigProgramDock"));
  test_program_dock_->setWidget(program_list_);
  test_program_dock_->installEventFilter(this);
  addDockWidget(Qt::LeftDockWidgetArea, test_program_dock_);

  // 可视化组件 dock（visualizer 拖放源，左侧测试程序下方）
  palette_widget_ = new VisualizerPaletteWidget(this);
  palette_dock_ =
      new etest::ui::EtDockWidget(QStringLiteral("可视化组件"), this);
  palette_dock_->setObjectName(QStringLiteral("runConfigPaletteDock"));
  palette_dock_->setWidget(palette_widget_);
  palette_dock_->installEventFilter(this);
  addDockWidget(Qt::LeftDockWidgetArea, palette_dock_);
  palette_toggle_action_ = toolbar_->addAction(QStringLiteral("可视化组件"));
  palette_toggle_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("monitor")));
  palette_toggle_action_->setCheckable(true);
  palette_toggle_action_->setChecked(true);
  connect(palette_toggle_action_, &QAction::toggled, palette_dock_,
          &QWidget::setVisible);

  // 属性面板 dock（选中卡片加载，右侧）
  property_widget_ = new MonitorPropertyWidget(this);
  property_dock_ = new etest::ui::EtDockWidget(QStringLiteral("属性"), this);
  property_dock_->setObjectName(QStringLiteral("runConfigPropertyDock"));
  property_dock_->setWidget(property_widget_);
  property_dock_->installEventFilter(this);
  addDockWidget(Qt::RightDockWidgetArea, property_dock_);
  property_toggle_action_ = toolbar_->addAction(QStringLiteral("属性"));
  property_toggle_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("file_json")));
  property_toggle_action_->setCheckable(true);
  property_toggle_action_->setChecked(true);
  connect(property_toggle_action_, &QAction::toggled, property_dock_,
          &QWidget::setVisible);

  // 测试程序勾选变化 → 更新 config_.programs + 压快照 + 置脏
  connect(program_list_, &ProgramChecklistWidget::programsChanged, this,
          [this]() {
            const QStringList sel = program_list_->selectedPrograms();
            if (sel == config_.programs) {
              return;  // 同值（程序性刷新）不动作
            }
            saveSnapshot();
            config_.programs = sel;
            markModified();
          });

  // 主视图：可视化区（编辑态，监听器卡片 + 手动布局）
  vis_area_ = new VisualizationArea(this);
  vis_area_->setEditMode(true);
  setCentralWidget(vis_area_);

  // 布局被改动（拖拽/resize/排列/分布）→ 记录快照 + 置脏，关闭 tab 时提示保存
  connect(vis_area_, &VisualizationArea::layoutChanged, this, [this]() {
    saveSnapshot();
    markModified();
  });

  // 拖放：可视化组件 visualizer 拖入 → 新建未绑定卡片
  connect(vis_area_, &VisualizationArea::visualizerDropped, this,
          [this](const QString& displayMode, const QPointF& pos) {
            addMonitorFromDrop(displayMode, pos);
          });

  // 属性面板信号 → 更新 config_ + 快照 + 置脏 + 刷新卡片
  connect(property_widget_, &MonitorPropertyWidget::nameChanged, this,
          [this](const QString& id, const QString& name) {
            for (auto& m : config_.monitors) {
              if (m.id == id) {
                saveSnapshot();
                m.name = name;
                if (auto* vis = vis_area_->visualizer(id)) {
                  vis->setTitle(name);
                }
                markModified();
                return;
              }
            }
          });
  connect(property_widget_, &MonitorPropertyWidget::connectionBound, this,
          [this](const QString& id, const QString& cid) {
            for (auto& m : config_.monitors) {
              if (m.id == id) {
                if (m.connectionId == cid) {
                  return;
                }
                saveSnapshot();
                m.connectionId = cid;
                markModified();
                refreshUi();            // 重建卡片（副标题更新 + 绑定状态）
                selectMonitorCard(id);  // 重建后保持选中（🔵3）
                return;
              }
            }
          });
  connect(property_widget_, &MonitorPropertyWidget::typeChanged, this,
          [this](const QString& id, const QString& mode) {
            for (auto& m : config_.monitors) {
              if (m.id == id) {
                if (m.displayMode == mode) {
                  return;
                }
                saveSnapshot();
                m.displayMode = mode;
                markModified();
                refreshUi();            // 单卡重建（保留 id/连接/几何/名称）
                selectMonitorCard(id);  // 重建后保持选中（🔵3）
                return;
              }
            }
          });
  connect(property_widget_, &MonitorPropertyWidget::deleteRequested, this,
          [this](const QString& id) { removeMonitorById(id); });

  // 右键"关闭可视化"（编辑态删除卡片）→ 收敛到 removeMonitorById
  connect(vis_area_, &VisualizationArea::visualizerRemoved, this,
          &RunConfigEditor::removeMonitorById);

  // 排列/分布门控：选中 ≥2 张卡片才启用；单卡选中 → 属性面板加载
  connect(vis_area_->scene(), &QGraphicsScene::selectionChanged, this,
          [this]() {
            const bool enabled = vis_area_->selectedVisualizerCount() >= 2;
            if (align_btn_) {
              align_btn_->setEnabled(enabled);
            }
            if (dist_btn_) {
              dist_btn_->setEnabled(enabled);
            }
            refreshPropertyPanel();
            emit commandsChanged();
          });

  // 面板可见性与工具栏 toggle 同步（与 Topology 一致：toggled 直接驱动 dock
  // 显隐）
  connect(test_program_toggle_action_, &QAction::toggled, test_program_dock_,
          &QWidget::setVisible);
}

void RunConfigEditor::refreshUi() {
  // 测试程序面板：按项目根刷新列表 + 恢复勾选（与 config_.programs 同步）
  if (program_list_) {
    program_list_->setProjectRoot(findProjectRoot());
    program_list_->setSelectedPrograms(config_.programs);
  }

  // 重建可视化区卡片：按 config_.monitors（key=id），应用 Monitor 内嵌几何
  const auto conns = loadConnectionsFromProject();
  const auto oldIds = vis_area_->monitorIds();
  for (const auto& id : oldIds) {
    vis_area_->removeVisualizer(id);
  }
  int fallback = 0;
  for (const auto& m : config_.monitors) {
    // parent 传 nullptr：visualizer 须为 top-level 才能被 VisualizerProxy
    // setWidget 嵌入（传 this 会被警告 "cannot embed widget"）
    auto* vis = etest::runconfig::createVisualizerFor(
        m.connectionId, m.displayMode, QString(), m.name, nullptr);
    if (!vis) {
      continue;
    }
    // 二级标题：未绑定 → 警示「未绑定到连线」；绑定 → 连接描述；
    // 非空但拓扑无此连接 → 警示「连接已删除」（决策 24）
    if (m.connectionId.isEmpty()) {
      vis->setSubtitle(QStringLiteral("未绑定到连线"));
      vis->setSubtitleState(QStringLiteral("warning"));
    } else {
      QString desc;
      for (const auto& c : conns) {
        if (c.first == m.connectionId) {
          desc = c.second;
          break;
        }
      }
      if (desc.isEmpty()) {
        vis->setSubtitle(QStringLiteral("连接已删除"));
        vis->setSubtitleState(QStringLiteral("deleted"));
      } else {
        vis->setSubtitle(desc);
        vis->setSubtitleState(QStringLiteral("normal"));
      }
    }
    vis_area_->addVisualizer(m.id, vis);
    // 全零几何兜底（旧 .erun / 手改文件）：sizeHint 默认尺寸 + 递增位置
    if (m.w == 0 && m.h == 0) {
      const QSize size = vis->sizeHint();
      vis_area_->setVisualizerGeometry(
          m.id, QRectF(20.0 + fallback * 40, 20.0 + fallback * 40, size.width(),
                       size.height()));
      ++fallback;
    } else {
      vis_area_->setVisualizerGeometry(m.id, QRectF(m.x, m.y, m.w, m.h));
    }
  }
  refreshPropertyPanel();
}

void RunConfigEditor::addMonitorFromDrop(const QString& displayMode,
                                         const QPointF& scenePos) {
  saveSnapshot();  // 添加前压快照
  RunConfig::Monitor m;
  m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m.displayMode = displayMode;
  // 默认尺寸按 visualizer sizeHint（决策 19）
  QSize size(320, 200);
  auto* probe = etest::runconfig::createVisualizerFor(
      QString(), displayMode, QString(), QString(), nullptr);
  if (probe) {
    size = probe->sizeHint();
    delete probe;
  }
  m.x = scenePos.x();
  m.y = scenePos.y();
  m.w = size.width();
  m.h = size.height();
  config_.monitors.append(m);
  markModified();
  refreshUi();
}

void RunConfigEditor::removeMonitorById(const QString& id) {
  for (int i = 0; i < config_.monitors.size(); ++i) {
    if (config_.monitors[i].id == id) {
      saveSnapshot();  // 删除前压快照
      config_.monitors.removeAt(i);
      markModified();
      refreshUi();
      return;
    }
  }
}

void RunConfigEditor::refreshPropertyPanel() {
  if (!property_widget_ || !vis_area_) {
    return;
  }
  // 单卡选中 → 加载属性；无/多选 → 清空
  RunConfig::Monitor* monitor = nullptr;
  const auto selected = vis_area_->scene()->selectedItems();
  for (auto* item : selected) {
    if (auto* vp = qgraphicsitem_cast<VisualizerProxy*>(item)) {
      const QString id = vp->monitorId();
      for (auto& m : config_.monitors) {
        if (m.id == id) {
          monitor = &m;
          break;
        }
      }
      break;  // 只处理第一个选中
    }
  }
  if (!monitor) {
    property_widget_->clear();
    return;
  }
  // 其他卡片已绑定的连接 → 属性面板禁用（一连接一监听器）
  QSet<QString> bound;
  for (const auto& m : config_.monitors) {
    if (!m.connectionId.isEmpty() && m.id != monitor->id) {
      bound.insert(m.connectionId);
    }
  }
  property_widget_->setMonitor(*monitor, loadConnectionsFromProject(), bound);
}

void RunConfigEditor::selectMonitorCard(const QString& id) {
  if (!vis_area_) {
    return;
  }
  const auto items = vis_area_->scene()->items();
  for (auto* item : items) {
    if (auto* vp = qgraphicsitem_cast<VisualizerProxy*>(item)) {
      if (vp->monitorId() == id) {
        vis_area_->scene()->clearSelection();
        vp->setSelected(true);
        return;
      }
    }
  }
}

// 从 .erun 所在目录向上找含 topology 的项目根
QString RunConfigEditor::findProjectRoot() const {
  if (file_path_.isEmpty()) {
    return QString();
  }
  // 先查 .erun 所在目录，再逐级向上；.erun 就在项目根时也能找到 topology
  QDir dir = QFileInfo(file_path_).absoluteDir();
  for (;;) {
    if (dir.exists(QStringLiteral("topology"))) {
      return dir.absolutePath();
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return QString();
}

// 从 .erun 所在项目读拓扑连接
QList<QPair<QString, QString>> RunConfigEditor::loadConnectionsFromProject()
    const {
  QList<QPair<QString, QString>> result;
  const QString root = findProjectRoot();
  if (root.isEmpty()) {
    return result;
  }
  const QString topoPath = root + QStringLiteral("/topology/topology.etopo");
  QFile file(topoPath);
  if (file.open(QIODevice::ReadOnly)) {
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (doc.isObject()) {
      const QJsonArray conns =
          doc.object()[QStringLiteral("connections")].toArray();
      for (const auto& cv : conns) {
        const QJsonObject co = cv.toObject();
        const QString cid = co[QStringLiteral("id")].toString();
        if (cid.isEmpty()) {
          continue;
        }
        const QString desc =
            QStringLiteral("%1.%2 ↔ %3")
                .arg(co[QStringLiteral("device")].toString(),
                     co[QStringLiteral("devicePort")].toString(),
                     co[QStringLiteral("port")].toString());
        result.append(qMakePair(cid, desc));
      }
    }
  }
  return result;
}

void RunConfigEditor::markModified() {
  if (!modified_) {
    modified_ = true;
    emit modificationChanged(true);
  }
}

bool RunConfigEditor::loadFromFile(const QString& path) {
  config_ = RunConfig();
  if (!RunConfig::loadFromFile(path, &config_)) {
    return false;
  }
  modified_ = false;
  return true;
}

bool RunConfigEditor::saveToFile(const QString& path) {
  return RunConfig::saveToFile(path, config_);
}

// ── 撤销/重做（快照式，仿 ProtocolEditorWidget） ──

void RunConfigEditor::collectLayout() {
  // 布局几何写回 config_.monitors（Monitor 自包含 x/y/w/h，废弃独立 layout
  // 数组）
  const auto geoms = vis_area_->visualizerGeometries();
  for (const auto& g : geoms) {
    for (auto& m : config_.monitors) {
      if (m.id == g.id) {
        m.x = g.rect.x();
        m.y = g.rect.y();
        m.w = g.rect.width();
        m.h = g.rect.height();
        break;
      }
    }
  }
}

void RunConfigEditor::saveSnapshot() {
  collectLayout();  // 布局几何不入 config_，压栈前先收集
  const QByteArray data =
      QJsonDocument(config_.toJson()).toJson(QJsonDocument::Compact);
  // 与栈顶同态（未实际变化）不压栈，避免连续拖拽堆叠无效快照
  if (!snapshots_.isEmpty() && snapshots_.last() == data) {
    return;
  }
  // 撤销后产生新分支：截断 redo 部分
  if (snapshot_index_ < snapshots_.size() - 1) {
    snapshots_.remove(snapshot_index_ + 1,
                      snapshots_.size() - snapshot_index_ - 1);
  }
  snapshots_.append(data);
  while (snapshots_.size() > kMaxSnapshots) {
    snapshots_.removeFirst();
  }
  snapshot_index_ = snapshots_.size() - 1;
  updateUndoRedoActions();
}

void RunConfigEditor::restoreSnapshot(const QByteArray& data) {
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    return;
  }
  config_ = RunConfig();
  if (!config_.fromJson(doc.object())) {
    return;
  }
  refreshUi();  // 重建卡片并应用布局
  // 与 Protocol 一致：快照 index 0（初始状态）视为未修改
  const bool modified = snapshot_index_ != 0;
  if (modified != modified_) {
    modified_ = modified;
    emit modificationChanged(modified);
  }
  updateUndoRedoActions();
}

void RunConfigEditor::updateUndoRedoActions() {
  if (undo_action_) {
    undo_action_->setEnabled(canUndo());
  }
  if (redo_action_) {
    redo_action_->setEnabled(canRedo());
  }
  emit commandsChanged();
  emit undoStateChanged();
}

}  // namespace etest::runconfig
