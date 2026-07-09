#pragma once

#include <QByteArray>
#include <QFutureWatcher>
#include <QHash>
#include <QTimer>
#include <QVector>
#include <QMainWindow>

#include <filesystem>
#include <memory>

#include "api/IEditor.h"

#include <icd/file_entry.hpp>
#include <icd/frame.hpp>
#include <icd/repository.hpp>

class QComboBox;
class QLabel;
class QDockWidget;
class QAction;
class QToolBar;
class QToolButton;
class QResizeEvent;

namespace etest::protocol {

class IcdNodeTreeWidget;
class IcdBitLayoutView;
class IcdPropertyPanel;
class IcdFramePreviewPanel;

class ProtocolEditorWidget : public QMainWindow, public etest::app::IEditor {
  Q_OBJECT
 public:
  explicit ProtocolEditorWidget(QWidget* parent = nullptr);
  ~ProtocolEditorWidget() override;

  // IEditor interface
  QString displayName() const override;
  bool isModified() const override;
  bool save() override;
  bool saveAs(const QString& path) override;
  QString filePath() const override;
  QString editorId() const override;
  QWidget* widget() override;
  QString editorType() const override;
  QObject* signalObject() override;

  // Undo/Redo
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;

  // 通过帧 ID 导航（用于 ConfigDriven 模式从外部跳转到指定帧）
  void navigateToFrame(int frameId);

  // Async load result carrying optional ConfigDriven metadata
  struct AsyncLoadResult {
    std::shared_ptr<icd::Repository> repo;
    std::vector<icd::FrameFileInfo> file_entries;
    std::filesystem::path config_path;
    icd::Format config_format = icd::Format::xml;
  };

  enum class ProtocolFormat {
    Json,           // .eproto
    Xml,            // .eprotox
    ConfigDriven    // ICDConfig.xml/.json
  };

  void openFile(const QString& filePath) override;

  // Embedded mode (hide menuBar/toolbar when hosted in IDE)
  void setEmbeddedMode(bool embedded);

  // Reload toolbar icons (for theme switching)
  void reloadToolbarIcons();

 signals:
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);

 private:
  void showLoadingOverlay();
  void hideLoadingOverlay();
  void resizeEvent(QResizeEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void showStatusMessage(const QString& msg);

  QWidget* loading_overlay_ = nullptr;
  QFutureWatcher<AsyncLoadResult>* load_watcher_ = nullptr;
  QTimer* modified_debounce_ = nullptr;
  bool embedded_ = false;

  bool saveEproto(const QString& path);
  bool saveEprotox(const QString& path);
  bool saveConfigDriven();
  bool saveByFormat();
  void initUi();
  void initSignals();
  void updateToolbar();
  void clearAll();
  void setModified(bool modified);
  void setCurrentFrame(icd::Frame* frame);
  void populateFrames();
  void refreshAndSelectFrame(icd::Frame* frame);
  void saveSnapshot();
  void restoreSnapshot(const QByteArray& data);

  // ConfigDriven helpers
  std::filesystem::path generateFrameFilePath(int frame_id, const std::string& frame_name) const;
  bool checkFrameFileNameConflict(const std::filesystem::path& rel_path) const;
  bool createConfigFrameFile(const icd::Frame& frame, const std::filesystem::path& rel_path);
  bool deleteConfigFrameFile(int frame_id);
  bool rewriteAllFrameFiles();
  bool rewriteConfigFile();
  void addConfigFrameEntry(int id, const std::string& name, icd::Frame& frame);
  void removeConfigFrameEntry(int id);

  static constexpr int kMaxSnapshots = 32;

  // Dock widgets
  QDockWidget* node_tree_dock_ = nullptr;
  QDockWidget* property_dock_ = nullptr;
  QDockWidget* preview_dock_ = nullptr;

  // Panels (owned by docks)
  IcdNodeTreeWidget* node_tree_ = nullptr;
  IcdBitLayoutView* bit_view_ = nullptr;
  IcdPropertyPanel* property_panel_ = nullptr;
  IcdFramePreviewPanel* preview_panel_ = nullptr;

  // Toolbar widgets
  QLabel* frame_name_label_ = nullptr;
  QLabel* frame_id_label_ = nullptr;
  QLabel* frame_length_label_ = nullptr;
  QComboBox* frame_type_combo_ = nullptr;
  QToolButton* byte_order_btn_ = nullptr;

  // Toolbar actions
  QAction* new_frame_action_ = nullptr;
  QAction* add_node_action_ = nullptr;
  QAction* delete_selected_action_ = nullptr;
  QAction* delete_frame_action_ = nullptr;
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;
  QAction* node_tree_toggle_action_ = nullptr;
  QAction* property_toggle_action_ = nullptr;
  QAction* preview_toggle_action_ = nullptr;

  icd::Repository repo_;
  icd::Frame* current_frame_ = nullptr;
  const icd::Node* current_selected_node_ = nullptr;

  int load_generation_ = 0;

  // ConfigDriven: 首次加载后导航到指定帧（-1 表示不导航）
  int initial_frame_id_ = -1;

  QString current_file_;

  // Format routing
  ProtocolFormat format_ = ProtocolFormat::Json;
  icd::Format config_format_ = icd::Format::xml;
  std::filesystem::path config_path_;
  std::vector<icd::FrameFileInfo> file_entries_;
  QHash<int, QString> frame_file_map_;  // frame_id → relative file path

  bool modified_ = false;
  QVector<QByteArray> snapshots_;
  QVector<int> snapshot_frame_ids_;
  int snapshot_index_ = -1;
};

}  // namespace etest::protocol
