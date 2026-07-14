#ifndef ETEST_APP_PROJECTCONTROLLER_H_
#define ETEST_APP_PROJECTCONTROLLER_H_

#include <QObject>
#include <QString>

class QMenu;
class QWidget;

namespace etest::app {

class EditorManager;

class ProjectController : public QObject {
  Q_OBJECT
 public:
  explicit ProjectController(QWidget* parent_widget,
                             EditorManager* editor_mgr,
                             QObject* parent = nullptr);

  void newProject();
  void openProject();
  void openFile();
  void closeProject();
  void openRecent(const QString& path);

  void updateWindowTitle();
  void updateRecentProjectsMenu(QMenu* menu);
  void updateRecentFilesMenu(QMenu* menu);

  // 工具方法 — 向上遍历最多 8 级目录查找 .etproj 文件
  static QString findProjectFile(const QString& dir_path);

 signals:
  void projectOpened(const QString& project_path);
  void projectClosed();
  void fileRequested(const QString& file_path);

 private:
  bool tryCloseCurrentProject();

  QWidget* parent_widget_;
  EditorManager* editor_mgr_;
};

}  // namespace etest::app

#endif  // ETEST_APP_PROJECTCONTROLLER_H_
