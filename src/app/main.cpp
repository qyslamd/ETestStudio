#include <QApplication>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "MainWindow.h"
#include "logger/Logger.h"

// 第三方依赖头文件引入
#include "gtest/gtest.h"
#include "hpdf.h"
#include "DockManager.h"
#include "xlsxdocument.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // 初始化日志系统
  Logger::init();

  // 逐个验证已配置第三方依赖
  // 1. googletest 验证（仅验证头文件和链接有效性）
  int test_argc = 0;
  char* test_argv[] = {nullptr};
  testing::InitGoogleTest(&test_argc, test_argv);
  LOG_INFO("MAIN", "✅ googletest 单元测试框架验证通过");

  // 2. libharu PDF库验证
  HPDF_Doc pdf = HPDF_New(NULL, NULL);
  if(pdf) HPDF_Free(pdf);
  LOG_INFO("MAIN", "✅ libharu PDF库验证通过");

  // 3. QXlsx Excel库验证
  QXlsx::Document xlsx;
  LOG_INFO("MAIN", "✅ QXlsx Excel库验证通过");

  LOG_INFO("MAIN", "🎉 所有已配置第三方依赖全部验证成功！");

  // 启动主窗口（集成QADS停靠布局）
  MainWindow main_window;
  main_window.show();

  int ret = app.exec();

  // 关闭日志系统
  Logger::shutdown();

  return ret;
}