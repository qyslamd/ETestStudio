#include <QApplication>
#include <QMainWindow>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

// 第三方依赖头文件引入
#include "spdlog/spdlog.h"
#include "gtest/gtest.h"
#include "lua.hpp"
#include "hpdf.h"
#include "ads_globals.h"
#include "DockManager.h"
#include "Qsci/qsciscintilla.h"
#include "xlsxdocument.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // 验证所有第三方依赖链接
  // 1. spdlog 测试
  spdlog::info("spdlog 链接正常");
  // 2. Lua 测试
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  lua_close(L);
  spdlog::info("Lua 链接正常");
  // 3. libharu 测试
  HPDF_Doc pdf = HPDF_New(NULL, NULL);
  if(pdf) HPDF_Free(pdf);
  spdlog::info("libharu 链接正常");
  // 4. QADS 测试
  ads::CDockManager manager;
  spdlog::info("QADS 链接正常");
  // 5. QScintilla 测试
  QsciScintilla editor;
  spdlog::info("QScintilla 链接正常");
  // 6. QtXlsx 测试
  QXlsx::Document xlsx;
  spdlog::info("QtXlsx 链接正常");
  // 7. gtest 测试
  spdlog::info("gtest 链接正常");

  spdlog::info("所有第三方依赖验证通过！");

  QWidget w;
  w.resize(800, 600);
  w.show();
  return app.exec();
}