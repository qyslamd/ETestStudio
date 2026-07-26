#ifndef ETEST_APP_VISUALIZATION_AREA_H_
#define ETEST_APP_VISUALIZATION_AREA_H_

#include <QGraphicsView>
#include <QHash>

class QGraphicsScene;
class QGraphicsProxyWidget;

namespace etest::app {

class SignalVisualizer;

class VisualizationArea : public QGraphicsView {
  Q_OBJECT

 public:
  explicit VisualizationArea(QWidget* parent = nullptr);
  ~VisualizationArea() override;

  void addVisualizer(int monitorIndex,
                     SignalVisualizer* visualizer);
  void removeVisualizer(int monitorIndex);

  SignalVisualizer* visualizer(int monitorIndex) const;

  void clearAll();
  int visualizerCount() const { return items_.size(); }

  QList<int> activeChannels() const;

 signals:
  void visualizerClosed(int monitorIndex);

 protected:
  void resizeEvent(QResizeEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  void relayout();

  struct Item {
    QGraphicsProxyWidget* proxy = nullptr;
    SignalVisualizer* widget = nullptr;
  };

  QGraphicsScene* scene_ = nullptr;
  QHash<int, Item> items_;
};

}  // namespace etest::app

#endif  // ETEST_APP_VISUALIZATION_AREA_H_
