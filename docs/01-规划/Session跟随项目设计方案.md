# Session 跟随项目 — 存储到项目目录

## Context

当前 `session.json` 存储在全局路径 `AppConfigLocation/session.json`，通过 `projectPath` 字段关联项目。问题：
- 切换项目时 session 被覆盖，无法为每个项目独立保存文件状态
- session 文件与项目分离，项目迁移/备份时不包含工作状态

改为将 `session.json` 存入项目目录的 `config/` 子目录中，让 session 跟随项目文件走。

## 存储位置

| 状态 | 路径 |
|------|------|
| 项目打开 | `<project_root>/config/session.json` |
| 无项目 | 不保存 session（无状态可恢复） |

ConfigManager 新增 `project/last_project_path` 记录最后打开的项目 `.etproj` 路径，用于恢复时定位 session 文件。

## 变更清单

### ConfigDefs.h

新增：
```cpp
constexpr const char* CONFIG_LAST_PROJECT_PATH = "project/last_project_path";
```

### ConfigManager.cpp

注册默认值：
```cpp
m_defaultValues[CONFIG_LAST_PROJECT_PATH] = "";
```

### main_window.cpp — 核心流程改造

**closeEvent() 保存流程调整：**

session 文件必须在 `closeProject()` **之前**写入（因为需要项目信息确定路径）：
```
captureSessionData() → closeAllFiles() →
  if projectOpen:
    writeSessionFile(projectRoot + "/config/session.json")
    cfg.set(CONFIG_LAST_PROJECT_PATH, projectFilePath)
  else:
    // 无项目不保存
closeProject()
saveWindowState()
```

**writeSessionFile() 改造：**

不再使用硬编码的全局路径，根据是否有项目打开决定路径：
```cpp
void MainWindow::writeSessionFile(const QJsonObject& data) {
  auto& projectMgr = ProjectManager::instance();
  QString sessionPath;
  if (projectMgr.isProjectOpen()) {
    sessionPath = QDir(projectMgr.currentProjectRoot()).filePath("config/session.json");
  } else {
    return;  // 无项目不保存
  }
  // ... write to sessionPath
}
```

**restoreSession() 恢复流程调整：**

现在是先读 session → 开项目 → 恢复文件。改为先从 ConfigManager 读最后项目路径 → 开项目 → 从项目目录读 session → 恢复文件：
```
cfg.get(CONFIG_LAST_PROJECT_PATH) →
  if exists:
    openProject(lastProjectPath)
    read session from projectRoot/config/session.json
    restore sidebar/panel/aux layout
    restore editors
    restore QADS layout
    restore splitter states
    restore cursor positions
  else:
    return  // 无最近项目，显示欢迎页
```

从 session 数据中去掉 `projectPath` 字段（不再需要——session 文件自身位置即指明了所属项目）。

### SettingsWidget

已有"恢复上次会话"复选框（`CONFIG_SESSION_RESTORE_ENABLED`），无需新增 UI。

## 涉及文件

| 操作 | 文件 |
|------|------|
| 修改 | `src/core/config/ConfigDefs.h` |
| 修改 | `src/core/config/ConfigManager.cpp` |
| 修改 | `src/app/main_window.cpp` |

## 验证

1. 勾选"恢复上次会话"
2. 打开一个项目，打开一些文件，关闭程序
3. 检查 `<project>/config/session.json` 已生成
4. 重新打开程序 → 项目自动打开，文件恢复
5. 不勾选"恢复上次会话" → 启动显示欢迎页
6. 切换项目时各自维护独立的 session
