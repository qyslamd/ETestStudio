#include "ProtocolEditorWidget.h"

#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
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

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "AppIconProvider.h"
#include "IcdBitLayoutView.h"
#include "IcdFramePreviewPanel.h"
#include "IcdNodeTreeWidget.h"
#include "IcdPropertyPanel.h"
#include "ThemeManager.h"
#include "format/json_parser.hpp"
#include "format/json_serializer.hpp"
#include "format/xml_serializer.hpp"
#include "libui/EtDockWidget.h"
#include "utils/FileUtil.h"

#include <icd/loader.hpp>

namespace etest::protocol {

using etest::core::utils::toFsPath;
namespace {

void syncDockCloseAction(QAction* action) {
  action->blockSignals(true);
  action->setChecked(false);
  action->blockSignals(false);
}

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
  connect(&etest::core_ui::ThemeManager::instance(),
          &etest::core_ui::ThemeManager::themeChanged, this,
          &ProtocolEditorWidget::reloadToolbarIcons);
}

ProtocolEditorWidget::~ProtocolEditorWidget() {}

// ── Embedded mode ──────────────────────────────────────────────
void ProtocolEditorWidget::setEmbeddedMode(bool embedded) {
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

QList<etest::app::EditorCommand> ProtocolEditorWidget::editorCommands() {
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
  // 帧
  cmds.append(addCmd(QStringLiteral("帧"), QStringLiteral("新建帧"),
                     QStringLiteral("protocol_new_frame"), false,
                     new_frame_action_));
  cmds.append(addCmd(QStringLiteral("帧"), QStringLiteral("删除帧"),
                     QStringLiteral("protocol_delete_frame"), false,
                     delete_frame_action_));
  // 字节序（D11：widget 项改造为 checkable 命令，触发走 byte_order_btn_
  // 统一逻辑）
  {
    etest::app::EditorCommand c;
    c.group = QStringLiteral("帧");
    c.title = QStringLiteral("字节序");
    c.iconName = QStringLiteral("protocol_byte_order");
    c.checkable = true;
    c.trigger = [this]() { byte_order_btn_->click(); };
    c.isEnabled = [this]() { return byte_order_btn_->isEnabled(); };
    c.isChecked = [this]() { return byte_order_btn_->isChecked(); };
    cmds.append(c);
  }
  // 节点
  cmds.append(addCmd(QStringLiteral("节点"), QStringLiteral("添加节点"),
                     QStringLiteral("protocol_add_node"), false,
                     add_node_action_));
  cmds.append(addCmd(QStringLiteral("节点"), QStringLiteral("删除选中"),
                     QStringLiteral("protocol_delete_node"), false,
                     delete_selected_action_));
  // 视图
  cmds.append(addCheckableCmd(
      QStringLiteral("视图"), QStringLiteral("节点列表"),
      QStringLiteral("protocol_node_tree"), false, node_tree_toggle_action_));
  cmds.append(addCheckableCmd(
      QStringLiteral("视图"), QStringLiteral("属性面板"),
      QStringLiteral("protocol_property"), false, property_toggle_action_));
  cmds.append(addCheckableCmd(
      QStringLiteral("视图"), QStringLiteral("报文预览"),
      QStringLiteral("protocol_preview"), false, preview_toggle_action_));
  return cmds;
}

QObject* ProtocolEditorWidget::commandStateObject() {
  return this;
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
  if (saveByFormat()) {
    setModified(false);
    return true;
  }
  return false;
}

bool ProtocolEditorWidget::saveAs(const QString& path) {
  QString old = current_file_;
  current_file_ = path;

  // Determine format from target path
  if (path.endsWith(QStringLiteral(".eproto"))) {
    format_ = ProtocolFormat::Json;
  } else if (path.endsWith(QStringLiteral(".eprotox"))) {
    format_ = ProtocolFormat::Xml;
  } else if (path.endsWith(QStringLiteral("ICDConfig.xml"),
                           Qt::CaseInsensitive)) {
    format_ = ProtocolFormat::ConfigDriven;
    config_format_ = icd::Format::xml;
  } else if (path.endsWith(QStringLiteral("ICDConfig.json"),
                           Qt::CaseInsensitive)) {
    format_ = ProtocolFormat::ConfigDriven;
    config_format_ = icd::Format::json;
  } else {
    // 未识别的扩展名（或无扩展名）默认走 .eprotox（XML）
    format_ = ProtocolFormat::Xml;
  }

  if (format_ == ProtocolFormat::ConfigDriven) {
    config_path_ = toFsPath(path);
    auto base_dir = config_path_.parent_path();
    std::error_code ec;
    std::filesystem::create_directories(base_dir, ec);

    // Build file entries from current repo frames
    file_entries_.clear();
    frame_file_map_.clear();
    for (const auto& frame_ptr : repo_.frames()) {
      auto rel_path = generateFrameFilePath(frame_ptr->id(),
                                            std::string(frame_ptr->name()));
      icd::FrameFileInfo info;
      info.id = frame_ptr->id();
      info.name = std::string(frame_ptr->name());
      info.description = std::string(frame_ptr->description());
      info.path = rel_path.string();
      info.type = frame_ptr->type();
      info.order = frame_ptr->order();
      info.format = icd::Format::xml;
      file_entries_.push_back(std::move(info));
      frame_file_map_.insert(frame_ptr->id(),
                             QString::fromStdString(rel_path.string()));
    }

    // saveConfigDriven() writes all frame files + ICDConfig
    if (!saveConfigDriven()) {
      current_file_ = old;
      return false;
    }
  } else {
    // Single-file format
    if (saveByFormat()) {
      setModified(false);
      emit editorIdChanged(old, path);
      return true;
    }
    current_file_ = old;
    return false;
  }

  setModified(false);
  emit editorIdChanged(old, path);
  return true;
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
  emit undoStateChanged();
  emit commandsChanged();
}
void ProtocolEditorWidget::redo() {
  if (!canRedo())
    return;
  ++snapshot_index_;
  restoreSnapshot(snapshots_[snapshot_index_]);
  setModified(snapshot_index_ != 0);
  emit undoStateChanged();
  emit commandsChanged();
}

void ProtocolEditorWidget::setReadOnly(bool readOnly) {
  if (node_tree_)
    node_tree_->setEnabled(!readOnly);
  if (bit_view_)
    bit_view_->setEnabled(!readOnly);
  if (property_panel_)
    property_panel_->setEnabled(!readOnly);
  if (new_frame_action_)
    new_frame_action_->setEnabled(!readOnly);
  if (delete_frame_action_)
    delete_frame_action_->setEnabled(!readOnly);
  if (add_node_action_)
    add_node_action_->setEnabled(!readOnly);
  if (delete_selected_action_)
    delete_selected_action_->setEnabled(!readOnly);
  if (undo_action_)
    undo_action_->setEnabled(!readOnly);
  if (redo_action_)
    redo_action_->setEnabled(!readOnly);
  if (frame_type_combo_)
    frame_type_combo_->setEnabled(!readOnly);
}

void ProtocolEditorWidget::openFile(const QString& filePath) {
  if (filePath == current_file_)
    return;
  current_file_ = filePath;

  if (!QFileInfo::exists(filePath))
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

  // ── Determine format from file extension ──
  auto path = toFsPath(filePath);
  auto ext = path.extension().string();

  // Detect format
  if (ext == ".eproto") {
    format_ = ProtocolFormat::Json;
  } else if (ext == ".eprotox") {
    format_ = ProtocolFormat::Xml;
  } else if (ext == ".xml" || ext == ".json") {
    format_ = ProtocolFormat::ConfigDriven;
  } else {
    format_ = ProtocolFormat::Json;  // default
  }

  // ── Async load based on format ──
  load_watcher_ = new QFutureWatcher<AsyncLoadResult>(this);
  connect(load_watcher_, &QFutureWatcher<AsyncLoadResult>::finished, this,
          [this, thisGeneration]() {
            if (thisGeneration != load_generation_)
              return;

            auto result = load_watcher_->result();
            load_watcher_->deleteLater();
            load_watcher_ = nullptr;

            if (!result.repo) {
              hideLoadingOverlay();
              QMessageBox::warning(
                  this, QStringLiteral("加载失败"),
                  QStringLiteral("无法加载协议文件: %1").arg(current_file_));
              return;
            }

            // Apply ConfigDriven metadata if present
            if (!result.config_path.empty()) {
              config_path_ = result.config_path;
              config_format_ = result.config_format;
              file_entries_ = std::move(result.file_entries);
              frame_file_map_.clear();
              for (const auto& entry : file_entries_) {
                frame_file_map_.insert(entry.id,
                                       QString::fromStdString(entry.path));
              }
            }

            repo_ = std::move(*result.repo);
            populateFrames();

            if (!repo_.frames().empty()) {
              if (initial_frame_id_ >= 0) {
                // 导航到指定帧（来自 openFrameRequested）
                auto* target = repo_.find(initial_frame_id_);
                if (target) {
                  setCurrentFrame(const_cast<icd::Frame*>(target));
                } else {
                  setCurrentFrame(repo_.frames()[0].get());
                }
                initial_frame_id_ = -1;
              } else {
                setCurrentFrame(repo_.frames()[0].get());
              }
            }

            saveSnapshot();
            hideLoadingOverlay();
          });

  switch (format_) {
    case ProtocolFormat::Json: {
      auto jsonPath = toFsPath(filePath);
      load_watcher_->setFuture(
          QtConcurrent::run([jsonPath]() -> AsyncLoadResult {
            auto result = icd::format::deserialize_repository(jsonPath);
            if (!result)
              return {};
            return {std::make_shared<icd::Repository>(std::move(*result))};
          }));
      break;
    }

    case ProtocolFormat::Xml: {
      auto xmlPath = toFsPath(filePath);
      load_watcher_->setFuture(
          QtConcurrent::run([xmlPath]() -> AsyncLoadResult {
            auto result = icd::format::deserialize_xml_repository(xmlPath);
            if (!result)
              return {};
            return {std::make_shared<icd::Repository>(std::move(*result))};
          }));
      break;
    }

    case ProtocolFormat::ConfigDriven:
      load_watcher_->setFuture(QtConcurrent::run([path]() -> AsyncLoadResult {
        auto loadResult = icd::Loader::init_with_metadata(path);
        if (!loadResult)
          return {};
        AsyncLoadResult ar;
        ar.repo = std::make_shared<icd::Repository>(
            std::move(loadResult->repository));
        ar.config_path = loadResult->config_path;
        ar.config_format = loadResult->format;
        ar.file_entries = std::move(loadResult->file_entries);
        return ar;
      }));
      break;
  }
}

// ── Loading overlay ─────────────────────────────────────────

void ProtocolEditorWidget::showLoadingOverlay() {
  if (!loading_overlay_) {
    loading_overlay_ = new QWidget(this);
    loading_overlay_->setObjectName(QStringLiteral("PhLoadingOverlay"));
    loading_overlay_->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    auto* lay = new QVBoxLayout(loading_overlay_);
    lay->setAlignment(Qt::AlignCenter);
    auto* label =
        new QLabel(QStringLiteral("正在加载协议文件..."), loading_overlay_);
    label->setObjectName(QStringLiteral("PhLoadingOverlayLabel"));
    label->setAlignment(Qt::AlignCenter);
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

bool ProtocolEditorWidget::eventFilter(QObject* obj, QEvent* event) {
  if (event->type() == QEvent::Close) {
    if (obj == node_tree_dock_)
      syncDockCloseAction(node_tree_toggle_action_);
    else if (obj == property_dock_)
      syncDockCloseAction(property_toggle_action_);
    else if (obj == preview_dock_)
      syncDockCloseAction(preview_toggle_action_);
  }
  return QMainWindow::eventFilter(obj, event);
}

// ── Save format routing ────────────────────────────────────────
bool ProtocolEditorWidget::saveByFormat() {
  switch (format_) {
    case ProtocolFormat::Json:
      return saveEproto(current_file_);
    case ProtocolFormat::Xml:
      return saveEprotox(current_file_);
    case ProtocolFormat::ConfigDriven:
      return saveConfigDriven();
  }
  return false;
}

// ── Save .eproto JSON ─────────────────────────────────────────
bool ProtocolEditorWidget::saveEproto(const QString& path) {
  auto result = icd::format::serialize_repository(toFsPath(path), repo_);
  return result.has_value();
}

// ── Save .eprotox XML ─────────────────────────────────────────
bool ProtocolEditorWidget::saveEprotox(const QString& path) {
  auto result = icd::format::serialize_xml_repository(toFsPath(path), repo_);
  return result.has_value();
}

// ── Save ConfigDriven (ICDConfig + frame files) ────────────────
bool ProtocolEditorWidget::saveConfigDriven() {
  if (config_path_.empty())
    return false;

  auto base_dir = config_path_.parent_path();
  std::error_code ec;
  std::filesystem::create_directories(base_dir, ec);

  // Update file entries from current repo state
  for (auto& entry : file_entries_) {
    const auto* frame = repo_.find(entry.id);
    if (frame) {
      entry.name = std::string(frame->name());
      entry.description = std::string(frame->description());
      entry.type = frame->type();
      entry.order = frame->order();
    }
  }

  // Sync frame_file_map_ keys with file_entries_
  frame_file_map_.clear();
  for (const auto& entry : file_entries_) {
    frame_file_map_.insert(entry.id, QString::fromStdString(entry.path));
  }

  // Write all frame files
  for (const auto& frame_ptr : repo_.frames()) {
    QString relPath = frame_file_map_.value(frame_ptr->id());
    if (relPath.isEmpty()) {
      qWarning("saveConfigDriven: no file path for frame id %d",
               frame_ptr->id());
      continue;
    }
    auto abs_path = base_dir / toFsPath(relPath);
    auto result = icd::format::serialize_xml_frame_file(abs_path, *frame_ptr);
    if (!result) {
      qWarning("saveConfigDriven: failed to write frame file: %s",
               result.error().message.c_str());
      return false;
    }
  }

  // Write ICDConfig
  return rewriteConfigFile();
}

bool ProtocolEditorWidget::rewriteConfigFile() {
  if (config_format_ == icd::Format::json) {
    auto result =
        icd::format::serialize_json_config(config_path_, file_entries_);
    return result.has_value();
  }
  auto result = icd::format::serialize_xml_config(config_path_, file_entries_);
  return result.has_value();
}

// ── ConfigDriven: generate frame file path ────────────────────
std::filesystem::path ProtocolEditorWidget::generateFrameFilePath(
    int frame_id,
    const std::string& frame_name) const {
  // Sanitize frame name: replace illegal Windows file chars with '_'
  auto sanitized = frame_name;
  static const char illegal_chars[] = {'\\', '/', ':', '*', '?',
                                       '"',  '<', '>', '|', '\0'};
  for (auto& c : sanitized) {
    for (auto ic : illegal_chars) {
      if (c == ic || static_cast<unsigned char>(c) < 0x20) {
        c = '_';
        break;
      }
    }
  }
  if (sanitized.size() > 50) {
    sanitized = sanitized.substr(0, 50);
  }

  auto rel_path = std::filesystem::path("frame_" + std::to_string(frame_id) +
                                        "_" + sanitized + ".xml");

  // Check conflict and append suffix if needed
  if (checkFrameFileNameConflict(rel_path)) {
    int suffix = 2;
    while (true) {
      auto candidate = std::filesystem::path(
          "frame_" + std::to_string(frame_id) + "_" + sanitized + "_" +
          std::to_string(suffix) + ".xml");
      if (!checkFrameFileNameConflict(candidate)) {
        rel_path = candidate;
        break;
      }
      ++suffix;
    }
  }

  return rel_path;
}

bool ProtocolEditorWidget::checkFrameFileNameConflict(
    const std::filesystem::path& rel_path) const {
  // Check disk existence
  if (!config_path_.empty()) {
    auto abs_path = config_path_.parent_path() / rel_path;
    std::error_code ec;
    if (std::filesystem::exists(abs_path, ec))
      return true;
  }
  // Check current file_entries_
  auto relStr = rel_path.string();
  for (const auto& entry : file_entries_) {
    if (entry.path == relStr)
      return true;
  }
  return false;
}

bool ProtocolEditorWidget::createConfigFrameFile(
    const icd::Frame& frame,
    const std::filesystem::path& rel_path) {
  if (config_path_.empty())
    return false;
  auto abs_path = config_path_.parent_path() / rel_path;
  auto result = icd::format::serialize_xml_frame_file(abs_path, frame);
  return result.has_value();
}

bool ProtocolEditorWidget::deleteConfigFrameFile(int frame_id) {
  QString relPath = frame_file_map_.value(frame_id);
  if (relPath.isEmpty())
    return false;
  if (!config_path_.empty()) {
    auto abs_path = config_path_.parent_path() / toFsPath(relPath);
    std::error_code ec;
    std::filesystem::remove(abs_path, ec);
  }
  return true;
}

bool ProtocolEditorWidget::rewriteAllFrameFiles() {
  if (config_path_.empty())
    return false;
  auto base_dir = config_path_.parent_path();
  for (const auto& frame_ptr : repo_.frames()) {
    QString relPath = frame_file_map_.value(frame_ptr->id());
    if (relPath.isEmpty()) {
      qWarning("rewriteAllFrameFiles: no file path for frame id %d",
               frame_ptr->id());
      continue;
    }
    auto abs_path = base_dir / toFsPath(relPath);
    auto result = icd::format::serialize_xml_frame_file(abs_path, *frame_ptr);
    if (!result) {
      qWarning("rewriteAllFrameFiles: failed: %s",
               result.error().message.c_str());
      return false;
    }
  }
  return rewriteConfigFile();
}

// ── ConfigDriven helpers: extracted to eliminate duplication ──────
void ProtocolEditorWidget::addConfigFrameEntry(int id,
                                               const std::string& name,
                                               icd::Frame& frame) {
  auto rel_path = generateFrameFilePath(id, name);

  // Write frame file first — if it fails, do not touch metadata or config
  if (!createConfigFrameFile(frame, rel_path)) {
    qWarning("addConfigFrameEntry: 帧文件创建失败 (id=%d, name=%s)", id,
             name.c_str());
    return;
  }

  icd::FrameFileInfo info;
  info.id = id;
  info.name = name;
  info.description = "";
  info.path = rel_path.string();
  info.type = icd::FrameType::data;
  info.order = icd::ByteOrder::little_endian;
  info.format = icd::Format::xml;
  file_entries_.push_back(std::move(info));
  frame_file_map_.insert(id, QString::fromStdString(rel_path.string()));
  rewriteConfigFile();
}

void ProtocolEditorWidget::removeConfigFrameEntry(int id) {
  deleteConfigFrameFile(id);
  file_entries_.erase(
      std::remove_if(file_entries_.begin(), file_entries_.end(),
                     [id](const icd::FrameFileInfo& e) { return e.id == id; }),
      file_entries_.end());
  frame_file_map_.remove(id);
  rewriteConfigFile();
}

// ── UI ─────────────────────────────────────────────────────────
void ProtocolEditorWidget::initUi() {
  setAutoFillBackground(true);

  // ── Icon loader (theme-aware) ──
  auto protoIcon = [](const QString& name) {
    return etest::core_ui::AppIconProvider::instance().icon(name);
  };

  // ── QToolBar (帧操作 + 帧属性) ──
  toolbar_ = addToolBar(QStringLiteral("帧工具栏"));
  toolbar_->setObjectName(QStringLiteral("protocolToolbar"));
  toolbar_->setMovable(false);
  toolbar_->setFloatable(false);

  // 撤销 / 重做
  undo_action_ = new QAction(protoIcon(QStringLiteral("undo")),
                             QStringLiteral("撤销"), this);
  undo_action_->setToolTip(QStringLiteral("撤销 (Ctrl+Z)"));
  undo_action_->setEnabled(false);
  toolbar_->addAction(undo_action_);

  redo_action_ = new QAction(protoIcon(QStringLiteral("redo")),
                             QStringLiteral("重做"), this);
  redo_action_->setToolTip(QStringLiteral("重做 (Ctrl+Y)"));
  redo_action_->setEnabled(false);
  toolbar_->addAction(redo_action_);

  toolbar_->addSeparator();

  // 新建帧
  new_frame_action_ =
      new QAction(protoIcon(QStringLiteral("protocol_new_frame")),
                  QStringLiteral("+帧"), this);
  new_frame_action_->setToolTip(QStringLiteral("新建帧"));
  toolbar_->addAction(new_frame_action_);

  // 删除帧
  delete_frame_action_ =
      new QAction(protoIcon(QStringLiteral("protocol_delete_frame")),
                  QStringLiteral("-帧"), this);
  delete_frame_action_->setToolTip(QStringLiteral("删除当前帧"));
  delete_frame_action_->setEnabled(false);
  toolbar_->addAction(delete_frame_action_);

  toolbar_->addSeparator();

  // 添加节点（当前选中字段下加子字段，无选中则在当前帧加根字段）
  add_node_action_ = new QAction(protoIcon(QStringLiteral("protocol_add_node")),
                                 QStringLiteral("+节点"), this);
  add_node_action_->setToolTip(
      QStringLiteral("添加信号（选中帧→根字段，选中字段→子字段）"));
  add_node_action_->setEnabled(false);
  toolbar_->addAction(add_node_action_);

  // 删除选中节点
  delete_selected_action_ =
      new QAction(protoIcon(QStringLiteral("protocol_delete_node")),
                  QStringLiteral("-选中"), this);
  delete_selected_action_->setToolTip(QStringLiteral("删除选中的字段"));
  delete_selected_action_->setEnabled(false);
  toolbar_->addAction(delete_selected_action_);

  toolbar_->addSeparator();

  // 帧属性区
  auto* title_label = new QLabel(QStringLiteral("帧属性"), toolbar_);
  title_label->setObjectName(QStringLiteral("protocolTitleLabel"));
  toolbar_->addWidget(title_label);

  frame_name_label_ = new QLabel(QStringLiteral("(无帧)"), toolbar_);
  frame_name_label_->setObjectName(QStringLiteral("frameNameLabel"));
  toolbar_->addWidget(frame_name_label_);

  frame_type_combo_ = new QComboBox(toolbar_);
  frame_type_combo_->addItem(QStringLiteral("数据 (Data)"));
  frame_type_combo_->addItem(QStringLiteral("命令 (Cmd)"));
  frame_type_combo_->addItem(QStringLiteral("数据/命令 (DataCfg)"));
  frame_type_combo_->setEnabled(false);
  toolbar_->addWidget(frame_type_combo_);

  // 字节序切换按钮（取代之前的 QComboBox）
  byte_order_btn_ = new QToolButton(toolbar_);
  byte_order_btn_->setIcon(protoIcon(QStringLiteral("protocol_byte_order")));
  byte_order_btn_->setText(QStringLiteral("LE"));
  byte_order_btn_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  byte_order_btn_->setCheckable(true);
  byte_order_btn_->setToolTip(
      QStringLiteral("切换字节序：小端(LE) / 大端(BE)"));
  byte_order_btn_->setEnabled(false);
  toolbar_->addWidget(byte_order_btn_);

  frame_id_label_ = new QLabel(QStringLiteral("ID: -"), toolbar_);
  frame_id_label_->setObjectName(QStringLiteral("idLabel"));
  toolbar_->addWidget(frame_id_label_);

  frame_length_label_ = new QLabel(QStringLiteral("长度: -"), toolbar_);
  frame_length_label_->setObjectName(QStringLiteral("lengthLabel"));

  // 弹簧 + 长度标签（右对齐）
  auto* spacer = new QWidget(toolbar_);
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar_->addWidget(spacer);

  toolbar_->addWidget(frame_length_label_);

  toolbar_->addSeparator();

  // 面板开关 toggle actions
  node_tree_toggle_action_ =
      new QAction(protoIcon(QStringLiteral("protocol_node_tree")),
                  QStringLiteral("节点列表"), this);
  node_tree_toggle_action_->setCheckable(true);
  node_tree_toggle_action_->setChecked(true);
  node_tree_toggle_action_->setToolTip(QStringLiteral("显示/隐藏节点列表"));
  toolbar_->addAction(node_tree_toggle_action_);

  property_toggle_action_ =
      new QAction(protoIcon(QStringLiteral("protocol_property")),
                  QStringLiteral("属性面板"), this);
  property_toggle_action_->setCheckable(true);
  property_toggle_action_->setChecked(true);
  property_toggle_action_->setToolTip(QStringLiteral("显示/隐藏属性面板"));
  toolbar_->addAction(property_toggle_action_);

  preview_toggle_action_ =
      new QAction(protoIcon(QStringLiteral("protocol_preview")),
                  QStringLiteral("报文预览"), this);
  preview_toggle_action_->setCheckable(true);
  preview_toggle_action_->setChecked(true);
  preview_toggle_action_->setToolTip(QStringLiteral("显示/隐藏报文预览"));
  toolbar_->addAction(preview_toggle_action_);

  // ── Dock: 节点列表 (左侧) ──
  node_tree_ = new IcdNodeTreeWidget(this);
  node_tree_->setMinimumWidth(200);
  node_tree_->setObjectName(QStringLiteral("protocolNodeTree"));

  node_tree_dock_ =
      new ::etest::ui::EtDockWidget(QStringLiteral("节点列表"), this);
  node_tree_dock_->setObjectName(QStringLiteral("protocolNodeTreeDock"));
  node_tree_dock_->setWidget(node_tree_);
  addDockWidget(Qt::LeftDockWidgetArea, node_tree_dock_);

  // ── Dock: 属性面板 (右侧) ──
  property_panel_ = new IcdPropertyPanel(this);
  property_panel_->setMinimumWidth(220);
  property_panel_->setObjectName(QStringLiteral("protocolPropertyPanel"));

  property_dock_ =
      new ::etest::ui::EtDockWidget(QStringLiteral("属性面板"), this);
  property_dock_->setObjectName(QStringLiteral("protocolPropertyDock"));
  property_dock_->setWidget(property_panel_);
  addDockWidget(Qt::RightDockWidgetArea, property_dock_);

  // ── Dock: 报文预览 (节点列表下方) ──
  preview_panel_ = new IcdFramePreviewPanel(this);
  preview_panel_->setMinimumHeight(120);
  preview_panel_->setObjectName(QStringLiteral("protocolPreviewPanel"));

  preview_dock_ =
      new ::etest::ui::EtDockWidget(QStringLiteral("报文预览"), this);
  preview_dock_->setObjectName(QStringLiteral("protocolPreviewDock"));
  preview_dock_->setWidget(preview_panel_);
  addDockWidget(Qt::LeftDockWidgetArea, preview_dock_);
  splitDockWidget(node_tree_dock_, preview_dock_, Qt::Vertical);

  node_tree_dock_->installEventFilter(this);
  property_dock_->installEventFilter(this);
  preview_dock_->installEventFilter(this);

  // ── Central Widget: 位图视图 ──
  bit_view_ = new IcdBitLayoutView(this);
  setCentralWidget(bit_view_);
}

// ── Signals ───────────────────────────────────────────────────
void ProtocolEditorWidget::initSignals() {
  // Frame selection from tree
  connect(node_tree_, &IcdNodeTreeWidget::frameSelected, this,
          [this](icd::Frame* frame) {
            current_selected_node_ = nullptr;
            updateToolbar();
            setCurrentFrame(frame);
          });

  // Node selection from tree → property panel + bit view highlight
  connect(node_tree_, &IcdNodeTreeWidget::nodeSelected, this,
          [this](icd::Node* node) {
            current_selected_node_ = node;
            updateToolbar();
            if (node) {
              property_panel_->showNode(*node);
              bit_view_->highlightNode(node);
              showStatusMessage(
                  QStringLiteral("Node: %1  |  Offset: %2  |  Bit: %3~%4")
                      .arg(QString::fromStdString(std::string(node->name())))
                      .arg(node->offset())
                      .arg(node->bit_offset())
                      .arg(node->bit_offset() + node->bit_width() - 1));
            }
          });

  // Bit node clicked → highlight node + property panel
  connect(bit_view_, &IcdBitLayoutView::nodeClicked, this,
          [this](const icd::Node* node) {
            if (!current_frame_ || !node) {
              return;
            }
            bit_view_->highlightNode(node);
            auto* editable_node = const_cast<icd::Node*>(node);
            property_panel_->showNode(*editable_node);
            node_tree_->selectNode(node);
            showStatusMessage(
                QStringLiteral("Node: %1  |  Offset: %2  |  Bit: %3~%4")
                    .arg(QString::fromStdString(std::string(node->name())))
                    .arg(node->offset())
                    .arg(node->bit_offset())
                    .arg(node->bit_offset() + node->bit_width() - 1));
          });

  // Bit node hover → reveal tree node (scroll only, no selection change)
  connect(bit_view_, &IcdBitLayoutView::nodeHovered, this,
          [this](const icd::Node* node, bool /*on*/) {
            if (!current_frame_ || !node) {
              return;
            }
            node_tree_->revealNode(node);
          });

  // 报文预览选中行 → 联动位布局高亮 + 节点树选中 + 属性面板
  connect(preview_panel_, &IcdFramePreviewPanel::nodeActivated, this,
          [this](const icd::Node* node) {
            if (!current_frame_ || !node) {
              return;
            }
            bit_view_->highlightNode(node);
            node_tree_->selectNode(node);
            auto* editable_node = const_cast<icd::Node*>(node);
            property_panel_->showNode(*editable_node);
          });

  // Bit node context menu actions
  connect(
      bit_view_, &IcdBitLayoutView::nodeContextMenuAction, this,
      [this](const icd::Node* node, const QString& action) {
        if (!current_frame_ || !node)
          return;
        auto* frame = current_frame_;
        const QString name = QString::fromStdString(std::string(node->name()));

        if (action == QStringLiteral("delete")) {
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

    // ConfigDriven: create frame file and update metadata
    if (format_ == ProtocolFormat::ConfigDriven && !config_path_.empty()) {
      addConfigFrameEntry(new_id, name, *frame_ptr);
    }

    setModified(true);
  });

  // Delete frame (action)
  connect(delete_frame_action_, &QAction::triggered, this, [this]() {
    if (!current_frame_)
      return;
    saveSnapshot();
    int id = current_frame_->id();
    setCurrentFrame(nullptr);

    if (format_ == ProtocolFormat::ConfigDriven) {
      removeConfigFrameEntry(id);
    }

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
    auto order =
        checked ? icd::ByteOrder::big_endian : icd::ByteOrder::little_endian;
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
  // 撤销/重做可用性变化（undo/redo/saveSnapshot 无条件发射，不受
  // setModified 的 modified_ != modified 守卫门控）→ 刷新自身 toolbar
  connect(this, &ProtocolEditorWidget::undoStateChanged, this, [this]() {
    undo_action_->setEnabled(canUndo());
    redo_action_->setEnabled(canRedo());
  });

  // ── Toolbar: 添加 / 删除 节点 ──
  connect(add_node_action_, &QAction::triggered, this, [this]() {
    if (!current_frame_)
      return;
    saveSnapshot();
    auto node = std::make_unique<icd::Node>("NewNode", "", 0, 0, 8,
                                            icd::ValueType::byte_,
                                            icd::Tag::none, icd::NodeAttrs{});
    if (current_selected_node_) {
      const_cast<icd::Node*>(current_selected_node_)
          ->add_child(std::move(node));
    } else {
      current_frame_->add_root(std::move(node));
    }
    refreshAndSelectFrame(current_frame_);
    setModified(true);
  });

  connect(delete_selected_action_, &QAction::triggered, this, [this]() {
    if (!current_frame_ || !current_selected_node_)
      return;
    saveSnapshot();
    auto* parent = const_cast<icd::Node*>(current_selected_node_->parent());
    if (!parent) {
      const auto& roots = current_frame_->roots();
      for (std::size_t i = 0; i < roots.size(); ++i) {
        if (roots[i].get() == current_selected_node_) {
          current_frame_->remove_root(i);
          break;
        }
      }
    } else {
      const auto& children = parent->children();
      for (std::size_t i = 0; i < children.size(); ++i) {
        if (children[i].get() == current_selected_node_) {
          parent->remove_child(i);
          break;
        }
      }
    }
    current_selected_node_ = nullptr;
    refreshAndSelectFrame(current_frame_);
    setModified(true);
  });

  // ── Dock toggle actions（eventFilter 拦截 QEvent::Close） ──
  connect(node_tree_toggle_action_, &QAction::toggled, this,
          [this](bool checked) { node_tree_dock_->setVisible(checked); });
  connect(property_toggle_action_, &QAction::toggled, this,
          [this](bool checked) { property_dock_->setVisible(checked); });
  connect(preview_toggle_action_, &QAction::toggled, this,
          [this](bool checked) { preview_dock_->setVisible(checked); });

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

    // ConfigDriven: create frame file and update metadata
    if (format_ == ProtocolFormat::ConfigDriven && !config_path_.empty()) {
      addConfigFrameEntry(new_id, name, *frame_ptr);
    }

    setModified(true);
  });

  connect(node_tree_, &IcdNodeTreeWidget::deleteFrameRequested, this,
          [this](int frameId) {
            saveSnapshot();

            if (format_ == ProtocolFormat::ConfigDriven) {
              removeConfigFrameEntry(frameId);
            }

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
    byte_order_btn_->setChecked(current_frame_->order() ==
                                icd::ByteOrder::big_endian);
    byte_order_btn_->setText(current_frame_->order() ==
                                     icd::ByteOrder::little_endian
                                 ? QStringLiteral("LE")
                                 : QStringLiteral("BE"));
    byte_order_btn_->blockSignals(false);

    frame_length_label_->setText(
        QStringLiteral("长度: %1 bytes").arg(calcFrameLength(*current_frame_)));

    frame_type_combo_->setEnabled(true);
    byte_order_btn_->setEnabled(true);
    delete_frame_action_->setEnabled(true);
    add_node_action_->setEnabled(true);
    delete_selected_action_->setEnabled(current_selected_node_ != nullptr);
  } else {
    frame_name_label_->setText(QStringLiteral("(无帧)"));
    frame_id_label_->setText(QStringLiteral("ID: -"));
    frame_length_label_->setText(QStringLiteral("长度: -"));
    frame_type_combo_->setEnabled(false);
    byte_order_btn_->setEnabled(false);
    delete_frame_action_->setEnabled(false);
    add_node_action_->setEnabled(false);
    delete_selected_action_->setEnabled(false);
  }
  emit commandsChanged();
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
    preview_panel_->setFrame(frame);
    showStatusMessage(
        QStringLiteral("Frame: %1  |  ID: %2")
            .arg(QString::fromStdString(std::string(frame->name())))
            .arg(frame->id()));
  } else {
    bit_view_->clearBlocks();
    property_panel_->clear();
    preview_panel_->setFrame(nullptr);
    showStatusMessage(QStringLiteral("就绪"));
  }
  updateToolbar();
}

void ProtocolEditorWidget::navigateToFrame(int frameId) {
  if (config_path_.empty() || format_ != ProtocolFormat::ConfigDriven) {
    return;
  }
  // 如果加载尚未完成，记下来等加载完成后跳转
  if (load_watcher_) {
    initial_frame_id_ = frameId;
    return;
  }
  // 加载已完成，直接查找并跳转
  auto* frame = repo_.find(frameId);
  if (frame) {
    setCurrentFrame(const_cast<icd::Frame*>(frame));
  }
}

// ── Refresh tree + select frame ──────────────────────────────
void ProtocolEditorWidget::refreshAndSelectFrame(icd::Frame* frame) {
  populateFrames();
  setCurrentFrame(frame);
}

// ── Clear all data ───────────────────────────────────────────
void ProtocolEditorWidget::clearAll() {
  current_frame_ = nullptr;
  repo_ = icd::Repository();
  node_tree_->clear();
  bit_view_->clearBlocks();
  property_panel_->clear();
  updateToolbar();
  snapshots_.clear();
  snapshot_frame_ids_.clear();
  snapshot_index_ = -1;

  // Reset format-specific state
  format_ = ProtocolFormat::Json;
  config_format_ = icd::Format::xml;
  config_path_.clear();
  file_entries_.clear();
  frame_file_map_.clear();

  // 快照重置后 canUndo/canRedo 归零，通知撤销状态变化
  emit undoStateChanged();
  emit commandsChanged();
}

// ── Snapshot (undo/redo) ──────────────────────────────────────
void ProtocolEditorWidget::saveSnapshot() {
  nlohmann::json snapshot;
  snapshot["snapshot_version"] = 1;
  snapshot["repository"] = icd::format::serialize_repository_to_json(repo_);

  // Include file_entries for ConfigDriven mode (always included for simplicity)
  nlohmann::json entries = nlohmann::json::array();
  for (const auto& entry : file_entries_) {
    entries.push_back(
        {{"id", entry.id}, {"name", entry.name}, {"path", entry.path}});
  }
  snapshot["file_entries"] = entries;
  // ⚠️ 必须用 u8string() 而非 string()：后者在中文路径下会输出 ANSI/GBK 编码，
  // 导致 nlohmann::json::dump() 序列化时抛出 type_error(316) "invalid UTF-8
  // byte"
  snapshot["config_path"] = config_path_.u8string();

  QString jsonStr = QString::fromStdString(snapshot.dump());
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

  // 快照截断/追加后 canUndo/canRedo 可能变化，无条件通知
  // （setModified 有 modified_ != modified 守卫，脏态间 undo 不触发
  // modificationChanged）
  emit undoStateChanged();
  emit commandsChanged();
}

void ProtocolEditorWidget::restoreSnapshot(const QByteArray& data) {
  QByteArray raw = qUncompress(data);
  if (raw.isEmpty())
    return;
  auto j = nlohmann::json::parse(raw.toStdString());
  auto result = icd::format::deserialize_repository(j["repository"]);
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

  // Handle ConfigDriven disk reconstruction
  // Use current editor format, not snapshot content — even an empty-frame
  // snapshot needs disk cleanup (delete stale files, rewrite ICDConfig with
  // zero entries).
  bool isConfigDriven =
      (format_ == ProtocolFormat::ConfigDriven) && !config_path_.empty();
  if (isConfigDriven) {
    // Delete current frame files
    for (const auto& entry : file_entries_) {
      auto abs_path = config_path_.parent_path() / entry.path;
      std::error_code ec;
      std::filesystem::remove(abs_path, ec);
    }

    // Restore file_entries_ from snapshot
    file_entries_.clear();
    frame_file_map_.clear();
    for (const auto& entry_json : j["file_entries"]) {
      icd::FrameFileInfo entry;
      entry.id = entry_json["id"].get<int>();
      entry.name = entry_json["name"].get<std::string>();
      entry.path = entry_json["path"].get<std::string>();
      file_entries_.push_back(std::move(entry));
      frame_file_map_.insert(file_entries_.back().id,
                             QString::fromStdString(file_entries_.back().path));
    }

    // Restore config_path from snapshot
    if (j.contains("config_path") && !j["config_path"].is_null()) {
      config_path_ = std::filesystem::path(j["config_path"].get<std::string>());
    }
  }

  repo_ = std::move(*result);
  populateFrames();

  // If ConfigDriven, rewrite all frame files after restoring repo
  if (isConfigDriven && !config_path_.empty()) {
    // Restore metadata (description/type/order) from repo_ before writing
    for (auto& entry : file_entries_) {
      const auto* frame = repo_.find(entry.id);
      if (frame) {
        entry.description = std::string(frame->description());
        entry.type = frame->type();
        entry.order = frame->order();
      }
    }
    rewriteAllFrameFiles();
  }

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
    return etest::core_ui::AppIconProvider::instance().icon(name);
  };
  new_frame_action_->setIcon(icon(QStringLiteral("protocol_new_frame")));
  delete_frame_action_->setIcon(icon(QStringLiteral("protocol_delete_frame")));
  undo_action_->setIcon(icon(QStringLiteral("undo")));
  redo_action_->setIcon(icon(QStringLiteral("redo")));
  byte_order_btn_->setIcon(icon(QStringLiteral("protocol_byte_order")));
  node_tree_toggle_action_->setIcon(icon(QStringLiteral("protocol_node_tree")));
  property_toggle_action_->setIcon(icon(QStringLiteral("protocol_property")));
  preview_toggle_action_->setIcon(icon(QStringLiteral("protocol_preview")));
  add_node_action_->setIcon(icon(QStringLiteral("protocol_add_node")));
  delete_selected_action_->setIcon(
      icon(QStringLiteral("protocol_delete_node")));
}

}  // namespace etest::protocol
