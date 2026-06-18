#ifndef ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_
#define ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_

#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

class QToolButton;

namespace etest::app {

class BottomContainerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit BottomContainerWidget(QWidget* parent = nullptr);

  void addPanel(const QString& title, QWidget* panel);
  void setCurrentPanel(int index);
  int currentPanelIndex() const;

 signals:
  void panelClosed();

 private:
  void setupUi();

  QTabWidget* tab_widget_;
  QToolButton* close_button_;
};

}  // namespace etest::app

#endif  // ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_
