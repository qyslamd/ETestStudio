#pragma once

#include <QWidget>

class QStackedWidget;

namespace etest::app {

class WelcomeV1Widget;
class WelcomeV2Widget;

// Welcome 容器：承载业务（版本切换、对外信号、刷新入口）。
// 内建 v1（旧网格仪表盘）与 v2（新启动页），按 CONFIG_WELCOME_VERSION 即时切换，
// 将激活版本的信号统一转发给上层（MainWindow）。
class WelcomeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit WelcomeWidget(QWidget* parent = nullptr);

  /// 刷新最近项目（转发给当前激活版本）
  void refreshRecentProjects();

 signals:
  void newProjectRequested();
  void openProjectRequested();
  void createFileRequested(const QString& categoryId, const QString& extension,
                           const QString& baseName);
  void projectOpenRequested(const QString& projectPath);
  void settingsRequested();

 private:
  void initUi();
  void initSignals();
  void switchVersion();

  QStackedWidget* stack_ = nullptr;
  WelcomeV1Widget* v1_ = nullptr;
  WelcomeV2Widget* v2_ = nullptr;
};

}  // namespace etest::app
