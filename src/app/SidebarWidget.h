#ifndef ETEST_APP_SIDEBAR_WIDGET_H_
#define ETEST_APP_SIDEBAR_WIDGET_H_

#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

class QToolButton;

namespace etest::app {
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
  QLabel* title_label_;
  FileExplorerWidget* file_explorer_ = nullptr;

  // 视图名称列表
  QStringList view_titles_;
};

}  // namespace etest::app
#endif  // ETEST_APP_SIDEBAR_WIDGET_H_
