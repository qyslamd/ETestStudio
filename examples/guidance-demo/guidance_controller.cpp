#include "guidance_controller.h"
#include <QApplication>
#include <QDebug>
#include <QDesktopWidget>
#include <QStandardItemModel>
#include <QTimer>
#include <variant>
#include "guidance_presentation.h"

GuidanceController::GuidanceController(QObject* parent)
    : QObject(parent),
      viewport_(nullptr),
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

  if (!viewport_) {
    qWarning() << "Viewport must be set before start()";
    return;
  }

  run_all_ = true;
  auto_mode_ = autoMode;
  curFlowIndex_ = 0;
  curStepIndex_ = 0;
  execute();
}

void GuidanceController::startOne(GuidanceFlow* const flow, bool autoMode) {
  if (!flow) {
    return;
  }

  if (!config_.flows().contains(flow)) {
    return;
  }

  if (!viewport_) {
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
  if (viewport_) {
    presentation_->setGeometry(viewport_->rect());
  }
}

void GuidanceController::stop() {
  isRunning_ = false;

  presentation_->setParent(nullptr);
  presentation_->hide();
}

void GuidanceController::loadFlowToListModel(QStandardItemModel* model) {
  model->clear();
  for (auto const& flow : config_.flows()) {
    auto item = new QStandardItem(QIcon(flow->icon()), flow->name());
    item->setData(QVariant::fromValue(flow), Qt::UserRole + 1);
    model->appendRow(item);
  }
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

  // 如果是运行单个 flow 或者 运行所有且所有 flow 结束了
  if (!run_all_ || curFlowIndex_ >= config_.flows().size() - 1) {
    stop();
    emit finished();
    return;
  }

  curFlowIndex_++;
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

  // 回退 flow
  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  if (curFlow->exitFunc()) {
    curFlow->exitFunc()();
  }

  curFlowIndex_--;
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

void GuidanceController::onViewportStateChanged() {
  if (viewport_ == nullptr) {
    return;
  }

  presentation_->setGeometry(viewport_->rect());
}

void GuidanceController::updateStep() {
  if (config_.flows().isEmpty()) {
    return;
  }

  GuidanceFlow* curFlow = config_.flows().at(curFlowIndex_);
  GuidanceStep* curStep = curFlow->steps().at(curStepIndex_);

  if (curStep->enterFunc()) {
    curStep->enterFunc()();
  }

  // ------------------------------------
  // guess waht?
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
  // ------------------------------------

  int globalIndex = 0;
  for (int i = 0; i < curFlowIndex_; ++i) {
    globalIndex += config_.flows().at(i)->stepCount();
  }
  globalIndex += curStepIndex_;

  emit stepChanged(globalIndex, config_.totalSteps());
}
