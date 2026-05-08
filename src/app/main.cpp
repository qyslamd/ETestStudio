#include <QApplication>
#include <QFont>

#include "MainWindow.h"
#include "config/ConfigManager.h"
#include "common/GlobalExceptionHandler.h"
#include "crashhandler/CrashHandler.h"
#include "logger/Logger.h"

using namespace etest::core::config;
using namespace etest::core::logger;
using namespace etest::core::crashhandler;
using namespace etest::app;

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setFont(QFont("Microsoft YaHei", 10));

  // 初始化全局配置管理（优先于日志初始化，日志需要读取配置）
  ConfigManager::instance();

  // 初始化日志系统
  Logger::init();

  // 初始化全局异常处理器（信号捕获 + Qt消息重定向）
  etest::core::common::GlobalExceptionHandler::instance().init();

  LOG_INFO("MAIN", "全局配置管理模块初始化完成");

  // 初始化崩溃捕获模块
  auto crashHandler = CrashHandler::create();
  if (crashHandler) {
    crashHandler->init();
    LOG_INFO("MAIN", "崩溃捕获模块初始化完成");
  }

  // 启动主窗口
  MainWindow main_window;
  main_window.show();

  int ret = app.exec();

  // 关闭全局异常处理器
  etest::core::common::GlobalExceptionHandler::instance().shutdown();

  // 关闭日志系统
  Logger::shutdown();

  return ret;
}
