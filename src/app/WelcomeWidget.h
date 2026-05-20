#ifndef ETEST_APP_WELCOME_WIDGET_H_
#define ETEST_APP_WELCOME_WIDGET_H_

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

namespace etest::app {

class WelcomeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit WelcomeWidget(QWidget* parent = nullptr);

  void refreshRecentProjects();

 signals:
  void newProjectRequested();
  void openProjectRequested();
  void projectOpenRequested(const QString& projectPath);

 private:
  void initUi();
  void initSignals();

  QPushButton* btn_new_project_ = nullptr;
  QPushButton* btn_open_project_ = nullptr;
  QListWidget* recent_list_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_WELCOME_WIDGET_H_
