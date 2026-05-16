#pragma once

#include <QWidget>

class QAction;
class QSplitter;
class QGraphicsItem;
class QLabel;

namespace etest::topology {

class TopologyDocument;
class TopologyScene;
class TopologyView;
class PropertyPanelWidget;

class TopologyEditorWidget : public QWidget {
  Q_OBJECT
 public:
  explicit TopologyEditorWidget(QWidget* parent = nullptr);
  ~TopologyEditorWidget() override;

 Q_SIGNALS:
  void editorTitleChanged(const QString& title);

 private slots:
  void onAddUut(const QPointF& scenePos = QPointF());
  void onAddDevice(const QPointF& scenePos = QPointF());
  void onDeleteItem(QGraphicsItem* item);
  void onSaveTemplate(QGraphicsItem* item);
  void onNewFile();
  void onOpenFile();
  void onSaveFile();
  void onSaveAsFile();
  void onSelectionChanged(QGraphicsItem* item);
  void onDocumentChanged();

 private:
  void initUi();
  void initSignals();
  void buildDefaultDocument();

  TopologyDocument* doc_;
  TopologyScene* scene_;
  TopologyView* view_;
  PropertyPanelWidget* property_panel_;
  QSplitter* splitter_;
  QLabel* status_label_ = nullptr;

  // Toolbar actions
  QAction* add_uut_action_ = nullptr;
  QAction* add_device_action_ = nullptr;
  QAction* zoom_in_action_ = nullptr;
  QAction* zoom_out_action_ = nullptr;
  QAction* zoom_reset_action_ = nullptr;

  QString current_file_;
};

}  // namespace etest::topology
