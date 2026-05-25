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
#include <QStyledItemDelegate>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include "editor/EditorFactory.h"
#include "logger/Logger.h"
#include "utils/FileUtil.h"
#include "FileTypeIconProvider.h"
#include "ThemeManager.h"

using namespace etest::core::utils;
using namespace etest::core::logger;

namespace {

// 自定义委托：编辑时扩展编辑器宽度，避免文字被截断
class FileItemDelegate : public QStyledItemDelegate {
 public:
  explicit FileItemDelegate(QObject* parent = nullptr)
      : QStyledItemDelegate(parent) {}

  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override {
    QStyledItemDelegate::updateEditorGeometry(editor, option, index);
    // 扩展编辑器宽度：在原始宽度基础上加一段余量，避免文字截断
    int minWidth = option.fontMetrics.horizontalAdvance(
                       index.data(Qt::EditRole).toString()) +
                   20;
    if (editor->width() < minWidth) {
      auto* tree = qobject_cast<QTreeView*>(parent());
      int maxW = tree ? tree->viewport()->width() - option.rect.left() : minWidth;
      editor->resize(qMin(minWidth, maxW), editor->height());
    }
  }
};

// 项目默认目录映射：目录名 -> (中文显示名, 主题色)
static const QMap<QString, QPair<QString, QColor>> kDefaultDirs = {
    {QStringLiteral("backup"),   {QStringLiteral("备份"),     QColor("#E67E22")}},
    {QStringLiteral("cases"),    {QStringLiteral("测试用例"), QColor("#2ECC71")}},
    {QStringLiteral("config"),   {QStringLiteral("配置"),     QColor("#3498DB")}},
    {QStringLiteral("scripts"),  {QStringLiteral("脚本"),     QColor("#9B59B6")}},
    {QStringLiteral("protocol"), {QStringLiteral("协议"),     QColor("#1ABC9C")}},
    {QStringLiteral("topology"), {QStringLiteral("拓扑"),     QColor("#E74C3C")}},
    {QStringLiteral("reports"),  {QStringLiteral("报告"),     QColor("#F39C12")}},
};

// 代理模型：过滤 .etproj 文件，默认目录显示中文名
class ProjectFileProxyModel : public QSortFilterProxyModel {
 public:
  explicit ProjectFileProxyModel(QObject* parent = nullptr)
      : QSortFilterProxyModel(parent) {}

 protected:
  bool filterAcceptsRow(int sourceRow,
                        const QModelIndex& sourceParent) const override {
    auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
    if (!fsModel)
      return true;

    QModelIndex idx = fsModel->index(sourceRow, 0, sourceParent);
    QFileInfo fi = fsModel->fileInfo(idx);

    // 隐藏 .etproj 项目文件
    if (fi.isFile() &&
        fi.suffix().toLower() == QStringLiteral("etproj"))
      return false;

    return true;
  }

  QVariant data(const QModelIndex& index, int role) const override {
    if (role == Qt::DisplayRole && index.column() == 0) {
      auto* fsModel = qobject_cast<QFileSystemModel*>(sourceModel());
      if (fsModel) {
        QFileInfo fi = fsModel->fileInfo(mapToSource(index));
        if (fi.isDir()) {
          auto it = kDefaultDirs.constFind(fi.fileName());
          if (it != kDefaultDirs.constEnd()) {
            return it.value().first;  // 返回中文显示名
          }
        }
      }
    }
    // EditRole 等其它角色返回实际文件名
    return QSortFilterProxyModel::data(index, role);
  }
};

}  // namespace

namespace etest::app {

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
    if (!icon_provider_) {
      icon_provider_ = new FileTypeIconProvider();
    }
    model_->setIconProvider(icon_provider_);

    // 代理模型：过滤 .etproj 文件并提供中文显示名
    proxy_model_ = new ProjectFileProxyModel(this);
    proxy_model_->setSourceModel(model_);
    tree_view_->setModel(proxy_model_);

    for (int i = 1; i < model_->columnCount(); ++i) {
      tree_view_->hideColumn(i);
    }

    // 监听文件重命名信号（从 source model 接收）
    connect(model_, &QFileSystemModel::fileRenamed, this,
            [this](const QString& path, const QString& oldName, const QString& newName) {
              QFileInfo oldFi(QDir(path), oldName);
              QFileInfo newFi(QDir(path), newName);
              emit fileRenamed(oldFi.absoluteFilePath(), newFi.absoluteFilePath());
            });
  } else {
    // 确保 tree_view 使用的是代理模型（关闭项目时会被设为 nullptr）
    if (tree_view_->model() != proxy_model_) {
      proxy_model_->setSourceModel(model_);
      tree_view_->setModel(proxy_model_);
      for (int i = 1; i < model_->columnCount(); ++i) {
        tree_view_->hideColumn(i);
      }
    }
  }

  model_->setRootPath(path);
  tree_view_->setRootIndex(proxy_model_->mapFromSource(model_->index(path)));
  tree_view_->expandToDepth(0);
}

QString FileExplorerWidget::rootPath() const {
  return root_path_;
}

void FileExplorerWidget::initUi() {
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 文件树
  tree_view_ = new QTreeView(this);
  tree_view_->setHeaderHidden(true);
  tree_view_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_view_->setEditTriggers(QAbstractItemView::EditKeyPressed);
  tree_view_->setDragEnabled(true);
  tree_view_->setAcceptDrops(false);
  tree_view_->setIndentation(16);
  tree_view_->setMinimumWidth(150);
  tree_view_->setItemDelegate(new FileItemDelegate(tree_view_));
  mainLayout->addWidget(tree_view_);
}

void FileExplorerWidget::initSignals() {
  // 主题切换时刷新文件图标
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this](bool) {
            if (icon_provider_) {
              icon_provider_->reload();
            }
          });

  connect(tree_view_, &QTreeView::doubleClicked, this,
          [this](const QModelIndex& index) {
            if (!model_)
              return;
            QString path = sourceFilePath(index);
            QFileInfo fi(path);
            if (fi.isFile()) {
              LOG_INFO("EXPLORER", "双击打开文件：{}", path.toStdString());
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
    ctx_new_file_ = context_menu_->addAction(QStringLiteral("新建文件"), this,
                                             &FileExplorerWidget::onNewFile);
    ctx_new_folder_ = context_menu_->addAction(QStringLiteral("新建文件夹"), this,
                                               &FileExplorerWidget::onNewFolder);
    context_menu_->addSeparator();
    ctx_rename_ = context_menu_->addAction(QStringLiteral("重命名"), this,
                                           &FileExplorerWidget::onRename);
    ctx_delete_ = context_menu_->addAction(QStringLiteral("删除"), this,
                                           &FileExplorerWidget::onDelete);
    context_menu_->addSeparator();
    ctx_copy_path_ = context_menu_->addAction(QStringLiteral("复制路径"), this,
                                              &FileExplorerWidget::onCopyPath);
    ctx_copy_rel_path_ = context_menu_->addAction(QStringLiteral("复制相对路径"), this,
                                                  &FileExplorerWidget::onCopyRelativePath);
    context_menu_->addSeparator();
    ctx_open_in_fs_ = context_menu_->addAction(QStringLiteral("在文件系统中打开"), this,
                                               &FileExplorerWidget::onOpenInFileSystem);
    context_menu_->addSeparator();
    ctx_open_as_text_ = context_menu_->addAction(QStringLiteral("用文本编辑器打开"), this,
                                                 [this]() {
      if (!context_index_.isValid()) return;
      emit fileOpenAsTextRequested(sourceFilePath(context_index_));
    });
  }

  // 根据上下文状态启用/禁用菜单项
  bool hasSelection = context_index_.isValid();
  bool hasRoot = !root_path_.isEmpty();

  ctx_new_file_->setEnabled(hasRoot);
  ctx_new_folder_->setEnabled(hasRoot);

  // 检查选中项是否为项目默认目录（不可重命名/删除）
  bool isProtected = false;
  if (hasSelection && model_) {
    QString path = sourceFilePath(context_index_);
    QFileInfo fi(path);
    isProtected = fi.isDir() && isDefaultProjectDir(fi.fileName());
  }

  ctx_rename_->setEnabled(hasSelection && !isProtected);
  ctx_delete_->setEnabled(hasSelection && !isProtected);
  ctx_copy_path_->setEnabled(hasSelection);
  ctx_copy_rel_path_->setEnabled(hasSelection);
  ctx_open_in_fs_->setEnabled(hasRoot);

  // "用文本编辑器打开"：仅对非 text、非 image 的已知文件类型启用
  bool canOpenAsText = false;
  if (hasSelection && model_) {
    QString path = sourceFilePath(context_index_);
    QFileInfo fi(path);
    if (fi.isFile()) {
      QString suffix = fi.suffix().toLower();
      QString editorType = etest::app::EditorFactoryRegistry::typeForExtension(suffix);
      canOpenAsText = !editorType.isEmpty()
                      && editorType != QStringLiteral("text")
                      && editorType != QStringLiteral("image");
    }
  }
  ctx_open_as_text_->setVisible(canOpenAsText);

  context_menu_->popup(tree_view_->viewport()->mapToGlobal(pos));
}

void FileExplorerWidget::onNewFile() {
  if (root_path_.isEmpty())
    return;

  QString parentDir = root_path_;
  if (context_index_.isValid() && model_) {
    QString path = sourceFilePath(context_index_);
    QFileInfo fi(path);
    parentDir = fi.isDir() ? path : fi.absolutePath();
  }

  QString fileName = QStringLiteral("新建文件");
  QString filePath = QDir(parentDir).filePath(fileName);

  int counter = 1;
  while (QFile::exists(filePath)) {
    filePath =
        QDir(parentDir).filePath(QStringLiteral("新建文件%1").arg(counter++));
  }

  etest::core::utils::FileUtil::writeTextFile(filePath, "");
  LOG_INFO("EXPLORER", "新建文件：{}", filePath.toStdString());

  // 展开父目录并进入重命名编辑状态
  if (model_) {
    QModelIndex parentIndex = model_->index(parentDir);
    tree_view_->expand(
        proxy_model_ ? proxy_model_->mapFromSource(parentIndex) : parentIndex);
    // QFileSystemModel 需要时间刷新，用延时等待新文件出现在模型中
    QTimer::singleShot(100, this, [this, filePath]() {
      QModelIndex idx = model_->index(filePath);
      if (idx.isValid()) {
        QModelIndex proxyIdx =
            proxy_model_ ? proxy_model_->mapFromSource(idx) : idx;
        tree_view_->scrollTo(proxyIdx);
        tree_view_->setCurrentIndex(proxyIdx);
        tree_view_->edit(proxyIdx);
      }
    });
  }
}

void FileExplorerWidget::onNewFolder() {
  if (root_path_.isEmpty())
    return;

  QString parentDir = root_path_;
  if (context_index_.isValid() && model_) {
    QString path = sourceFilePath(context_index_);
    QFileInfo fi(path);
    parentDir = fi.isDir() ? path : fi.absolutePath();
  }

  QString folderName = QStringLiteral("新建文件夹");
  QString folderPath = QDir(parentDir).filePath(folderName);

  int counter = 1;
  while (QDir(folderPath).exists()) {
    folderPath =
        QDir(parentDir).filePath(QStringLiteral("新建文件夹%1").arg(counter++));
  }

  etest::core::utils::FileUtil::createDirectory(folderPath);
  LOG_INFO("EXPLORER", "新建文件夹：{}", folderPath.toStdString());

  // 展开父目录并进入重命名编辑状态
  if (model_) {
    QModelIndex parentIndex = model_->index(parentDir);
    tree_view_->expand(
        proxy_model_ ? proxy_model_->mapFromSource(parentIndex) : parentIndex);
    QTimer::singleShot(100, this, [this, folderPath]() {
      QModelIndex idx = model_->index(folderPath);
      if (idx.isValid()) {
        QModelIndex proxyIdx =
            proxy_model_ ? proxy_model_->mapFromSource(idx) : idx;
        tree_view_->scrollTo(proxyIdx);
        tree_view_->setCurrentIndex(proxyIdx);
        tree_view_->edit(proxyIdx);
      }
    });
  }
}

void FileExplorerWidget::onRename() {
  if (!context_index_.isValid() || !model_)
    return;

  // 项目默认目录不可重命名
  QString path = sourceFilePath(context_index_);
  QFileInfo fi(path);
  if (fi.isDir() && isDefaultProjectDir(fi.fileName()))
    return;

  tree_view_->setCurrentIndex(context_index_);
  tree_view_->edit(context_index_);
}

void FileExplorerWidget::onDelete() {
  if (!context_index_.isValid() || !model_)
    return;

  QString path = sourceFilePath(context_index_);
  QFileInfo fi(path);

  // 项目默认目录不可删除
  if (fi.isDir() && isDefaultProjectDir(fi.fileName())) {
    auto it = kDefaultDirs.constFind(fi.fileName());
    QString displayName =
        it != kDefaultDirs.constEnd() ? it.value().first : fi.fileName();
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("\"%1\" 是项目默认目录，无法删除。")
                                 .arg(displayName));
    return;
  }

  int ret = QMessageBox::question(
      this, QStringLiteral("确认删除"),
      QStringLiteral("确定要删除 \"%1\" 吗？").arg(fi.fileName()),
      QMessageBox::Yes | QMessageBox::No);

  if (ret != QMessageBox::Yes)
    return;

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
    emit fileDeleted(path);
  }
}

void FileExplorerWidget::onCopyPath() {
  if (context_index_.isValid() && model_) {
    QApplication::clipboard()->setText(sourceFilePath(context_index_));
  } else if (!root_path_.isEmpty()) {
    QApplication::clipboard()->setText(root_path_);
  }
}

void FileExplorerWidget::onCopyRelativePath() {
  if (context_index_.isValid() && model_ && !root_path_.isEmpty()) {
    QString absPath = sourceFilePath(context_index_);
    QString relPath = QDir(root_path_).relativeFilePath(absPath);
    QApplication::clipboard()->setText(relPath);
  }
}

void FileExplorerWidget::onOpenInFileSystem() {
  QString path = root_path_;
  if (context_index_.isValid() && model_) {
    QString filePath = sourceFilePath(context_index_);
    QFileInfo fi(filePath);
    path = fi.isDir() ? filePath : fi.absolutePath();
  }

  if (!path.isEmpty()) {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  }
}

QString FileExplorerWidget::sourceFilePath(const QModelIndex& proxyIndex) const {
  if (!proxyIndex.isValid() || !model_)
    return {};
  QModelIndex sourceIndex =
      proxy_model_ ? proxy_model_->mapToSource(proxyIndex) : proxyIndex;
  return model_->filePath(sourceIndex);
}

bool FileExplorerWidget::isDefaultProjectDir(const QString& dirName) const {
  return kDefaultDirs.contains(dirName);
}

}  // namespace etest::app
