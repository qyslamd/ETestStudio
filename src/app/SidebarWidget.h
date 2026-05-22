#ifndef ETEST_APP_SIDEBAR_WIDGET_H_
#define ETEST_APP_SIDEBAR_WIDGET_H_

#include <QLabel>
#include <QPushButton>
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

class SidebarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SidebarWidget(QWidget* parent = nullptr);

  void switchPage(int index);
  int pageCount() const;

  // Activity bar
  void setActiveIndex(int index);
  int activeIndex() const;
  void toggleContentPanel();

  FileExplorerWidget* fileExplorer() const;
  HardwareTreeWidget* hardwareTree() const;
  ProtocolManagerWidget* protocolManager() const;
  SearchWidget* searchWidget() const;
  GitWidget* gitWidget() const;

 signals:
  void settingsTriggered();
  void contentPanelToggled(bool visible);

 private:
  void setupUi();
  QPushButton* createButton(const QString& tooltip,
                            const QString& darkIconPath,
                            const QString& lightIconPath);

  QStackedWidget* stack_;
  QLabel* title_label_;
  QWidget* content_panel_ = nullptr;
  FileExplorerWidget* file_explorer_ = nullptr;
  HardwareTreeWidget* hardware_tree_ = nullptr;
  ProtocolManagerWidget* protocol_manager_ = nullptr;
  SearchWidget* search_widget_ = nullptr;
  GitWidget* git_widget_ = nullptr;

  QStringList view_titles_;
  QVector<QPushButton*> buttons_;
  int active_index_ = 0;
};

}  // namespace etest::app

#endif  // ETEST_APP_SIDEBAR_WIDGET_H_
