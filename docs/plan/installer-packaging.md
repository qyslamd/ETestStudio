# Windows 安装包打包方案

> 参考 `D:\sb\eph213` 的打包流程，**去粗取精**后重新设计

---

## 1. 设计原则（吸取 EPH213 教训）

| 不良设计（EPH213） | 我们的方案 |
|---|---|
| ISCC 放在 POST_BUILD，每次编译都打包 | **独立 custom target `make_package`**，按需调用 |
| windeployqt 放在 POST_BUILD，每次链接都部署 | **独立 custom target `make_deploy`**，部署与编译解耦 |
| 输出写入 `CMAKE_SOURCE_DIR/..`，污染源码树 | 输出到项目根下的 `dist/` |
| CMake 版本管理自动递增 patch，多人冲突 | **保留 `version.txt`**，patch 手动维护（或 git tag 驱动）|
| file(GLOB_RECURSE) 收集 DLL | **无需收集**——所有第三方库均静态链接，无外部 runtime DLL |
| 用 cmake install 过渡 | **跳过 install**——windeployqt 直接作用在构建输出目录，ISCC 直接打包同一目录 |

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

## 3. 实施步骤

### 3.1 自动解压 Inno Setup 6（已配置）

已添加到 `cmake/scripts/extract_dependencies.cmake`，CMake configure 时自动解压到 `3rdparty/Inno Setup 6/`。

### 3.2 查找 ISCC 和 windeployqt

放在 `src/app/CMakeLists.txt` 开头（仅在 app target 内使用，不污染顶层）：

> **为什么不在顶层**：顶层已负责项目配置 + 所有第三方库编译（279 行），不应再塞应用层细节。

```cmake
# find windeployqt — 从 Qt5::qmake 目标推导 bin 目录
get_target_property(_qt_qmake Qt5::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${_qt_qmake}" DIRECTORY)

find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt
    HINTS "${QT_BIN_DIR}"
    NO_DEFAULT_PATH          # 避免匹配到系统其它 Qt 版本
)
if(NOT EXISTS ${WINDEPLOYQT_EXECUTABLE})
    message(WARNING "windeployqt.exe not found; Qt DLLs will not be auto-deployed")
endif()

# find ISCC (Inno Setup Compiler)
find_program(ISCC_EXECUTABLE
    NAMES ISCC
    HINTS "${CMAKE_SOURCE_DIR}/3rdparty/Inno Setup 6"
)
if(NOT EXISTS ${ISCC_EXECUTABLE})
    message(WARNING "ISCC.exe not found; installer package target unavailable")
endif()
```

两点说明：
- **不设 REQUIRED**，找不到只 warning 不影响编译
- 查找路径依赖 `3rdparty/Inno Setup 6/` 已解压（解压由 `extract_dependencies.cmake` 自动处理）

### 3.3 部署 target — `make_deploy`

不塞 POST_BUILD。创建一个 `make_deploy` target，按需触发 windeployqt：

```cmake
if(EXISTS ${WINDEPLOYQT_EXECUTABLE})
    add_custom_target(make_deploy
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --verbose 0
            --no-angle
            --no-opengl-sw
            --no-system-d3d-compiler
            --compiler-runtime
            "$<TARGET_FILE:${TARGET_NAME}>"
        COMMENT "Running windeployqt..."
    )
    add_dependencies(make_deploy ${TARGET_NAME})
endif()
```

`--compiler-runtime` 自动部署 VC 运行时 DLL（`msvcp140.dll`、`vcruntime140.dll`、`vcruntime140_1.dll`、`concrt140.dll`），无需额外收集逻辑。

### 3.4 打包 target — `make_package`

```cmake
if(EXISTS ${ISCC_EXECUTABLE})
    set(PACKAGE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/dist")
    add_custom_target(make_package
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_OUTPUT_DIR}"
        COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --verbose 0
            --no-angle
            --no-opengl-sw
            --no-system-d3d-compiler
            --compiler-runtime
            "$<TARGET_FILE:${TARGET_NAME}>"
        COMMAND ${ISCC_EXECUTABLE}
            "/Qp"
            "/DWhereAreFiles=$<TARGET_FILE_DIR:${TARGET_NAME}>"
            "/DWhereToOutput=${PACKAGE_OUTPUT_DIR}"
            "/DMyAppVersion=${APP_VERSION}"
            "/DMyAppExeName=${TARGET_NAME}.exe"
            "/DMySetupIcon=${CMAKE_CURRENT_SOURCE_DIR}/icon.ico"
            "/DMyProductName=${PROJECT_NAME}"
            ${CMAKE_CURRENT_SOURCE_DIR}/make_install_package.iss
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TARGET_NAME}>"
        COMMENT "Building installer package with Inno Setup..."
    )
    add_dependencies(make_package ${TARGET_NAME})
endif()
```

`make_package` 内部自己先调 windeployqt 再调 ISCC，不需要前置 `make_deploy`。推荐用 `RelWithDebInfo` 配置构建（既优化体积，又保留 PDB 符号用于崩溃分析）。

使用方式：

```bash
# 仅编译（日常开发）
scripts\build_ninja.bat -t debug -m ETestStudio
# 运行靠 run_app.bat，Qt DLL 由 PATH 环境变量提供

# 编译 + 部署 + 打包安装包（推荐 RelWithDebInfo）
scripts\build_ninja.bat -t relwithdebinfo -m ETestStudio -p

# 也可用 Release（但无 PDB，崩溃日志只有原始地址）
scripts\build_ninja.bat -t release -m ETestStudio -p
```

### 3.5 创建 ISS 脚本

位置：`src/app/make_install_package.iss`

关键字段：

| ISS 宏 | 值 |
|---|---|
| `MyProductName` | `ETestStudio` |
| `MyAppName` | `ETestStudio 自动化测试系统` |
| `AppId` | 生成新的 GUID（`{{GUID}`） |
| `MyAppVersion` | 由 CMake `/DMyAppVersion` 传入 |
| `MySetupIcon` | 由 CMake `/DMySetupIcon` 传入 |

核心文件规则（全部从构建输出目录递归打包，排除 PDB）：

```iss
[Files]
Source: "{#WhereAreFiles}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
    Excludes: "*.pdb"
```

关键 ISS 选项优化（对比 EPH213）：

| 选项 | EPH213 | 我们的值 | 原因 |
|---|---|---|---|
| `PrivilegesRequired` | `admin` | `lowest` | 不装驱动/VC redist，无需提权 |
| `ArchitecturesInstallIn64BitMode` | 无 | `x64compatible` | 明确 32 位系统不兼容 |
| `[Run] vc_redist` | 有 | 移除 | 走 `--compiler-runtime` 带 DLL |
| 安装向导皮肤 | 默认 | 默认（不换肤） | 开发测试工具，面向工程师，默认 Modern 足够干净；botva2 换肤只增加维护负担 |

加强项：AppMutex + 卸载清理（来自此前 DeepBlue 浏览器 ISS 的成熟模式）

**AppMutex：代替 CloseApplications=force**

用 ISS 内置的 `AppMutex` + 自定义弹窗替代暴力 `CloseApplications=force`，更友好：

```iss
#define MyAppMutex "ETestStudioAppMutex"
#define MySetupMutex "ETestStudioSetupMutex"

[Setup]
AppMutex={#MyAppMutex}
SetupMutex={#MySetupMutex}

[Code]
function InitializeSetup(): Boolean;
var
  ErrorCode: Integer;
begin
  if CheckForMutexes('{#MyAppMutex}') then
  begin
    if MsgBox('检测到 ETestStudio 正在运行，是否关闭后继续安装？',
              mbConfirmation, MB_YESNO) = IDYES then
      begin
        ShellExec('open', ExpandConstant('{cmd}'),
          '/c taskkill /f /t /im ETestStudio.exe', '',
          SW_HIDE, ewNoWait, ErrorCode);
        Result := True;
      end
    else
      Result := False;
  end
  else
    Result := True;
end;
```

**卸载清理：删除用户数据**

卸载时清理 `%LOCALAPPDATA%/ETestStudio/` 下的缓存、日志和配置：

```iss
const
  CSIDL_LOCAL_APPDATA = $001c;

procedure DelConfigAndCache();
var
  LocalAppData: String;
begin
  LocalAppData := GetShellFolderByCSIDL(CSIDL_LOCAL_APPDATA, True);
  if LocalAppData <> '' then
    DelTree(LocalAppData + '\ETestStudio', True, True, True);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    DelConfigAndCache();
end;
```

### 3.6 PDB 与崩溃报告

> 项目已有 `WindowsCrashHandler`，发版推荐用 `RelWithDebInfo` 以保留 PDB。

**PDB 的处理策略**：

```
cmake --build build\ninja-relwithdebinfo (产生 .pdb)
    │
    ├── 开发者存档 .pdb → 符号服务器
    │    （手动复制或将来 CI 自动 symstore add）
    │
    └── cmake --build . --target make_package
        │
        ├── windeployqt (部署 Qt DLL，不部署 PDB)
        └── ISCC (Excludes: "*.pdb") → setup.exe
```

| 决策 | 原因 |
|---|---|
| **PDB 不打包进安装包** | Qt5Core.pdb 等一个就上百 MB，不值得发给用户 |
| **PDB 存档到符号服务器** | 收到用户 .dmp 后取匹配 PDB 用 WinDbg 分析 |
| **ISS Excludes `*.pdb`** | 避免 RelWithDebInfo 构建目录中的 PDB 被误打包 |
| 崩溃时 `.log` 无符号 | 有原始地址 + `.dmp` 足够定位大多数崩溃 |

发版操作流程：
```bash
# 1. 构建 RelWithDebInfo
scripts\build_ninja.bat -t relwithdebinfo -m ETestStudio

# 2. 存档 PDB（先备份，再打包）
copy build\ninja-relwithdebinfo\bin\*.pdb \\symbols\eteststudio\

# 3. 打包安装包（ISS 已排除 *.pdb）
scripts\build_ninja.bat -t relwithdebinfo -m ETestStudio -p
```

### 3.7 构建脚本支持

`scripts/build_ninja.bat` 支持新旧两种参数模式：

```bat
:: 旧模式（位置参数）
scripts\build_ninja.bat debug
scripts\build_ninja.bat relwithdebinfo package

:: 新模式（显式参数，推荐）
scripts\build_ninja.bat -t debug -m ETestStudio
scripts\build_ninja.bat -t relwithdebinfo -m ETestStudio -p
```

---

## 4. 与 EPH213 对比总结

| 方面 | EPH213 | ETestStudio | 理由 |
|---|---|---|---|---|
| windeployqt 时机 | POST_BUILD | 独立 target `make_deploy` / 内嵌于 `make_package` | 避免每次链接都部署 |
| ISCC 时机 | POST_BUILD | 独立 target `make_package` | 避免每次编译都打包 |
| 输出位置 | `..`（源码树外一层） | `dist/`（项目根下） | 更可预期，与 CI 集成更好 |
| 第三方 DLL | file(GLOB_RECURSE) 全量收集 | 无需收集（全静态链接） | 架构更干净 |
| VC 运行时 | 手动 `file(GLOB_RECURSE VCToolsRedistDir)` | `windeployqt --compiler-runtime` | 更简洁 |
| CLOC 集成 | POST_BUILD | 不做 | 开瓶器功能，不值得集成 |

---

## 5. 涉及文件清单

| 文件 | 改动类型 |
|---|---|
| `cmake/scripts/extract_dependencies.cmake` | **已改**（添加 `.7z` 支持 + Inno Setup 条目） |
| `src/app/make_install_package.iss` | **新建** |
| `src/app/CMakeLists.txt` | 追加 `find_program` + `make_deploy` + `make_package` targets |
| `scripts/build_ninja.bat` | 已改：支持 `-t/-m/-d/-p` 显式参数 |
| `dist/` | 新建目录，用于存放生成的 setup.exe |

## 6. 风险

| # | 风险 | 缓解 |
|---|---|---|
| 1 | windeployqt 找不到 Qt 路径 | `Qt5_DIR` CMake 缓存变量推导，构建前确认 `ETest_Qt5_Path` 环境变量已设；日常开发走 `run_app.bat` 不受影响 |
| 2 | ISCC.exe 找不到 | 确认 `7z` 在 PATH 中（CMake 首次 configure 会自动解压） |
| 3 | 打包体积偏大 | windeployqt 已有 `--no-*` 参数剪裁；`Excludes: "*.pdb"` 避免 PDB 误入 |
| 4 | 安装后运行时缺 DLL | 用 Dependency Walker 或 Procmon 检查；缺啥在 ISS 中补充对应的 Source 条目 |

---

## 7. 未来可选方案（闲了再做）

当前方案（ISS 原生 Modern 向导）已经够用。以下两种更高阶的方案来自此前其他项目的实践经验，留作以后产品化阶段备选。

### 7.1 botva2 换肤

借助 `botva2.dll` + `InnoCallback.dll` 在 ISS 中对安装界面做 GDI+ 自绘：

| 能力 | 说明 |
|---|---|
| 圆角窗口、自定义标题栏 | `CreateRoundRectRgn` + `SetWindowRgn` 实现 |
| PNG 按钮、复选框、背景图 | GDI+ 图像加载，按钮事件通过回调绑定 |
| 自定义进度条 | 劫持 `ProgressGauge` 的窗口过程，绘制个性样式 |
| 彻底替换默认向导 | `InnerNotebook.Hide()` 隐藏所有标准页面，完全自绘 |

**为什么不现在做**：ETestStudio 是开发测试工具，用户是工程师，默认 Modern 向导干净专业，不值得为皮肤投入维护成本。且 `botva2.dll` 是第三方非官方 DLL，有兼容性风险。

**什么时候值得做**：如果你哪天想把 ETestStudio 做成像 VSCode 或者 Qt Creator 那种品牌化安装体验，可以上。参考 `D:\workspace\cef\workspace\install-package\一键安装+拷贝配置文件示例\oneKeySetupWin7.iss`。

### 7.2 Qt 安装器（ISS 只解压，UI 交给 Qt）

思路：Inno Setup 的 `[Files]` 照常打包全部文件，但安装界面的许可协议、目录选择、进度条等全部由我们自己写的一个 Qt 小程序来展示。

```
setup.exe（ISS 打包，无 UI 或最小 UI）
  │
  ├── 静默释放 → {tmp}
  │     ├── Qt 运行时 DLL
  │     ├── ETestStudio.exe
  │     └── Installer.exe（Qt 安装器）
  │
  └── [Run] 启动 Installer.exe
        │
        ├── 显示许可协议（QTextBrowser）
        ├── 安装目录选择（QFileDialog / QLineEdit + Browse）
        ├── 执行安装：复制文件到 {app}
        ├── 创建快捷方式（IShellLink / QSettings 写入 Uninstall 注册表）
        ├── 启动 ETestStudio.exe 或勾选"完成时启动"
        └── 卸载仍靠 ISS（AppId 写入的注册表项）
```

**优势**：

| 方面 | ISS 原生 | Qt 安装器 |
|---|---|---|
| UI 自由度 | ISS Pascal 有限 | C++/Qt 全栈，任意控件、动画、换肤 |
| 品牌一致性 | 向导风格可能与应用割裂 | 安装器 = 主应用风格的子集 |
| 调试体验 | 难以调试 | 桌面程序常规断点、热重载 |
| 可测试性 | 无单元测试 | 可拆出 QInstallLogic 做逻辑测试 |
| 复杂业务 | 难维护 | 可以引入埋点、在线许可验证、升级检测 |

**劣势**：

| 方面 | 分析 |
|---|---|
| 多一个构建目标 | 需要新建 `Installer` 子项目，CMakeLists.txt + 源码 |
| 边际体积 | 需要打包 Qt DLL，但随主应用一起发布，边际成本很低 |
| 卸载 | 仍需 ISS 保留。卸载是注册表驱动的，ISS 的 `[UninstallRun]` 不会被替代 |
| 核心矛盾 | 投入时间和提前加载 Qt 的故障风险 vs. 换来 UI 自由，是否划算 |

**为什么不现在做**：当前阶段 ISS 原生够用。Qt 安装器适合用户量级大、安装体验作为品牌触点来打磨的产品。ETestStudio 目前不需要。

**什么时候值得做**：当你有以下需求之一时——① 安装过程中需要联网验证许可；② 安装器需要多步配置（选组件、选数据目录、选协议驱动）；③ 需要无缝的在线升级体验（下载增量包 → Installer 处理替换）。

---

## 8. 选型参考：Inno Setup vs NSIS

既然项目已有 Inno Setup 6 在 `3rdparty/` 中，为什么可能考虑 NSIS？以下从实际工程角度对比。

### 8.1 一句话总结

| 工具 | 定位 |
|---|---|
| **Inno Setup** | **开箱即用，功能完整**。装好就能打出专业安装包，不需要额外插件 |
| **NSIS** | **高度可定制，体积极小**。但开箱是个毛坯房，要装修成成品需要自己搭 |

### 8.2 详细对比

| 方面 | Inno Setup | NSIS |
|---|---|---|
| **脚本语言** | Pascal（Delphi 方言），有 `begin/end`、`function`、`procedure` | 类 C，基于栈的脚本语言，语法诡异（`Push/Pop`、`StrCmp`） |
| **学习曲线** | 低。Pascal 可读性强，官方文档完善 | 高。语法反直觉，调试困难，文档偏简略 |
| **默认 UI** | Modern 风格，专业美观，零配置 | 经典老式 Windows 95 风格。要好看需要 `Modern UI 2` 插件 |
| **安装程序体积** | Stub ~300KB | Stub ~34KB。NSIS 极致的小是它的核心卖点 |
| **压缩算法** | 内置 LZMA（7z 同款）、bzip2、zip | 内置 zlib、bzip2、LZMA（需插件 `NsisLZMA`） |
| **插件生态** | 有但较少（botva2、InnoCallback 等） | 丰富。FileWrite、ShellExec、nsProcess、NSISdl 等大量社区插件 |
| **Unicode 支持** | 从 v5（2008）起原生 | 从 v3（2017）起原生，v2 时代是痛点 |
| **64 位支持** | 好。`{commonpf64}`、`ArchitecturesInstallIn64BitMode` 等 | 好。但需要手动处理 `$PROGRAMFILES64` |
| **Pascal 代码能力** | 强。完整 Pascal 脚本环境，`[Code]` 段可写复杂逻辑 | 弱。语言本身受限，复杂逻辑靠 Plugin DLL |
| **中文社区** | 一般 | 相对更多（早年许多国内汉化组用 NSIS） |
| **维护状态** | 活跃。Martijn Laan 持续维护（2024 仍有更新） | 维护缓慢。v3 发布后更新频率低，核心团队几乎停滞 |
| **典型用户** | TortoiseSVN、Notepad++、Git for Windows | Spotify（早期）、Chrome（早期）、国内汉化安装包 |

### 8.3 对 ETestStudio 的结论

| 角度 | 谁赢 | 理由 |
|---|---|---|
| 学习成本 | **ISS ✓** | Team 里 ISS 经验积累厚（深蓝浏览器 + EPH213），NSIS 要重新踩坑 |
| 开箱体验 | **ISS ✓** | ISS 装好就是 Modern 专业向导，NSIS 默认 UI 没法直接见人 |
| 安装包体积 | **NSIS ✓** | ISS stub 大一个数量级。但对 ETestStudio（整体 >100MB 的应用），~250KB 的差异可以忽略 |
| 自定义灵活度 | NSIS（理论） / **ISS（实际） ✓** | NSIS 插件多但质量参差；ISS 靠 Pascal 脚本能自己搞定大部分需求，botva2 换肤或 Qt 安装器也都在 ISS 上验证过 |
| 长期维护 | **ISS ✓** | NSIS 核心团队几乎停滞，ISS 仍在更新 |

**结论：无脑选 Inno Setup，不用引入 NSIS。**

唯一值得讨论 NSIS 的场景是——**你需要做一个极小的更新程序（几百 KB）**，比如仅包含 `lzma` 解压 + 文件覆盖的轻量升级器。NSIS 34KB 的 stub 在这里有价值。但 ETestStudio 的 Qt 安装器方案本身也能覆盖这个场景，且 Qt 安装器可以实现更丰富的升级逻辑（增量包校验、回滚等）。

**不列入计划**：NSIS 不会作为 ETestStudio 的安装工具引入。当前已确定的 ISS + 未来可选的 Qt 安装器已经覆盖了所有需要。
