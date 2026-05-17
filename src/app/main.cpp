#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "MainWindow.h"
#include "TextEditorWidget.h"
#include "common/GlobalExceptionHandler.h"
#include "common/SingleInstance.h"
#include "config/ConfigManager.h"
#include "crashhandler/CrashHandler.h"
#include "editor/EditorFactory.h"
#include "logger/Logger.h"
#include "topology/TopologyDocument.h"
#include "topology/TopologyEditorWidget.h"
#include "topology/TopologyJsonSerializer.h"

using namespace etest::core::config;
using namespace etest::core::logger;
using namespace etest::core::crashhandler;
using namespace etest::core::common;
using namespace etest::app;

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setFont(QFont("Microsoft YaHei", 10));

  // 注册编辑器工厂
  EditorFactoryRegistry::registerExtension("cpp", "text");
  EditorFactoryRegistry::registerExtension("h", "text");
  EditorFactoryRegistry::registerExtension("hpp", "text");
  EditorFactoryRegistry::registerExtension("c", "text");
  EditorFactoryRegistry::registerExtension("cc", "text");
  EditorFactoryRegistry::registerExtension("cxx", "text");
  EditorFactoryRegistry::registerExtension("py", "text");
  EditorFactoryRegistry::registerExtension("lua", "text");
  EditorFactoryRegistry::registerExtension("json", "text");
  EditorFactoryRegistry::registerExtension("xml", "text");
  EditorFactoryRegistry::registerExtension("html", "text");
  EditorFactoryRegistry::registerExtension("yaml", "text");
  EditorFactoryRegistry::registerExtension("yml", "text");
  EditorFactoryRegistry::registerExtension("md", "text");
  EditorFactoryRegistry::registerExtension("js", "text");
  EditorFactoryRegistry::registerExtension("cmake", "text");
  EditorFactoryRegistry::registerExtension("txt", "text");

  EditorFactoryRegistry::registerFactory("text", [](const QString& path, QWidget* parent) {
    return new TextEditorWidget(path, parent);
  });

  EditorFactoryRegistry::registerFactory("topology", [](const QString& id, QWidget* parent) {
    auto* editor = new etest::topology::TopologyEditorWidget(parent);
    if (!id.startsWith("editor://") && QFileInfo::exists(id)) {
      QFile file(id);
      if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError err;
        QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll(), &err);
        file.close();
        if (err.error == QJsonParseError::NoError) {
          etest::topology::TopologyJsonSerializer::deserialize(jdoc.object(), editor->document());
          editor->document()->setModified(false);
          editor->reloadScene();
        }
      }
    }
    return editor;
  });

  // 单实例检测
  SingleInstance singleInstance("etest_demo");
  if (singleInstance.isAppAlreadyRunning()) {
    singleInstance.connectToExistingInstance(QCoreApplication::arguments());
    return 0;
  }
  singleInstance.startListening();

  // 初始化全局配置管理（优先于日志初始化，日志需要读取配置）
  ConfigManager::instance();

  // 初始化日志系统
  Logger::init();

  // 初始化全局异常处理器（信号捕获 + Qt消息重定向）
  GlobalExceptionHandler::instance().init();

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

  singleInstance.setActivationWindow(&main_window);

  int ret = app.exec();

  // 关闭全局异常处理器
  GlobalExceptionHandler::instance().shutdown();

  // 关闭日志系统
  Logger::shutdown();

  return ret;
}
