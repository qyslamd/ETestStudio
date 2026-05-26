#ifndef ETEST_APP_PROJECT_STRUCTURE_WIDGET_H_
#define ETEST_APP_PROJECT_STRUCTURE_WIDGET_H_

#include <QMap>
#include <QStandardItemModel>
#include <QStackedWidget>
#include <QString>
#include <QWidget>

class QFileSystemWatcher;
class QMenu;
class QPoint;
class QTreeView;
class QTimer;

namespace etest::app {

// 自定义 role 常量（Qt::UserRole + 偏移）
enum ProjectNodeRole {
  NodeTypeRole = Qt::UserRole + 1,  // "root" | "category" | "file"
  RelativePathRole,                  // 相对于项目根目录的路径
  CategoryIdRole,                    // category 的标识（如 "protocol"）
};

struct CategoryInfo {
  QString id;          // 目录名，如 "protocol"
  QString displayName; // 显示名，如 "协议"
  QString dirPath;     // 相对于项目根的实际路径
  QString iconName;    // 图标名称
  QString newFileExt;  // 在此分类下新建文件的默认扩展名，空表示不支持新建
  QString newFileLabel; // "新建"菜单项的标签，如"新建协议文件"
};

class ProjectStructureWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ProjectStructureWidget(QWidget* parent = nullptr);

  void setProjectPath(const QString& path);
  void clearProjectPath();
  QString projectPath() const;

 signals:
  void fileOpenRequested(const QString& path);
  void fileOpenAsTextRequested(const QString& path);
  void fileCreated(const QString& path);
  void fileDeleted(const QString& path);
  void fileRenamed(const QString& oldPath, const QString& newPath);
  void missingDirectoryDetected(const QString& categoryId);

 private slots:
  void onCustomContextMenu(const QPoint& pos);
  void onItemDoubleClicked(const QModelIndex& index);
  void onItemChanged(QStandardItem* item);

 private:
  void setupUi();
  void buildTree();
  void refreshCategory(const QString& dirPath);
  void onDirectoryChanged(const QString& path);

  // 文件操作
  void createNewFile(const QString& categoryId, const QString& extension,
                     const QString& baseName);
  void deleteSelectedFile();
  void copyFilePath();
  void copyRelativePath();
  void openInFileManager();
  void openWithTextEditor();

  // 工具方法
  QList<CategoryInfo> defaultCategories() const;
  QStandardItem* createCategoryItem(const CategoryInfo& info, int fileCount);
  QStandardItem* createFileItem(const QString& fileName,
                                const QString& relativePath);
  QString absolutePath(const QString& relativePath) const;
  QString categoryDirPath(const QString& categoryId) const;

  QStackedWidget* stack_;
  QTreeView* tree_view_;
  QStandardItemModel* model_;
  QWidget* placeholder_widget_ = nullptr;
  QFileSystemWatcher* file_watcher_ = nullptr;
  QTimer* debounce_timer_ = nullptr;

  QString project_path_;
  QStandardItem* root_item_ = nullptr;
  QString debounce_timer_queued_path_;

  // 用于重命名时跟踪旧路径
  QString rename_old_path_;
};

}  // namespace etest::app

#endif  // ETEST_APP_PROJECT_STRUCTURE_WIDGET_H_
