#include "ProtocolEditorWidget.h"

#include <QComboBox>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QResizeEvent>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <QtConcurrent/QtConcurrentRun>

#include <memory>
#include <string>
#include <vector>

#include "IcdBitLayoutView.h"
#include "IcdNodeTreeWidget.h"
#include "IcdPropertyPanel.h"
#include "AppIconProvider.h"
#include "ThemeManager.h"
#include "libui/dock_title_bar/DockTitleBar.h"
#include "format/json_parser.hpp"
#include "format/json_serializer.hpp"

namespace etest::protocol {
namespace {

void updateMaxBits(const icd::Node& node, int& max_bits) {
  int end = (node.offset() * 8) + node.bit_offset() + node.bit_width();
  if (end > max_bits)
    max_bits = end;
  for (const auto& child : node.children())
    updateMaxBits(*child, max_bits);
}

int calcFrameLength(const icd::Frame& frame) {
  if (frame.roots().empty())
    return 0;
  int max_bits = 0;
  for (const auto& root : frame.roots())
    updateMaxBits(*root, max_bits);
  return (max_bits + 7) / 8;
}

}  // namespace

// ──────────────────────────────────────────────────────────────
// ProtocolEditorWidget
// ──────────────────────────────────────────────────────────────
ProtocolEditorWidget::ProtocolEditorWidget(QWidget* parent)
    : QMainWindow(parent) {
  setAutoFillBackground(true);

  initUi();
  initSignals();

  // Debounce timer coalesces rapid property edits into one refresh
  modified_debounce_ = new QTimer(this);
  modified_debounce_->setSingleShot(true);
  modified_debounce_->setInterval(0);
  connect(modified_debounce_, &QTimer::timeout, this, [this]() {
    if (current_frame_) {
      bit_view_->loadFromFrame(*current_frame_);
    }
  });

  // Theme switch → reload toolbar icons
  connect(&etest::app::ThemeManager::instance(),
          &etest::app::ThemeManager::themeChanged, this,
          &ProtocolEditorWidget::reloadToolbarIcons);
}

ProtocolEditorWidget::~ProtocolEditorWidget() {}

// ── Embedded mode ──────────────────────────────────────────────
void ProtocolEditorWidget::setEmbeddedMode(bool embedded) {
  embedded_ = embedded;
  if (embedded) {
    menuBar()->hide();
    for (auto* tb : findChildren<QToolBar*>())
      tb->hide();
  } else {
    menuBar()->show();
    for (auto* tb : findChildren<QToolBar*>())
      tb->show();
  }
}

// ── Status message routing ─────────────────────────────────────
void ProtocolEditorWidget::showStatusMessage(const QString& msg) {
  if (embedded_) {
    auto* w = window();
    if (auto* mainWin = qobject_cast<QMainWindow*>(w))
      mainWin->statusBar()->showMessage(msg, 3000);
  } else {
    statusBar()->showMessage(msg, 3000);
  }
}

QString ProtocolEditorWidget::displayName() const {
  if (current_file_.isEmpty())
    return QStringLiteral("未命名协议");
  return QFileInfo(current_file_).fileName();
}

bool ProtocolEditorWidget::isModified() const {
  return modified_;
}

bool ProtocolEditorWidget::save() {
  if (current_file_.isEmpty())
    return false;
  if (saveEproto(current_file_)) {
    setModified(false);
    return true;
  }
  return false;
}

bool ProtocolEditorWidget::saveAs(const QString& path) {
  QString old = current_file_;
  current_file_ = path;
  if (saveEproto(path)) {
    setModified(false);
    emit editorIdChanged(old, path);
    return true;
  }
  current_file_ = old;
  return false;
}

QString ProtocolEditorWidget::filePath() const {
  return current_file_;
}

QString ProtocolEditorWidget::editorId() const {
  if (current_file_.isEmpty())
    return QStringLiteral("editor://protocol/new");
  return current_file_;
}

QWidget* ProtocolEditorWidget::widget() {
  return this;
}

QString ProtocolEditorWidget::editorType() const {
  return QStringLiteral("protocol");
}

QObject* ProtocolEditorWidget::signalObject() {
  return this;
}

bool ProtocolEditorWidget::canUndo() const {
  return snapshot_index_ > 0;
}
bool ProtocolEditorWidget::canRedo() const {
  return snapshot_index_ < snapshots_.size() - 1;
}
void ProtocolEditorWidget::undo() {
  if (!canUndo())
    return;
  --snapshot_index_;
  restoreSnapshot(snapshots_[snapshot_index_]);
  setModified(snapshot_index_ != 0);
}
void ProtocolEditorWidget::redo() {
  if (!canRedo())
    return;
  ++snapshot_index_;
  restoreSnapshot(snapshots_[snapshot_index_]);
  setModified(snapshot_index_ != 0);
}

void ProtocolEditorWidget::setEditorId(const QString& id) {
  if (id == current_file_)
    return;
  current_file_ = id;

  if (!QFileInfo::exists(id))
    return;

  // Bump generation; any stale lambda with an older generation discards its
  // result
  int thisGeneration = ++load_generation_;

  // 如果已有之前的异步加载未完成，取消它
  if (load_watcher_) {
    load_watcher_->cancel();
    load_watcher_->deleteLater();
    load_watcher_ = nullptr;
  }

  // 先清空旧数据，显示空白 UI + loading 覆层
  clearAll();
  showLoadingOverlay();

  // 后台解析 ICD 文件
  load_watcher_ = new QFutureWatcher<std::shared_ptr<icd::Repository>>(this);
  connect(load_watcher_,
          &QFutureWatcher<std::shared_ptr<icd::Repository>>::finished, this,
          [this, thisGeneration]() {
            // Stale result — a newer load has superceded this one
            if (thisGeneration != load_generation_)
              return;

            auto repoPtr = load_watcher_->result();
            load_watcher_->deleteLater();
            load_watcher_ = nullptr;

            if (!repoPtr) {
              hideLoadingOverlay();
              QMessageBox::warning(
                  this, QStringLiteral("加载失败"),
                  QStringLiteral("无法加载协议文件: %1").arg(current_file_));
              return;
            }

            repo_ = std::move(*repoPtr);
            populateFrames();

            if (!repo_.frames().empty())
              setCurrentFrame(repo_.frames()[0].get());

            saveSnapshot();
            hideLoadingOverlay();
          });
  load_watcher_->setFuture(
      QtConcurrent::run([id]() -> std::shared_ptr<icd::Repository> {
        auto result = icd::format::deserialize_repository(
            std::filesystem::path(id.toStdWString()));
        if (!result)
          return nullptr;
        return std::make_shared<icd::Repository>(std::move(*result));
      }));
}

// ── Loading overlay ─────────────────────────────────────────

void ProtocolEditorWidget::showLoadingOverlay() {
  if (!loading_overlay_) {
    loading_overlay_ = new QWidget(this);
    loading_overlay_->setObjectName(QStringLiteral("PhLoadingOverlay"));
    loading_overlay_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    loading_overlay_->setStyleSheet(
        QStringLiteral("background-color:%1;")
            .arg(palette().window().color().name()));
    auto* lay = new QVBoxLayout(loading_overlay_);
    lay->setAlignment(Qt::AlignCenter);
    auto* label =
        new QLabel(QStringLiteral("正在加载协议文件..."), loading_overlay_);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        QStringLiteral("font-size:13px;color:#888;background:transparent;"));
    lay->addWidget(label);
  }
  loading_overlay_->setGeometry(centralWidget()->rect());
  loading_overlay_->raise();
  loading_overlay_->show();
}

void ProtocolEditorWidget::hideLoadingOverlay() {
  if (loading_overlay_)
    loading_overlay_->hide();
}

void ProtocolEditorWidget::resizeEvent(QResizeEvent* event) {
  QMainWindow::resizeEvent(event);
  if (loading_overlay_ && loading_overlay_->isVisible())
    loading_overlay_->setGeometry(centralWidget()->rect());
}

void ProtocolEditorWidget::hideEvent(QHideEvent* event) {
  QMainWindow::hideEvent(event);
}

// ── Save .eproto JSON ─────────────────────────────────────────
bool ProtocolEditorWidget::saveEproto(const QString& path) {
  auto result = icd::format::serialize_repository(
      std::filesystem::path(path.toStdWString()), repo_);
  return result.has_value();
}

// ── UI ─────────────────────────────────────────────────────────
void ProtocolEditorWidget::initUi() {
  setAutoFillBackground(true);

  // ── Icon loader (theme-aware) ──
  auto protoIcon = [](const QString& name) {
    return etest::app::AppIconProvider::instance().icon(name);
  };

  // ── QToolBar (帧操作 + 帧属性) ──
  auto* toolbar = addToolBar(QStringLiteral("帧工具栏"));
  toolbar->setObjectName(QStringLiteral("protocolToolbar"));
  toolbar->setMovable(false);
  toolbar->setFloatable(false);

  // 新建帧
  new_frame_action_ = new QAction(
      protoIcon(QStringLiteral("protocol_new_frame")),
      QStringLiteral("+帧"), this);
  new_frame_action_->setToolTip(QStringLiteral("新建帧"));
  toolbar->addAction(new_frame_action_);

  // 删除帧
  delete_frame_action_ = new QAction(
      protoIcon(QStringLiteral("protocol_delete_frame")),
      QStringLiteral("-帧"), this);
  delete_frame_action_->setToolTip(QStringLiteral("删除当前帧"));
  delete_frame_action_->setEnabled(false);
  toolbar->addAction(delete_frame_action_);

  toolbar->addSeparator();

  // 撤销 / 重做
  undo_action_ = new QAction(
      protoIcon(QStringLiteral("undo")),
      QStringLiteral("撤销"), this);
  undo_action_->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
  undo_action_->setEnabled(false);
  toolbar->addAction(undo_action_);

  redo_action_ = new QAction(
      protoIcon(QStringLiteral("redo")),
      QStringLiteral("重做"), this);
  redo_action_->setToolTip(QStringLiteral("重做 (Ctrl+Y)"));
  redo_action_->setEnabled(false);
  toolbar->addAction(redo_action_);

  toolbar->addSeparator();

  // 帧属性区
  auto* title_label = new QLabel(QStringLiteral("帧属性"), toolbar);
  title_label->setObjectName(QStringLiteral("protocolTitleLabel"));
  toolbar->addWidget(title_label);

  frame_name_label_ = new QLabel(QStringLiteral("(无帧)"), toolbar);
  frame_name_label_->setObjectName(QStringLiteral("frameNameLabel"));
  toolbar->addWidget(frame_name_label_);

  frame_type_combo_ = new QComboBox(toolbar);
  frame_type_combo_->addItem(QStringLiteral("数据 (Data)"));
  frame_type_combo_->addItem(QStringLiteral("命令 (Cmd)"));
  frame_type_combo_->addItem(QStringLiteral("数据/命令 (DataCfg)"));
  frame_type_combo_->setEnabled(false);
  toolbar->addWidget(frame_type_combo_);

  // 字节序切换按钮（取代之前的 QComboBox）
  byte_order_btn_ = new QToolButton(toolbar);
  byte_order_btn_->setIcon(protoIcon(QStringLiteral("protocol_byte_order")));
  byte_order_btn_->setText(QStringLiteral("LE"));
  byte_order_btn_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  byte_order_btn_->setCheckable(true);
  byte_order_btn_->setToolTip(QStringLiteral("切换字节序：小端(LE) / 大端(BE)"));
  byte_order_btn_->setEnabled(false);
  toolbar->addWidget(byte_order_btn_);

  frame_id_label_ = new QLabel(QStringLiteral("ID: -"), toolbar);
  frame_id_label_->setObjectName(QStringLiteral("idLabel"));
  toolbar->addWidget(frame_id_label_);

  frame_length_label_ = new QLabel(QStringLiteral("长度: -"), toolbar);
  frame_length_label_->setObjectName(QStringLiteral("lengthLabel"));

  // 弹簧 + 长度标签（右对齐）
  auto* spacer = new QWidget(toolbar);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(spacer);

  toolbar->addWidget(frame_length_label_);

  toolbar->addSeparator();

  // 面板开关 toggle actions
  node_tree_toggle_action_ = new QAction(
      protoIcon(QStringLiteral("protocol_node_tree")),
      QStringLiteral("节点列表"), this);
  node_tree_toggle_action_->setCheckable(true);
  node_tree_toggle_action_->setChecked(true);
  node_tree_toggle_action_->setToolTip(QStringLiteral("显示/隐藏节点列表"));
  toolbar->addAction(node_tree_toggle_action_);

  property_toggle_action_ = new QAction(
      protoIcon(QStringLiteral("protocol_property")),
      QStringLiteral("属性面板"), this);
  property_toggle_action_->setCheckable(true);
  property_toggle_action_->setChecked(true);
  property_toggle_action_->setToolTip(QStringLiteral("显示/隐藏属性面板"));
  toolbar->addAction(property_toggle_action_);

  // ── Dock: 节点列表 (左侧) ──
  node_tree_ = new IcdNodeTreeWidget(this);
  node_tree_->setMinimumWidth(200);
  node_tree_->setObjectName(QStringLiteral("protocolNodeTree"));

  node_tree_dock_ = new QDockWidget(QStringLiteral("节点列表"), this);
  node_tree_dock_->setObjectName(QStringLiteral("protocolNodeTreeDock"));
  node_tree_dock_->setWidget(node_tree_);
  addDockWidget(Qt::LeftDockWidgetArea, node_tree_dock_);

  // ── Dock: 属性面板 (右侧) ──
  property_panel_ = new IcdPropertyPanel(this);
  property_panel_->setMinimumWidth(220);
  property_panel_->setObjectName(QStringLiteral("protocolPropertyPanel"));

  property_dock_ = new QDockWidget(QStringLiteral("属性面板"), this);
  property_dock_->setObjectName(QStringLiteral("protocolPropertyDock"));
  property_dock_->setWidget(property_panel_);
  addDockWidget(Qt::RightDockWidgetArea, property_dock_);

  // Custom title bars for full control over button size and icon
  node_tree_dock_->setTitleBarWidget(
      new ::etest::ui::DockTitleBar(QStringLiteral("节点列表"), node_tree_dock_));
  property_dock_->setTitleBarWidget(
      new ::etest::ui::DockTitleBar(QStringLiteral("属性面板"), property_dock_));

  // Dock features: 允许关闭/浮动/拖拽/标签页组合
  for (auto* dock : {node_tree_dock_, property_dock_}) {
    dock->setFeatures(QDockWidget::AllDockWidgetFeatures);
  }

  // ── Central Widget: 位图视图 ──
  bit_view_ = new IcdBitLayoutView(this);
  setCentralWidget(bit_view_);

  // ── StatusBar ──
  statusBar()->showMessage(QStringLiteral("就绪"));
}

// ── Signals ───────────────────────────────────────────────────
void ProtocolEditorWidget::initSignals() {
  // Frame selection from tree
  connect(node_tree_, &IcdNodeTreeWidget::frameSelected, this,
          [this](icd::Frame* frame) { setCurrentFrame(frame); });

  // Node selection from tree → property panel + bit view highlight
  connect(node_tree_, &IcdNodeTreeWidget::nodeSelected, this,
          [this](icd::Node* node) {
            if (node) {
              property_panel_->showNode(*node);
              bit_view_->highlightBlock(
                  QString::fromStdString(std::string(node->name())));
              showStatusMessage(
                  QStringLiteral("Node: %1  |  Offset: %2  |  Bit: %3~%4")
                      .arg(QString::fromStdString(std::string(node->name())))
                      .arg(node->offset())
                      .arg(node->bit_offset())
                      .arg(node->bit_offset() + node->bit_width() - 1));
            }
          });

  // Bit block clicked → highlight block + property panel
  connect(bit_view_, &IcdBitLayoutView::blockClicked, this,
          [this](const QString& name) {
            if (!current_frame_)
              return;
            bit_view_->highlightBlock(name);
            auto* node = const_cast<icd::Node*>(
                current_frame_->find(name.toStdString()));
            if (node) {
              property_panel_->showNode(*node);
              node_tree_->selectNode(node);
              showStatusMessage(
                  QStringLiteral("Node: %1  |  Offset: %2  |  Bit: %3~%4")
                      .arg(QString::fromStdString(std::string(node->name())))
                      .arg(node->offset())
                      .arg(node->bit_offset())
                      .arg(node->bit_offset() + node->bit_width() - 1));
            }
          });

  // Bit block hover → reveal tree node (scroll only, no selection change)
  connect(bit_view_, &IcdBitLayoutView::blockHovered, this,
          [this](const QString& name, bool /*on*/) {
            if (!current_frame_)
              return;
            const auto* node = current_frame_->find(name.toStdString());
            if (node) {
              node_tree_->revealNode(node);
            }
          });

  // Bit block context menu actions
  connect(
      bit_view_, &IcdBitLayoutView::contextMenuAction, this,
      [this](const QString& name, const QString& action) {
        if (!current_frame_)
          return;
        auto* frame = current_frame_;

        if (action == QStringLiteral("delete")) {
          const auto* node = current_frame_->find(name.toStdString());
          if (!node)
            return;

          saveSnapshot();
          auto* parent = const_cast<icd::Node*>(node->parent());
          if (!parent) {
            // Root node — find in roots and remove
            const auto& roots = frame->roots();
            for (std::size_t i = 0; i < roots.size(); ++i) {
              if (roots[i].get() == node) {
                frame->remove_root(i);
                break;
              }
            }
          } else {
            // Child node — find in parent's children and remove
            const auto& children = parent->children();
            for (std::size_t i = 0; i < children.size(); ++i) {
              if (children[i].get() == node) {
                parent->remove_child(i);
                break;
              }
            }
          }
          refreshAndSelectFrame(frame);
          setModified(true);
          showStatusMessage(QStringLiteral("已删除信号: %1").arg(name));

        } else if (action == QStringLiteral("addBefore") ||
                   action == QStringLiteral("addAfter")) {
          const auto* node = current_frame_->find(name.toStdString());
          if (!node)
            return;

          saveSnapshot();
          auto new_node = std::make_unique<icd::Node>(
              "NewNode", "", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none,
              icd::NodeAttrs{});

          auto* parent = const_cast<icd::Node*>(node->parent());
          if (!parent) {
            // Root node — find index and insert adjacent
            const auto& roots = frame->roots();
            for (std::size_t i = 0; i < roots.size(); ++i) {
              if (roots[i].get() == node) {
                std::size_t pos =
                    (action == QStringLiteral("addBefore")) ? i : i + 1;
                frame->insert_root(pos, std::move(new_node));
                break;
              }
            }
          } else {
            // Child node — find in parent's children and insert adjacent
            const auto& children = parent->children();
            for (std::size_t i = 0; i < children.size(); ++i) {
              if (children[i].get() == node) {
                std::size_t pos =
                    (action == QStringLiteral("addBefore")) ? i : i + 1;
                parent->insert_child(pos, std::move(new_node));
                break;
              }
            }
          }
          refreshAndSelectFrame(frame);
          setModified(true);
          showStatusMessage(QStringLiteral("已插入信号 (相邻: %1)").arg(name));
        }
      });

  // Node property modified (debounced to avoid signal storm)
  connect(property_panel_, &IcdPropertyPanel::nodeModified, this, [this]() {
    setModified(true);
    modified_debounce_->start();
  });

  // Frame property modified (type/byte order) — update toolbar only
  connect(property_panel_, &IcdPropertyPanel::frameModified, this, [this]() {
    setModified(true);
    updateToolbar();
  });

  // New frame (action)
  connect(new_frame_action_, &QAction::triggered, this, [this]() {
    saveSnapshot();
    // Find max existing id
    int max_id = 0;
    for (const auto& f : repo_.frames()) {
      if (f->id() > max_id)
        max_id = f->id();
    }
    int new_id = max_id + 1;
    auto name = "Frame_" + std::to_string(new_id);

    auto frame = std::make_unique<icd::Frame>(
        new_id, name, "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto* frame_ptr = frame.get();
    repo_.add_frame(std::move(frame));
    refreshAndSelectFrame(frame_ptr);
    setModified(true);
  });

  // Delete frame (action)
  connect(delete_frame_action_, &QAction::triggered, this, [this]() {
    if (!current_frame_)
      return;
    saveSnapshot();
    int id = current_frame_->id();
    setCurrentFrame(nullptr);
    if (repo_.remove_frame(id)) {
      populateFrames();
      if (!repo_.frames().empty())
        setCurrentFrame(repo_.frames()[0].get());
      setModified(true);
      showStatusMessage(QStringLiteral("已删除帧"));
    }
  });

  // Frame type combo changed
  connect(frame_type_combo_,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            if (!current_frame_)
              return;
            saveSnapshot();
            icd::FrameType new_type;
            switch (index) {
              case 0:
                new_type = icd::FrameType::data;
                break;
              case 1:
                new_type = icd::FrameType::cmd;
                break;
              case 2:
                new_type = icd::FrameType::data_cmd;
                break;
              default:
                return;
            }
            current_frame_->setType(new_type);
            setModified(true);
          });

  // Byte order button toggled
  connect(byte_order_btn_, &QToolButton::toggled, this, [this](bool checked) {
    if (!current_frame_)
      return;
    saveSnapshot();
    auto order = checked ? icd::ByteOrder::big_endian
                         : icd::ByteOrder::little_endian;
    current_frame_->setOrder(order);
    byte_order_btn_->setText(checked ? QStringLiteral("BE")
                                     : QStringLiteral("LE"));
    setModified(true);
  });

  // ── Undo / Redo ──
  connect(undo_action_, &QAction::triggered, this, [this]() {
    undo();
    updateToolbar();
  });
  connect(redo_action_, &QAction::triggered, this, [this]() {
    redo();
    updateToolbar();
  });
  // Update undo/redo enabled state after each edit
  connect(this, &ProtocolEditorWidget::modificationChanged, this, [this]() {
    undo_action_->setEnabled(canUndo());
    redo_action_->setEnabled(canRedo());
  });

  // ── Dock toggle actions ──
  connect(node_tree_toggle_action_, &QAction::toggled, this,
          [this](bool checked) { node_tree_dock_->setVisible(checked); });
  connect(node_tree_dock_, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            node_tree_toggle_action_->blockSignals(true);
            node_tree_toggle_action_->setChecked(visible);
            node_tree_toggle_action_->blockSignals(false);
          });

  connect(property_toggle_action_, &QAction::toggled, this,
          [this](bool checked) { property_dock_->setVisible(checked); });
  connect(property_dock_, &QDockWidget::visibilityChanged, this,
          [this](bool visible) {
            property_toggle_action_->blockSignals(true);
            property_toggle_action_->setChecked(visible);
            property_toggle_action_->blockSignals(false);
          });

  // ── Context-menu from tree widget ──────────────────────────
  connect(node_tree_, &IcdNodeTreeWidget::addFrameRequested, this, [this]() {
    saveSnapshot();
    int max_id = 0;
    for (const auto& f : repo_.frames()) {
      if (f->id() > max_id)
        max_id = f->id();
    }
    int new_id = max_id + 1;
    auto name = "Frame_" + std::to_string(new_id);
    auto frame = std::make_unique<icd::Frame>(
        new_id, name, "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto* frame_ptr = frame.get();
    repo_.add_frame(std::move(frame));
    refreshAndSelectFrame(frame_ptr);
    setModified(true);
  });

  connect(node_tree_, &IcdNodeTreeWidget::deleteFrameRequested, this,
          [this](int frameId) {
            saveSnapshot();
            if (current_frame_ && current_frame_->id() == frameId) {
              setCurrentFrame(nullptr);
            }
            if (repo_.remove_frame(frameId)) {
              populateFrames();
              if (!repo_.frames().empty())
                setCurrentFrame(repo_.frames()[0].get());
              setModified(true);
            }
          });

  connect(
      node_tree_, &IcdNodeTreeWidget::addNodeRequested, this,
      [this](int frameId, const icd::Node* parentNode) {
        saveSnapshot();
        for (const auto& frame_ptr : repo_.frames()) {
          if (frame_ptr->id() == frameId) {
            icd::Frame* frame = frame_ptr.get();
            auto node = std::make_unique<icd::Node>(
                "NewNode", "", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none,
                icd::NodeAttrs{});
            if (parentNode) {
              const_cast<icd::Node*>(parentNode)->add_child(std::move(node));
            } else {
              frame->add_root(std::move(node));
            }
            refreshAndSelectFrame(frame);
            setModified(true);
            break;
          }
        }
      });

  connect(node_tree_, &IcdNodeTreeWidget::deleteNodeRequested, this,
          [this](int frameId, const icd::Node* node) {
            if (!node)
              return;
            saveSnapshot();
            for (const auto& frame_ptr : repo_.frames()) {
              if (frame_ptr->id() != frameId)
                continue;
              auto* frame = frame_ptr.get();

              // Check root nodes
              const auto& roots = frame->roots();
              for (std::size_t i = 0; i < roots.size(); ++i) {
                if (roots[i].get() == node) {
                  frame->remove_root(i);
                  refreshAndSelectFrame(frame);
                  setModified(true);
                  return;
                }
              }

              // Check child nodes
              for (auto* n : frame->nodes()) {
                if (n == node)
                  continue;
                const auto& children = n->children();
                for (std::size_t i = 0; i < children.size(); ++i) {
                  if (children[i].get() == node) {
                    n->remove_child(i);
                    refreshAndSelectFrame(frame);
                    setModified(true);
                    return;
                  }
                }
              }
              break;
            }
          });
}

// ── Toolbar update ────────────────────────────────────────────
void ProtocolEditorWidget::updateToolbar() {
  if (current_frame_) {
    frame_name_label_->setText(
        QString::fromStdString(std::string(current_frame_->name())));
    frame_id_label_->setText(
        QStringLiteral("ID: %1").arg(current_frame_->id()));

    // Block signals to avoid recursive modification
    frame_type_combo_->blockSignals(true);
    switch (current_frame_->type()) {
      case icd::FrameType::data:
        frame_type_combo_->setCurrentIndex(0);
        break;
      case icd::FrameType::cmd:
        frame_type_combo_->setCurrentIndex(1);
        break;
      case icd::FrameType::data_cmd:
        frame_type_combo_->setCurrentIndex(2);
        break;
    }
    frame_type_combo_->blockSignals(false);

    byte_order_btn_->blockSignals(true);
    byte_order_btn_->setChecked(
        current_frame_->order() == icd::ByteOrder::big_endian);
    byte_order_btn_->setText(
        current_frame_->order() == icd::ByteOrder::little_endian
            ? QStringLiteral("LE")
            : QStringLiteral("BE"));
    byte_order_btn_->blockSignals(false);

    frame_length_label_->setText(
        QStringLiteral("长度: %1 bytes").arg(calcFrameLength(*current_frame_)));

    frame_type_combo_->setEnabled(true);
    byte_order_btn_->setEnabled(true);
    delete_frame_action_->setEnabled(true);
  } else {
    frame_name_label_->setText(QStringLiteral("(无帧)"));
    frame_id_label_->setText(QStringLiteral("ID: -"));
    frame_length_label_->setText(QStringLiteral("长度: -"));
    frame_type_combo_->setEnabled(false);
    byte_order_btn_->setEnabled(false);
    delete_frame_action_->setEnabled(false);
  }
}

// ── Populate tree from repo ──────────────────────────────────
void ProtocolEditorWidget::populateFrames() {
  node_tree_->loadFromRepository(repo_);
}

// ── Set current frame ────────────────────────────────────────
void ProtocolEditorWidget::setCurrentFrame(icd::Frame* frame) {
  current_frame_ = frame;
  if (frame) {
    bit_view_->loadFromFrame(*frame);
    property_panel_->showFrame(*frame);
    showStatusMessage(
        QStringLiteral("Frame: %1  |  ID: %2")
            .arg(QString::fromStdString(std::string(frame->name())))
            .arg(frame->id()));
  } else {
    bit_view_->clearBlocks();
    property_panel_->clear();
    showStatusMessage(QStringLiteral("就绪"));
  }
  updateToolbar();
}

// ── Refresh tree + select frame ──────────────────────────────
void ProtocolEditorWidget::refreshAndSelectFrame(icd::Frame* frame) {
  populateFrames();
  setCurrentFrame(frame);
}

// ── Clear all data ───────────────────────────────────────────
void ProtocolEditorWidget::clearAll() {
  current_frame_ = nullptr;
  // Cannot directly clear icd::Repository — but we can assign a new one
  repo_ = icd::Repository();
  node_tree_->clear();
  bit_view_->clearBlocks();
  property_panel_->clear();
  updateToolbar();
  snapshots_.clear();
  snapshot_frame_ids_.clear();
  snapshot_index_ = -1;
}

// ── Snapshot (undo/redo) ──────────────────────────────────────
void ProtocolEditorWidget::saveSnapshot() {
  auto j = icd::format::serialize_repository_to_json(repo_);
  QString jsonStr = QString::fromStdString(j.dump());
  QByteArray bytes = jsonStr.toUtf8();

  int frameId = current_frame_ ? current_frame_->id() : -1;

  // Truncate redo history
  if (snapshot_index_ < snapshots_.size() - 1) {
    snapshots_.resize(snapshot_index_ + 1);
    snapshot_frame_ids_.resize(snapshot_index_ + 1);
  }

  snapshots_.append(qCompress(bytes, 9));
  snapshot_frame_ids_.append(frameId);
  snapshot_index_ = snapshots_.size() - 1;

  // Enforce max history: discard oldest entries
  while (snapshots_.size() > kMaxSnapshots) {
    snapshots_.removeFirst();
    snapshot_frame_ids_.removeFirst();
    --snapshot_index_;
  }
}

void ProtocolEditorWidget::restoreSnapshot(const QByteArray& data) {
  QByteArray raw = qUncompress(data);
  if (raw.isEmpty())
    return;
  auto j = nlohmann::json::parse(raw.toStdString());
  auto result = icd::format::deserialize_repository(j);
  if (!result)
    return;

  // Save the target frame ID before anything is invalidated
  int targetFrameId =
      (snapshot_index_ >= 0 && snapshot_index_ < snapshot_frame_ids_.size())
          ? snapshot_frame_ids_[snapshot_index_]
          : -1;

  // Clear dangling refs BEFORE repo_ is reassigned
  bit_view_->clearBlocks();
  property_panel_->clear();
  current_frame_ = nullptr;

  repo_ = std::move(*result);
  populateFrames();

  // Restore the previously selected frame by ID, not blindly frames[0]
  icd::Frame* target = nullptr;
  if (targetFrameId >= 0) {
    target = const_cast<icd::Frame*>(repo_.find(targetFrameId));
  }
  if (!target && !repo_.frames().empty()) {
    target = repo_.frames()[0].get();
  }

  if (target) {
    setCurrentFrame(target);
  } else {
    updateToolbar();
  }
}

// ── Modified flag ────────────────────────────────────────────
void ProtocolEditorWidget::setModified(bool modified) {
  if (modified_ != modified) {
    modified_ = modified;
    emit modificationChanged(modified);
  }
}

// ── Reload toolbar icons (theme switch) ──────────────────────
void ProtocolEditorWidget::reloadToolbarIcons() {
  auto icon = [](const QString& name) {
    return etest::app::AppIconProvider::instance().icon(name);
  };
  new_frame_action_->setIcon(icon(QStringLiteral("protocol_new_frame")));
  delete_frame_action_->setIcon(icon(QStringLiteral("protocol_delete_frame")));
  undo_action_->setIcon(icon(QStringLiteral("undo")));
  redo_action_->setIcon(icon(QStringLiteral("redo")));
  byte_order_btn_->setIcon(icon(QStringLiteral("protocol_byte_order")));
  node_tree_toggle_action_->setIcon(icon(QStringLiteral("protocol_node_tree")));
  property_toggle_action_->setIcon(icon(QStringLiteral("protocol_property")));
}

}  // namespace etest::protocol
