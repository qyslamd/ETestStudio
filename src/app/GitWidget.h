#ifndef ETEST_APP_GIT_WIDGET_H_
#define ETEST_APP_GIT_WIDGET_H_

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>

namespace etest::app {

class GitWidget : public QWidget {
  Q_OBJECT

 public:
  explicit GitWidget(QWidget* parent = nullptr);

  void setProjectRoot(const QString& path);
  QString projectRoot() const;
  void refresh();

 signals:
  void fileOpenRequested(const QString& filePath);

 private:
  void initUi();
  void initSignals();

  bool isGitRepo() const;
  QString currentBranch() const;
  void runGitStatus();
  bool stageFile(const QString& filePath);
  bool unstageFile(const QString& filePath);
  bool doCommit(const QString& message);
  void showContent();
  void showEmpty();

  bool eventFilter(QObject* obj, QEvent* event) override;

  QLabel* branch_label_ = nullptr;
  QTextEdit* commit_input_ = nullptr;
  QPushButton* commit_button_ = nullptr;
  QTreeWidget* changes_tree_ = nullptr;
  QPushButton* refresh_button_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  QWidget* content_widget_ = nullptr;
  QWidget* empty_widget_ = nullptr;

  QString project_root_;
  bool is_git_repo_ = false;

  QTreeWidgetItem* staged_group_ = nullptr;
  QTreeWidgetItem* unstaged_group_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_GIT_WIDGET_H_
