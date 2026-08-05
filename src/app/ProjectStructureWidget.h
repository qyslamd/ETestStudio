#ifndef ETEST_APP_PROJECT_STRUCTURE_WIDGET_H_
#define ETEST_APP_PROJECT_STRUCTURE_WIDGET_H_

#include <QMap>
#include <QSet>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

#include <QString>
#include <QWidget>
#include "plugin_sdk/IDevicePlugin.h"
#include "plugin_sdk/PluginManager.h"
#include "widgets/OpenFileDelegate.h"
#include "widgets/ProjectTreeDelegate.h"

class QFileSystemWatcher;
class QLabel;
class QListView;
class QMenu;
class QStandardItemModel;
class QPoint;
class QToolButton;
class QPushButton;
class QScrollArea;
class QSplitter;
class QToolButton;
class QTreeView;
class QTimer;

namespace etest::app {

// 自定义 role 常量（Qt::UserRole + 偏移）
enum ProjectNodeRole {
  NodeTypeRole = Qt::UserRole + 1,  // "root" | "category" | "file"
  RelativePathRole,                 // 相对于项目根目录的路径
  CategoryIdRole,                   // category 的标识（如 "protocol"）
  IsLatestRole,                     // bool: 报告分类中每个程序名的最新文件
  IsEffectiveTopologyRole,          // bool: topology.etopo（引擎加载的唯一拓扑文件）
  IsIcdConfigRole,                  // bool: ICDConfig.xml/json（协议配置容器文件）
  IsMockConfigRole,                 // bool: MockResponses.emock（Mock 响应配置）
};

struct CategoryInfo {
  QString id;            // 目录名，如 "protocol"
  QString displayName;   // 显示名，如 "协议"
  QString dirPath;       // 相对于项目根的实际路径
  QString iconName;      // 图标名称
  QString newFileExt;    // 在此分类下新建文件的默认扩展名，空表示不支持新建
  QString newFileLabel;  // "新建"菜单项的标签，如"新建协议文件"
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

  // 占位页操作
  void newProjectRequested();
  void openProjectRequested();
  void projectOpenRequested(const QString& projectPath);

  // 已打开文件列表操作
  void openFileActivateRequested(const QString& filePath);
  void openFileCloseRequested(const QString& filePath);

  // 最近文件操作
  void recentFileOpenRequested(const QString& filePath);

  // 文件系统监控 — 目录内容变化时发出，由 main_window 路由到对应管理器
  void directoryContentChanged(const QString& dirPath);

  // 文件列表变化时发出（buildTree / refreshCategory 后）
  void fileListChanged();

  // 硬件节点导航请求 — 右键/双击硬件设备跳转到平台设备树
  void hardwareDeviceNavigateRequested(const QString& deviceType,
                                       const QString& pluginId);

  public:
  void refreshRecentProjects();
  void refreshRecentFiles();
  void setOpenFiles(const QStringList& paths);
  void onFileOpened(const QString& path);
  void onFileClosed(const QString& path);

  /// 收集树中所有文件节点的文件名
  QStringList allFileNames() const;
  /// 按文件名精确定位项目树节点（选中 + 展开 + 滚动）
  bool locateFile(const QString& fileName);
  /// 清除树选中状态
  void clearTreeSelection();

 private slots:
  void onCustomContextMenu(const QPoint& pos);
  void onItemDoubleClicked(const QModelIndex& index);
  void onItemChanged(QStandardItem* item);

 private:
  void initUi();
  void initSignals();
  void buildTree();
  void refreshCategory(const QString& dirPath);
  void onDirectoryChanged(const QString& path);

  // 文件操作
  void createNewFile(const QString& categoryId,
                     const QString& extension,
                     const QString& baseName);
  void createStandaloneFile(const QString& extension, const QString& baseName);
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
  void populateReportCategory(QStandardItem* catItem,
                              const QFileInfoList& entries,
                              const QString& dirPath);
  void populateBackupCategory(QStandardItem* catItem,
                              const QFileInfoList& entries,
                              const QString& dirPath);
  QString absolutePath(const QString& relativePath) const;
  QString categoryDirPath(const QString& categoryId) const;
  void clearAllEtlogFiles(const QString& categoryId);
  void refreshOtherCategory();

  // 硬件节点管理
  void refreshHardwareDevices();
  void connectHardwareRefresh();

  // 最近文件
  void removeRecentFileFromConfig(const QString& path);

  // 已打开文件列表
  void addOpenFileItem(const QString& path);
  void removeOpenFileItem(const QString& path);
  void clearOpenFiles();
  void updateOpenFilesCount();

  QStackedWidget* stack_;
  QTreeView* tree_view_;
  QStandardItemModel* model_;
  QWidget* page_default_ = nullptr;

  QListView* recent_projects_view_ = nullptr;
  QStandardItemModel* recent_projects_model_ = nullptr;
  QListView* recent_files_view_ = nullptr;
  QStandardItemModel* recent_files_model_ = nullptr;
  QPushButton* new_proj_btn_ = nullptr;
  QPushButton* open_proj_btn_ = nullptr;
  QFileSystemWatcher* file_watcher_ = nullptr;
  QTimer* debounce_timer_ = nullptr;

  QWidget* page_project_ = nullptr;
  QSplitter* tree_splitter_ = nullptr;
  QWidget* open_files_widget_ = nullptr;
  QLabel* open_files_header_label_ = nullptr;
  QListView* open_files_view_ = nullptr;
  QStandardItemModel* open_files_model_ = nullptr;
  OpenFileDelegate* open_file_delegate_ = nullptr;

  QString project_path_;
  QStandardItem* root_item_ = nullptr;
  QSet<QString> debounce_timer_queued_paths_;
  // 新建文件后,短时间内忽略该目录的 watcher 刷新(避免破坏刚打开的 inline
  // editor)
  QSet<QString> suppressed_watch_paths_;
  ProjectTreeDelegate* tree_delegate_ = nullptr;

  // 用于重命名时跟踪旧路径
  QString rename_old_path_;
};

}  // namespace etest::app

#endif  // ETEST_APP_PROJECT_STRUCTURE_WIDGET_H_
