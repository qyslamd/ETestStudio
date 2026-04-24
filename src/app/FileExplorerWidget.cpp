#include "FileExplorerWidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QTimer>
#include <QVBoxLayout>

#include "logger/Logger.h"
#include "utils/FileUtil.h"

FileExplorerWidget::FileExplorerWidget(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();
}

void FileExplorerWidget::setRootPath(const QString& path) {
  root_path_ = path;
  if (path.isEmpty()) {
    tree_view_->setModel(nullptr);
    return;
  }

  if (!model_) {
    model_ = new QFileSystemModel(this);
    model_->setReadOnly(false);
    model_->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    model_->setNameFilterDisables(false);
    tree_view_->setModel(model_);
    for (int i = 1; i < model_->columnCount(); ++i) {
      tree_view_->hideColumn(i);
    }
  }

  model_->setRootPath(path);
  tree_view_->setRootIndex(model_->index(path));
  tree_view_->expandToDepth(0);
}

QString FileExplorerWidget::rootPath() const { return root_path_; }

void FileExplorerWidget::initUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 标题栏
  auto* titleBar = new QWidget(this);
  auto* titleLayout = new QHBoxLayout(titleBar);
  titleLayout->setContentsMargins(8, 4, 4, 4);
  auto* titleLabel = new QLabel(QStringLiteral("资源管理器"), this);
  titleLayout->addWidget(titleLabel);
  titleLayout->addStretch();
  titleBar->setFixedHeight(28);
  mainLayout->addWidget(titleBar);

  // 文件树
  tree_view_ = new QTreeView(this);
  tree_view_->setHeaderHidden(true);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->setEditTriggers(QAbstractItemView::EditKeyPressed);
  tree_view_->setDragEnabled(true);
  tree_view_->setAcceptDrops(false);
  tree_view_->setIndentation(16);
  tree_view_->setMinimumWidth(150);
  mainLayout->addWidget(tree_view_);
}

void FileExplorerWidget::initSignals() {
  connect(tree_view_, &QTreeView::doubleClicked, this,
          [this](const QModelIndex& index) {
            if (!model_) return;
            QString path = model_->filePath(index);
            QFileInfo fi(path);
            if (fi.isFile()) {
              LOG_INFO("EXPLORER", "双击打开文件：{}",
                       path.toStdString());
              emit fileOpenRequested(path);
            }
          });

  connect(tree_view_, &QTreeView::customContextMenuRequested, this,
          &FileExplorerWidget::onCustomContextMenu);
}

void FileExplorerWidget::onCustomContextMenu(const QPoint& pos) {
  context_index_ = tree_view_->indexAt(pos);

  if (!context_menu_) {
    context_menu_ = new QMenu(this);
    context_menu_->addAction(QStringLiteral("新建文件"), this,
                              &FileExplorerWidget::onNewFile);
    context_menu_->addAction(QStringLiteral("新建文件夹"), this,
                              &FileExplorerWidget::onNewFolder);
    context_menu_->addSeparator();
    context_menu_->addAction(QStringLiteral("重命名"), this,
                              &FileExplorerWidget::onRename);
    context_menu_->addAction(QStringLiteral("删除"), this,
                              &FileExplorerWidget::onDelete);
    context_menu_->addSeparator();
    context_menu_->addAction(QStringLiteral("复制路径"), this,
                              &FileExplorerWidget::onCopyPath);
    context_menu_->addAction(QStringLiteral("复制相对路径"), this,
                              &FileExplorerWidget::onCopyRelativePath);
    context_menu_->addSeparator();
    context_menu_->addAction(QStringLiteral("在文件系统中打开"), this,
                              &FileExplorerWidget::onOpenInFileSystem);
  }

  context_menu_->popup(tree_view_->viewport()->mapToGlobal(pos));
}

void FileExplorerWidget::onNewFile() {
  if (root_path_.isEmpty()) return;

  QString parentDir = root_path_;
  if (context_index_.isValid() && model_) {
    QString path = model_->filePath(context_index_);
    QFileInfo fi(path);
    parentDir = fi.isDir() ? path : fi.absolutePath();
  }

  QString fileName = QStringLiteral("新建文件");
  QString filePath = QDir(parentDir).filePath(fileName);

  int counter = 1;
  while (QFile::exists(filePath)) {
    filePath = QDir(parentDir).filePath(
        QStringLiteral("新建文件%1").arg(counter++));
  }

  etest::core::utils::FileUtil::writeTextFile(filePath, "");
  LOG_INFO("EXPLORER", "新建文件：{}", filePath.toStdString());
}

void FileExplorerWidget::onNewFolder() {
  if (root_path_.isEmpty()) return;

  QString parentDir = root_path_;
  if (context_index_.isValid() && model_) {
    QString path = model_->filePath(context_index_);
    QFileInfo fi(path);
    parentDir = fi.isDir() ? path : fi.absolutePath();
  }

  QString folderName = QStringLiteral("新建文件夹");
  QString folderPath = QDir(parentDir).filePath(folderName);

  int counter = 1;
  while (QDir(folderPath).exists()) {
    folderPath = QDir(parentDir).filePath(
        QStringLiteral("新建文件夹%1").arg(counter++));
  }

  etest::core::utils::FileUtil::createDirectory(folderPath);
  LOG_INFO("EXPLORER", "新建文件夹：{}", folderPath.toStdString());
}

void FileExplorerWidget::onRename() {
  if (context_index_.isValid()) {
    tree_view_->edit(context_index_);
  }
}

void FileExplorerWidget::onDelete() {
  if (!context_index_.isValid() || !model_) return;

  QString path = model_->filePath(context_index_);
  QFileInfo fi(path);

  int ret = QMessageBox::question(
      this, QStringLiteral("确认删除"),
      QStringLiteral("确定要删除 \"%1\" 吗？").arg(fi.fileName()),
      QMessageBox::Yes | QMessageBox::No);

  if (ret != QMessageBox::Yes) return;

  bool success = false;
  if (fi.isDir()) {
    success = QDir(path).removeRecursively();
  } else {
    success = QFile::remove(path);
  }

  if (!success) {
    QMessageBox::warning(this, QStringLiteral("删除失败"),
                         QStringLiteral("无法删除 \"%1\"").arg(fi.fileName()));
  } else {
    LOG_INFO("EXPLORER", "已删除：{}", path.toStdString());
  }
}

void FileExplorerWidget::onCopyPath() {
  if (context_index_.isValid() && model_) {
    QApplication::clipboard()->setText(model_->filePath(context_index_));
  } else if (!root_path_.isEmpty()) {
    QApplication::clipboard()->setText(root_path_);
  }
}

void FileExplorerWidget::onCopyRelativePath() {
  if (context_index_.isValid() && model_ && !root_path_.isEmpty()) {
    QString absPath = model_->filePath(context_index_);
    QString relPath = QDir(root_path_).relativeFilePath(absPath);
    QApplication::clipboard()->setText(relPath);
  }
}

void FileExplorerWidget::onOpenInFileSystem() {
  QString path = root_path_;
  if (context_index_.isValid() && model_) {
    QString filePath = model_->filePath(context_index_);
    QFileInfo fi(filePath);
    path = fi.isDir() ? filePath : fi.absolutePath();
  }

  if (!path.isEmpty()) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }
}
