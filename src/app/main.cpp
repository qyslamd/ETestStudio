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

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  // 1. spdlog 测试
  spdlog::info("spdlog 集成验证通过！");

  QWidget w;
  w.resize(800, 600);
  w.show();
  return app.exec();
}