#ifndef ETEST_APP_SIDEBAR_WIDGET_H_
#define ETEST_APP_SIDEBAR_WIDGET_H_

#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

class FileExplorerWidget;

class SidebarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SidebarWidget(QWidget* parent = nullptr);

  void switchPage(int index);

  FileExplorerWidget* fileExplorer() const;

 private:
  void setupUi();

  QStackedWidget* stack_;
  FileExplorerWidget* file_explorer_ = nullptr;
};

#endif  // ETEST_APP_SIDEBAR_WIDGET_H_
