#ifndef GUIDANCE_CONTROLLER_H
#define GUIDANCE_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QWidget>
#include "guidance_config.h"

class GuidancePresentation;
class QStandardItemModel;
class GuidanceController : public QObject {
  Q_OBJECT

 public:
  explicit GuidanceController(QObject* parent = nullptr);
  ~GuidanceController();

  void setViewport(QWidget* viewport);
  GuidanceFlow* addFlow(GuidanceFlow* flow);
  void clearFlows();
  void startAll(bool autoMode = false);
  void startOne(GuidanceFlow* const flow, bool autoMode = false);
  void stop();

  bool isRunning() const { return isRunning_; }
  int currentStepIndex() const { return curStepIndex_; }
  int totalSteps() const { return config_.totalSteps(); }

  void loadFlowToListModel(QStandardItemModel* model);

 signals:
  void started();
  void finished();
  void finishedByUser();
  void stepChanged(int index, int total);
  void flowChanged(const QString& flowName);

 public slots:
  void onViewportStateChanged();
 private slots:
  void onNextClicked();
  void onPrevClicked();
  void onSkipClicked();

 private:
  void execute();
  void updateOverlayGeometry();
  void updateStep();

 private:
  GuidanceConfig config_;
  QWidget* viewport_ = nullptr;
  GuidancePresentation* presentation_ = nullptr;

  bool run_all_ = false;    // 指示是否运行所有的Flow
  bool auto_mode_ = false;  // 指示是否是自动演示模式
  bool isRunning_ = false;
  int curFlowIndex_ = 0;
  int curStepIndex_ = 0;
};

#endif  // GUIDANCE_CONTROLLER_H
