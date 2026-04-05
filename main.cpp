#include <QApplication>
#include <QMainWindow>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>


int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QMainWindow window;
  window.show();
  return app.exec();
}