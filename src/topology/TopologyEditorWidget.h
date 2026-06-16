#pragma once

#include <QFutureWatcher>
#include <QJsonDocument>
#include <QMainWindow>

#include <memory>

#include "api/IEditor.h"

class QAction;
class QDockWidget;
class QResizeEvent;
class QGraphicsItem;
class QLabel;

namespace etest::topology {

class TopologyDocument;
class TopologyScene;
class TopologyView;
class PropertyPanelWidget;
class TopologyOutlineWidget;
class DevicePaletteWidget;

class TopologyEditorWidget : public QMainWindow, public etest::app::IEditor {
  Q_OBJECT
 public:
  explicit TopologyEditorWidget(QWidget* parent = nullptr);
  ~TopologyEditorWidget() override;

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

  // Topology specific
  TopologyDocument* document() const;
  void reloadScene();
  void setEditorId(const QString& newId);

  // 嵌入模式（IDE 中隐藏 menuBar）
  void setEmbeddedMode(bool embedded);

 signals:
  void editorTitleChanged(const QString& title);
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);

 private slots:
  void onAddUut(const QPointF& scenePos = QPointF());
  void onAddDevice(const QPointF& scenePos = QPointF());
  void onDeleteItem(QGraphicsItem* item);
  void onSaveTemplate(QGraphicsItem* item);
  void onSelectionChanged(QGraphicsItem* item);
  void onDocumentChanged();
  void onUndo();
  void onRedo();
  void onCopy();
  void onPaste();
  void onExportImage();
  void onAddDeviceFromTemplate(const QPointF& scenePos);
  void onDropDevice(const QString& deviceType,
                    int channelCount,
                    int direction,
                    int functionType,
                    const QPointF& scenePos);
  void onDropMonitor(const QString& deviceType,
                     const QPointF& scenePos);
  void onOutlineNavigate(int itemType, int mainIndex, int subIndex);

 private:
  void initUi();
  void initSignals();
  void buildDefaultDocument();
  void rebuildSceneAndRestoreSelection();

  // 窗口布局持久化
  void saveWindowLayout();
  void restoreWindowLayout();

  // 状态消息
  void showStatusMessage(const QString& msg);

  // 异步加载
  void showLoadingOverlay();
  void hideLoadingOverlay();
  void resizeEvent(QResizeEvent* event) override;
  void hideEvent(QHideEvent* event) override;

  bool embedded_ = false;
  QWidget* loading_overlay_ = nullptr;
  QFutureWatcher<QJsonDocument>* load_watcher_ = nullptr;

  TopologyDocument* doc_;
  TopologyScene* scene_;
  TopologyView* view_;
  PropertyPanelWidget* property_panel_;
  DevicePaletteWidget* device_palette_ = nullptr;
  TopologyOutlineWidget* outline_widget_ = nullptr;

  // Dock widgets
  QDockWidget* device_palette_dock_ = nullptr;
  QDockWidget* outline_dock_ = nullptr;
  QDockWidget* property_dock_ = nullptr;

  // Toolbar
  QLabel* zoom_label_ = nullptr;

  QAction* outline_toggle_action_ = nullptr;
  QAction* add_uut_action_ = nullptr;
  QAction* add_device_action_ = nullptr;
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;
  QAction* copy_action_ = nullptr;
  QAction* paste_action_ = nullptr;
  QAction* delete_action_ = nullptr;
  QAction* zoom_in_action_ = nullptr;
  QAction* zoom_out_action_ = nullptr;
  QAction* zoom_reset_action_ = nullptr;
  QAction* export_image_action_ = nullptr;
  QAction* monitor_view_action_ = nullptr;
  QAction* mount_action_ = nullptr;

  void reloadToolbarIcons();

  enum class Align { Left, HCenter, Right, Top, VCenter, Bottom };
  enum class Distribute { Horizontal, Vertical };

  void doAlign(Align alignType);
  void doDistribute(Distribute distType);
  void updateAlignDistributeActions();

  QAction* align_left_action_ = nullptr;
  QAction* align_hcenter_action_ = nullptr;
  QAction* align_right_action_ = nullptr;
  QAction* align_top_action_ = nullptr;
  QAction* align_vcenter_action_ = nullptr;
  QAction* align_bottom_action_ = nullptr;
  QAction* distribute_horizontal_action_ = nullptr;
  QAction* distribute_vertical_action_ = nullptr;

  QString current_file_;
};

}  // namespace etest::topology
