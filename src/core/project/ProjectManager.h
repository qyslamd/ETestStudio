#ifndef ETEST_CORE_PROJECT_PROJECTMANAGER_H_
#define ETEST_CORE_PROJECT_PROJECTMANAGER_H_

#include <QObject>
#include <functional>
#include <memory>

#include "ProjectInfo.h"

namespace etest {
namespace core {
namespace project {

class ProjectManager : public QObject {
  Q_OBJECT

 public:
  static ProjectManager& instance();
  ~ProjectManager() override;

  ProjectManager(const ProjectManager&) = delete;
  ProjectManager& operator=(const ProjectManager&) = delete;

  // 项目生命周期
  bool createProject(const QString& name, const QString& location);
  bool openProject(const QString& filePath);
  bool closeProject();
  bool saveProject();

  // 项目状态
  bool isProjectOpen() const;
  const ProjectInfo* currentProject() const;
  QString currentProjectRoot() const;

  // 最近项目
  QStringList recentProjects() const;
  void addToRecentProjects(const QString& projectPath);
  void removeFromRecentProjects(const QString& projectPath);
  void clearRecentProjects();

  // 工件引用管理
  void registerTopologyRef(const QString& filePath);
  void removeTopologyRef(const QString& id);
  void registerProtocolRef(const QString& filePath);
  void removeProtocolRef(const QString& id);

  // 脏文件检查接口
  using DirtyCheckCallback = std::function<bool()>;
  void setDirtyCheckCallback(DirtyCheckCallback callback);
  bool hasUnsavedChanges() const;

 Q_SIGNALS:
  void projectCreated(const QString& projectPath);
  void projectOpened(const QString& projectPath);
  void projectAboutToClose(const QString& projectPath);
  void projectClosed();
  void recentProjectsChanged();

 private:
  ProjectManager();

  bool doCloseProject();

  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace project
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_PROJECT_PROJECTMANAGER_H_
