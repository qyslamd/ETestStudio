#include "ProjectManager.h"

#include <QDir>
#include <QFileInfo>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "logger/Logger.h"
#include "utils/FileUtil.h"

namespace etest {
namespace core {
namespace project {

class ProjectManager::Impl {
 public:
  std::unique_ptr<ProjectInfo> current_project;
  DirtyCheckCallback dirty_check_callback;

  bool createProjectStructure(const QString& projectDir) {
    if (!utils::FileUtil::createDirectory(projectDir)) return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("scripts")))
      return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("protocol")))
      return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("config")))
      return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("backup")))
      return false;
    return true;
  }

  QStringList loadRecentProjects() {
    auto& cfg = ConfigManager::instance();
    return cfg.get<QStringList>(CONFIG_RECENT_PROJECT_LIST);
  }

  void saveRecentProjects(const QStringList& projects) {
    auto& cfg = ConfigManager::instance();
    cfg.set(CONFIG_RECENT_PROJECT_LIST, projects);
  }
};

ProjectManager::ProjectManager() : m_impl(std::make_unique<Impl>()) {}

ProjectManager::~ProjectManager() = default;

ProjectManager& ProjectManager::instance() {
  static ProjectManager inst;
  return inst;
}

bool ProjectManager::createProject(const QString& name, const QString& location) {
  if (name.isEmpty() || location.isEmpty()) {
    LOG_WARN("PROJECT", "创建项目失败：名称或路径为空");
    return false;
  }

  QString projectDir = QDir(location).filePath(name);
  QString etprojPath = QDir(projectDir).filePath(name + ".etproj");

  // 检查目录是否已存在
  if (utils::FileUtil::exists(projectDir)) {
    LOG_WARN("PROJECT", "创建项目失败：目录已存在 {}", projectDir.toStdString());
    return false;
  }

  // 创建目录结构
  if (!m_impl->createProjectStructure(projectDir)) {
    LOG_ERROR("PROJECT", "创建项目目录结构失败");
    return false;
  }

  // 生成项目信息
  auto info = std::make_unique<ProjectInfo>();
  info->setVersion("1.0");
  info->setName(name);
  info->setCreateTime(QDateTime::currentDateTime());
  info->setRootPath(projectDir);
  info->setProjectFilePath(etprojPath);

  // 写入.etproj文件
  if (!info->saveToFile()) {
    LOG_ERROR("PROJECT", "写入项目文件失败：{}", etprojPath.toStdString());
    return false;
  }

  // 关闭当前项目（如有）
  if (isProjectOpen()) {
    doCloseProject();
  }

  m_impl->current_project = std::move(info);
  addToRecentProjects(etprojPath);

  LOG_INFO("PROJECT", "项目创建成功：{}", projectDir.toStdString());
  emit projectCreated(projectDir);
  emit projectOpened(projectDir);
  return true;
}

bool ProjectManager::openProject(const QString& filePath) {
  if (!utils::FileUtil::isFile(filePath)) {
    LOG_WARN("PROJECT", "打开项目失败：文件不存在 {}", filePath.toStdString());
    return false;
  }

  if (!filePath.endsWith(".etproj")) {
    LOG_WARN("PROJECT", "打开项目失败：不是.etproj文件");
    return false;
  }

  auto info = std::make_unique<ProjectInfo>();
  if (!info->loadFromFile(filePath)) {
    LOG_ERROR("PROJECT", "解析项目文件失败：{}", filePath.toStdString());
    return false;
  }

  // 修正rootPath为绝对路径
  QFileInfo fi(filePath);
  QString rootPath = info->rootPath();
  if (QDir::isRelativePath(rootPath)) {
    rootPath = fi.absoluteDir().filePath(rootPath);
    info->setRootPath(rootPath);
  }

  // 关闭当前项目
  if (isProjectOpen()) {
    doCloseProject();
  }

  QString projectPath = info->rootPath();
  m_impl->current_project = std::move(info);

  // 保存最近打开路径
  auto& cfg = ConfigManager::instance();
  cfg.set(CONFIG_RECENT_LAST_OPEN_PATH, fi.absolutePath());

  addToRecentProjects(filePath);

  LOG_INFO("PROJECT", "项目打开成功：{}", projectPath.toStdString());
  emit projectOpened(projectPath);
  return true;
}

bool ProjectManager::closeProject() {
  if (!isProjectOpen()) return true;

  QString projectPath = m_impl->current_project->rootPath();

  if (hasUnsavedChanges()) {
    emit projectAboutToClose(projectPath);
  }

  return doCloseProject();
}

bool ProjectManager::saveProject() {
  if (!isProjectOpen()) return false;
  return m_impl->current_project->saveToFile();
}

bool ProjectManager::isProjectOpen() const {
  return m_impl->current_project != nullptr;
}

const ProjectInfo* ProjectManager::currentProject() const {
  return m_impl->current_project.get();
}

QStringList ProjectManager::recentProjects() const {
  return m_impl->loadRecentProjects();
}

void ProjectManager::addToRecentProjects(const QString& projectPath) {
  QStringList projects = m_impl->loadRecentProjects();

  // 去重并移到最前
  projects.removeAll(projectPath);
  projects.prepend(projectPath);

  // 限制数量
  while (projects.size() > CONFIG_RECENT_MAX_COUNT) {
    projects.removeLast();
  }

  m_impl->saveRecentProjects(projects);
  emit recentProjectsChanged();
}

void ProjectManager::removeFromRecentProjects(const QString& projectPath) {
  QStringList projects = m_impl->loadRecentProjects();
  if (projects.removeAll(projectPath) > 0) {
    m_impl->saveRecentProjects(projects);
    emit recentProjectsChanged();
  }
}

void ProjectManager::clearRecentProjects() {
  m_impl->saveRecentProjects(QStringList());
  emit recentProjectsChanged();
}

void ProjectManager::setDirtyCheckCallback(DirtyCheckCallback callback) {
  m_impl->dirty_check_callback = std::move(callback);
}

bool ProjectManager::hasUnsavedChanges() const {
  if (m_impl->dirty_check_callback) {
    return m_impl->dirty_check_callback();
  }
  return false;
}

bool ProjectManager::doCloseProject() {
  if (!isProjectOpen()) return true;

  // 保存recent_files等信息
  m_impl->current_project->saveToFile();

  LOG_INFO("PROJECT", "项目关闭");
  m_impl->current_project.reset();
  emit projectClosed();
  return true;
}

}  // namespace project
}  // namespace core
}  // namespace etest
