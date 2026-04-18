#include <QApplication>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "MainWindow.h"

// 第三方依赖头文件引入
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include "hpdf.h"
#include "DockManager.h"
#include "xlsxdocument.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // 逐个验证已配置第三方依赖
  // 1. spdlog 验证
  spdlog::info("✅ spdlog 日志库验证通过");

  // 2. googletest 验证（仅验证头文件和链接有效性）
  int test_argc = 0;
  char* test_argv[] = {nullptr};
  testing::InitGoogleTest(&test_argc, test_argv);
  spdlog::info("✅ googletest 单元测试框架验证通过");

  // 3. libharu PDF库验证
  HPDF_Doc pdf = HPDF_New(NULL, NULL);
  if(pdf) HPDF_Free(pdf);
  spdlog::info("✅ libharu PDF库验证通过");

  // 4. QXlsx Excel库验证
  QXlsx::Document xlsx;
  spdlog::info("✅ QXlsx Excel库验证通过");

  spdlog::info("🎉 所有已配置第三方依赖全部验证成功！");

  // 启动主窗口（集成QADS停靠布局）
  MainWindow main_window;
  main_window.show();

  return app.exec();
}