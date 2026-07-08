# Windows 安装包打包方案

> 当前状态：已实现。本文档记录架构设计与关键决策，供后续维护参考。

---

## 1. 设计原则

| 不良设计（历史教训） | 当前方案 |
|---|---|
| ISCC 放在 POST_BUILD，每次编译都打包 | **独立 custom target `make_package`**，按需调用 |
| windeployqt 放在 POST_BUILD，每次链接都部署 | **独立 custom target `make_deploy`**，部署与编译解耦 |
| 输出写入源码树外层 | 输出到项目根下的 `dist/` |
| CMake 版本管理自动递增 patch | **保留 `version.txt`**，patch 手动维护（或 git tag 驱动）|
| file(GLOB_RECURSE) 收集 DLL | **无需收集**——所有第三方库均静态链接，无外部 runtime DLL |
| 用 cmake install 过渡 | **跳过 install**——windeployqt 直接作用在构建输出目录，ISCC 直接打包同一目录 |
| ISS 脚本混在 src/app/ 中 | **统一放 `scripts/`**，与 CMakeLists.txt 解耦 |
| 卸载时无条件删除用户数据 | **CreateCustomForm + TCheckBox**，由用户勾选决定 |

---

## 2. 总体流程

```
▶ 日常开发
cmake --build . (Debug)  → 纯编译，exe 依赖 run_app.bat 找 Qt DLL（PATH 环境变量）

▶ 需要自包含部署
cmake --build . --target make_deploy  → 编译 → windeployqt → 构建目录自包含

▶ 发版（推荐 RelWithDebInfo）
cmake --build . --target make_package  → 编译 → windeployqt → ISCC → dist/setup.exe
```

三层解耦：**编译**是编译，**部署**是部署，**打包**是打包，各自独立按需调用。

---

## 3. CMake 构建架构

### 3.1 模板文件体系

打包脚本采用 **CMake `configure_file(@ONLY)`** 模板生成模式，一份模板可同时产出 CLI 脚本和 IDE 可编辑脚本：

```
scripts/
├── make_install_package.iss        ← 保留（旧版本参考，不用于构建）
├── make_install_package.iss.in     ← CMake @ONLY 模板（单文件，无 ISPP 预处理器依赖）
└── make_installer.bat.in           ← CLI/CI 打包脚本模板

build/ninja-<type>/scripts/
├── make_install_package.iss        ← configure_file(.iss.in → 完全解析版，可被 Compile32.exe 打开直接编译)
└── make_installer.bat              ← configure_file(.bat.in → CLI 脚本，接受 binary_dir 参数)
```

#### `.iss.in` 与 `.iss` 核心差异

| 特性 | `.iss.in`（源模板） | 生成的 `.iss`（构建目录） |
|---|---|---|
| CMake `@ONLY` 占位符 | 有，`@WhereAreFiles@` 等 | 全部替换为具体路径 |
| ISPP `#ifndef`/`#define`/`#if` | 无 | 无 |
| ISPP `#ifexist` | 有（ISPP 运行时特性） | 保留（Compile32 正常处理） |
| 打开即编译 | 否（缺宏定义） | **是** |

#### `.bat.in` 参数说明

```bat
:: 用法
make_installer.bat <binary_dir>

:: 示例
make_installer.bat D:\sbb\EpTestStudio\build\ninja-release-x64\bin
```

- `<binary_dir>` 为构建时求值的生成器表达式 `$<TARGET_FILE_DIR:...>`，通过 `add_custom_target` 传入
- 脚本内部调 `windeployqt` → `ISCC`，顺序执行，任一失败则中止

### 3.2 架构变量计算

在 `src/app/CMakeLists.txt` 中，根据 `CMAKE_SIZEOF_VOID_P` 计算架构相关变量：

| CMake 变量 | x64 值 | x86 值 | 用途 |
|---|---|---|---|
| `ISCC_APP_ID` | `{{6811a736-4ffd-4dae-a7a7-459a6e3d572f}` | `{{46AB6095-36F9-49F4-ABB1-6A36D8BB7E39}` | `[Setup] AppId` |
| `ISCC_APP_PF` | `{autopf64}` | `{autopf}` | 安装目录前缀 |
| `ISCC_VC_REDIST` | `vc_redist.x64.exe` | `vc_redist.x86.exe` | VC 运行时分发 |
| `ISCC_APP_ARCH_SUFFIX` | `x64` | `x86` | 输出文件名后缀 |

两个架构的 AppId 不同，允许在同一台机器上 x64 和 x86 共存互不干扰。

### 3.3 两个 `configure_file`

```cmake
configure_file(
    "${CMAKE_SOURCE_DIR}/scripts/make_install_package.iss.in"
    "${CMAKE_BINARY_DIR}/scripts/make_install_package.iss"
    @ONLY
)

configure_file(
    "${CMAKE_SOURCE_DIR}/scripts/make_installer.bat.in"
    "${CMAKE_BINARY_DIR}/scripts/make_installer.bat"
    @ONLY
)
```

### 3.4 `add_custom_target(make_package)`

```cmake
add_custom_target(make_package
    COMMAND "${CMAKE_BINARY_DIR}/scripts/make_installer.bat"
        "$<TARGET_FILE_DIR:${TARGET_NAME}>"
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>"
    COMMENT "Building installer package with Inno Setup (${APP_ARCH})..."
)
add_dependencies(make_package ${TARGET_NAME})
```

### 3.5 使用方式

```bash
# ninja 一键打包
ninja make_package

# CLI 脚本手动调用（不经过 ninja）
scripts\make_installer.bat build\ninja-release-x64\bin

# Compile32.exe IDE
# 打开 build\ninja-release-x64\scripts\make_install_package.iss → Ctrl+F9
```

---

## 4. ISS 脚本清单与关键决策

### 4.1 ISS 模板配置映射

| 字段 | 源 | 值 |
|---|---|---|
| `MyProductName` | 硬编码 | `ETestStudio` |
| `MyAppName` | 硬编码 | `ETestStudio 自动化测试系统` |
| `MyAppVersion` | 模板 `@MyAppVersion@` | CMake `APP_VERSION`（来自 `version_manage.cmake`） |
| `AppId` | 模板 `@MyAppId@` | CMake `ISCC_APP_ID`（架构相关） |
| `MyAppPf` | 模板 `@MyAppPf@` | CMake `ISCC_APP_PF`（架构相关） |
| `MyAppMutex` | 硬编码 | `ETestStudioAppMutex` |
| `MySetupIcon` | 模板 `@MySetupIcon@` | `src/app/resources/icons/app_icon.ico` |

### 4.2 打包文件清单

| 来源 | 文件 | 说明 |
|---|---|---|
| 主程序 | `ETestStudio.exe` | 主应用 |
| 独立产品 | `topology-editor.exe` | 拓扑编辑器 |
| 独立产品 | `protocol-editor.exe` | 协议编辑器 |
| 独立产品 | `test-program-editor.exe` | 测试程序编辑器 |
| windeployqt | `Qt5*.dll` | Qt 运行时 |
| windeployqt | `bearer/`, `iconengines/`, `imageformats/`, `platforms/`, `styles/` | Qt 插件 |
| windeployqt | `translations/*.qm` | Qt 翻译文件 |
| windeployqt | `vc_redist.*.exe` | VC 运行时（`--compiler-runtime` 部署 DLL） |
| 条件打包 | `plugins/*.dll` | 内置模拟设备插件（`#ifexist`） |
| 条件打包 | `test_projects/*` | 测试工程（调试用，`#ifexist`） |

### 4.3 安装 / 卸载互斥

**安装时（`InitializeSetup`）**：

```
CheckForMutexes('ETestStudioAppMutex')
  ├─ 未运行 → 继续安装
  └─ 运行中 → 弹出中文提示"是否关闭后继续安装？"
       ├─ 是 → taskkill /f /t /im ETestStudio.exe → 继续
       └─ 否 → 终止安装
```

**卸载时（`InitializeUninstall`）**：

```
CheckForMutexes('ETestStudioAppMutex')
  ├─ 未运行 → 继续卸载
  └─ 运行中 → 弹出中文提示"是否自动关闭后继续卸载？"
       ├─ 是 → taskkill /f /t /im ETestStudio.exe（等待进程退出）→ 继续
       └─ 否 → 终止卸载

（静默 taskkill 三个独立产品：topology-editor / protocol-editor / test-program-editor，不询问）
```

> `[Setup]` 中不使用 `AppMutex` 指令（避免英文系统提示和代码双重弹框），统一走 Pascal 代码手动检测。

### 4.4 卸载时用户数据删除

通过 `CreateCustomForm` + `TCheckBox` 自绘模态对话框，在卸载进度界面弹出：

```
InitializeUninstallProgressForm
  └─ CreateCustomForm 模态对话框
       ├─ Label: "选择卸载时要清理的额外数据："
       ├─ CheckBox: "删除用户配置和缓存数据（%LOCALAPPDATA%\ETestStudio）"
       └─ 按钮: "卸载" / "取消"
            ├─ 取消 → Abort（终止卸载）
            └─ 确定 → 保存勾选状态到全局变量 DeleteUserData
```

```
CurUninstallStepChanged(usPostUninstall)
  └─ if DeleteUserData = True
       └─ DelTree('{localappdata}\ETestStudio', ...)
```

静默卸载（`/verysilent`）时不弹对话框，`DeleteUserData` 默认为 `False`。

### 4.5 ISS 选项

| 选项 | 值 | 原因 |
|---|---|---|
| `PrivilegesRequired` | `lowest` | 不装驱动/VC redist，无需提权 |
| `ArchitecturesInstallIn64BitMode` | `x64compatible`（仅 x64） | 明确 32 位系统不兼容 |
| `DisableProgramGroupPage` | `yes` | 不询问开始菜单文件夹 |
| `WizardStyle` | `modern` | 开发测试工具，默认足够 |

---

## 5. 涉及文件清单

| 文件 | 角色 | 说明 |
|---|---|---|
| `scripts/make_install_package.iss.in` | **源模板** | CMake `@ONLY` 模板，无 ISPP 预处理器依赖 |
| `scripts/make_installer.bat.in` | **源模板** | CLI/CI 打包脚本 |
| `scripts/make_install_package.iss` | 参考 | 保留旧版本，不用于构建 |
| `src/app/CMakeLists.txt` | **CMake 配置** | `find_program` + 架构变量 + `configure_file` + `make_package` |
| `cmake/version_manage.cmake` | 版本管理 | `APP_VERSION` 由该模块提供 |

---

## 6. 与旧版对比

| 方面 | 旧版（2024-2025） | 当前 |
|---|---|---|
| ISS 位置 | `src/app/make_install_package.iss` | `scripts/make_install_package.iss.in`（模板） |
| ISCC 调用 | 内联在 `add_custom_target` | 通过生成 `.bat` 调用 |
| 独立产品 exe | 无 | `topology-editor` / `protocol-editor` / `test-program-editor` |
| AppMutex | `[Setup]` 指令 + `InitializeSetup` 双重检测 | 仅 `[Code]`，统一中文提示 |
| 卸载用户数据 | 无条件删除 | CreateCustomForm + TCheckBox 可选 |
| 卸载进程检测 | 无 | `InitializeUninstall()` 检测 + 询问 kill |
| Compile32 支持 | 否（缺 `/D` 参数无法独立编译） | **是**（生成完全解析版 .iss） |

---

## 7. 已知问题 / 待办

| # | 问题 | 状态 |
|---|---|---|
| 1 | `taskkill /f /im ETestStudio.exe` 不区分 x64/x86，一杀全杀 | **已确认，暂不处理**（32/64 位程序共存场景罕见） |
| 2 | 卸载时若三个独立产品运行，静默 kill 不带确认 | **当前行为**（产品 exe 无独立 mutex，无法精确检测） |
| 3 | 图标 `EndUpdateResource failed (110)` 偶现 | 瞬态 PE 文件锁，重试即可，非脚本问题 |
