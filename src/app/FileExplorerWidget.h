#ifndef ETEST_APP_FILEEXPLORERWIDGET_H_
#define ETEST_APP_FILEEXPLORERWIDGET_H_

#include <QFileSystemModel>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QString>
#include <QTreeView>
#include <QWidget>

class QAction;
class QMenu;

namespace etest::app {
class FileTypeIconProvider;

class FileExplorerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit FileExplorerWidget(QWidget* parent = nullptr);

  void setRootPath(const QString& path);
  QString rootPath() const;

 signals:
  void fileOpenRequested(const QString& filePath);
  void fileOpenAsTextRequested(const QString& filePath);
  void fileDeleted(const QString& filePath);
  void fileRenamed(const QString& oldPath, const QString& newPath);

 private:
  void initUi();
  void initSignals();

  void onCustomContextMenu(const QPoint& pos);
  void onNewFile();
  void onNewFolder();
  void onRename();
  void onDelete();
  void onCopyPath();
  void onCopyRelativePath();
  void onOpenInFileSystem();

  QString sourceFilePath(const QModelIndex& proxyIndex) const;
  bool isDefaultProjectDir(const QString& dirName) const;

  QTreeView* tree_view_ = nullptr;
  QFileSystemModel* model_ = nullptr;
  QSortFilterProxyModel* proxy_model_ = nullptr;
  FileTypeIconProvider* icon_provider_ = nullptr;
  QString root_path_;

  QModelIndex context_index_;
  QMenu* context_menu_ = nullptr;

  // Action pointers for dynamic context menu state
  QAction* ctx_new_file_ = nullptr;
  QAction* ctx_new_folder_ = nullptr;
  QAction* ctx_rename_ = nullptr;
  QAction* ctx_delete_ = nullptr;
  QAction* ctx_copy_path_ = nullptr;
  QAction* ctx_copy_rel_path_ = nullptr;
  QAction* ctx_open_in_fs_ = nullptr;
  QAction* ctx_open_as_text_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_FILEEXPLORERWIDGET_H_
