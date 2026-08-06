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

  // 只读模式
  void setReadOnly(bool readOnly) override;

  // Topology specific
  TopologyDocument* document() const;
  void reloadScene();
  void openFile(const QString& filePath) override;

  // 嵌入模式（IDE 中隐藏 menuBar）
  void setEmbeddedMode(bool embedded);

  // M3+: 设置可用 ICD 帧名列表（由上层注入，在绑定对话框中显示）
  void setAvailableIcdFrames(const QStringList& frames);

 signals:
  void editorTitleChanged(const QString& title);
  void modificationChanged(bool modified);
  void editorIdChanged(const QString& oldId, const QString& newId);
  void undoStateChanged();

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
                    const QString& pluginId,
                    const QPointF& scenePos);
  void onOutlineNavigate(int itemType, int mainIndex, int subIndex);
  void onAddUutPort(int productIndex);
  void onCleanupInvalidConnections();

 private:
  void initUi();
  void initSignals();
  void buildDefaultDocument();
  void rebuildSceneAndRestoreSelection();

  // 状态消息
  void showStatusMessage(const QString& msg);

  // 异步加载
  void showLoadingOverlay();
  void hideLoadingOverlay();
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

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
  QAction* device_palette_toggle_action_ = nullptr;
  QAction* property_toggle_action_ = nullptr;
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;
  QAction* copy_action_ = nullptr;
  QAction* paste_action_ = nullptr;
  QAction* delete_action_ = nullptr;
  QAction* zoom_in_action_ = nullptr;
  QAction* zoom_out_action_ = nullptr;
  QAction* zoom_reset_action_ = nullptr;
  QAction* zoom_fit_action_ = nullptr;
  QAction* export_image_action_ = nullptr;
  QAction* cleanup_action_ = nullptr;
  // mount_action_ removed — badge click now drives the property panel

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
  QAction* align_action_ = nullptr;
  QAction* distribute_action_ = nullptr;
  QAction* distribute_horizontal_action_ = nullptr;
  QAction* distribute_vertical_action_ = nullptr;

  QString current_file_;
};

}  // namespace etest::topology
