#ifndef ETEST_APP_PANEL_CONTAINER_WIDGET_H_
#define ETEST_APP_PANEL_CONTAINER_WIDGET_H_

#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

class QPushButton;
class QToolButton;

namespace etest {
namespace app {

class PanelContainerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit PanelContainerWidget(QWidget* parent = nullptr);

  void addPanel(const QString& title, QWidget* panel);
  void setCurrentPanel(int index);
  int currentPanelIndex() const;

  bool isMaximized() const;
  void setMaximized(bool maximized);

 signals:
  void panelMaximized();
  void panelRestored();
  void panelClosed();

 private:
  void setupUi();

  QTabWidget* tab_widget_;
  QToolButton* max_button_;
  QToolButton* close_button_;
  bool maximized_ = false;
};

}  // namespace app
}  // namespace etest

#endif  // ETEST_APP_PANEL_CONTAINER_WIDGET_H_
