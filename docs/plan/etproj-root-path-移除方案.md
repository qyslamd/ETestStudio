# .etproj root_path 与 recent_files 字段移除方案

## 背景

项目文件 `.etproj` 中的 `root_path` 字段用于记录项目根目录的绝对路径。该设计在跨平台使用场景下存在根本性缺陷，会导致项目文件永久损坏。同时 `.etproj` 中的 `recent_files` 字段为已废弃的死代码，应一并清理。本文档记录问题根因分析及移除这两个字段的设计方案。

## 问题现象

### 案例：demo_mock 项目无法加载

文件 `temp/projects/demo_mock/demo_mock.etproj` 内容：

```json
{
    "root_path": "/home/zhou/works/etest-demo/temp/projects/demo_mock/D:/trae_workspace/etest-demo/temp/projects/demo_mock"
}
```

`root_path` 是 Linux 路径与 Windows 路径的拼接产物，指向一个不存在的目录。项目"加载"从 `ProjectManager::openProject` 返回 `true`（JSON 合法、解析通过），但 `rootPath` 指向无效目录，下游所有子目录扫描（`topology/`、`protocol/`、`cases/` 等）返回空，UI 表现为项目打开后无任何内容。

## 根因分析

### 1. `root_path` 存储绝对路径是冗余设计

`.etproj` 文件永远位于项目根目录内（创建时 `etprojPath = projectDir/name.etproj`），`root_path` **永远等于 `.etproj` 的父目录**。存储绝对路径属于冗余信息，必然随项目目录移动或跨平台迁移而失效。

### 2. 跨平台路径判断失效

`ProjectManager::openProject`（`src/core/project/ProjectManager.cpp:219-225`）中的路径修正逻辑：

```cpp
QString rootPath = info->rootPath();
if (QDir::isRelativePath(rootPath)) {
  rootPath = fi.absoluteDir().filePath(rootPath);
  info->setRootPath(rootPath);
}
```

`QDir::isRelativePath()` 的判断是平台相关的：

| 平台 | 输入 `D:/projects/myproj` | 结果 |
|------|--------------------------|------|
| Windows | 以盘符 `D:` 开头 | `false`（绝对路径）-> 不修正 |
| Linux | 不以 `/` 开头 | `true`（相对路径）-> **错误拼接** |

在 Linux 上，`fi.absoluteDir().filePath("D:/projects/myproj")` 把 Windows 路径当作相对子目录拼接，产生 `/home/user/.../D:/projects/myproj` 的无效路径。

### 3. 污染路径被写回 .etproj（永久损坏）

`ProjectManager::doCloseProject`（`src/core/project/ProjectManager.cpp:328-340`）在关闭项目时调用 `saveToFile()`，将内存中已被污染的 `root_path` 序列化写回 `.etproj` 文件：

```
污染链路：
1. Linux 打开 Windows 创建的 .etproj（root_path: "D:/projects/myproj"）
2. isRelativePath 误判 -> rootPath 被改为 "/home/.../D:/projects/myproj"
3. setRootPath 写入内存
4. 关闭项目 -> saveToFile -> 污染路径永久写回 .etproj
5. 此后双平台都失效
```

### 4. 全代码库无跨平台路径检测

排查结果：

- `QDir::isRelativePath`：仅 1 处（即上述 BUG 所在）
- `QDir::isAbsolutePath`：仅 1 处（测试断言）
- `cleanPath` / `canonicalPath`：0 处
- 盘符正则检测 `^[A-Za-z]:[/\\]`：0 处

没有任何代码处理跨平台路径迁移场景。

### 5. `recent_files` 字段为死代码

`ProjectInfo` 中的 `recent_files_` 成员及其 getter/setter（`recentFiles()` / `setRecentFiles()`）在**生产代码中无任何调用方**。实际在用的"最近文件"功能走的是 ConfigManager 的 `recent/file_list`（全局配置，存绝对路径），与 `.etproj` 的 `recent_files` 完全无关。该字段在序列化时永远是空数组，反序列化时读到也无人使用，属于和 `root_path` 同类的冗余字段，应一并清理。

## 设计方案

### 核心原则

| 原则 | 说明 |
|------|------|
| `.etproj` 是唯一锚点 | 项目根 = `.etproj` 所在目录，不再存储 `root_path` |
| `.etproj` 内部不存绝对路径 | 消除冗余，消除跨平台失效根源 |
| 运行时用绝对路径 | `rootPath()` 从 `project_file_path_` 推导，内存中全是绝对路径 |
| 保存时不写路径字段 | `toJson` 不输出 `root_path` |
| 死代码一并清理 | `recent_files` 及其 getter/setter 删除 |

### rootPath() 推导逻辑

```cpp
QString ProjectInfo::rootPath() const {
  QFileInfo fi(project_file_path_);
  return fi.absoluteDir().absolutePath();
}
```

### 向后兼容

| 场景 | 处理 |
|------|------|
| 旧 `.etproj` 含 `root_path` 字段 | `fromJson` 忽略，不读取 |
| 旧 `.etproj` 的 `root_path` 已被污染 | 忽略，`rootPath` 从 `.etproj` 位置推导，自动正确 |
| 旧 `.etproj` 含 `recent_files` 字段 | `fromJson` 忽略，不读取 |
| 保存时 | `toJson` 不写 `root_path` 和 `recent_files`，旧字段在下次保存时自然消失 |

## 改动清单

### 1. ProjectInfo（核心改造）

**文件**：`src/core/project/ProjectInfo.h`、`src/core/project/ProjectInfo.cpp`

| 改动项 | 说明 |
|--------|------|
| 删除 `root_path_` 成员 | 不再存储 |
| 删除 `setRootPath()` 声明与定义 | 不再可设 |
| `rootPath()` 改为从 `project_file_path_` 推导 | 返回 `.etproj` 父目录绝对路径 |
| `toJson()` 删除 `root_path` 字段 | 不再序列化 |
| `fromJson()` 不再读取 `root_path` | 旧文件兼容：读到也丢弃 |
| `isValid()` 改判 `project_file_path_` | `!name_.isEmpty() && !project_file_path_.isEmpty()` |
| 7 个 `*Path()` 方法 | `root_path_` -> `rootPath()` |
| 2 个 `scanDirectory()` | `root_path_` -> `rootPath()` |
| 删除 `recent_files_` 成员 | 死代码，生产代码无人调用 getter/setter |
| 删除 `recentFiles()` / `setRecentFiles()` 声明与定义 | 同上 |
| `toJson()` 删除 `recent_files` 字段 | 不再序列化 |
| `fromJson()` 不再读取 `recent_files` | 旧文件兼容：读到也丢弃 |

受影响的方法列表：

- `scriptsPath()`、`protocolPath()`、`configPath()`、`backupPath()`
- `topologyPath()`、`reportsPath()`、`casesPath()`
- `scanDirectory(subDir, suffix)`、`scanDirectory(subDir, suffixes)`

### 2. ProjectManager（删除路径修正逻辑）

**文件**：`src/core/project/ProjectManager.cpp`

| 位置 | 改动 |
|------|------|
| `createProject()` line 158 | 删除 `info->setRootPath(projectDir)` |
| `openProject()` line 219-225 | **整块删除**路径修正逻辑（不再需要） |

### 3. BackupManager（去掉临时 ProjectInfo 构造）

**文件**：`src/core/backup/BackupManager.cpp`

3 处相同模式（line 102-104、141-143、178-180）：

```cpp
// 旧：
ProjectInfo info;
info.setRootPath(currentProjectPath_);
backupDirPath = info.backupPath();

// 新：直接用路径拼接
backupDirPath = QDir(currentProjectPath_).filePath("backup");
```

### 4. 测试代码

**文件**：`tests/core/project_manager_test.cpp`

6 处 `setRootPath()` 调用改为 `setProjectFilePath()`，并更新对应的断言：

| 测试用例 | 旧代码 | 新代码 |
|----------|--------|--------|
| `SettersAndGetters` (line 47) | `setRootPath("/tmp/test")` | `setProjectFilePath("/tmp/test/test.etproj")` |
| `ToJsonFromJson` (line 58) | `setRootPath("/home/user/my_project")` | 删除，并移除 `root_path` 的 JSON 断言 |
| `SaveAndLoadFile` (line 92) | `setRootPath(dir)` | 删除（line 93 已有 `setProjectFilePath(filePath)`） |
| `DirectoryPathHelpers` (line 126) | `setRootPath("/home/user/project")` | `setProjectFilePath("/home/user/project/test.etproj")` |
| `ScanDirectory` (line 157) | `setRootPath(dir)` | `setProjectFilePath(dir + "/test.etproj")` |
| `IsValid` (line 187) | `setRootPath("/tmp/test")` | `setProjectFilePath("/tmp/test/test.etproj")` |

`ToJsonFromJson` 测试中涉及 `recent_files` 的代码一并清理：删除 `setRecentFiles` 调用（line 60）和 `recent_files` JSON 断言、`restored.recentFiles()` 断言（line 72）。

### 5. 修复已损坏的 demo_mock.etproj

**文件**：`temp/projects/demo_mock/demo_mock.etproj`

删除 `root_path` 行。加载时 `fromJson` 忽略该字段，`rootPath` 从 `.etproj` 位置推导，自动指向正确的项目根目录。

## 不改动的部分

| 项目 | 原因 |
|------|------|
| ConfigManager `recent/project_list` | 应用级配置，存绝对路径合理（只影响最近列表，不损坏项目文件） |
| ConfigManager `recent/file_list` | 同上 |
| `.etproj` 的 `settings` 字段 | 保留 |

## 风险评估

| 风险 | 等级 | 缓解 |
|------|------|------|
| `ProjectInfo` 默认构造时 `project_file_path_` 为空，`rootPath()` 返回应用当前工作目录 | 中 | `loadFromFile` 在 line 103 设置 `project_file_path_`，早于任何 `rootPath()` 调用；`createProject` 中 `setProjectFilePath` 在 `saveToFile` 之前调用。`BackupManager` 的 3 处临时构造改为直接 `QDir::filePath`，不再依赖 `ProjectInfo`，故不会触发 |
| 旧 `.etproj` 未保存前 `rootPath()` 依赖 `project_file_path_` 为空 | 中 | 同上，`loadFromFile` 先于 `rootPath()` 调用 |
| `BackupManager` 临时构造 `ProjectInfo` 不再可用 | 低 | 已改为直接 `QDir::filePath`，不依赖 `ProjectInfo` |
| `BackupManager::currentProjectPath_` 与 `ProjectInfo::rootPath()` 不一致 | 低 | `currentProjectPath_` 来自 `projectOpened` 信号参数，`createProject` 发射 `projectDir`（绝对），`openProject` 发射 `info->rootPath()`（从 `.etproj` 推导绝对），两者一致 |
| 测试用例断言变化导致失败 | 低 | 同步更新测试代码 |
| 第三方工具读取 `root_path` / `recent_files` 字段 | 低 | 无第三方工具，`fromJson` 仍兼容旧字段（忽略） |

## 验证方式

1. 编译通过（Linux + Windows）
2. 单元测试 `test_core_project_manager` 全部通过
3. Linux 上打开 Windows 创建的 `.etproj`，项目正常加载
4. 关闭项目后 `.etproj` 不再包含 `root_path` 和 `recent_files` 字段
5. `demo_mock.etproj` 修复后可正常加载
