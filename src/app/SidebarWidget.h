#ifndef ETEST_APP_SIDEBAR_WIDGET_H_
#define ETEST_APP_SIDEBAR_WIDGET_H_

#include <QLabel>
#include <QStackedWidget>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

class QToolButton;

namespace etest::app {
class FileExplorerWidget;
class HardwareTreeWidget;
class ProtocolManagerWidget;
class SearchWidget;
class GitWidget;
class TestProgramManagerWidget;

class SidebarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SidebarWidget(QWidget* parent = nullptr);

  void switchPage(int index);
  int pageCount() const;

  void showContent();
  void hideContent();
  bool isContentVisible() const;

  FileExplorerWidget* fileExplorer() const;
  HardwareTreeWidget* hardwareTree() const;
  ProtocolManagerWidget* protocolManager() const;
  SearchWidget* searchWidget() const;
  GitWidget* gitWidget() const;
  TestProgramManagerWidget* testProgramManager() const;

 private:
  void setupUi();

  QStackedWidget* stack_;
  QLabel* title_label_;
  QWidget* content_panel_ = nullptr;
  FileExplorerWidget* file_explorer_ = nullptr;
  HardwareTreeWidget* hardware_tree_ = nullptr;
  ProtocolManagerWidget* protocol_manager_ = nullptr;
  SearchWidget* search_widget_ = nullptr;
  GitWidget* git_widget_ = nullptr;
  TestProgramManagerWidget* test_program_manager_ = nullptr;

  QStringList view_titles_;
};

}  // namespace etest::app

#endif  // ETEST_APP_SIDEBAR_WIDGET_H_
