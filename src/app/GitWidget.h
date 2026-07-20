#ifndef ETEST_APP_GIT_WIDGET_H_
#define ETEST_APP_GIT_WIDGET_H_

#include <QLabel>
#include <QToolButton>
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
  /// 在指定目录执行 git init，root 为空时使用 project_root_
  /// 成功返回 true。不依赖 project_root_ 的赋值时机，避免信号链
  /// 顺序导致 onProjectOpened 调用时 project_root_ 尚未设置。
  bool initRepository(const QString& root = QString());

 signals:
  void fileOpenRequested(const QString& filePath);
  /// 请求在项目根目录初始化 Git 仓库（由上层确认后执行）
  void initRepoRequested();

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
  QToolButton* commit_button_ = nullptr;
  QTreeWidget* changes_tree_ = nullptr;
  QToolButton* refresh_button_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  QWidget* content_widget_ = nullptr;
  QWidget* empty_widget_ = nullptr;
  QToolButton* init_repo_button_ = nullptr;  ///< 空状态页的初始化按钮

  QString project_root_;
  bool is_git_repo_ = false;

  QTreeWidgetItem* staged_group_ = nullptr;
  QTreeWidgetItem* unstaged_group_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_GIT_WIDGET_H_
