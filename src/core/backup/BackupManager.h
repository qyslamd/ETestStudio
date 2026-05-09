#ifndef ETEST_CORE_BACKUP_BACKUPMANAGER_H_
#define ETEST_CORE_BACKUP_BACKUPMANAGER_H_

#include <QObject>
#include <QFileInfo>
#include <QTimer>
#include <QString>

namespace etest {
namespace core {
namespace backup {

class BackupManager : public QObject {
  Q_OBJECT

 public:
  static BackupManager& instance();

  void onProjectOpened(const QString& projectPath);
  void onProjectClosed();

  bool manualBackup();
  bool restoreFromBackup(const QString& backupFilePath);
  QList<QFileInfo> listBackups() const;

 signals:
  void backupCompleted(const QString& backupPath);
  void backupFailed(const QString& error);
  void restoreCompleted(const QString& restoredFilePath);
  void restoreFailed(const QString& error);

 private:
  BackupManager();
  bool performBackup();
  void cleanupOldBackups();
  void startTimer();
  void stopTimer();

  QTimer* timer_ = nullptr;
  QString currentProjectPath_;
};

}  // namespace backup
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_BACKUP_BACKUPMANAGER_H_
