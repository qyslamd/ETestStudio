# Splash 启动屏替代懒加载覆盖层方案

## 问题陈述

当前 ETestStudio 启动分为三个阶段,反馈覆盖情况不均:

| 阶段 | 内容 | 反馈 | release 实测 |
|------|------|------|--------------|
| A. main 重初始化 | `registerEditorTypes` / `ThemeManager::instance()` / `CDockComponentsFactory` | **无** | 约 3.3s 静默 |
| B. MainWindow 构造 | `initUi`(Ribbon/中央堆叠/活动栏/侧边栏/splitter/dock/Welcome) | **无**(窗口未显示) | 包含在上方 |
| C. lazyInit | 活动栏+侧边栏注册/底部面板/EditorManager/插件加载/布局恢复/Tux | LoadingOverlay 脉冲 | 核心 814ms + Tux 453ms |

release(x86)日志片段:

```
32.242 崩溃捕获模块初始化完成
35.551 点击每日提示            ← 此处之前约 3.3s 无任何日志
35.685 主窗口初始化完成
37.267 懒加载核心步骤: 814 ms
37.720 懒加载全部完成, 总计: 1267 ms
```

**核心问题**: 阶段 A+B 是最大耗时(约 3.3s),但窗口尚未显示、无任何反馈,用户面对桌面感到"点了没反应"。阶段 C 的 LoadingOverlay 只覆盖内容区且窗口已显示,期间 Ribbon 和窗口框架可见,观感是"半成品界面"。两层反馈都不足。

**目标**: 用自绘无边框 Splash 启动屏覆盖全部三阶段,提供 logo + 详细状态文本 + 进度条 + 百分比;主窗口在 lazyInit 全部完成后由 Splash 统一 reveal,全程不暴露半成品界面,顺带消除现有 lazyInit 阶段的半成品闪烁。

## 架构回顾

- 启动入口:`src/app/main.cpp`,`MainWindow main_window; main_window.show();`(第 84-85 行)
- `MainWindow` 构造:`initUi()` + `initSignalsEarly()` + `QTimer::singleShot(0, &lazyInit)`(MainWindow.cpp:110-120)
- `lazyInit`:`MainWindow.cpp:1059-1401`,12 个带日志的分步,已有 `QCoreApplication::processEvents()` 分步刷界面
- 现有覆盖层:`src/app/widgets/LoadingOverlay.h/.cpp`(namespace `etest::app`),脉冲+淡出自绘,仅被 MainWindow 使用
- 主题:`ThemeManager` 单例,`loadQss` 在实例构造中完成(位于阶段 A)
- **关键陷阱**: `MainWindow::restoreWindowState()`(initUi 末段调用)内含 `showMaximized()`(MainWindow.cpp:2679-2681),会在构造期间直接显示主窗口。任何保存过最大化状态(`CONFIG_WINDOW_MAXIMIZED`)的用户都会触发。**必须拆分**(见风险 🔴-1)

## 方案设计

### 新增组件 `StartupSplashWidget`

位于 `src/app/widgets/StartupSplashWidget.h/.cpp`,namespace `etest::app`,自绘无边框 QWidget。

**视觉布局**(自上而下):
- 应用 logo:`QIcon(":/resources/icons/app_icon.svg")` 直接加载
- 应用名"ETestStudio"
- 当前步骤状态文本(一行,随进度更新)
- 进度条(QProgressBar,通过 objectName + startup.qss 定制)
- 百分比文本

**交互特征**:
- 无边框、置顶(`Qt::WindowStaysOnTopHint`)、居中于主屏
- 非交互,拦截鼠标/键盘事件(防点击穿透到尚未初始化的主窗口)

**关键接口**:

```cpp
namespace etest::app {

class StartupSplashWidget : public QWidget {
  Q_OBJECT
 public:
  explicit StartupSplashWidget(QWidget* parent = nullptr);  // 普通类,非单例
  void setStatusText(const QString& text);   // 更新步骤文案 + 触发重绘
  void setProgress(int percent);             // 0-100,更新进度条 + 百分比
  void finish();                             // 隐藏 splash(主窗口显示由 MainWindow 控制)
};

}  // namespace etest::app
```

**生命周期:main() 局部对象 + 构造器注入**(决策记录 #5):

```cpp
// main.cpp,SingleInstance 检查通过后
StartupSplashWidget splash;
splash.show();
QCoreApplication::processEvents();
// ...阶段 A 初始化...
MainWindow main_window(nullptr, &splash);   // 构造器注入(非 setter)
QCoreApplication::processEvents();      // 构造完成后刷新一次(B 阶段进度一次性显示)
app.exec();                              // lazyInit 各步正常 processEvents
```

- `splash` 是 main() 栈对象,作用域覆盖 app.exec() 全程,lazyInit 结束时 `finish()` 时 splash 必然存活,无悬垂、无需 delete
- **构造器注入而非 setter**(代码审查 R1 修复):setter 在 MainWindow 构造完成后才调用,而 initUi 的进度上报在构造期间执行——setter 方案下 `splash_widget_` 构造期恒为 nullptr,阶段 B 的 7 个进度点全部静默丢弃(死代码)。构造器注入使 `splash_widget_` 在 initUi 执行时即有效,进度实时生效
- 无全局状态,符合 SOLID,可测试

### 样式处理:独立 startup.qss 资源

Splash 显示于 `ThemeManager` 加载(阶段 A)之前,无法引用主题 QSS。采用**独立 QSS 资源** `src/app/resources/styles/startup.qss`(决策记录 #6):

- 用 objectName 选择器定位 splash 及其子控件(如 `#StartupSplash`、`#StartupSplashProgressBar`、`#StartupSplashStatus`)
- main() 里从资源读取后 `splash->setStyleSheet(qss)`(仅"加载哪个资源"在 C++ 中,样式本体全部在 QSS 文件)
- 完全符合 CLAUDE.md"样式统一写入 styles/ 下的 QSS 文件"规则
- 需要在 `resource.qrc` 注册该文件

### 启动时序改造

```
QApplication 初始化
SingleInstance 检测(第二实例在此 return)          ← splash 在它之后,避免第二实例闪现
StartupSplashWidget splash; splash.show(); processEvents()
registerEditorTypes / ConfigManager / Logger / ExceptionHandler / CrashHandler
  └ 每块前后: setStatusText + setProgress + processEvents
ThemeManager::instance()                 ← 状态: 加载主题样式...
CDockComponentsFactory::setFactory
MainWindow main_window                   ← 构造内部 initUi 各步骤之间:
  │                                        setStatusText + setProgress(不插 processEvents)
  │                                        restoreWindowState 拆分: 不再 showMaximized,改存标志
main_window.setSplashWidget(&splash)
processEvents()                          ← 构造完成后安全刷新(B 阶段进度一次性显示)
app.exec()                               ← 事件循环;可能在此前 processEvents 已提前触发 lazyInit(见 🟡-A)
lazyInit 各步: setStatusText + setProgress + processEvents(已有)
lazyInit 末尾: reveal 主窗口(show/showMaximized + raise + activate),然后 splash.finish()
```

**要点**:
- `main_window.show()` 移至 lazyInit 末尾,由 MainWindow 自身 reveal(含最大化标志),再调 `splash.finish()`
- `setActivationWindow(main_window.winId())` 保留,`winId()` 对隐藏窗口安全(🔵-2)
- 阶段 A 需在每个进度点间调用 `processEvents()`,否则阻塞期进度条不刷新
- **initUi 构造期间只上报进度、不插 `processEvents()`**(决策记录 #7),重入风险为零;B 阶段进度攒到构造完成后统一刷新(分块跳变,非平滑,已接受)
- **🟡-A 时序确认**: 构造器(110-120)调度的 `QTimer::singleShot(0, &lazyInit)` 在构造后的 `processEvents()` 时已过期,可能**提前于 app.exec() 触发 lazyInit**。功能无害(主窗口仍隐藏、splash 全程覆盖、lazyInit 不依赖 exec),"B 分块跳变→C 平滑" UX 恰好达成。实施时实测确认;若需严格保序,改用 `processEvents(QEventLoop::ExcludeTimers)`

### finish() 次序

`MainWindow::revealAfterSplash()` 在 lazyInit 末尾执行,次序固定,避免桌面闪缝(🟡-2):

1. 先显示主窗口:`show()`(普通)或 `showMaximized()`(用户曾最大化)——主窗口在置顶 splash 底下出现
2. `splash.finish()`:隐藏 splash
3. `raise()` + `activateWindow()` 主窗口

先 show 主窗口再 hide splash,利用 splash 置顶关系遮住过渡帧,不露桌面。

**🟡-B showEvent 触发点迁移(显式声明)**: 现状首次 show 由 main.cpp:85 触发,`showEvent`(MainWindow.cpp:2013-2019)在首次 show 时调 `onThemeChanged`。新方案下首次 show 延后到 reveal 时执行——功能安全(`ThemeManager` 已加载,`onThemeChanged` 在 show() 内同步执行且被置顶 splash 遮住),但 MainWindow 级 ribbon/QADS QSS 在 lazyInit 期间未应用、仅 qApp 全局 QSS 生效。此迁移是预期行为,实施时不视为异常。

### 进度映射表

百分比为**按时间占比估算的权重**(🟡-4):A+B 约 3.3s 分配 60 点,lazyInit 约 1.27s 分配 40 点,避免"最快的阶段等最久"。

| 阶段 | 状态文本 | 百分比 |
|------|----------|--------|
| main 早期 | 正在初始化日志与配置 | 0-3 |
| registerEditorTypes | 注册编辑器类型 | 3-6 |
| Logger/Exception/Crash | 初始化异常与崩溃处理 | 6-10 |
| ThemeManager | 加载主题样式 | 10-35 |
| initUi: setupRibbon | 构建功能区 | 35-45 |
| initUi: createStatusBar | 构建状态栏 | 45-48 |
| initUi: 中央堆叠/活动栏/sidebar | 构建中央界面 | 48-53 |
| initUi: dock manager | 构建停靠系统 | 53-57 |
| initUi: Welcome/restore | 构建欢迎页 | 57-60 |
| lazyInit [2] | 注册侧边栏页面 | 60-65 |
| lazyInit [3] | 创建底部面板 | 65-68 |
| lazyInit [4] | 创建编辑器管理器 | 68-72 |
| lazyInit [6] | 连接组件信号 | 72-75 |
| lazyInit [7] | 初始化认证服务 | 75-77 |
| lazyInit [8] | 加载硬件插件 | 77-85 |
| lazyInit [10] | 恢复界面布局 | 85-90 |
| lazyInit [12] | 初始化屏幕保护 | 90-97 |
| reveal | 完成 | 97-100 |

**说明**: 阶段 A 无法细分到内部(`ThemeManager` 构造为黑盒),仅在两块之间插粗粒度点;ThemeManager 是阶段 A 绝对大头(预估 2s+/3.3s),分配 10-35 宽区间(🔵-F);阶段 C 按 lazyInit 步号映射(步号以日志 `[n/12]` 为准对齐,编号跳过的 5/9 不存在,🔵-3)。插件加载(实测 339ms)和 Tux(实测 453ms)分配更宽区间。

### 删除 LoadingOverlay

- `src/app/widgets/LoadingOverlay.h/.cpp` 删除
- `src/app/CMakeLists.txt:122-123` 移除
- `MainWindow.h:37` 前向声明、`:217` 成员 `loading_overlay_` 移除
- `MainWindow.cpp` lazyInit step1(覆盖层创建)、step11(延迟移除调度)删除
- **不影响** topology/protocol 编辑器:其 `loading_overlay_` 是各自内部 QWidget(`PhLoadingOverlay`),与本组件无关

### 文件改动清单

| 文件 | 改动 |
|------|------|
| `src/app/widgets/StartupSplashWidget.h/.cpp` | **新增** |
| `src/app/resources/styles/startup.qss` | **新增**(splash 专属样式) |
| `src/app/resource.qrc` | 注册 startup.qss |
| `src/app/CMakeLists.txt` | 移除 LoadingOverlay,新增 StartupSplashWidget |
| `src/app/main.cpp` | 插入 splash 创建与阶段 A 进度点;删除 `main_window.show()`;构造器注入 splash;timeout 兜底连接 |
| `src/app/MainWindow.cpp` | initUi 插进度点(不插 processEvents);`restoreWindowState` 拆分 `showMaximized`;lazyInit 各步上报进度,末尾 reveal + splash.finish();删除 step1/step11;**清理 `resizeEvent` 中 `loading_overlay_` 死代码(MainWindow.cpp:2006-2010)** |
| `src/app/MainWindow.h` | 移除 LoadingOverlay 声明与成员;构造器增加 splash 参数;新增 `reportSplashProgress` 成员函数 + 最大化标志 |
| `src/app/widgets/LoadingOverlay.h/.cpp` | **删除** |

## 风险与缓解

1. **🔴 `restoreWindowState()` 的 `showMaximized()` 构造期显示主窗口**(MainWindow.cpp:2679-2681): 已拆分为只存 `maximize_on_reveal_` 标志,不 show;lazyInit 末尾 `revealAfterSplash()` 根据标志执行 `show()` 或 `showMaximized()`。此修复是"窗口全程隐藏"前提成立的关键。**标志默认 false**(`CONFIG_WINDOW_DEFAULT_MAXIMIZED=false`,ConfigDefs.h:43);`resize/move` 保留在构造期(隐藏窗口执行,现状已验证可行),非最大化用户走 `show()` 复用构造期已设几何。
2. **🔴 logo 加载路径**: `AppIconProvider` 的构造函数(AppIconProvider.cpp:16)与 `resolvePath`(:37)都调用 `ThemeManager::instance()`,且 `resolvePath` 只查 `svg/` 子目录变体(:39-41,47),加载不到 `icons/` 根目录的 `app_icon.svg`(空图标 + 强制提前初始化 ThemeManager)。**禁用 AppIconProvider**,明确用 `QIcon(":/resources/icons/app_icon.svg")` 直接加载(qrc 已注册,256x256,无 _light/_dark 依赖)。
3. **🟡 lazyInit step10 splitter `restoreState` 从可见窗口改为隐藏窗口执行**: 现状 `restoreWindowState()` 本就跑在未 show 窗口上(已验证可行),真正变化的是 lazyInit step10 的 `h_splitter_->restoreState`/`v_splitter_->restoreState`。实施时**收窄实测点到此一处**;若隐藏窗口下 splitter 尺寸不生效,将这两个 restore 移到 reveal 之后一步(只影响布局恢复,不影响启动)。
4. **🟡 构造期 `processEvents()` 重入**: 已用"initUi 只上报不 processEvents"消除;阶段 A 的 `processEvents()` 发生在 main_window 构造之前,事件队列只有 splash 重绘,安全。构造完成后、`app.exec()` 前的那次 `processEvents()` 发生在 MainWindow 构造已返回之后,同样安全。**注意其可能提前触发 lazyInit**(🟡-A),功能无害,实施时实测确认。
5. **Splash 固定配色**: startup.qss 使用固定中性配色,与主题 QSS 解耦;splash 显示于主题加载前,无法跟随主题,属设计取舍(决策记录 #6)。
6. **`lazyInit` 末尾 reveal 时机**: Tux 屏保 453ms 计入等待(决策记录 #4)。reveal 必须在 lazyInit 全部步骤完成后执行。
7. **lazyInit 步号处理(🔵-D)**: 删除 step1(LoadingOverlay)后 `[n/12]` 日志标签整体移位。进度映射表按日志标签编号映射会错位,实施时二选一:保留 `[n/12]` 编号跳过空位,或统一重编号并同步进度表(推荐后者,步号连续、日志易读)。

## 决策记录

| # | 决策 | 结论 |
|---|------|------|
| 1 | Splash 实现方式 | 自绘无边框 QWidget(否决 QSplashScreen,进度表现力不足) |
| 2 | MainWindow 显示时机 | lazyInit 完成后统一 reveal(否决立即 show) |
| 3 | LoadingOverlay 去留 | 删除(被 splash 全覆盖) |
| 4 | Tux 屏保是否计入等待 | 计入(lazyInit 全完成后 reveal) |
| 5 | 生命周期与上报机制 | **main 局部对象 + 构造器注入**(否决全局单例:生命周期天然正确、无悬垂、SOLID;创建位置在 SingleInstance 检查之后,避免第二实例闪现 splash。原定 setter 注入被代码审查推翻——setter 晚于构造导致 B 阶段进度死代码,改为构造器注入) |
| 6 | Splash 配色 | **独立 startup.qss 资源**(否决 C++ 硬编码;样式进 QSS 文件,符合 CLAUDE.md) |
| 7 | initUi 构造期进度刷新 | **只上报进度、不插 processEvents**(否决构造期 processEvents:重入风险;进度分块跳变可接受) |
| 8 | reveal 与 finish 次序 | 先 show 主窗口、再 hide splash、再 raise+activate(利用置顶遮住过渡帧) |

## 实施步骤

1. 新增 `StartupSplashWidget.h/.cpp`(logo + 文本 + 进度条 + 百分比,objectName,finish 只 hide)
2. 新增 `startup.qss` 并注册到 `resource.qrc`
3. `MainWindow.h`: 移除 LoadingOverlay 声明与成员;新增 `setSplashWidget`、`splash_widget_` 指针、`maximize_on_reveal_` 标志、`revealAfterSplash()` 声明
4. `MainWindow.cpp`: `restoreWindowState` 拆分 `showMaximized` 为 `maximize_on_reveal_` 标志;initUi 插进度点(不插 processEvents);lazyInit 各步上报 + 末尾 reveal + splash.finish();删除 step1/step11;清理 `resizeEvent` 中 `loading_overlay_` 死代码(2006-2010);lazyInit 步号统一重编号(🔵-D)
5. `main.cpp`: SingleInstance 后创建 splash + 阶段 A 进度点 + setSplashWidget + 删除 `show()`
6. `CMakeLists.txt`: 移除 LoadingOverlay,新增 StartupSplashWidget
7. 删除 `src/app/widgets/LoadingOverlay.h/.cpp`
8. 编译验证 + 运行验证: 启动全程可见 splash,进度递增(A+B 分块跳变、C 平滑),主窗口完整 reveal,无半成品闪烁,最大化用户窗口状态正确恢复;**实测确认构造后 processEvents 是否提前触发 lazyInit(🟡-A),必要时改 ExcludeTimers**
