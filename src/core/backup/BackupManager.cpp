#include "backup/BackupManager.h"

#include <QDir>
#include <QFile>
#include <QDateTime>

#include "config/ConfigManager.h"
#include "config/ConfigDefs.h"
#include "logger/Logger.h"
#include "project/ProjectInfo.h"

using namespace etest::core::config;
using namespace etest::core::project;

namespace etest {
namespace core {
namespace backup {

BackupManager& BackupManager::instance() {
  static BackupManager mgr;
  return mgr;
}

BackupManager::BackupManager() : QObject(nullptr) {
  timer_ = new QTimer(this);
  connect(timer_, &QTimer::timeout, this, [this]() {
    if (!performBackup()) {
      LOG_WARN("BACKUP", "定时自动备份失败");
    }
  });
}

void BackupManager::onProjectOpened(const QString& projectPath) {
  currentProjectPath_ = projectPath;
  startTimer();
  LOG_INFO("BACKUP", "项目已打开，自动备份已启动: {}", projectPath.toStdString());
}

void BackupManager::onProjectClosed() {
  stopTimer();
  // 关闭前做一次备份
  if (!currentProjectPath_.isEmpty()) {
    performBackup();
  }
  currentProjectPath_.clear();
  LOG_INFO("BACKUP", "项目已关闭，自动备份已停止");
}

bool BackupManager::manualBackup() {
  if (currentProjectPath_.isEmpty()) {
    emit backupFailed("没有打开的项目");
    return false;
  }
  return performBackup();
}

bool BackupManager::restoreFromBackup(const QString& backupFilePath) {
  if (currentProjectPath_.isEmpty()) {
    emit restoreFailed("没有打开的项目");
    return false;
  }

  if (!QFile::exists(backupFilePath)) {
    emit restoreFailed("备份文件不存在: " + backupFilePath);
    return false;
  }

  // 查找当前项目的 .etproj 文件
  QDir projectDir(currentProjectPath_);
  QStringList etprojFiles = projectDir.entryList({"*.etproj"}, QDir::Files);
  if (etprojFiles.isEmpty()) {
    emit restoreFailed("项目目录下未找到 .etproj 文件");
    return false;
  }

  QString targetPath = projectDir.absoluteFilePath(etprojFiles.first());

  // 先备份当前文件
  performBackup();

  // 用备份文件替换当前项目文件
  if (QFile::exists(targetPath)) {
    QFile::remove(targetPath);
  }

  if (!QFile::copy(backupFilePath, targetPath)) {
    emit restoreFailed("无法将备份恢复到: " + targetPath);
    return false;
  }

  LOG_INFO("BACKUP", "已从备份恢复: {} -> {}",
           backupFilePath.toStdString(), targetPath.toStdString());
  emit restoreCompleted(targetPath);
  return true;
}

QList<QFileInfo> BackupManager::listBackups() const {
  if (currentProjectPath_.isEmpty()) {
    return {};
  }

  ProjectInfo info;
  info.setRootPath(currentProjectPath_);
  QDir backupDir(info.backupPath());

  if (!backupDir.exists()) {
    return {};
  }

  QFileInfoList files = backupDir.entryInfoList(
      {"*.etproj"}, QDir::Files, QDir::Time | QDir::Reversed);
  return files;
}

bool BackupManager::performBackup() {
  if (currentProjectPath_.isEmpty()) {
    return false;
  }

  bool enabled = ConfigManager::instance().get<bool>(
      CONFIG_BACKUP_ENABLED, CONFIG_BACKUP_DEFAULT_ENABLED);
  if (!enabled) {
    return true;  // 未启用不算失败
  }

  QDir projectDir(currentProjectPath_);
  QStringList etprojFiles = projectDir.entryList({"*.etproj"}, QDir::Files);
  if (etprojFiles.isEmpty()) {
    emit backupFailed("项目目录下未找到 .etproj 文件");
    return false;
  }

  QString srcPath = projectDir.absoluteFilePath(etprojFiles.first());

  // 确定备份目录
  QString customPath = ConfigManager::instance().get<QString>(CONFIG_BACKUP_PATH, "");
  QString backupDirPath;
  if (!customPath.isEmpty()) {
    backupDirPath = customPath;
  } else {
    ProjectInfo info;
    info.setRootPath(currentProjectPath_);
    backupDirPath = info.backupPath();
  }

  QDir backupDir(backupDirPath);
  if (!backupDir.exists()) {
    backupDir.mkpath(".");
  }

  // 生成备份文件名：<项目名>_YYYYMMDD_HHmmss.etproj
  QFileInfo srcInfo(srcPath);
  QString baseName = srcInfo.completeBaseName();
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  QString backupFileName = QString("%1_%2.etproj").arg(baseName, timestamp);
  QString backupFilePath = backupDir.absoluteFilePath(backupFileName);

  if (!QFile::copy(srcPath, backupFilePath)) {
    emit backupFailed("无法复制项目文件到备份目录");
    return false;
  }

  LOG_INFO("BACKUP", "备份完成: {}", backupFilePath.toStdString());
  cleanupOldBackups();
  emit backupCompleted(backupFilePath);
  return true;
}

void BackupManager::cleanupOldBackups() {
  int maxCount = ConfigManager::instance().get<int>(
      CONFIG_BACKUP_MAX_COUNT, CONFIG_BACKUP_DEFAULT_MAX_COUNT);

  QString customPath = ConfigManager::instance().get<QString>(CONFIG_BACKUP_PATH, "");
  QString backupDirPath;
  if (!customPath.isEmpty()) {
    backupDirPath = customPath;
  } else {
    ProjectInfo info;
    info.setRootPath(currentProjectPath_);
    backupDirPath = info.backupPath();
  }

  QDir backupDir(backupDirPath);
  QFileInfoList files = backupDir.entryInfoList(
      {"*.etproj"}, QDir::Files, QDir::Time);

  while (files.size() > maxCount) {
    QFileInfo oldest = files.takeLast();
    QFile::remove(oldest.absoluteFilePath());
    LOG_DEBUG("BACKUP", "清理旧备份: {}", oldest.fileName().toStdString());
  }
}

void BackupManager::startTimer() {
  int intervalMin = ConfigManager::instance().get<int>(
      CONFIG_BACKUP_INTERVAL_MIN, CONFIG_BACKUP_DEFAULT_INTERVAL_MIN);
  timer_->start(intervalMin * 60 * 1000);
}

void BackupManager::stopTimer() {
  timer_->stop();
}

}  // namespace backup
}  // namespace core
}  // namespace etest
