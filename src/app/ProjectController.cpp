#include "ProjectController.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QStringList>

#include "EditorManager.h"
#include "editors/EditorFactory.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "dialogs/NewProjectDialog.h"
#include "logger/Logger.h"
#include "project/ProjectManager.h"

using namespace etest::core::config;

namespace etest::app {

ProjectController::ProjectController(QWidget* parent_widget,
                                     EditorManager* editor_mgr,
                                     QObject* parent)
    : QObject(parent),
      parent_widget_(parent_widget),
      editor_mgr_(editor_mgr) {}

void ProjectController::newProject() {
  LOG_INFO("MAIN_UI", "点击「新建项目」");
  // 先尝试关闭当前项目
  if (!tryCloseCurrentProject()) {
    return;
  }

  etest::app::NewProjectDialog dlg(parent_widget_);
  if (dlg.exec() == QDialog::Accepted) {
    auto& project_mgr = etest::core::project::ProjectManager::instance();
    if (!project_mgr.createProject(dlg.projectName(),
                                   dlg.projectLocation())) {
      QMessageBox::warning(
          parent_widget_, QStringLiteral("新建项目失败"),
          QStringLiteral("无法创建项目，请检查名称和路径。"));
    } else {
      // createProject 会打开项目，通知监听方更新状态
      emit projectOpened(project_mgr.currentProjectRoot());
    }
  }
}

void ProjectController::openProject() {
  LOG_INFO("MAIN_UI", "点击「打开项目」");
  if (!tryCloseCurrentProject()) {
    return;
  }

  auto& cfg = ConfigManager::instance();
  QString last_path = cfg.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
  if (last_path.isEmpty()) {
    last_path = QDir::homePath();
  }

  QString file_path =
      QFileDialog::getOpenFileName(parent_widget_,
                                   QStringLiteral("打开项目"),
                                   last_path,
                                   QStringLiteral("ETest项目文件 (*.etproj)"));
  if (!file_path.isEmpty()) {
    auto& project_mgr = etest::core::project::ProjectManager::instance();
    if (!project_mgr.openProject(file_path)) {
      QMessageBox::warning(
          parent_widget_, QStringLiteral("打开项目失败"),
          QStringLiteral("无法打开项目文件：%1").arg(file_path));
    } else {
      emit projectOpened(file_path);
    }
  }
}

void ProjectController::openFile() {
  LOG_INFO("MAIN_UI", "点击「打开文件」");

  QStringList exts = EditorFactoryRegistry::registeredExtensions();
  exts.sort();
  QStringList patterns;
  for (const auto& e : exts) {
    patterns << (QStringLiteral("*.") + e);
  }
  QString filter =
      QStringLiteral("所有支持的文件 (%1)").arg(patterns.join(QChar::Space));

  auto& cfg = ConfigManager::instance();
  QString last_path = cfg.get<QString>(CONFIG_RECENT_LAST_OPEN_PATH);
  if (last_path.isEmpty()) {
    last_path = QDir::homePath();
  }

  QString file_path = QFileDialog::getOpenFileName(
      parent_widget_, QStringLiteral("打开文件"), last_path, filter);
  if (file_path.isEmpty())
    return;

  cfg.set(CONFIG_RECENT_LAST_OPEN_PATH,
          QFileInfo(file_path).absolutePath());
  emit fileRequested(file_path);
}

void ProjectController::closeProject() {
  LOG_INFO("MAIN_UI", "点击「关闭项目」");
  tryCloseCurrentProject();
}

void ProjectController::openRecent(const QString& path) {
  LOG_INFO("MAIN_UI", "打开最近项目: {}", path.toStdString());
  if (!tryCloseCurrentProject()) {
    return;
  }
  auto& project_mgr = etest::core::project::ProjectManager::instance();
  if (!project_mgr.openProject(path)) {
    QMessageBox::warning(
        parent_widget_, QStringLiteral("打开项目失败"),
        QStringLiteral("无法打开项目文件：%1").arg(path));
  } else {
    emit projectOpened(path);
  }
}

void ProjectController::updateWindowTitle() {
  if (!parent_widget_)
    return;
  auto& project_mgr = etest::core::project::ProjectManager::instance();
  auto* project = project_mgr.currentProject();
  if (project) {
    parent_widget_->setWindowTitle(
        QStringLiteral("ETest Studio - %1").arg(project->name()));
  } else {
    parent_widget_->setWindowTitle(QStringLiteral("ETest Studio"));
  }
}

bool ProjectController::tryCloseCurrentProject() {
  auto& project_mgr = etest::core::project::ProjectManager::instance();
  if (!project_mgr.isProjectOpen())
    return true;

  // 检查是否有未保存的更改
  if (editor_mgr_ && editor_mgr_->hasUnsavedChanges()) {
    int ret = QMessageBox::warning(
        parent_widget_, QStringLiteral("关闭项目"),
        QStringLiteral("当前项目有未保存的更改。"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save) {
      editor_mgr_->saveAllFiles();
    } else if (ret == QMessageBox::Cancel) {
      return false;
    }
  }

  emit projectClosed();
  return true;
}

void ProjectController::updateRecentProjectsMenu(QMenu* menu) {
  if (!menu)
    return;
  menu->clear();
  auto& cfg = ConfigManager::instance();
  auto recent_list = cfg.get<QStringList>(CONFIG_RECENT_PROJECT_LIST);
  for (const auto& path : recent_list) {
    auto* action = menu->addAction(
        QFileInfo(path).fileName() +
        QStringLiteral("  [%1]").arg(QDir::toNativeSeparators(
            QFileInfo(path).absolutePath())));
    action->setData(path);
  }
  if (recent_list.isEmpty()) {
    auto* action = menu->addAction(QStringLiteral("(无最近项目)"));
    action->setEnabled(false);
  }
}

void ProjectController::updateRecentFilesMenu(QMenu* menu) {
  if (!menu)
    return;
  menu->clear();
  auto& cfg = ConfigManager::instance();
  auto recent_list = cfg.get<QStringList>(CONFIG_RECENT_FILE_LIST);
  for (const auto& path : recent_list) {
    auto* action = menu->addAction(
        QFileInfo(path).fileName() +
        QStringLiteral("  [%1]").arg(QDir::toNativeSeparators(
            QFileInfo(path).absolutePath())));
    action->setData(path);
  }
  if (recent_list.isEmpty()) {
    auto* action = menu->addAction(QStringLiteral("(无最近文件)"));
    action->setEnabled(false);
  }
}

QString ProjectController::findProjectFile(const QString& dir_path) {
  QDir dir(dir_path);
  for (int i = 0; i < 8 && !dir.isRoot(); ++i) {
    QStringList entries = dir.entryList({QStringLiteral("*.etproj")},
                                        QDir::Files | QDir::NoDotAndDotDot);
    if (!entries.isEmpty()) {
      return dir.absoluteFilePath(entries.first());
    }
    if (!dir.cdUp())
      break;
  }
  return {};
}

}  // namespace etest::app
