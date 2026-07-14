# 目录结构调整计划

## 概述

两项调整：
1. `src/core/plugin/` → `src/core/plugin_sdk/` — 明确框架层标识
2. `tools/` → `src/tools/` — 附属工具统一归入源码目录，简化构建

两项均为纯目录/文件路径变更，不涉及运行时行为、API、数据结构改动。

**执行顺序：先做调整①，构建验证通过后，再做调整②。** 详见验证步骤。

---

## 调整①：`src/core/plugin/` → `src/core/plugin_sdk/`

### 理由

`plugin_sdk` 准确传达"这是插件开发套件"的含义，与 `src/plugins/`（插件实现）形成语义对照，消除命名混淆。

### 改动清单

#### 1.1 目录重命名

```
git mv src/core/plugin src/core/plugin_sdk
```

#### 1.2 `src/core/CMakeLists.txt`

SOURCES 和 HEADERS 中所有 `plugin/` 前缀 → `plugin_sdk/`：

```cmake
# SOURCES
plugin_sdk/PluginManager.cpp

# HEADERS
plugin_sdk/IPlugin.h
plugin_sdk/IDevicePlugin.h
plugin_sdk/IADevicePlugin.h
plugin_sdk/IDADevicePlugin.h
plugin_sdk/ISerialDevicePlugin.h
plugin_sdk/IArinc429Plugin.h
plugin_sdk/ICANPlugin.h
plugin_sdk/PluginMetaData.h
plugin_sdk/PluginManager.h
```

`target_include_directories` PUBLIC 路径：

```cmake
${CMAKE_CURRENT_SOURCE_DIR}/plugin_sdk
```

#### 1.3 `#include "plugin/..."` 全局替换

所有源文件中的 `#include "plugin/..."` → `#include "plugin_sdk/..."`：

| # | 文件 | include |
|---|------|---------|
| 1 | `src/app/MainWindow.cpp:76` | `PluginManager.h` |
| 2 | `src/app/ProjectStructureWidget.h:11-12` | `IDevicePlugin.h`, `PluginManager.h` |
| 3 | `src/topology/TopologyEditorWidget.cpp:57` | `PluginManager.h` |
| 4 | `src/topology/DevicePaletteWidget.cpp:11` | `PluginManager.h` |
| 5 | `src/engine/HardwareManager.cpp:11-17` | `IADevicePlugin.h`, `IArinc429Plugin.h`, `ICANPlugin.h`, `IDADevicePlugin.h`, `IDevicePlugin.h`, `ISerialDevicePlugin.h`, `PluginManager.h` |
| 6 | `tests/core/plugin_manager_test.cpp:5-7` | `IPlugin.h`, `PluginManager.h`, `PluginMetaData.h` |
| 7 | `tests/engine/hardware_manager_test.cpp:14-20` | `IADevicePlugin.h`, `IArinc429Plugin.h`, `ICANPlugin.h`, `IDADevicePlugin.h`, `IDevicePlugin.h`, `ISerialDevicePlugin.h`, `PluginManager.h` |
| 8 | `tests/engine/step_runner_test.cpp:16` | `ICANPlugin.h` |
| 9 | `examples/plugins/hello_plugin/HelloPlugin.h:5` | `IPlugin.h` |

> 注意：mock 插件（`src/plugins/mock/*/`）的源文件使用无前缀 `#include "IADevicePlugin.h"`（依赖 CMake include 路径），不需要改动源文件，只需更新 CMake 路径（见 1.4）。

#### 1.4 各插件 CMake PRIVATE include 路径

所有 `${CMAKE_SOURCE_DIR}/src/core/plugin` → `${CMAKE_SOURCE_DIR}/src/core/plugin_sdk`：

| # | 文件 |
|---|------|
| 1 | `src/plugins/mock/ad/CMakeLists.txt` |
| 2 | `src/plugins/mock/da/CMakeLists.txt` |
| 3 | `src/plugins/mock/serial/CMakeLists.txt` |
| 4 | `src/plugins/mock/can/CMakeLists.txt` |
| 5 | `src/plugins/mock/a429/CMakeLists.txt` |
| 6 | `examples/plugins/hello_plugin/CMakeLists.txt` |

#### 1.5 Namespace 保留

namespace `etest::core::plugin` **不变**。文件路径和 C++ namespace 之间没有强制绑定关系，无需改动用户代码。

---

## 调整②：`tools/` → `src/tools/`

### 理由

- 附属工具随主程序一起发布，ribbon 上已有入口
- 迁入 `src/` 后依赖路径简化，去掉 `if(NOT TARGET ...)` 自举守卫
- 所有项目源码统一在 `src/` 下，降低认知负担

### 改动清单

#### 2.1 目录迁移

```
git mv tools src/tools
```

#### 2.2 新建 `src/tools/CMakeLists.txt`

全新创建，从顶层 `CMakeLists.txt` 搬运条件构建逻辑：

```cmake
# src/tools/CMakeLists.txt
option(BUILD_ETEST_TOOLS "Build standalone product tools" ON)

if(BUILD_ETEST_TOOLS)
    add_subdirectory(topology-editor)
    add_subdirectory(protocol-editor)
    add_subdirectory(test-program-editor)
    add_subdirectory(test-executor)
    add_subdirectory(test-executor-cli)
endif()
```

#### 2.3 顶层 `CMakeLists.txt`

替换原有 5 行 `add_subdirectory(tools/...)` 为：

```cmake
option(BUILD_ETEST_TOOLS "Build standalone product tools" ON)
if(BUILD_ETEST_TOOLS)
    add_subdirectory(src/tools)
endif()
```

保持在原有位置（第三方依赖加载之后），不移动。

#### 2.4 各工具 CMakeLists.txt — 移除自举守卫

工具迁入 `src/tools/` 后，依赖由 `src/CMakeLists.txt` 的顺序加载保证（见下方注），不再需要 `if(NOT TARGET ...)` 自举守卫。

以下 5 个文件中对应的守卫区块全部移除，保留 `target_link_libraries` 和 `target_include_directories` 不变：

| # | 文件 | 移除的守卫 |
|---|------|-----------|
| 1 | `src/tools/topology-editor/CMakeLists.txt` | `etest_ui`、`etest_topology` |
| 2 | `src/tools/protocol-editor/CMakeLists.txt` | `etest_ui`、`etest_protocol` |
| 3 | `src/tools/test-program-editor/CMakeLists.txt` | `etest_core`、`etest_ui`、`etest_program` |
| 4 | `src/tools/test-executor/CMakeLists.txt` | `etest_engine`、`etest_core`、`etest_ui` |
| 5 | `src/tools/test-executor-cli/CMakeLists.txt` | `etest_engine`、`etest_core` |

> `src/CMakeLists.txt` 的 `add_subdirectory` 顺序保证依赖在前：
> `core` → `core_ui` → `libui` → `api` → `topology` → `protocol` → `icd_utility` → `engine` → `test_program` → `app` → `plugins` → `tools`
>
> 因此 tools 的 CMakeLists 中所有依赖目标在 `add_subdirectory(tools)` 时已加载完毕，无需手动 `add_subdirectory`。

#### 2.5 `src/tools/test-executor-cli/main.cpp` include 更新

该文件同时受两项调整影响。调整②完成后其路径变为 `src/tools/test-executor-cli/main.cpp`，其中的 include 路径 `"core/plugin/PluginManager.h"` 需同步更新为 `"core/plugin_sdk/PluginManager.h"`（调整①已完成）。

---

## 文档更新

#### 3.1 `CLAUDE.md`

第 78 行附近，更新路径和章节标题以匹配"附属工具"的定位：

```markdown
## 附属工具约束

`src/tools/topology-editor`、`src/tools/protocol-editor`、`src/tools/test-program-editor` 是三个附属工具，
各自输出独自可用的文件格式（`.etopo` / `.eproto` / `.tcase`），
集成在主程序 ribbon 中随主程序发布。通过 `BUILD_ETEST_TOOLS` CMake 选项控制编译。
```

#### 3.2 `docs/规划/硬件接入与测试执行引擎实现计划.md`

搜索替换以下模式：

| 旧路径 | 新路径 |
|--------|--------|
| `tools/test-executor/` | `src/tools/test-executor/` |
| `tools/test-executor-cli/` | `src/tools/test-executor-cli/` |
| `tools/CMakeLists.txt` | `src/tools/CMakeLists.txt` |
| `"core/plugin/` | `"core/plugin_sdk/`（第 683-688 行 6 处 include 引用） |

#### 3.3 `docs/thinking/2026-07-08-硬件接入与测试执行引擎设计.md`

- `add_subdirectory(tools/topology-editor)` → `add_subdirectory(src/tools/topology-editor)`（共 4 处）
- 目录树部分 `tools/` 缩进改为在 `src/` 下
- 第 1226 行 `plugin/` 目录树引用 → `plugin_sdk/`

#### 3.4 `docs/plan/Qt剥离计划Phase1.md`

4 处 `tools/*` → `src/tools/*`

#### 3.5 `docs/plan/TestProgramEditorWidget布局重构.md`

2 处 `tools/test-program-editor` → `src/tools/test-program-editor`

#### 3.6 `docs/plan/测试执行功能重新设计决策记录.md`

2 处 `tools/test-executor` → `src/tools/test-executor`

#### 3.7 `docs/plan/project-hardware-redesign.md`

| 行号 | 旧内容 | 新内容 |
|------|--------|--------|
| 80 | `src/core/plugin/PluginMetaData.h` | `src/core/plugin_sdk/PluginMetaData.h` |
| 156 | `#include "plugin/PluginManager.h"` | `#include "plugin_sdk/PluginManager.h"` |
| 389-390 | `"plugin/PluginManager.h"`、`"plugin/IDevicePlugin.h"` | `"plugin_sdk/..."` |
| 508-509 | `src/core/plugin/PluginMetaData.h`、`src/core/plugin/PluginManager.cpp` | `src/core/plugin_sdk/...` |

#### 3.8 `docs/plan/MockUUTSimulator设计.md`

| 行号 | 旧内容 | 新内容 |
|------|--------|--------|
| 1101-1102 | `src/core/plugin/PluginMetaData.h`、`src/core/plugin/PluginManager.h/.cpp` | `src/core/plugin_sdk/...` |

#### 3.9 `docs/others/分析思路记录.md`

第 25 行：`src/core/plugin/` → `src/core/plugin_sdk/`

#### 3.10 `docs/03-开发/AD插件接口设计.md`

第 358 行：`src/core/plugin/IADevicePlugin.h` → `src/core/plugin_sdk/IADevicePlugin.h`

#### 3.11 `docs/03-开发/1.3-通用插件框架开发内容.md`

4 处 `src/core/plugin/` → `src/core/plugin_sdk/`（第 13、14、26、124 行）

---

## 验证步骤

**分段验证，缩小问题排查范围：**

### Phase 1：调整① plugin_sdk

1. 执行 1.1 ~ 1.4 的改动
2. **全量编译**：`scripts/build_ninja.bat`
3. 确认所有 targets 编译通过，topic 失败项
4. **提交**

### Phase 2：调整② tools + 文档

5. 执行 2.1 ~ 2.5 + 文档更新章节的全部改动
6. **全量编译**：`scripts/build_ninja.bat`
7. 确认所有 targets 编译通过
8. **增量确认**：手动启动主程序和几个工具，确认 ribbon 入口和工具有效

### Phase 3：最终

9. **文档检查**：确认所有文档中旧路径已更新
10. **提交**

---

## 影响范围汇总

| 项目 | 文件数 | 说明 |
|------|--------|------|
| `plugin_sdk` — 源文件 include | ~9 | `src/`、`tests/`、`examples/` 下的源文件 |
| `plugin_sdk` — CMake 路径 | ~7 | `src/core/` + mock 插件 ×5 + `examples/hello_plugin` |
| `tools` 迁入 `src/tools/` | ~10 | 目录 mv + CMake 守卫移除 + `main.cpp` include 修复 |
| 文档更新 | ~10 | CLAUDE.md + 9 个文档文件路径引用同步 |
| **总计** | **~36** | 纯构建/文档配置变更，无运行时逻辑改动 |
