#pragma once

#include <QByteArray>
#include <QFutureWatcher>
#include <QTimer>
#include <QVector>
#include <QMainWindow>

#include <memory>

#include "api/IEditor.h"

#include <icd/frame.hpp>
#include <icd/repository.hpp>

class QComboBox;
class QLabel;
class QDockWidget;
class QAction;
class QToolBar;
class QResizeEvent;

namespace etest::protocal {

class IcdNodeTreeWidget;
class IcdBitLayoutView;
class IcdPropertyPanel;

class ProtocalEditorWidget : public QMainWindow, public etest::app::IEditor {
  Q_OBJECT
 public:
  explicit ProtocalEditorWidget(QWidget* parent = nullptr);
  ~ProtocalEditorWidget() override;

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

  void setEditorId(const QString& id);

  // Embedded mode (hide menuBar/toolbar when hosted in IDE)
  void setEmbeddedMode(bool embedded);

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
  QFutureWatcher<std::shared_ptr<icd::Repository>>* load_watcher_ = nullptr;
  QTimer* modified_debounce_ = nullptr;
  bool embedded_ = false;

  bool saveEproto(const QString& path);
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

  static constexpr int kMaxSnapshots = 32;

  // Dock widgets
  QDockWidget* node_tree_dock_ = nullptr;
  QDockWidget* property_dock_ = nullptr;

  // Panels (owned by docks)
  IcdNodeTreeWidget* node_tree_ = nullptr;
  IcdBitLayoutView* bit_view_ = nullptr;
  IcdPropertyPanel* property_panel_ = nullptr;

  // Toolbar widgets
  QLabel* frame_name_label_ = nullptr;
  QLabel* frame_id_label_ = nullptr;
  QLabel* frame_length_label_ = nullptr;
  QComboBox* frame_type_combo_ = nullptr;
  QComboBox* byte_order_combo_ = nullptr;

  // Toolbar actions
  QAction* new_frame_action_ = nullptr;
  QAction* delete_frame_action_ = nullptr;
  QAction* node_tree_toggle_action_ = nullptr;
  QAction* property_toggle_action_ = nullptr;

  icd::Repository repo_;
  icd::Frame* current_frame_ = nullptr;

  int load_generation_ = 0;

  QString current_file_;
  bool modified_ = false;
  QVector<QByteArray> snapshots_;
  QVector<int> snapshot_frame_ids_;
  int snapshot_index_ = -1;
};

}  // namespace etest::protocal
