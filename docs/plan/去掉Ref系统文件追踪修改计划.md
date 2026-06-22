# 去掉 Ref 系统 — 文件系统即项目

## 目标

消除 `ProjectInfo` 中的 4 套 Ref 跟踪系统（TopologyRef/ProtocolRef/TestProgramRef/ReportRef），让文件系统成为唯一真相源。项目目录下的文件存在即属于项目，删除/重命名文件即同步反映到项目状态，无需维护并行数据。

## 设计原则

1. **文件系统是唯一真相源** — 扫描目录发现文件，不维护并行 Ref 列表
2. **强制目录约束** — 每种文件类型有固定目录，扩展名决定类型：

   | 扩展名 | 目录 | 类型 |
   |--------|------|------|
   | `.eproto` | `protocol/` | 协议 |
   | `.etopo` | `topology/` | 拓扑 |
   | `.tcase` | `cases/` | 测试用例 |
   | `.lua` | `scripts/` | 脚本 |
   | `.json` | `config/` | 配置 |

3. **ProjectInfo 退化为元数据容器** — 只存 name/version/createTime/rootPath/settings/recentFiles
4. **旧 project.etest 向后兼容** — 加载时忽略 `topology`/`protocols`/`test_programs`/`reports` 字段
5. **列表显示名用文件名（去扩展名）** — 不再从 Ref.name 读取，不解析文件内容

## 改动清单

### 阶段 1：Core 层改造

#### 1.1 `src/core/project/ProjectInfo.h`
- 删除 `TopologyRef`、`ProtocolRef`、`TestProgramRef`、`ReportRef` 4 个结构体
- 删除 `ProjectInfo` 中 4 套 CRUD 方法（共 16 个方法）：
  - `topologies()/setTopologies()/addTopology()/removeTopology()/clearTopologies()`
  - `protocols()/setProtocols()/addProtocol()/removeProtocol()/clearProtocols()`
  - `testPrograms()/setTestPrograms()/addTestProgram()/removeTestProgram()/clearTestPrograms()`
  - `reports()/setReports()/addReport()/removeReport()/clearReports()`
- 删除 4 个私有成员变量
- 新增目录扫描辅助方法：
  ```cpp
  QStringList scanDirectory(const QString& subDir, const QString& suffix) const;
  // 返回绝对路径列表
  // 示例: scanDirectory("protocol", "eproto")
  //   → ["D:/proj/protocol/a.eproto", "D:/proj/protocol/b.eproto"]
  ```

#### 1.2 `src/core/project/ProjectInfo.cpp`
- 删除 4 个 Ref 结构体的 `toJson()`/`fromJson()` 实现
- 删除 4 套 CRUD 方法实现
- `toJson()` 中移除 `topology`/`protocols`/`test_programs`/`reports` JSON 字段
- `fromJson()` 中保留对旧字段的静默忽略（不报错，不加载）
- 新增 `scanDirectory()` 实现：基于 `rootPath_` 拼接 `subDir`，用 `QDir::entryInfoList()` 过滤指定后缀，返回绝对路径列表

#### 1.3 `src/core/project/ProjectManager.h`
- 删除 6 个方法声明：
  - `registerTopologyRef(const QString&)`
  - `removeTopologyRef(const QString&)`
  - `registerProtocolRef(const QString&)`
  - `removeProtocolRef(const QString&)`
  - `registerTestProgramRef(const QString&)`
  - `removeTestProgramRef(const QString&)`

#### 1.4 `src/core/project/ProjectManager.cpp`
- 删除上述 6 个方法实现

### 阶段 2：App 层改造

#### 2.1 `src/app/EditorManager.cpp`
- `openFile()` (line 275-284)：删除 `registerXxxRef` 调用
- `closeFile()` (line 385-396)：删除 `removeXxxRef` 调用
- `updateEditorId()` (line 599-621)：删除 ref 迁移逻辑（remove old + register new）

#### 2.2 `src/app/ProtocolManagerWidget.cpp`
- `refreshList()`：改为调用 `ProjectInfo::scanDirectory("protocol", "eproto")` 获取绝对路径列表，替代 `project->protocols()`；显示名用 `QFileInfo::completeBaseName()` 提取文件名（去扩展名）；**保留子节点解析逻辑**——对每个文件仍调 `parseEprotoFrames()` 生成帧列表子节点
- `onNewProtocol()`：删除 `pm.registerProtocolRef()` 调用；**保留**现有 `refreshList()` 调用（line 225，非新增）
- `onImportXml()` (line 321)：删除 `pm.registerProtocolRef(outputPath)` 调用；**保留**现有 `refreshList()` 调用（line 322，非新增）
- `removeProtocolFile()`：删除 ref 查找和 `pm.removeProtocolRef()` 调用，只删文件；**保留**现有 `refreshList()` 调用（line 355）；将确认提示文案"此操作将从项目中移除引用，文件将被删除"改为"文件将被删除"（去掉"移除引用"措辞）
- `renameProtocolFile()`：删除 ref 更新逻辑（`removeProtocol` + `addProtocol`），只做 `QFile::rename()` + `saveProject()`；**保留**现有 `refreshList()` 调用（line 413）
- `parseEprotoFrames()`：保持不变（或后续改为用 `icd_utility` 的 json_parser）

#### 2.3 `src/app/TestProgramManagerWidget.cpp`
- `refreshList()`：改为调用 `ProjectInfo::scanDirectory("cases", "tcase")` 获取绝对路径列表，替代 `project->testPrograms()`；显示名用 `QFileInfo::completeBaseName()` 提取文件名（去扩展名）；**保留子节点解析逻辑**——对每个文件仍调 `loadTestProgram()` 生成测试用例子节点
- `onNewTestProgram()`：删除 `pm.registerTestProgramRef()`；**保留**现有 `refreshList()` 调用（line 194，非新增）
- `renameTestProgramFile()`：删除 ref 更新逻辑（`removeTestProgram` + `addTestProgram`），只做 `QFile::rename()` + `saveProject()`；**保留**现有 `refreshList()` 调用（line 253）
- `removeTestProgramFile()`：删除 ref 查找和 `pm.removeTestProgramRef()`，只删文件；**保留**现有 `refreshList()` 调用（line 285）；将确认提示文案"此操作将从项目中移除引用，文件将被删除"改为"文件将被删除"（去掉"移除引用"措辞）

#### 2.4 `src/app/main_window.cpp`
- **保留** line 647-650 的 `projectOpened`/`projectClosed` → `ProtocolManagerWidget::refreshList` 连接（计划初版误写为删除）
- **保留** line 658-661 的 `projectOpened`/`projectClosed` → `TestProgramManagerWidget::refreshList` 连接
- **新增**：连接 `ProjectStructureWidget::directoryContentChanged` 信号，按路径路由到对应 Widget：
  ```cpp
  connect(psWidget, &ProjectStructureWidget::directoryContentChanged,
          this, [this, protocolMgr, tpMgr](const QString& dirPath) {
            if (dirPath.endsWith("/protocol") || dirPath.endsWith("\\protocol"))
              protocolMgr->refreshList();
            else if (dirPath.endsWith("/cases") || dirPath.endsWith("\\cases"))
              tpMgr->refreshList();
          });
  ```

#### 2.5 `src/app/ProjectStructureWidget.{h,cpp}`

原计划认为无需改动，实际需要以下变更：

**头文件改动：**
- 成员变量 `debounce_timer_queued_path_`（`QString`）改为 `debounce_timer_queued_paths_`（`QSet<QString>`）
- 新增信号：`void directoryContentChanged(const QString& dirPath);`

**实现文件改动：**
- `onDirectoryChanged()`：改为 `debounce_timer_queued_paths_.insert(path)` + `debounce_timer_->start()`
- 防抖 timer timeout 回调：遍历 `debounce_timer_queued_paths_`，对每个路径调用 `refreshCategory(path)` 后 `emit directoryContentChanged(path)`，最后清空集合
- `refreshCategory()`：新增 watch 重新注册逻辑——处理完目录变化后检查该路径是否仍在 `file_watcher_->directories()` 中，若不在则重新 `file_watcher_->addPath(dirPath)`，以覆盖目录被删除后重建的场景
- 以下原有逻辑确认无需改动：
  - `createStandaloneFile()`：已基于文件系统，不碰 ref
  - `deleteSelectedFile()`：已只删文件不碰 ref
  - `onItemChanged()` (重命名)：已只改文件不碰 ref

### 阶段 3：文件系统监听

#### 3.1 方案 B：ProjectStructureWidget 统一路由

采用 `ProjectStructureWidget` 作为唯一的文件系统监听者，通过信号通知其他 Widget：

- `ProjectStructureWidget` 已有 `QFileSystemWatcher` 监听 `protocol/`、`cases/`、`topology/` 等目录
- 目录变化时防抖触发 `refreshCategory()` + `emit directoryContentChanged(dirPath)`
- `main_window.cpp` 中按 `dirPath` 后缀路由到 `ProtocolManagerWidget::refreshList()` 或 `TestProgramManagerWidget::refreshList()`
- `ProtocolManagerWidget` 和 `TestProgramManagerWidget` 不再创建各自的 `QFileSystemWatcher`

**防抖队列 bug 修复**：原实现 `debounce_timer_queued_path_` 是单个 `QString`，多目录同时变化时只保留最后一个路径。改为 `QSet<QString>` 收集所有变化路径，timer 触发时遍历处理。

**已知边界情况**：项目打开时若 `protocol/` 或 `cases/` 目录不存在，watcher 不会监听该目录。此时 `onNewProtocol()`/`onImportXml()`/`onNewTestProgram()` 中手动调用 `refreshList()` 保证列表刷新。后续对该目录的文件增删改由 watcher 正常接管（目录创建后 `refreshCategory` 会重新注册 watch）。

### 阶段 4：测试更新

#### 4.1 `tests/core/project_manager_test.cpp`
- 删除或重写 Ref 相关测试（`RefRoundTrip`、`RefListSerialization`、`RefIdAutoGenerated`，line 140-215）
- 新增 `scanDirectory()` 测试：验证返回绝对路径、后缀过滤、空目录返回空列表

### 阶段 5：数据迁移（可选）

#### 5.1 旧项目兼容
- `ProjectInfo::fromJson()` 遇到旧的 `topology`/`protocols`/`test_programs`/`reports` 字段时静默忽略
- 项目保存时自然不再写入这些字段，旧字段消失
- 不需要主动迁移——文件本来就在磁盘上，去掉 ref 不影响文件本身

## 不涉及的部分

- `icd_utility` — 不改动，它的 Repository/Loader 是纯数据层，与 Ref 无关
- `ProtocolEditorWidget` — 不改动，它通过 `Loader::init()` 直接加载 `.eproto` 文件
- `SearchWidget`/`GitWidget` — 不改动，它们是路径感知型，不涉及 Ref
- `ProtocolManagerWidget::parseEprotoFrames()` — 不改动（后续可独立改为用 `icd_utility` 的 json_parser）

## 风险点

1. **文件系统监听性能** — 大量文件时 `QFileSystemWatcher` 可能有限制（Windows 上无硬性上限但事件量大时可能漏）。当前项目规模下不是问题。
2. **并发修改** — 外部程序删除项目文件时，UI 需要响应。`QFileSystemWatcher` 能覆盖，但编辑器中打开的文件被删除需要额外处理（EditorManager 已有 `onFileDeleted` 机制）。
3. **ReportRef 的 generateTime** — 完全去掉后，报告的时间信息只能从文件 mtime 或文件内容获取。当前 Report 功能未实现，暂不处理。
4. **目录不存在时 watcher 未注册** — 项目打开时若标准子目录不存在，watcher 不会监听。首次创建文件后通过保留的 `refreshList()` 调用刷新（见 2.2/2.3）。目录删除后重建的场景通过 `refreshCategory()` 中新增的 watch 重新注册逻辑处理（见 2.5）。
