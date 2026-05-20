#pragma once

#include <QWidget>

#include "api/IEditor.h"

class QAction;
class QSplitter;
class QGraphicsItem;
class QLabel;

namespace etest::topology {

class TopologyDocument;
class TopologyScene;
class TopologyView;
class PropertyPanelWidget;

class TopologyEditorWidget : public QWidget, public etest::app::IEditor {
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

 private:
  void initUi();
  void initSignals();
  void buildDefaultDocument();
  void rebuildSceneAndRestoreSelection();

  TopologyDocument* doc_;
  TopologyScene* scene_;
  TopologyView* view_;
  PropertyPanelWidget* property_panel_;
  QSplitter* splitter_;
  QLabel* status_label_ = nullptr;

  QAction* add_uut_action_ = nullptr;
  QAction* add_device_action_ = nullptr;
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;
  QAction* zoom_in_action_ = nullptr;
  QAction* zoom_out_action_ = nullptr;
  QAction* zoom_reset_action_ = nullptr;
  QAction* export_image_action_ = nullptr;

  // Align / Distribute
  void doAlign(int alignType);
  void doDistribute(int distType);
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
