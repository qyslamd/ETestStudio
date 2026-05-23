#include "ProtocalEditorWidget.h"

#pragma push_macro("slots")
#undef slots
#include <nlohmann/json.hpp>
#pragma pop_macro("slots")

#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "IcdBitLayoutView.h"
#include "IcdNodeTreeWidget.h"
#include "IcdPropertyPanel.h"

namespace etest::protocal {
namespace {

using json = nlohmann::json;

// ── ValueType string → enum ──────────────────────────────────
icd::ValueType valueTypeFromString(const std::string& s) {
  if (s == "boolean") return icd::ValueType::boolean;
  if (s == "uint8")   return icd::ValueType::byte_;
  if (s == "bytes")   return icd::ValueType::bytes;
  if (s == "uint16")  return icd::ValueType::word;
  if (s == "int16")   return icd::ValueType::shortint;
  if (s == "smallint") return icd::ValueType::smallint;
  if (s == "uint32")  return icd::ValueType::longword;
  if (s == "int32")   return icd::ValueType::integer;
  if (s == "uint64")  return icd::ValueType::ulong_;
  if (s == "float")   return icd::ValueType::single;
  if (s == "double")  return icd::ValueType::double_;
  if (s == "string")  return icd::ValueType::string_;
  return icd::ValueType::unknown;
}

// ── FrameType string → enum ──────────────────────────────────
icd::FrameType frameTypeFromString(const std::string& s) {
  if (s == "cmd")      return icd::FrameType::cmd;
  if (s == "data")     return icd::FrameType::data;
  if (s == "dataCfg")  return icd::FrameType::data_cmd;
  return icd::FrameType::data;
}

// ── ByteOrder string → enum ─────────────────────────────────
icd::ByteOrder byteOrderFromString(const std::string& s) {
  return (s == "bigEndian") ? icd::ByteOrder::big_endian
                            : icd::ByteOrder::little_endian;
}

// ── ValueType enum → string ──────────────────────────────────
std::string valueTypeToString(icd::ValueType vt) {
  switch (vt) {
  case icd::ValueType::boolean:  return "boolean";
  case icd::ValueType::byte_:    return "uint8";
  case icd::ValueType::bytes:    return "bytes";
  case icd::ValueType::word:     return "uint16";
  case icd::ValueType::shortint: return "int16";
  case icd::ValueType::smallint: return "smallint";
  case icd::ValueType::longword: return "uint32";
  case icd::ValueType::integer:  return "int32";
  case icd::ValueType::ulong_:   return "uint64";
  case icd::ValueType::single:   return "float";
  case icd::ValueType::double_:  return "double";
  case icd::ValueType::string_:  return "string";
  case icd::ValueType::unknown:  return "unknown";
  }
  return "unknown";
}

// ── Recursive node JSON serializer ──────────────────────────
json serializeNode(const icd::Node& n) {
  json obj;
  obj["name"]        = std::string(n.name());
  obj["description"] = std::string(n.description());
  obj["offset"]      = n.offset();
  obj["startBit"]    = n.bit_offset();
  obj["bitWidth"]    = n.bit_width();
  obj["valueType"]   = valueTypeToString(n.value_type());
  obj["tag"]         = static_cast<int>(n.tag());

  json attrs_obj;
  const auto& a = n.attrs();
  attrs_obj["systemName"]    = a.system_name;
  attrs_obj["groupName"]     = a.group_name;
  attrs_obj["unit"]          = a.unit;
  attrs_obj["valueTextList"] = a.value_text_list;
  attrs_obj["scaleFormula"]  = a.scale_formula;
  attrs_obj["scaleConveror"] = a.scale_convertor;
  attrs_obj["linkTo"]        = a.link_to;
  attrs_obj["isScaled"]      = a.is_scaled;
  if (a.scale_a.has_value()) attrs_obj["scaleA"] = *a.scale_a;
  if (a.scale_b.has_value()) attrs_obj["scaleB"] = *a.scale_b;
  if (a.min.has_value())     attrs_obj["min"]    = *a.min;
  if (a.max.has_value())     attrs_obj["max"]    = *a.max;
  obj["attrs"] = std::move(attrs_obj);

  if (!n.children().empty()) {
    json children = json::array();
    for (const auto& child : n.children())
      children.push_back(serializeNode(*child));
    obj["children"] = std::move(children);
  }
  return obj;
}

// ── Recursive node JSON parser ───────────────────────────────
std::unique_ptr<icd::Node> parseNode(const json& j) {
  auto value_type = valueTypeFromString(
      j.value("valueType", std::string("unknown")));

  icd::NodeAttrs attrs;
  if (auto it = j.find("attrs"); it != j.end()) {
    const auto& a = *it;
    attrs.system_name      = a.value("systemName", std::string());
    attrs.group_name       = a.value("groupName", std::string());
    attrs.unit             = a.value("unit", std::string());
    attrs.value_text_list  = a.value("valueTextList", std::string());
    attrs.scale_formula    = a.value("scaleFormula", std::string());
    attrs.scale_convertor  = a.value("scaleConveror", std::string());
    attrs.link_to          = a.value("linkTo", std::string());
    attrs.is_scaled        = a.value("isScaled", false);
    if (auto v = a.find("scaleA"); v != a.end() && v->is_number())
      attrs.scale_a = v->get<float>();
    if (auto v = a.find("scaleB"); v != a.end() && v->is_number())
      attrs.scale_b = v->get<float>();
    if (auto v = a.find("min"); v != a.end() && v->is_number())
      attrs.min = v->get<float>();
    if (auto v = a.find("max"); v != a.end() && v->is_number())
      attrs.max = v->get<float>();
  }

  auto tag = static_cast<icd::Tag>(j.value("tag", 0));

  auto node = std::make_unique<icd::Node>(
      j.value("name", std::string()),
      j.value("description", std::string()),
      j.value("offset", 0),
      j.value("startBit", 0),
      j.value("bitWidth", 8),
      value_type, tag, std::move(attrs));

  // Parse children recursively
  if (auto it = j.find("children"); it != j.end() && it->is_array()) {
    for (const auto& child : *it) {
      node->add_child(parseNode(child));
    }
  }

  return node;
}

// ── JSON node recursion for frame length calc ───────────────
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
  std::ifstream stream(path.toStdString());
  if (!stream.is_open()) return false;

  json document;
  try {
    stream >> document;
  } catch (...) {
    return false;
  }

  clearAll();

  if (auto it = document.find("frames"); it != document.end() && it->is_array()) {
    for (const auto& frame_json : *it) {
      int id = frame_json.value("id", 0);
      std::string name = frame_json.value("name", std::string());
      std::string desc = frame_json.value("description", std::string());
      auto type = frameTypeFromString(frame_json.value("type", std::string("data")));
      auto order = byteOrderFromString(frame_json.value("byteOrder", std::string("littleEndian")));

      auto frame = std::make_unique<icd::Frame>(id, name, desc, type, order);

      if (auto nodes_it = frame_json.find("nodes"); nodes_it != frame_json.end() && nodes_it->is_array()) {
        for (const auto& node_json : *nodes_it) {
          frame->add_root(parseNode(node_json));
        }
      }

      repo_.add_frame(std::move(frame));
    }
  }

  populateFrames();

  // Select first frame if available
  if (!repo_.frames().empty()) {
    setCurrentFrame(repo_.frames()[0].get());
  }

  saveSnapshot();
  return true;
}

// ── Save .eproto JSON ─────────────────────────────────────────
bool ProtocalEditorWidget::saveEproto(const QString& path) {
  json document;
  document["version"] = "1.0";

  json frames = json::array();
  for (const auto& f : repo_.frames()) {
    json frame_obj;
    frame_obj["id"] = f->id();
    frame_obj["name"] = std::string(f->name());
    frame_obj["description"] = std::string(f->description());

    switch (f->type()) {
    case icd::FrameType::cmd:      frame_obj["type"] = "cmd";     break;
    case icd::FrameType::data:     frame_obj["type"] = "data";    break;
    case icd::FrameType::data_cmd: frame_obj["type"] = "dataCfg"; break;
    }

    frame_obj["byteOrder"] = (f->order() == icd::ByteOrder::little_endian)
                                 ? "littleEndian" : "bigEndian";

    frame_obj["length"] = calcFrameLength(*f);

    json nodes = json::array();
    // Use a lambda to serialize nodes since we need recursion
    // and icd::Node has complex access patterns
    struct Serializer {
      static json serialize(const icd::Node& n) {
        json obj;
        obj["name"]        = std::string(n.name());
        obj["description"] = std::string(n.description());
        obj["offset"]      = n.offset();
        obj["startBit"]    = n.bit_offset();
        obj["bitWidth"]    = n.bit_width();
        obj["valueType"]   = valueTypeToString(n.value_type());
        obj["tag"]         = static_cast<int>(n.tag());

        json attrs_obj;
        const auto& a = n.attrs();
        attrs_obj["systemName"]    = a.system_name;
        attrs_obj["groupName"]     = a.group_name;
        attrs_obj["unit"]          = a.unit;
        attrs_obj["valueTextList"] = a.value_text_list;
        attrs_obj["scaleFormula"]  = a.scale_formula;
        attrs_obj["scaleConveror"] = a.scale_convertor;
        attrs_obj["linkTo"]        = a.link_to;
        attrs_obj["isScaled"]      = a.is_scaled;
        if (a.scale_a.has_value()) attrs_obj["scaleA"] = *a.scale_a;
        if (a.scale_b.has_value()) attrs_obj["scaleB"] = *a.scale_b;
        if (a.min.has_value())     attrs_obj["min"]    = *a.min;
        if (a.max.has_value())     attrs_obj["max"]    = *a.max;
        obj["attrs"] = std::move(attrs_obj);

        if (!n.children().empty()) {
          json children = json::array();
          for (const auto& child : n.children())
            children.push_back(serialize(*child));
          obj["children"] = std::move(children);
        }

        return obj;
      }

      static std::string valueTypeToString(icd::ValueType vt) {
        switch (vt) {
        case icd::ValueType::boolean:  return "boolean";
        case icd::ValueType::byte_:    return "uint8";
        case icd::ValueType::bytes:    return "bytes";
        case icd::ValueType::word:     return "uint16";
        case icd::ValueType::shortint: return "int16";
        case icd::ValueType::smallint: return "smallint";
        case icd::ValueType::longword: return "uint32";
        case icd::ValueType::integer:  return "int32";
        case icd::ValueType::ulong_:   return "uint64";
        case icd::ValueType::single:   return "float";
        case icd::ValueType::double_:  return "double";
        case icd::ValueType::string_:  return "string";
        case icd::ValueType::unknown:  return "unknown";
        }
        return "unknown";
      }
    };

    for (const auto& root : f->roots())
      nodes.push_back(Serializer::serialize(*root));
    frame_obj["nodes"] = std::move(nodes);

    frames.push_back(std::move(frame_obj));
  }
  document["frames"] = std::move(frames);

  std::ofstream stream(path.toStdString());
  if (!stream.is_open()) return false;
  try {
    stream << document.dump(2);
  } catch (...) {
    return false;
  }
  return true;
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

  // Bit block clicked → find node → property panel
  connect(bit_view_, &IcdBitLayoutView::blockClicked,
          this, [this](const QString& name) {
    if (!current_frame_) return;
    const auto* node = current_frame_->find(name.toStdString());
    if (node) {
      property_panel_->showNode(const_cast<icd::Node&>(*node));
      status_label_->setText(
          QStringLiteral("Node: %1  |  Offset: %2  |  Bit: %3~%4")
              .arg(QString::fromStdString(std::string(node->name())))
              .arg(node->offset())
              .arg(node->bit_offset())
              .arg(node->bit_offset() + node->bit_width() - 1));
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
  json document;
  document["version"] = "1.0";

  json frames = json::array();
  for (const auto& f : repo_.frames()) {
    json frame_obj;
    frame_obj["id"] = f->id();
    frame_obj["name"] = std::string(f->name());
    frame_obj["description"] = std::string(f->description());

    switch (f->type()) {
    case icd::FrameType::cmd:      frame_obj["type"] = "cmd";     break;
    case icd::FrameType::data:     frame_obj["type"] = "data";    break;
    case icd::FrameType::data_cmd: frame_obj["type"] = "dataCfg"; break;
    }

    frame_obj["byteOrder"] = (f->order() == icd::ByteOrder::little_endian)
                                 ? "littleEndian" : "bigEndian";
    frame_obj["length"] = calcFrameLength(*f);

    json nodes = json::array();
    for (const auto& root : f->roots())
      nodes.push_back(serializeNode(*root));
    frame_obj["nodes"] = std::move(nodes);

    frames.push_back(std::move(frame_obj));
  }
  document["frames"] = std::move(frames);

  // Convert nlohmann::json → QJsonObject
  QString jsonStr = QString::fromStdString(document.dump());
  QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());

  // Truncate redo history
  if (snapshot_index_ < snapshots_.size() - 1) {
    snapshots_.resize(snapshot_index_ + 1);
  }

  snapshots_.append(doc.object());
  snapshot_index_ = snapshots_.size() - 1;
}

void ProtocalEditorWidget::restoreSnapshot(const QJsonObject& obj) {
  QJsonDocument doc(obj);
  QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

  json document = json::parse(jsonStr.toStdString());

  current_frame_ = nullptr;
  repo_ = icd::Repository();

  if (auto it = document.find("frames"); it != document.end() && it->is_array()) {
    for (const auto& frame_json : *it) {
      int id = frame_json.value("id", 0);
      std::string name = frame_json.value("name", std::string());
      std::string desc = frame_json.value("description", std::string());
      auto type = frameTypeFromString(frame_json.value("type", std::string("data")));
      auto order = byteOrderFromString(frame_json.value("byteOrder", std::string("littleEndian")));

      auto frame = std::make_unique<icd::Frame>(id, name, desc, type, order);

      if (auto nodes_it = frame_json.find("nodes"); nodes_it != frame_json.end() && nodes_it->is_array()) {
        for (const auto& node_json : *nodes_it) {
          frame->add_root(parseNode(node_json));
        }
      }

      repo_.add_frame(std::move(frame));
    }
  }

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
