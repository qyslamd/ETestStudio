# BackupManager 实现计划

## Context
阶段1.2遗留最后一项：自动备份。配置键和目录脚手架已就绪，缺少 BackupManager 类、备份逻辑、设置页面UI、恢复功能。

## 已有基础设施
- `ConfigDefs.h`: 4个备份配置键 + 默认值（enabled=true, interval=5min, maxCount=5, path=""）
- `ProjectInfo::backupPath()`: 返回 `<项目根>/backup/`
- `ProjectManager::createProject()`: 创建 backup/ 目录
- `ConfigManager`: 读写备份配置

## 新增文件

### 1. `src/core/backup/BackupManager.h/.cpp`

单例 QObject，职责：
- **定时自动备份**：QTimer，打开项目后启动，关闭项目后停止
- **手动触发备份**：`manualBackup()` 公开方法
- **从备份恢复**：`restoreFromBackup(backupFilePath)` 公开方法
- **清理过期备份**：保留最近 N 份，删除更早的

```cpp
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
```

**备份内容**：复制 `.etproj` 项目文件到 `<项目根>/backup/<项目名>_YYYYMMDD_HHmmss.etproj`

**备份逻辑**：
1. 检查配置是否启用
2. 读取间隔和最大份数
3. 复制 .etproj 文件到 backup/ 目录，文件名加时间戳
4. 清理超出 maxCount 的旧备份
5. emit backupCompleted/backupFailed

**恢复逻辑**：
1. 将备份文件复制回项目根，替换当前 .etproj
2. emit restoreCompleted/restoreFailed

**定时器**：
- 从 ConfigManager 读取间隔（分钟），转为毫秒
- 配置变更时重启定时器

### 2. `src/core/CMakeLists.txt` — 添加 backup/ 源文件

### 3. `src/app/SettingsWidget` — 添加"备份"设置页

在现有4个tab（常用/编辑器/终端/外观）后添加第5个"备份"tab：
- 自动备份开关（QCheckBox，绑定 CONFIG_BACKUP_ENABLED）
- 备份间隔（QSpinBox 分钟，绑定 CONFIG_BACKUP_INTERVAL_MIN）
- 最大份数（QSpinBox，绑定 CONFIG_BACKUP_MAX_COUNT）
- 手动备份按钮（QPushButton，调用 BackupManager::manualBackup()）

### 4. `src/app/MainWindow` — 连接 BackupManager 到项目生命周期

在 `initSignals()` 中：
- `ProjectManager::projectOpened` → `BackupManager::onProjectOpened`
- `ProjectManager::projectClosed` → `BackupManager::onProjectClosed`

## 修改文件清单
| 文件 | 改动 |
|------|------|
| `src/core/backup/BackupManager.h` | 新增 |
| `src/core/backup/BackupManager.cpp` | 新增 |
| `src/core/CMakeLists.txt` | 添加源文件和头文件 |
| `src/app/SettingsWidget.h` | 添加 createBackupPage() 声明 |
| `src/app/SettingsWidget.cpp` | 添加备份tab UI和绑定 |
| `src/app/MainWindow.cpp` | 连接 BackupManager 信号 |

## 验证
1. 构建通过
2. 打开项目后检查 backup/ 目录下是否按时生成备份文件
3. 设置页面修改备份间隔后定时器是否更新
4. 手动备份按钮是否生成备份
5. 超过最大份数时旧备份是否被清理
