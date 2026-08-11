#pragma once

#include <QObject>
#include <QWidget>

#include "guidance_config.h"

namespace etest::app {

class GuidancePresentation;

// 引导流程控制器：持有全部 Flow/Step 配置，驱动演示层（遮罩 + 高亮 + 气泡）
// 前进/后退/跳过，并把状态通过信号暴露给上层（MainWindow）。
class GuidanceController : public QObject {
  Q_OBJECT

 public:
  explicit GuidanceController(QObject* parent = nullptr);
  ~GuidanceController() override;

  void setViewport(QWidget* viewport);
  GuidanceFlow* addFlow(GuidanceFlow* flow);
  void clearFlows();
  void startAll(bool autoMode = false);
  void startOne(GuidanceFlow* const flow, bool autoMode = false);
  void stop();

  bool isRunning() const { return isRunning_; }
  int currentStepIndex() const { return curStepIndex_; }
  int totalSteps() const { return config_.totalSteps(); }

  // 首页构建主题卡片用：当前已注册的 Flow 列表。
  const QList<GuidanceFlow*>& flows() const { return config_.flows(); }

  // 目标控件在 enter 回调中异步就绪（如编辑器 QtConcurrent 打开）后，
  // 重新定位当前步骤的高亮（D12）。
  void refreshCurrentStepHighlight();

 signals:
  void started();
  void finished();
  void finishedByUser();
  void stepChanged(int index, int total);
  void flowChanged(const QString& flowName);

 private slots:
  void onNextClicked();
  void onPrevClicked();
  void onSkipClicked();

 private:
  void execute();
  void updateOverlayGeometry();
  void updateStep();
  void refreshStepPresentation();
  // 自 fromIndex 起向后/向前找第一个可播放（stepCount()>0）的 Flow 下标，无则 -1。
  int nextPlayableFlowIndex(int fromIndex) const;
  int previousPlayableFlowIndex(int fromIndex) const;

 private:
  GuidanceConfig config_;
  QWidget* viewport_ = nullptr;
  GuidancePresentation* presentation_ = nullptr;

  bool run_all_ = false;    // 指示是否运行所有的 Flow
  bool auto_mode_ = false;  // 指示是否是自动演示模式
  bool isRunning_ = false;
  int curFlowIndex_ = 0;
  int curStepIndex_ = 0;
};

}  // namespace etest::app
