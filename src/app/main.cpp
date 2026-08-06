#include <QApplication>
#include <QFile>
#include <QFont>

#include <QTranslator>
#include "EditorManager.h"
#include "MainWindow.h"
#include "common/GlobalExceptionHandler.h"
#include "common/SingleInstance.h"
#include "config/ConfigManager.h"
#include "core_ui/ThemeManager.h"
#include "crashhandler/CrashHandler.h"
#include "editors/EditorFactory.h"
#include "editors/TextEditorWidget.h"
#include "libui/styles/EtestComponentsFactory.h"
#include "libui/styles/NoFocusRectStyle.h"
#include "logger/Logger.h"
#include "widgets/StartupSplashWidget.h"

// Debug 构建启用控制台，方便查看 spdlog 输出
#ifdef _DEBUG
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

using namespace etest::core::config;
using namespace etest::core::logger;
using namespace etest::core::crashhandler;
using namespace etest::core::common;
using namespace etest::app;

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setFont(QFont("Microsoft YaHei", 10));
  app.setStyle(new NoFocusRectStyle);

  // 单实例检测
  SingleInstance singleInstance("ETestStudio");
  if (singleInstance.isAppAlreadyRunning()) {
    singleInstance.connectToExistingInstance(QCoreApplication::arguments());
    return 0;
  }
  singleInstance.startListening();

  // ── 启动 Splash（覆盖全部重初始化阶段；第二实例已在上面 return，不会闪现） ──
  StartupSplashWidget splash;
  {
    // Splash 专属样式（主题加载前无法使用主题 QSS，独立 startup.qss）
    QFile styleFile(QStringLiteral(":/resources/styles/startup.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      splash.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    } else {
      // 日志系统尚未初始化，用 qWarning 兜底（B3）
      qWarning("startup.qss 加载失败，Splash 将以默认样式显示");
    }
  }

  splash.show();
  QCoreApplication::processEvents();  // 立即渲染 splash

  // 阶段 A：main 重初始化（每块间 processEvents 让进度条刷新）
  splash.setStatusText(QStringLiteral("注册编辑器类型"));
  splash.setProgress(5);
  QCoreApplication::processEvents();
  EditorManager::registerEditorTypes();

  splash.setStatusText(QStringLiteral("初始化日志与配置"));
  splash.setProgress(8);
  QCoreApplication::processEvents();

  // 初始化全局配置管理（优先于日志初始化，日志需要读取配置）
  ConfigManager::instance();

  // 初始化日志系统
  Logger::init();
  LOG_INFO("MAIN",
           app.applicationDirPath().toStdString() + " 目录下启动应用程序");

  // 初始化全局异常处理器（信号捕获 + Qt消息重定向）
  GlobalExceptionHandler::instance().init();

  LOG_INFO("MAIN", "全局配置管理模块初始化完成");

  // 初始化 SARibbon 静态资源
  Q_INIT_RESOURCE(SARibbonResource);

  // 加载翻译
  QTranslator adsTrans;
  adsTrans.load(":/translations/ads_zh_CN.qm");
  app.installTranslator(&adsTrans);

  QTranslator saribbonTrans;
  saribbonTrans.load(":/translations/SARibbon_zh_CN.qm");
  app.installTranslator(&saribbonTrans);

  // 初始化崩溃捕获模块
  auto crashHandler = CrashHandler::create();
  if (crashHandler) {
    crashHandler->init();
    LOG_INFO("MAIN", "崩溃捕获模块初始化完成");
  }

  // 加载主题样式（构造期黑盒，粗粒度进度；内部无法细分）
  splash.setStatusText(QStringLiteral("加载主题样式"));
  splash.setProgress(20);
  QCoreApplication::processEvents();

  // 提前初始化主题管理（加载 QSS，确保在 QADS widget 创建前完成）
  etest::core_ui::ThemeManager::instance();

  // 注册自定义 QADS 组件工厂（必须在任何 CDockWidget 创建之前）
  ads::CDockComponentsFactory::setFactory(new EtestComponentsFactory());

  // 阶段 B：MainWindow 构造（构造器注入 splash，initUi 内部上报进度，不插
  // processEvents；构造期内 splash_widget_ 即有效，进度实时生效）
  MainWindow main_window(nullptr, &splash);

  // 构造完成后安全刷新（B 阶段进度一次性显示；会提前触发 lazyInit，
  // 功能无害且 UX 恰好达成，见方案时序确认；Qt5 无 ExcludeTimers 故直接默认）
  QCoreApplication::processEvents();

  // 超时兜底：lazyInit 挂起超时后强制 reveal（替代旧 LoadingOverlay 的 10s 兜底）
  QObject::connect(&splash, &StartupSplashWidget::timeout, [&main_window]() {
    main_window.revealAfterSplash();
  });

  // 主窗口延迟显示：lazyInit 全部完成后由 revealAfterSplash() 统一 reveal
  singleInstance.setActivationWindow(
      reinterpret_cast<void*>(main_window.winId()));

  int ret = app.exec();

  // 关闭全局异常处理器
  GlobalExceptionHandler::instance().shutdown();

  LOG_INFO("MAIN", "应用程序即将退出");
  // 关闭日志系统
  Logger::shutdown();

  return ret;
}
