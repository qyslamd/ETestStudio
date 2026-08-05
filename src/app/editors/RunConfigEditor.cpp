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
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRectF>
#include <QSet>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "AppIconProvider.h"
#include "VisualizationArea.h"
#include "dialogs/MonitorConfigDialog.h"
#include "libui/dock_title_bar/DockTitleBar.h"
#include "logger/Logger.h"
#include "visualizers/VisualizerFactory.h"

using etest::core_ui::AppIconProvider;

namespace {
// dock 关闭按钮 → 同步取消工具栏 toggle 勾选（与三编辑器统一）
void syncDockCloseAction(QAction* action) {
  action->blockSignals(true);
  action->setChecked(false);
  action->blockSignals(false);
}
}  // namespace

namespace etest::app {

// ══════════════════════════════════════════════════════════════════════════════
// RunConfig — .erun 数据模型
// ══════════════════════════════════════════════════════════════════════════════

QJsonObject RunConfig::toJson() const {
  QJsonObject root;
  root[QStringLiteral("version")] = QStringLiteral("1.0");
  if (!testProgram.isEmpty()) {
    root[QStringLiteral("testProgram")] = testProgram;
  }

  QJsonArray monitorsArr;
  for (const auto& m : monitors) {
    QJsonObject mo;
    mo[QStringLiteral("connectionId")] = m.connectionId;
    mo[QStringLiteral("displayMode")] = m.displayMode;
    mo[QStringLiteral("name")] = m.name;
    monitorsArr.append(mo);
  }
  root[QStringLiteral("monitors")] = monitorsArr;

  QJsonArray layoutArr;
  for (const auto& l : layout) {
    QJsonObject lo;
    lo[QStringLiteral("connectionId")] = l.connectionId;
    lo[QStringLiteral("x")] = l.x;
    lo[QStringLiteral("y")] = l.y;
    lo[QStringLiteral("w")] = l.w;
    lo[QStringLiteral("h")] = l.h;
    layoutArr.append(lo);
  }
  root[QStringLiteral("layout")] = layoutArr;

  root[QStringLiteral("runParams")] = runParams;
  return root;
}

bool RunConfig::fromJson(const QJsonObject& obj) {
  testProgram = obj[QStringLiteral("testProgram")].toString();

  // 归一化（格式不变量）：monitors 同 connectionId 去重，保留首个
  monitors.clear();
  const QJsonArray monitorsArr = obj[QStringLiteral("monitors")].toArray();
  QSet<QString> seenMonitors;
  for (const auto& v : monitorsArr) {
    const QJsonObject mo = v.toObject();
    const QString cid = mo[QStringLiteral("connectionId")].toString();
    if (cid.isEmpty() || seenMonitors.contains(cid)) {
      continue;
    }
    seenMonitors.insert(cid);
    Monitor m;
    m.connectionId = cid;
    m.displayMode = mo[QStringLiteral("displayMode")].toString();
    m.name = mo[QStringLiteral("name")].toString();
    monitors.append(m);
  }

  // 归一化：layout 丢弃无对应 monitor 的项 + 同 connectionId 去重（保留首个）
  layout.clear();
  const QJsonArray layoutArr = obj[QStringLiteral("layout")].toArray();
  QSet<QString> seenLayout;
  for (const auto& v : layoutArr) {
    const QJsonObject lo = v.toObject();
    const QString cid = lo[QStringLiteral("connectionId")].toString();
    if (cid.isEmpty() || !seenMonitors.contains(cid) ||
        seenLayout.contains(cid)) {
      continue;
    }
    seenLayout.insert(cid);
    LayoutItem l;
    l.connectionId = cid;
    l.x = lo[QStringLiteral("x")].toDouble();
    l.y = lo[QStringLiteral("y")].toDouble();
    l.w = lo[QStringLiteral("w")].toDouble();
    l.h = lo[QStringLiteral("h")].toDouble();
    layout.append(l);
  }

  runParams = obj[QStringLiteral("runParams")].toObject();
  return true;
}

// ══════════════════════════════════════════════════════════════════════════════
// RunConfigEditor
// ══════════════════════════════════════════════════════════════════════════════

RunConfigEditor::RunConfigEditor(const QString& id, QWidget* parent)
    : QMainWindow(parent), file_path_(id) {
  initUi();
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
  if (saveToFile(path)) {
    modified_ = false;
    emit modificationChanged(false);
    return true;
  }
  return false;
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
}

// ── 嵌入模式 / 主题图标 ──

void RunConfigEditor::setEmbeddedMode(bool embedded) {
  embedded_ = embedded;
  if (embedded) {
    menuBar()->hide();
  } else {
    menuBar()->show();
  }
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
  if (add_monitor_action_) {
    add_monitor_action_->setIcon(provider.icon(QStringLiteral("monitor")));
  }
  if (align_btn_) {
    align_btn_->setIcon(provider.icon(QStringLiteral("topo_align")));
  }
  if (dist_btn_) {
    dist_btn_->setIcon(provider.icon(QStringLiteral("topo_distribute")));
  }
  if (info_toggle_action_) {
    info_toggle_action_->setIcon(provider.icon(QStringLiteral("settings")));
  }
}

bool RunConfigEditor::eventFilter(QObject* obj, QEvent* event) {
  // 用户点 dock 关闭按钮 → 同步取消工具栏 toggle 勾选（与三编辑器统一）
  if (obj == info_dock_ && event->type() == QEvent::Close) {
    syncDockCloseAction(info_toggle_action_);
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
  add_monitor_action_ = toolbar_->addAction(QStringLiteral("添加监听器"));
  add_monitor_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("monitor")));
  add_monitor_action_->setToolTip(QStringLiteral("添加一个监听器卡片"));
  connect(add_monitor_action_, &QAction::triggered, this,
          [this]() { onAddMonitorClicked(); });

  // 信息面板显示/隐藏 toggle（与三编辑器统一：dock 可关，工具栏 toggle 重开）
  toolbar_->addSeparator();
  info_toggle_action_ = toolbar_->addAction(QStringLiteral("运行配置"));
  info_toggle_action_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("settings")));
  info_toggle_action_->setCheckable(true);
  info_toggle_action_->setChecked(true);
  info_toggle_action_->setToolTip(QStringLiteral("显示/隐藏信息面板"));

  // 排列按钮（QToolButton + QMenu，参考拓扑 TopologyEditorWidget）
  align_btn_ = new QToolButton(this);
  align_btn_->setText(QStringLiteral("排列"));
  align_btn_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("topo_align")));
  align_btn_->setPopupMode(QToolButton::InstantPopup);
  auto* alignMenu = new QMenu(align_btn_);
  auto addAlign = [this, alignMenu](const QString& text,
                                    VisualizationArea::AlignType t) {
    auto* act = alignMenu->addAction(text);
    connect(act, &QAction::triggered, this,
            [this, t]() { vis_area_->alignVisualizers(t); });
  };
  addAlign(QStringLiteral("左对齐"), VisualizationArea::AlignType::Left);
  addAlign(QStringLiteral("水平居中"), VisualizationArea::AlignType::HCenter);
  addAlign(QStringLiteral("右对齐"), VisualizationArea::AlignType::Right);
  addAlign(QStringLiteral("顶端对齐"), VisualizationArea::AlignType::Top);
  addAlign(QStringLiteral("垂直居中"), VisualizationArea::AlignType::VCenter);
  addAlign(QStringLiteral("底端对齐"), VisualizationArea::AlignType::Bottom);
  align_btn_->setMenu(alignMenu);
  align_btn_->setEnabled(false);  // 初始无选中，禁用直到选中 >=2
  toolbar_->addWidget(align_btn_);

  // 分布按钮
  dist_btn_ = new QToolButton(this);
  dist_btn_->setText(QStringLiteral("分布"));
  dist_btn_->setIcon(
      AppIconProvider::instance().icon(QStringLiteral("topo_distribute")));
  dist_btn_->setPopupMode(QToolButton::InstantPopup);
  auto* distMenu = new QMenu(dist_btn_);
  auto addDist = [this, distMenu](const QString& text,
                                  VisualizationArea::DistributeType t) {
    auto* act = distMenu->addAction(text);
    connect(act, &QAction::triggered, this,
            [this, t]() { vis_area_->distributeVisualizers(t); });
  };
  addDist(QStringLiteral("水平分布"),
          VisualizationArea::DistributeType::Horizontal);
  addDist(QStringLiteral("垂直分布"),
          VisualizationArea::DistributeType::Vertical);
  dist_btn_->setMenu(distMenu);
  dist_btn_->setEnabled(false);  // 初始无选中，禁用直到选中 >=2
  toolbar_->addWidget(dist_btn_);

  // 信息区：停靠面板（Left，禁浮动禁关闭，可拖拽改停靠位置）
  auto* infoWidget = new QWidget(this);
  auto* infoLayout = new QVBoxLayout(infoWidget);
  infoLayout->setContentsMargins(8, 8, 8, 8);
  file_label_ = new QLabel(QStringLiteral("未打开文件"), infoWidget);
  file_label_->setWordWrap(true);
  file_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  file_label_->setObjectName(QStringLiteral("runConfigFile"));
  test_program_label_ =
      new QLabel(QStringLiteral("测试程序: (未选择)"), infoWidget);
  test_program_label_->setObjectName(QStringLiteral("runConfigTestProgram"));
  infoLayout->addWidget(file_label_);
  infoLayout->addWidget(test_program_label_);
  infoLayout->addStretch(1);

  info_dock_ = new QDockWidget(QStringLiteral("运行配置"), this);
  info_dock_->setObjectName(QStringLiteral("runConfigInfoDock"));
  info_dock_->setWidget(infoWidget);
  // 与三编辑器统一：AllDockWidgetFeatures（可关可浮可拖），关闭由工具栏 toggle 重开
  info_dock_->setFeatures(QDockWidget::AllDockWidgetFeatures);
  info_dock_->setTitleBarWidget(new ::etest::ui::DockTitleBar(
      QStringLiteral("运行配置"), info_dock_));
  info_dock_->installEventFilter(this);
  addDockWidget(Qt::LeftDockWidgetArea, info_dock_);

  // 主视图：可视化区（编辑态，监听器卡片 + 手动布局）
  vis_area_ = new VisualizationArea(this);
  vis_area_->setEditMode(true);
  setCentralWidget(vis_area_);

  // 布局被改动（拖拽/resize/排列/分布）→ 记录快照 + 置脏，关闭 tab 时提示保存
  connect(vis_area_, &VisualizationArea::layoutChanged, this, [this]() {
    saveSnapshot();
    markModified();
  });

  // 右键"关闭可视化"（编辑态删除卡片）同步清理 config_.monitors
  connect(vis_area_, &VisualizationArea::visualizerClosed, this,
          [this](const QString& cid) {
            for (int i = 0; i < config_.monitors.size(); ++i) {
              if (config_.monitors[i].connectionId == cid) {
                saveSnapshot();  // 删除前压快照
                config_.monitors.removeAt(i);
                markModified();
                return;
              }
            }
          });

  // 排列/分布门控：选中 ≥2 张卡片才启用
  connect(vis_area_->scene(), &QGraphicsScene::selectionChanged, this,
          [this]() {
            const bool enabled = vis_area_->selectedVisualizerCount() >= 2;
            if (align_btn_) {
              align_btn_->setEnabled(enabled);
            }
            if (dist_btn_) {
              dist_btn_->setEnabled(enabled);
            }
          });

  // 信息面板可见性与工具栏 toggle 同步（dock 关闭按钮由 eventFilter 拦截）
  connect(info_toggle_action_, &QAction::toggled, this,
          [this](bool checked) { info_dock_->setVisible(checked); });
}

void RunConfigEditor::refreshUi() {
  file_label_->setText(file_path_.isEmpty() ? QStringLiteral("未打开文件")
                                            : file_path_);
  test_program_label_->setText(QStringLiteral("测试程序: %1")
                                   .arg(config_.testProgram.isEmpty()
                                            ? QStringLiteral("(未选择)")
                                            : config_.testProgram));

  // 重建可视化区卡片：按 monitors 创建 preview visualizer，再应用 layout
  const auto oldIds = vis_area_->activeChannels();
  for (const auto& id : oldIds) {
    vis_area_->removeVisualizer(id);
  }
  for (const auto& m : config_.monitors) {
    // parent 传 nullptr：visualizer 须为 top-level 才能被 VisualizerProxy
    // setWidget 嵌入（传 this 会被警告 "cannot embed widget"）
    auto* vis = createVisualizerFor(m.connectionId, m.displayMode, QString(),
                                    m.name, nullptr);
    vis_area_->addVisualizer(m.connectionId, vis);
  }
  for (const auto& l : config_.layout) {
    vis_area_->setVisualizerGeometry(l.connectionId,
                                     QRectF(l.x, l.y, l.w, l.h));
  }
  // 无 layout 项的新监听器：递增默认位置，避免重叠
  int fallback = 0;
  for (const auto& m : config_.monitors) {
    bool hasLayout = false;
    for (const auto& l : config_.layout) {
      if (l.connectionId == m.connectionId) {
        hasLayout = true;
        break;
      }
    }
    if (!hasLayout) {
      vis_area_->setVisualizerGeometry(
          m.connectionId,
          QRectF(20.0 + fallback * 40, 20.0 + fallback * 40, 320, 200));
      ++fallback;
    }
  }
}

void RunConfigEditor::onAddMonitorClicked() {
  // 复用 page1 的通道选择对话框：从连接列表选 + 点 visualizer 类型即添加
  if (!channel_dialog_) {
    channel_dialog_ = new MonitorConfigDialog(this);
    connect(channel_dialog_, &MonitorConfigDialog::visualizerChosen, this,
            [this](const QString& cid, const QString& mode) {
              LOG_INFO("RUNCONFIG", "visualizerChosen cid={} mode={}",
                       cid.toStdString(), mode.toStdString());
              for (const auto& m : config_.monitors) {
                if (m.connectionId == cid) {
                  return;  // 已存在，忽略
                }
              }
              saveSnapshot();  // 添加前压快照
              RunConfig::Monitor m;
              m.connectionId = cid;
              m.displayMode = mode;
              m.name = cid;
              config_.monitors.append(m);
              markModified();
              refreshUi();
            });
  }
  channel_dialog_->setConnections(loadConnectionsFromProject());
  channel_dialog_->show();
  channel_dialog_->raise();
  channel_dialog_->activateWindow();
}

// 从 .erun 所在项目读拓扑连接（向上找含 topology 目录的项目根）
QList<QPair<QString, QString>> RunConfigEditor::loadConnectionsFromProject()
    const {
  QList<QPair<QString, QString>> result;
  if (file_path_.isEmpty()) {
    return result;
  }
  // 先查 .erun 所在目录，再逐级向上；.erun 就在项目根时也能找到 topology
  QDir dir = QFileInfo(file_path_).absoluteDir();
  for (;;) {
    if (dir.exists(QStringLiteral("topology"))) {
      const QString topoPath =
          dir.filePath(QStringLiteral("topology/topology.etopo"));
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
      break;
    }
    if (!dir.cdUp()) {
      break;
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
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    LOG_WARN("RUNCONFIG", "解析 .erun 失败: {} ({})", path.toStdString(),
             error.errorString().toStdString());
    return false;
  }
  config_ = RunConfig();
  if (!config_.fromJson(doc.object())) {
    return false;
  }
  modified_ = false;
  return true;
}

bool RunConfigEditor::saveToFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }
  const QJsonDocument doc(config_.toJson());
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  return true;
}

// ── 撤销/重做（快照式，仿 ProtocolEditorWidget） ──

void RunConfigEditor::collectLayout() {
  config_.layout.clear();
  const auto geoms = vis_area_->visualizerGeometries();
  for (const auto& g : geoms) {
    RunConfig::LayoutItem l;
    l.connectionId = g.connectionId;
    l.x = g.rect.x();
    l.y = g.rect.y();
    l.w = g.rect.width();
    l.h = g.rect.height();
    config_.layout.append(l);
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
}

}  // namespace etest::app
