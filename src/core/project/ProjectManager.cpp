#include "ProjectManager.h"

#include <QDir>
#include <QFileInfo>

#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "logger/Logger.h"
#include "common/FileException.h"
#include "utils/FileUtil.h"

namespace etest::core::project {

using namespace etest::core::config;
using namespace etest::core::logger;

class ProjectManager::Impl {
 public:
  std::unique_ptr<ProjectInfo> current_project;
  DirtyCheckCallback dirty_check_callback;

  bool createProjectStructure(const QString& projectDir) {
    if (!utils::FileUtil::createDirectory(projectDir))
      return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("scripts")))
      return false;
    if (!utils::FileUtil::createDirectory(
            QDir(projectDir).filePath("protocol")))
      return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("config")))
      return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("backup")))
      return false;
    if (!utils::FileUtil::createDirectory(
            QDir(projectDir).filePath("topology")))
      return false;
    if (!utils::FileUtil::createDirectory(
            QDir(projectDir).filePath("reports")))
      return false;
    if (!utils::FileUtil::createDirectory(QDir(projectDir).filePath("cases")))
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

bool ProjectManager::createProject(const QString& name,
                                   const QString& location) {
  if (isProjectOpen()) {
    LOG_ERROR("PROJECT", "创建新项目失败：请先关闭当前项目");
    return false;
  }

  if (name.isEmpty() || location.isEmpty()) {
    LOG_WARN("PROJECT", "创建项目失败：名称或路径为空");
    return false;
  }

  QString projectDir = QDir(location).filePath(name);
  QString etprojPath = QDir(projectDir).filePath(name + ".etproj");

  // 检查目录是否已存在
  if (utils::FileUtil::exists(projectDir)) {
    LOG_WARN("PROJECT", "创建项目失败：目录已存在 {}",
             projectDir.toStdString());
    return false;
  }

  try {
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

    m_impl->current_project = std::move(info);
  } catch (const etest::core::common::FileException& e) {
    LOG_ERROR("PROJECT", "创建项目文件操作失败：{}", e.what());
    utils::FileUtil::remove(projectDir);
    return false;
  }
  addToRecentProjects(etprojPath);

  LOG_INFO("PROJECT", "项目创建成功：{}", projectDir.toStdString());
  emit projectCreated(projectDir);
  emit projectOpened(projectDir);
  return true;
}

bool ProjectManager::openProject(const QString& filePath) {
  LOG_INFO("PROJECT", "尝试打开项目文件：{}", filePath.toStdString());
  if (isProjectOpen()) {
    LOG_ERROR("PROJECT", "打开新项目失败：请先关闭当前项目");
    return false;
  }

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
  if (!isProjectOpen())
    return true;

  QString projectPath = m_impl->current_project->rootPath();

  // 无论有没有未保存更改，都要发出即将关闭的信号，用于关闭项目内的文件
  emit projectAboutToClose(projectPath);

  return doCloseProject();
}

bool ProjectManager::saveProject() {
  if (!isProjectOpen())
    return false;
  return m_impl->current_project->saveToFile();
}

bool ProjectManager::isProjectOpen() const {
  return m_impl->current_project != nullptr;
}

const ProjectInfo* ProjectManager::currentProject() const {
  return m_impl->current_project.get();
}

QString ProjectManager::currentProjectRoot() const {
  if (!isProjectOpen()) {
    return QString();
  }
  return m_impl->current_project->rootPath();
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

void ProjectManager::registerTopologyRef(const QString& filePath) {
  if (!isProjectOpen())
    return;
  QFileInfo fi(filePath);
  TopologyRef ref;
  ref.id = filePath;
  ref.name = fi.completeBaseName();
  ref.filePath = QDir(currentProjectRoot()).relativeFilePath(filePath);
  m_impl->current_project->addTopology(ref);
  m_impl->current_project->saveToFile();
}

void ProjectManager::removeTopologyRef(const QString& id) {
  if (!isProjectOpen())
    return;
  m_impl->current_project->removeTopology(id);
  m_impl->current_project->saveToFile();
}

void ProjectManager::registerProtocolRef(const QString& filePath) {
  if (!isProjectOpen())
    return;
  QFileInfo fi(filePath);
  ProtocolRef ref;
  ref.id = filePath;
  ref.name = fi.completeBaseName();
  ref.filePath = QDir(currentProjectRoot()).relativeFilePath(filePath);
  m_impl->current_project->addProtocol(ref);
  m_impl->current_project->saveToFile();
}

void ProjectManager::removeProtocolRef(const QString& id) {
  if (!isProjectOpen())
    return;
  m_impl->current_project->removeProtocol(id);
  m_impl->current_project->saveToFile();
}

void ProjectManager::registerTestProgramRef(const QString& filePath) {
  if (!isProjectOpen())
    return;
  QFileInfo fi(filePath);
  TestProgramRef ref;
  ref.id = filePath;
  ref.name = fi.completeBaseName();
  ref.filePath = QDir(currentProjectRoot()).relativeFilePath(filePath);
  ref.type = QStringLiteral("json");
  m_impl->current_project->addTestProgram(ref);
  m_impl->current_project->saveToFile();
}

void ProjectManager::removeTestProgramRef(const QString& id) {
  if (!isProjectOpen())
    return;
  m_impl->current_project->removeTestProgram(id);
  m_impl->current_project->saveToFile();
}

bool ProjectManager::doCloseProject() {
  if (!isProjectOpen())
    return true;

  // 保存recent_files等信息
  m_impl->current_project->saveToFile();

  LOG_INFO("PROJECT", "项目关闭");
  m_impl->current_project.reset();
  emit projectClosed();
  return true;
}

}  // namespace etest::core::project
