#ifndef ETEST_APP_SIDEBAR_WIDGET_H_
#define ETEST_APP_SIDEBAR_WIDGET_H_

#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

class SidebarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SidebarWidget(QWidget* parent = nullptr);

  void switchPage(int index);

 private:
  void setupUi();

  QStackedWidget* stack_;
};

#endif  // ETEST_APP_SIDEBAR_WIDGET_H_
