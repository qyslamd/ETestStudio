#include "ProtocalEditorWidget.h"

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>
#include <string>
#include <vector>

#include "format/json_serializer.hpp"
#include "format/json_parser.hpp"
#include "IcdBitLayoutView.h"
#include "IcdNodeTreeWidget.h"
#include "IcdPropertyPanel.h"

namespace etest::protocal {
namespace {

void updateMaxBits(const icd::Node& node, int& max_bits) {
  int end = (node.offset() * 8) + node.bit_offset() + node.bit_width();
  if (end > max_bits) max_bits = end;
  for (const auto& child : node.children())
    updateMaxBits(*child, max_bits);
}

int calcFrameLength(const icd::Frame& frame) {
  int max_bits = 0;
  for (const auto& root : frame.roots())
    updateMaxBits(*root, max_bits);
  return (max_bits + 7) / 8;
}

}  // namespace

// ──────────────────────────────────────────────────────────────
// ProtocalEditorWidget
// ──────────────────────────────────────────────────────────────
ProtocalEditorWidget::ProtocalEditorWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
  initSignals();
}

QString ProtocalEditorWidget::displayName() const {
  if (current_file_.isEmpty()) return QStringLiteral("未命名协议");
  return QFileInfo(current_file_).fileName();
}

bool ProtocalEditorWidget::isModified() const { return modified_; }

bool ProtocalEditorWidget::save() {
  if (current_file_.isEmpty()) return false;
  if (saveEproto(current_file_)) {
    setModified(false);
    return true;
  }
  return false;
}

bool ProtocalEditorWidget::saveAs(const QString& path) {
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

QString ProtocalEditorWidget::filePath() const { return current_file_; }

QString ProtocalEditorWidget::editorId() const {
  if (current_file_.isEmpty()) return QStringLiteral("editor://protocal/new");
  return current_file_;
}

QWidget* ProtocalEditorWidget::widget() { return this; }

QString ProtocalEditorWidget::editorType() const {
  return QStringLiteral("protocal");
}

QObject* ProtocalEditorWidget::signalObject() { return this; }

bool ProtocalEditorWidget::canUndo() const { return snapshot_index_ > 0; }
bool ProtocalEditorWidget::canRedo() const { return snapshot_index_ < snapshots_.size() - 1; }
void ProtocalEditorWidget::undo() {
  if (!canUndo()) return;
  --snapshot_index_;
  restoreSnapshot(snapshots_[snapshot_index_]);
  emit modificationChanged(modified_);
}
void ProtocalEditorWidget::redo() {
  if (!canRedo()) return;
  ++snapshot_index_;
  restoreSnapshot(snapshots_[snapshot_index_]);
  emit modificationChanged(modified_);
}

void ProtocalEditorWidget::setEditorId(const QString& id) {
  if (id == current_file_) return;
  current_file_ = id;
  if (QFileInfo::exists(id)) {
    if (!loadEproto(id)) {
      QMessageBox::warning(this, QStringLiteral("加载失败"),
          QStringLiteral("无法加载协议文件: %1").arg(id));
    }
  }
}

// ── Load .eproto JSON ─────────────────────────────────────────
bool ProtocalEditorWidget::loadEproto(const QString& path) {
  auto result = icd::format::deserialize_repository(
      std::filesystem::path(path.toStdWString()));
  if (!result) return false;

  clearAll();
  repo_ = std::move(*result);
  populateFrames();

  if (!repo_.frames().empty())
    setCurrentFrame(repo_.frames()[0].get());

  saveSnapshot();
  return true;
}

// ── Save .eproto JSON ─────────────────────────────────────────
bool ProtocalEditorWidget::saveEproto(const QString& path) {
  auto result = icd::format::serialize_repository(
      std::filesystem::path(path.toStdWString()), repo_);
  return result.has_value();
}

// ── UI ─────────────────────────────────────────────────────────
void ProtocalEditorWidget::initUi() {
  auto* main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // === Top Toolbar ===
  auto* toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("protocalToolbar"));
  toolbar->setFixedHeight(36);

  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(8, 0, 8, 0);

  auto* title_label = new QLabel(QStringLiteral("帧属性"), this);
  title_label->setObjectName(QStringLiteral("protocalTitleLabel"));

  frame_name_label_ = new QLabel(QStringLiteral("(无帧)"), this);
  frame_name_label_->setObjectName(QStringLiteral("frameNameLabel"));

  frame_type_combo_ = new QComboBox(this);
  frame_type_combo_->addItem(QStringLiteral("发送 (Cmd)"));
  frame_type_combo_->addItem(QStringLiteral("接收 (Data)"));
  frame_type_combo_->addItem(QStringLiteral("配置 (DataCfg)"));
  frame_type_combo_->setEnabled(false);

  byte_order_combo_ = new QComboBox(this);
  byte_order_combo_->addItem(QStringLiteral("小端 (Little Endian)"));
  byte_order_combo_->addItem(QStringLiteral("大端 (Big Endian)"));
  byte_order_combo_->setEnabled(false);

  frame_id_label_ = new QLabel(QStringLiteral("ID: -"), this);
  frame_id_label_->setObjectName(QStringLiteral("idLabel"));

  frame_length_label_ = new QLabel(QStringLiteral("长度: -"), this);
  frame_length_label_->setObjectName(QStringLiteral("lengthLabel"));

  new_frame_btn_ = new QToolButton(this);
  new_frame_btn_->setText(QStringLiteral("+帧"));

  delete_frame_btn_ = new QToolButton(this);
  delete_frame_btn_->setText(QStringLiteral("-帧"));
  delete_frame_btn_->setEnabled(false);

  toolbar_layout->addWidget(title_label);
  toolbar_layout->addWidget(frame_name_label_);
  toolbar_layout->addWidget(frame_type_combo_);
  toolbar_layout->addWidget(byte_order_combo_);
  toolbar_layout->addWidget(frame_id_label_);
  toolbar_layout->addWidget(new_frame_btn_);
  toolbar_layout->addWidget(delete_frame_btn_);
  toolbar_layout->addStretch();
  toolbar_layout->addWidget(frame_length_label_);

  main_layout->addWidget(toolbar);

  // === Central Splitter ===
  splitter_ = new QSplitter(Qt::Horizontal, this);
  splitter_->setHandleWidth(1);
  splitter_->setChildrenCollapsible(false);

  node_tree_ = new IcdNodeTreeWidget(this);
  node_tree_->setMinimumWidth(200);

  bit_view_ = new IcdBitLayoutView(this);

  property_panel_ = new IcdPropertyPanel(this);
  property_panel_->setMinimumWidth(220);

  splitter_->addWidget(node_tree_);
  splitter_->addWidget(bit_view_);
  splitter_->addWidget(property_panel_);
  splitter_->setStretchFactor(0, 30);
  splitter_->setStretchFactor(1, 40);
  splitter_->setStretchFactor(2, 30);

  main_layout->addWidget(splitter_, 1);

  // === Bottom Status Bar ===
  auto* status_bar = new QWidget(this);
  status_bar->setObjectName(QStringLiteral("protocalStatusBar"));
  status_bar->setFixedHeight(24);

  auto* status_layout = new QHBoxLayout(status_bar);
  status_layout->setContentsMargins(8, 0, 8, 0);

  status_label_ = new QLabel(QStringLiteral("就绪"), this);
  status_layout->addWidget(status_label_);
  status_layout->addStretch();

  auto* hint_label = new QLabel(
      QStringLiteral("选择帧或信号查看属性"), this);
  status_layout->addWidget(hint_label);

  main_layout->addWidget(status_bar);
}

// ── Signals ───────────────────────────────────────────────────
void ProtocalEditorWidget::initSignals() {
  // Frame selection from tree
  connect(node_tree_, &IcdNodeTreeWidget::frameSelected,
          this, &ProtocalEditorWidget::setCurrentFrame);

  // Node selection from tree → property panel + bit view highlight
  connect(node_tree_, &IcdNodeTreeWidget::nodeSelected,
          this, [this](const icd::Node* node) {
    if (node) {
      property_panel_->showNode(const_cast<icd::Node&>(*node));
      bit_view_->highlightBlock(
          QString::fromStdString(std::string(node->name())));
      status_label_->setText(
          QStringLiteral("Node: %1  |  Offset: %2  |  Bit: %3~%4")
              .arg(QString::fromStdString(std::string(node->name())))
              .arg(node->offset())
              .arg(node->bit_offset())
              .arg(node->bit_offset() + node->bit_width() - 1));
    }
  });

  // Bit block clicked → highlight block + property panel
  connect(bit_view_, &IcdBitLayoutView::blockClicked,
          this, [this](const QString& name) {
    if (!current_frame_) return;
    bit_view_->highlightBlock(name);
    const auto* node = current_frame_->find(name.toStdString());
    if (node) {
      property_panel_->showNode(const_cast<icd::Node&>(*node));
      node_tree_->selectNode(node);
      status_label_->setText(
          QStringLiteral("Node: %1  |  Offset: %2  |  Bit: %3~%4")
              .arg(QString::fromStdString(std::string(node->name())))
              .arg(node->offset())
              .arg(node->bit_offset())
              .arg(node->bit_offset() + node->bit_width() - 1));
    }
  });

  // Bit block hover → reveal tree node (scroll only, no selection change)
  connect(bit_view_, &IcdBitLayoutView::blockHovered,
          this, [this](const QString& name, bool /*on*/) {
    if (!current_frame_) return;
    const auto* node = current_frame_->find(name.toStdString());
    if (node) {
      node_tree_->revealNode(node);
    }
  });

  // Bit block context menu actions
  connect(bit_view_, &IcdBitLayoutView::contextMenuAction,
          this, [this](const QString& name, const QString& action) {
    if (!current_frame_) return;
    auto* frame = const_cast<icd::Frame*>(current_frame_);

    if (action == QStringLiteral("delete")) {
      const auto* node = current_frame_->find(name.toStdString());
      if (!node) return;

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
      populateFrames();
      setCurrentFrame(frame);
      setModified(true);
      status_label_->setText(QStringLiteral("已删除信号: %1").arg(name));

    } else if (action == QStringLiteral("addBefore") ||
               action == QStringLiteral("addAfter")) {
      const auto* node = current_frame_->find(name.toStdString());
      if (!node) return;

      saveSnapshot();
      auto new_node = std::make_unique<icd::Node>(
          "NewNode", "", 0, 0, 8,
          icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});

      auto* parent = const_cast<icd::Node*>(node->parent());
      if (!parent) {
        // Root node — find index and insert adjacent
        const auto& roots = frame->roots();
        for (std::size_t i = 0; i < roots.size(); ++i) {
          if (roots[i].get() == node) {
            std::size_t pos = (action == QStringLiteral("addBefore")) ? i : i + 1;
            frame->insert_root(pos, std::move(new_node));
            break;
          }
        }
      } else {
        // Child node — find in parent's children and insert adjacent
        const auto& children = parent->children();
        for (std::size_t i = 0; i < children.size(); ++i) {
          if (children[i].get() == node) {
            std::size_t pos = (action == QStringLiteral("addBefore")) ? i : i + 1;
            parent->insert_child(pos, std::move(new_node));
            break;
          }
        }
      }
      populateFrames();
      setCurrentFrame(frame);
      setModified(true);
      status_label_->setText(
          QStringLiteral("已插入信号 (相邻: %1)").arg(name));
    }
  });

  // Node property modified
  connect(property_panel_, &IcdPropertyPanel::nodeModified,
          this, [this]() {
    setModified(true);
    // Refresh bit view to reflect changes
    if (current_frame_) {
      bit_view_->loadFromFrame(*current_frame_);
    }
  });

  // New frame
  connect(new_frame_btn_, &QToolButton::clicked,
          this, [this]() {
    saveSnapshot();
    // Find max existing id
    int max_id = 0;
    for (const auto& f : repo_.frames()) {
      if (f->id() > max_id) max_id = f->id();
    }
    int new_id = max_id + 1;
    auto name = "Frame_" + std::to_string(new_id);

    auto frame = std::make_unique<icd::Frame>(
        new_id, name, "", icd::FrameType::data,
        icd::ByteOrder::little_endian);
    auto* frame_ptr = frame.get();
    repo_.add_frame(std::move(frame));
    populateFrames();
    setCurrentFrame(frame_ptr);
    setModified(true);
  });

  // Delete frame
  connect(delete_frame_btn_, &QToolButton::clicked,
          this, [this]() {
    if (!current_frame_) return;
    saveSnapshot();
    int id = current_frame_->id();
    setCurrentFrame(nullptr);
    if (repo_.remove_frame(id)) {
      populateFrames();
      if (!repo_.frames().empty())
        setCurrentFrame(repo_.frames()[0].get());
      setModified(true);
      status_label_->setText(QStringLiteral("已删除帧"));
    }
  });

  // Frame type combo changed
  connect(frame_type_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
    if (!current_frame_) return;
    saveSnapshot();
    icd::FrameType new_type;
    switch (index) {
    case 0: new_type = icd::FrameType::cmd; break;
    case 1: new_type = icd::FrameType::data; break;
    case 2: new_type = icd::FrameType::data_cmd; break;
    default: return;
    }
    const_cast<icd::Frame*>(current_frame_)->setType(new_type);
    setModified(true);
  });

  // Byte order combo changed
  connect(byte_order_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
    if (!current_frame_) return;
    saveSnapshot();
    auto order = (index == 0) ? icd::ByteOrder::little_endian
                              : icd::ByteOrder::big_endian;
    const_cast<icd::Frame*>(current_frame_)->setOrder(order);
    setModified(true);
  });

  // ── Context-menu from tree widget ──────────────────────────
  connect(node_tree_, &IcdNodeTreeWidget::addFrameRequested,
          this, [this]() {
    saveSnapshot();
    int max_id = 0;
    for (const auto& f : repo_.frames()) {
      if (f->id() > max_id) max_id = f->id();
    }
    int new_id = max_id + 1;
    auto name = "Frame_" + std::to_string(new_id);
    auto frame = std::make_unique<icd::Frame>(
        new_id, name, "", icd::FrameType::data,
        icd::ByteOrder::little_endian);
    auto* frame_ptr = frame.get();
    repo_.add_frame(std::move(frame));
    populateFrames();
    setCurrentFrame(frame_ptr);
    setModified(true);
  });

  connect(node_tree_, &IcdNodeTreeWidget::deleteFrameRequested,
          this, [this](int frameId) {
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

  connect(node_tree_, &IcdNodeTreeWidget::addNodeRequested,
          this, [this](int frameId) {
    saveSnapshot();
    for (const auto& frame_ptr : repo_.frames()) {
      if (frame_ptr->id() == frameId) {
        auto* frame = const_cast<icd::Frame*>(frame_ptr.get());
        auto node = std::make_unique<icd::Node>(
            "NewNode", "", 0, 0, 8,
            icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
        frame->add_root(std::move(node));
        populateFrames();
        setCurrentFrame(frame);
        setModified(true);
        break;
      }
    }
  });

  connect(node_tree_, &IcdNodeTreeWidget::deleteNodeRequested,
          this, [this](int frameId, const icd::Node* node) {
    if (!node) return;
    saveSnapshot();
    for (const auto& frame_ptr : repo_.frames()) {
      if (frame_ptr->id() != frameId) continue;
      auto* frame = const_cast<icd::Frame*>(frame_ptr.get());

      // Check root nodes
      const auto& roots = frame->roots();
      for (std::size_t i = 0; i < roots.size(); ++i) {
        if (roots[i].get() == node) {
          frame->remove_root(i);
          populateFrames();
          setCurrentFrame(frame);
          setModified(true);
          return;
        }
      }

      // Check child nodes
      for (auto* n : frame->nodes()) {
        if (n == node) continue;
        const auto& children = n->children();
        for (std::size_t i = 0; i < children.size(); ++i) {
          if (children[i].get() == node) {
            n->remove_child(i);
            populateFrames();
            setCurrentFrame(frame);
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
void ProtocalEditorWidget::updateToolbar() {
  if (current_frame_) {
    frame_name_label_->setText(
        QString::fromStdString(std::string(current_frame_->name())));
    frame_id_label_->setText(
        QStringLiteral("ID: %1").arg(current_frame_->id()));

    // Block signals to avoid recursive modification
    frame_type_combo_->blockSignals(true);
    switch (current_frame_->type()) {
    case icd::FrameType::cmd:      frame_type_combo_->setCurrentIndex(0); break;
    case icd::FrameType::data:     frame_type_combo_->setCurrentIndex(1); break;
    case icd::FrameType::data_cmd: frame_type_combo_->setCurrentIndex(2); break;
    }
    frame_type_combo_->blockSignals(false);

    byte_order_combo_->blockSignals(true);
    byte_order_combo_->setCurrentIndex(
        current_frame_->order() == icd::ByteOrder::little_endian ? 0 : 1);
    byte_order_combo_->blockSignals(false);

    frame_length_label_->setText(
        QStringLiteral("长度: %1 bytes").arg(calcFrameLength(*current_frame_)));

    frame_type_combo_->setEnabled(true);
    byte_order_combo_->setEnabled(true);
    delete_frame_btn_->setEnabled(true);
  } else {
    frame_name_label_->setText(QStringLiteral("(无帧)"));
    frame_id_label_->setText(QStringLiteral("ID: -"));
    frame_length_label_->setText(QStringLiteral("长度: -"));
    frame_type_combo_->setEnabled(false);
    byte_order_combo_->setEnabled(false);
    delete_frame_btn_->setEnabled(false);
  }
}

// ── Populate tree from repo ──────────────────────────────────
void ProtocalEditorWidget::populateFrames() {
  node_tree_->loadFromRepository(repo_);
}

// ── Set current frame ────────────────────────────────────────
void ProtocalEditorWidget::setCurrentFrame(const icd::Frame* frame) {
  current_frame_ = frame;
  if (frame) {
    bit_view_->loadFromFrame(*frame);
    property_panel_->showFrame(*frame);
    status_label_->setText(
        QStringLiteral("Frame: %1  |  ID: %2")
            .arg(QString::fromStdString(std::string(frame->name())))
            .arg(frame->id()));
  } else {
    bit_view_->clearBlocks();
    property_panel_->clear();
    status_label_->setText(QStringLiteral("就绪"));
  }
  updateToolbar();
}

// ── Clear all data ───────────────────────────────────────────
void ProtocalEditorWidget::clearAll() {
  current_frame_ = nullptr;
  // Cannot directly clear icd::Repository — but we can assign a new one
  repo_ = icd::Repository();
  node_tree_->clear();
  bit_view_->clearBlocks();
  property_panel_->clear();
  updateToolbar();
  snapshots_.clear();
  snapshot_index_ = -1;
}

// ── Snapshot (undo/redo) ──────────────────────────────────────
void ProtocalEditorWidget::saveSnapshot() {
  auto j = icd::format::serialize_repository_to_json(repo_);
  QString jsonStr = QString::fromStdString(j.dump());
  QByteArray bytes = jsonStr.toUtf8();

  // Truncate redo history
  if (snapshot_index_ < snapshots_.size() - 1) {
    snapshots_.resize(snapshot_index_ + 1);
  }

  snapshots_.append(bytes);
  snapshot_index_ = snapshots_.size() - 1;
}

void ProtocalEditorWidget::restoreSnapshot(const QByteArray& data) {
  auto j = nlohmann::json::parse(data.toStdString());
  auto result = icd::format::deserialize_repository(j);
  if (!result) return;

  current_frame_ = nullptr;
  repo_ = std::move(*result);
  populateFrames();

  if (!repo_.frames().empty()) {
    setCurrentFrame(repo_.frames()[0].get());
  } else {
    bit_view_->clearBlocks();
    property_panel_->clear();
    updateToolbar();
  }
}

// ── Modified flag ────────────────────────────────────────────
void ProtocalEditorWidget::setModified(bool modified) {
  if (modified_ != modified) {
    modified_ = modified;
    emit modificationChanged(modified);
  }
}

}  // namespace etest::protocal
