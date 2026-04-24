#ifndef ETEST_APP_FILEEXPLORERWIDGET_H_
#define ETEST_APP_FILEEXPLORERWIDGET_H_

#include <QFileSystemModel>
#include <QModelIndex>
#include <QString>
#include <QTreeView>
#include <QWidget>


class QMenu;

namespace etest::app {

class FileExplorerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit FileExplorerWidget(QWidget* parent = nullptr);

  void setRootPath(const QString& path);
  QString rootPath() const;

 Q_SIGNALS:
  void fileOpenRequested(const QString& filePath);

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

  QTreeView* tree_view_ = nullptr;
  QFileSystemModel* model_ = nullptr;
  QString root_path_;

  QModelIndex context_index_;
  QMenu* context_menu_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_FILEEXPLORERWIDGET_H_
