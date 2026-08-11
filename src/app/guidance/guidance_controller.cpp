#include "guidance_controller.h"

#include <QDebug>

#include "guidance_presentation.h"

namespace etest::app {

GuidanceController::GuidanceController(QObject* parent)
    : QObject(parent),
      presentation_(new GuidancePresentation(nullptr)),
      isRunning_(false),
      curFlowIndex_(0),
      curStepIndex_(0) {
  connect(presentation_, &GuidancePresentation::nextClicked, this,
          &GuidanceController::onNextClicked);
  connect(presentation_, &GuidancePresentation::prevClicked, this,
          &GuidanceController::onPrevClicked);
  connect(presentation_, &GuidancePresentation::skipClicked, this,
          &GuidanceController::onSkipClicked);
}

GuidanceController::~GuidanceController() {
  stop();
  // 演示层由控制器持有，销毁时回收（stop 后无父级，避免泄漏）。
  delete presentation_;
  presentation_ = nullptr;
  config_.clear();
}

void GuidanceController::setViewport(QWidget* viewport) {
  viewport_ = viewport;
}

GuidanceFlow* GuidanceController::addFlow(GuidanceFlow* flow) {
  return config_.addFlow(flow);
}

void GuidanceController::clearFlows() {
  config_.clear();
}

void GuidanceController::execute() {
  isRunning_ = true;

  presentation_->setParent(viewport_);
  presentation_->setGeometry(viewport_->rect());
  presentation_->setIsAutoMode(auto_mode_);
  presentation_->show();

  updateStep();

  emit started();
}

void GuidanceController::startAll(bool autoMode) {
  if (config_.flows().isEmpty()) {
    return;
  }

  if (viewport_ == nullptr) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  // 占位主题（stepCount()==0）仅供首页卡片展示，不进入播放。
  const int firstPlayable = nextPlayableFlowIndex(0);
  if (firstPlayable < 0) {
    qWarning() << "No playable flow to start";
    return;
  }

  run_all_ = true;
  auto_mode_ = autoMode;
  curFlowIndex_ = firstPlayable;
  curStepIndex_ = 0;
  execute();
}

void GuidanceController::startOne(GuidanceFlow* const flow, bool autoMode) {
  if (flow == nullptr) {
    return;
  }

  if (flow->stepCount() == 0) {
    qWarning() << "Placeholder flow can not be played";
    return;
  }

  if (!config_.flows().contains(flow)) {
    return;
  }

  if (viewport_ == nullptr) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  run_all_ = false;
  auto_mode_ = autoMode;
  curFlowIndex_ = config_.flows().indexOf(flow);
  curStepIndex_ = 0;
  execute();
}

void GuidanceController::updateOverlayGeometry() {
  if (viewport_ != nullptr) {
    presentation_->setGeometry(viewport_->rect());
  }
}

void GuidanceController::stop() {
  isRunning_ = false;

  presentation_->setParent(nullptr);
  presentation_->hide();
}

void GuidanceController::onNextClicked() {
  if (curFlowIndex_ >= config_.flows().size()) {
    stop();
    emit finished();
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curStepIndex_ >= curFlow->stepCount()) {
    stop();
    emit finished();
    return;
  }

  GuidanceStep* curStep = curFlow->steps().at(curStepIndex_);
  if (curStep->exitFunc()) {
    curStep->exitFunc()();
  }

  // ------------------------
  // 还在一个 flow 中
  if (curStepIndex_ < curFlow->stepCount() - 1) {
    curStepIndex_++;
    updateStep();
    return;
  }

  // ------------------------
  // 一个 Flow 结束
  // 先执行 退出函数
  if (curFlow->exitFunc()) {
    curFlow->exitFunc()();
  }

  // 单主题运行，或已无下一个可播放主题（跳过占位主题）时结束。
  if (!run_all_) {
    stop();
    emit finished();
    return;
  }

  const int nextIndex = nextPlayableFlowIndex(curFlowIndex_ + 1);
  if (nextIndex < 0) {
    stop();
    emit finished();
    return;
  }

  curFlowIndex_ = nextIndex;
  curStepIndex_ = 0;
  GuidanceFlow* nextFlow = config_.flows().at(curFlowIndex_);
  if (nextFlow->enterFunc()) {
    nextFlow->enterFunc()();
  }
  updateOverlayGeometry();
  emit flowChanged(nextFlow->name());
  updateStep();
}

void GuidanceController::onPrevClicked() {
  // 回退 step
  if (curStepIndex_ > 0) {
    curStepIndex_--;
    updateStep();
    return;
  }

  if (!run_all_) {
    return;
  }

  // 回退 flow（跳过占位主题）
  const int prevIndex = previousPlayableFlowIndex(curFlowIndex_ - 1);
  if (prevIndex < 0) {
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curFlow->exitFunc()) {
    curFlow->exitFunc()();
  }

  curFlowIndex_ = prevIndex;
  GuidanceFlow* prevFlow = config_.flows().at(curFlowIndex_);
  curStepIndex_ = prevFlow->stepCount() - 1;
  if (prevFlow->enterFunc()) {
    prevFlow->enterFunc()();
  }
  updateOverlayGeometry();
  emit flowChanged(prevFlow->name());
}

void GuidanceController::onSkipClicked() {
  stop();
  emit finishedByUser();
}

void GuidanceController::updateStep() {
  if (config_.flows().isEmpty()) {
    return;
  }
  if (curFlowIndex_ < 0 || curFlowIndex_ >= config_.flows().size()) {
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curStepIndex_ < 0 || curStepIndex_ >= curFlow->stepCount()) {
    return;
  }
  GuidanceStep* curStep = curFlow->steps().at(curStepIndex_);

  if (curStep->enterFunc()) {
    curStep->enterFunc()();
  }

  refreshStepPresentation();

  int globalIndex = 0;
  for (int i = 0; i < curFlowIndex_; ++i) {
    globalIndex += config_.flows().at(i)->stepCount();
  }
  globalIndex += curStepIndex_;

  emit stepChanged(globalIndex, config_.totalSteps());
}

void GuidanceController::refreshCurrentStepHighlight() {
  refreshStepPresentation();
}

// 刷新当前步骤的演示层（高亮/气泡/按钮态）。从 updateStep 抽出，
// 便于 enter 回调中目标控件异步就绪后重新定位（D12）。
void GuidanceController::refreshStepPresentation() {
  if (!isRunning_ || config_.flows().isEmpty()) {
    return;
  }
  if (curFlowIndex_ < 0 || curFlowIndex_ >= config_.flows().size()) {
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curStepIndex_ < 0 || curStepIndex_ >= curFlow->stepCount()) {
    return;
  }
  GuidanceStep* curStep = curFlow->steps().at(curStepIndex_);

  bool canPrev = true;
  bool canNext = true;
  if (run_all_) {
    if (curFlowIndex_ == 0 && curStepIndex_ == 0) {
      canPrev = false;
    }

    if (curFlowIndex_ == config_.flows().count() - 1 &&
        curStepIndex_ == curFlow->stepCount() - 1) {
      canNext = false;
    }
  } else {
    if (curStepIndex_ == 0) {
      canPrev = false;
    }

    if (curStepIndex_ == curFlow->stepCount() - 1) {
      canNext = false;
    }
  }
  presentation_->updateUi(curFlow, curStep, canPrev, canNext);
}

// 自 fromIndex 起向后找第一个可播放（stepCount()>0）的 Flow 下标，无则 -1。
int GuidanceController::nextPlayableFlowIndex(int fromIndex) const {
  for (int i = fromIndex; i < config_.flows().size(); ++i) {
    if (config_.flows().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}

// 自 fromIndex 起向前找第一个可播放（stepCount()>0）的 Flow 下标，无则 -1。
int GuidanceController::previousPlayableFlowIndex(int fromIndex) const {
  for (int i = fromIndex; i >= 0; --i) {
    if (config_.flows().at(i)->stepCount() > 0) {
      return i;
    }
  }
  return -1;
}

}  // namespace etest::app
