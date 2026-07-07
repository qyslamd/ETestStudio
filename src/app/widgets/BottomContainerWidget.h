#ifndef ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_
#define ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_

#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

class QToolButton;

namespace etest::app {

class BottomContainerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit BottomContainerWidget(QWidget* parent = nullptr);

  void addPanel(const QString& title,
                QWidget* panel,
                const QString& iconName = {});
  void setCurrentPanel(int index);
  int currentPanelIndex() const;

 signals:
  void panelClosed();

 private:
  void initUi();

  QTabWidget* tab_widget_;
  QToolButton* close_button_;
  QStringList tab_icon_names_;
};

}  // namespace etest::app

#endif  // ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_
