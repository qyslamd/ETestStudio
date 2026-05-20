#include "GitWidget.h"

#include <QDir>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMenu>
#include <QProcess>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "logger/Logger.h"

using namespace etest::core::logger;

namespace etest::app {

GitWidget::GitWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
  showEmpty();
}

void GitWidget::setProjectRoot(const QString& path) {
  project_root_ = path;
  is_git_repo_ = false;
  if (path.isEmpty()) {
    showEmpty();
    return;
  }
  refresh();
}

QString GitWidget::projectRoot() const {
  return project_root_;
}

void GitWidget::refresh() {
  if (project_root_.isEmpty()) {
    showEmpty();
    return;
  }

  is_git_repo_ = isGitRepo();
  if (!is_git_repo_) {
    showEmpty();
    return;
  }

  showContent();

  QString branch = currentBranch();
  branch_label_->setText(
      branch.isEmpty() ? QStringLiteral("(HEAD detached)") : branch);

  runGitStatus();
}

bool GitWidget::isGitRepo() const {
  QProcess git;
  git.setWorkingDirectory(project_root_);
  git.start("git", {"rev-parse", "--is-inside-work-tree"});
  if (!git.waitForFinished(3000)) return false;
  return git.readAllStandardOutput().trimmed() == "true";
}

QString GitWidget::currentBranch() const {
  QProcess git;
  git.setWorkingDirectory(project_root_);
  git.start("git", {"rev-parse", "--abbrev-ref", "HEAD"});
  if (!git.waitForFinished(3000)) return {};
  return QString::fromUtf8(git.readAllStandardOutput().trimmed());
}

void GitWidget::runGitStatus() {
  QProcess git;
  git.setWorkingDirectory(project_root_);
  git.start("git", {"status", "--porcelain=v1"});
  if (!git.waitForFinished(5000)) {
    LOG_WARN("GIT", "git status timed out");
    return;
  }

  staged_group_->takeChildren();
  unstaged_group_->takeChildren();

  int stagedCount = 0;
  int unstagedCount = 0;

  while (!git.atEnd()) {
    QString line = QString::fromUtf8(git.readLine());
    if (line.length() < 4) continue;

    QString x = line.mid(0, 1);
    QString y = line.mid(1, 1);
    QString file = line.mid(3).trimmed();

    if (file.contains(" -> ")) {
      file = file.section(" -> ", 1, 1);
    }

    bool isStaged = (x != " " && x != "?" && x != "!");
    bool isUnstaged = (y != " " && y != "!");

    QChar statusChar;
    if (x == "?" || y == "?")
      statusChar = 'U';
    else if (x == "A" || y == "A")
      statusChar = 'A';
    else if (x == "D" || y == "D")
      statusChar = 'D';
    else
      statusChar = 'M';

    if (isStaged) {
      auto* item = new QTreeWidgetItem(staged_group_);
      item->setText(0, file);
      item->setData(0, Qt::UserRole, file);
      item->setData(0, Qt::UserRole + 1, QStringLiteral("unstage"));
      ++stagedCount;
    }

    if (isUnstaged) {
      auto* item = new QTreeWidgetItem(unstaged_group_);
      item->setText(0, file);
      item->setData(0, Qt::UserRole, file);
      item->setData(0, Qt::UserRole + 1, QStringLiteral("stage"));
      ++unstagedCount;
    }
  }

  staged_group_->setText(
      0, QStringLiteral("暂存的更改 (%1)").arg(stagedCount));
  unstaged_group_->setText(
      0, QStringLiteral("更改 (%1)").arg(unstagedCount));
  staged_group_->setExpanded(stagedCount > 0);
  unstaged_group_->setExpanded(unstagedCount > 0);
}

bool GitWidget::stageFile(const QString& filePath) {
  QProcess git;
  git.setWorkingDirectory(project_root_);
  git.start("git", {"add", "--", filePath});
  if (!git.waitForFinished(5000)) return false;
  bool ok = (git.exitCode() == 0);
  if (ok) {
    LOG_INFO("GIT", "staged: {}", filePath.toStdString());
    refresh();
  }
  return ok;
}

bool GitWidget::unstageFile(const QString& filePath) {
  QProcess git;
  git.setWorkingDirectory(project_root_);
  git.start("git", {"reset", "HEAD", "--", filePath});
  if (!git.waitForFinished(5000)) return false;
  bool ok = (git.exitCode() == 0);
  if (ok) {
    LOG_INFO("GIT", "unstaged: {}", filePath.toStdString());
    refresh();
  }
  return ok;
}

bool GitWidget::doCommit(const QString& message) {
  if (message.trimmed().isEmpty()) return false;

  QProcess git;
  git.setWorkingDirectory(project_root_);
  git.start("git", {"commit", "-m", message});
  if (!git.waitForFinished(10000)) return false;
  bool ok = (git.exitCode() == 0);
  if (ok) {
    LOG_INFO("GIT", "committed: {}", message.left(40).toStdString());
    commit_input_->clear();
    refresh();
  } else {
    LOG_WARN("GIT", "commit failed: {}",
             QString::fromUtf8(git.readAllStandardError()).toStdString());
  }
  return ok;
}

void GitWidget::showContent() {
  stack_->setCurrentWidget(content_widget_);
}

void GitWidget::showEmpty() {
  stack_->setCurrentWidget(empty_widget_);

  auto* label = empty_widget_->findChild<QLabel*>();
  if (label) {
    if (project_root_.isEmpty()) {
      label->setText(QStringLiteral("未打开项目"));
    } else {
      label->setText(QStringLiteral("不是 Git 仓库"));
    }
  }
}

void GitWidget::initUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 标题行
  auto* headerRow = new QHBoxLayout();
  headerRow->setContentsMargins(8, 6, 8, 4);
  headerRow->setSpacing(4);

  auto* titleLabel = new QLabel(QStringLiteral("源代码管理"), this);
  titleLabel->setObjectName(QStringLiteral("gitTitleLabel"));

  refresh_button_ = new QPushButton(this);
  refresh_button_->setObjectName("refresh_button_");
  refresh_button_->setToolTip(QStringLiteral("刷新"));
  refresh_button_->setFixedSize(26, 26);
  QIcon refreshIcon;
  refreshIcon.addFile(":/resources/icons/svg/refresh_dark.svg", QSize(),
                      QIcon::Normal, QIcon::Off);
  refreshIcon.addFile(":/resources/icons/svg/refresh_light.svg", QSize(),
                      QIcon::Disabled, QIcon::Off);
  refresh_button_->setIcon(refreshIcon);
  refresh_button_->setIconSize(QSize(16, 16));

  headerRow->addWidget(titleLabel);
  headerRow->addStretch();
  headerRow->addWidget(refresh_button_);

  mainLayout->addLayout(headerRow);

  // QStackedWidget 切换内容/空状态
  stack_ = new QStackedWidget(this);

  // 内容页
  content_widget_ = new QWidget(this);
  auto* contentLayout = new QVBoxLayout(content_widget_);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  // 分支标签
  branch_label_ = new QLabel(this);
  branch_label_->setContentsMargins(8, 2, 8, 4);
  contentLayout->addWidget(branch_label_);

  // 提交区
  auto* commitRow = new QHBoxLayout();
  commitRow->setContentsMargins(8, 4, 8, 4);
  commitRow->setSpacing(4);

  commit_input_ = new QTextEdit(this);
  commit_input_->setPlaceholderText(QStringLiteral("提交消息..."));
  commit_input_->setFixedHeight(60);
  commit_input_->setAcceptRichText(false);

  commit_button_ = new QPushButton(QStringLiteral("提交"), this);
  commit_button_->setFixedSize(60, 28);
  commit_button_->setToolTip(QStringLiteral("提交 (Ctrl+Enter)"));

  commitRow->addWidget(commit_input_);
  commitRow->addWidget(commit_button_);

  contentLayout->addLayout(commitRow);

  // 变更文件树
  changes_tree_ = new QTreeWidget(this);
  changes_tree_->setHeaderHidden(true);
  changes_tree_->setIndentation(12);
  changes_tree_->setRootIsDecorated(true);
  changes_tree_->setUniformRowHeights(true);
  changes_tree_->setSelectionMode(QAbstractItemView::SingleSelection);
  changes_tree_->setContextMenuPolicy(Qt::CustomContextMenu);

  staged_group_ = new QTreeWidgetItem(changes_tree_);
  staged_group_->setText(0, QStringLiteral("暂存的更改 (0)"));

  unstaged_group_ = new QTreeWidgetItem(changes_tree_);
  unstaged_group_->setText(0, QStringLiteral("更改 (0)"));

  contentLayout->addWidget(changes_tree_);

  stack_->addWidget(content_widget_);

  // 空状态页
  empty_widget_ = new QWidget(this);
  auto* emptyLayout = new QVBoxLayout(empty_widget_);
  emptyLayout->setAlignment(Qt::AlignCenter);
  auto* emptyLabel = new QLabel(QStringLiteral("未打开项目"), this);
  emptyLabel->setAlignment(Qt::AlignCenter);
  emptyLabel->setObjectName("gitEmptyLabel");
  emptyLayout->addWidget(emptyLabel);

  stack_->addWidget(empty_widget_);

  mainLayout->addWidget(stack_);
}

void GitWidget::initSignals() {
  connect(refresh_button_, &QPushButton::clicked, this, &GitWidget::refresh);

  connect(commit_button_, &QPushButton::clicked, this, [this]() {
    doCommit(commit_input_->toPlainText());
  });

  commit_input_->installEventFilter(this);

  connect(changes_tree_, &QTreeWidget::itemClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);

            if (item == staged_group_ || item == unstaged_group_) return;
            if (!item->parent()) return;

            QString filePath = item->data(0, Qt::UserRole).toString();
            if (filePath.isEmpty()) return;

            QString action = item->data(0, Qt::UserRole + 1).toString();

            if (action == "stage") {
              stageFile(filePath);
            } else if (action == "unstage") {
              unstageFile(filePath);
            }

            QString absPath = QDir(project_root_).filePath(filePath);
            emit fileOpenRequested(absPath);
          });

  connect(changes_tree_, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            QTreeWidgetItem* item = changes_tree_->itemAt(pos);
            if (!item || !item->parent()) return;

            QString filePath = item->data(0, Qt::UserRole).toString();
            if (filePath.isEmpty()) return;

            QString action = item->data(0, Qt::UserRole + 1).toString();
            QString absPath = QDir(project_root_).filePath(filePath);

            QMenu menu(this);

            if (action == "stage") {
              menu.addAction(QStringLiteral("暂存更改"), this,
                             [this, filePath]() { stageFile(filePath); });
            } else if (action == "unstage") {
              menu.addAction(QStringLiteral("取消暂存"), this,
                             [this, filePath]() { unstageFile(filePath); });
            }

            menu.addSeparator();
            menu.addAction(
                QStringLiteral("打开文件"), this,
                [this, absPath]() { emit fileOpenRequested(absPath); });

            menu.exec(changes_tree_->viewport()->mapToGlobal(pos));
          });
}

bool GitWidget::eventFilter(QObject* obj, QEvent* event) {
  if (obj == commit_input_ && event->type() == QEvent::KeyPress) {
    auto* keyEvent = static_cast<QKeyEvent*>(event);
    if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
        && (keyEvent->modifiers() & Qt::ControlModifier)) {
      doCommit(commit_input_->toPlainText());
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

}  // namespace etest::app
