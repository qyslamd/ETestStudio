#include "ExecutionControlBar.h"

#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>

namespace etest::app {

ExecutionControlBar::ExecutionControlBar(QWidget* parent) : QWidget(parent) {
  initUi();
  initSignals();

  // 加载独立 QSS 样式
  QFile styleFile(
      QStringLiteral(":/resources/styles/execution_control_bar.qss"));
  if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
    setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    styleFile.close();
  }
}

void ExecutionControlBar::initUi() {
  setObjectName(QStringLiteral("ExecutionControlBar"));

  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 2, 6, 2);
  layout->setSpacing(4);

  // ── Run button ──
  btn_run_ = new QPushButton(QStringLiteral("▶ 运行"), this);
  btn_run_->setObjectName(QStringLiteral("execControlRunBtn"));
  btn_run_->setToolTip(QStringLiteral("运行测试程序"));

  // ── Pause button ──
  btn_pause_ = new QPushButton(QStringLiteral("⏸ 暂停"), this);
  btn_pause_->setObjectName(QStringLiteral("execControlPauseBtn"));
  btn_pause_->setToolTip(QStringLiteral("暂停执行"));
  btn_pause_->setEnabled(false);

  // ── Stop button ──
  btn_stop_ = new QPushButton(QStringLiteral("⏹ 停止"), this);
  btn_stop_->setObjectName(QStringLiteral("execControlStopBtn"));
  btn_stop_->setToolTip(QStringLiteral("停止执行"));
  btn_stop_->setEnabled(false);

  layout->addWidget(btn_run_);
  layout->addWidget(btn_pause_);
  layout->addWidget(btn_stop_);

  // ── Separator ──
  auto* sep = new QWidget(this);
  sep->setFixedWidth(1);
  sep->setObjectName(QStringLiteral("execControlSeparator"));
  layout->addWidget(sep);

  // ── Spacer ──
  layout->addStretch(1);

  // ── Stats label ──
  label_stats_ = new QLabel(QStringLiteral("✅ 0  ❌ 0  ⏱ 0s"), this);
  label_stats_->setObjectName(QStringLiteral("execControlStats"));
  layout->addWidget(label_stats_);

  // ── Status label ──
  label_status_ = new QLabel(QStringLiteral("就绪"), this);
  label_status_->setObjectName(QStringLiteral("execControlStatus"));
  layout->addWidget(label_status_);
}

void ExecutionControlBar::initSignals() {
  connect(btn_run_, &QPushButton::clicked, this,
          &ExecutionControlBar::runClicked);
  connect(btn_pause_, &QPushButton::clicked, this,
          &ExecutionControlBar::pauseClicked);
  connect(btn_stop_, &QPushButton::clicked, this,
          &ExecutionControlBar::stopClicked);
}

void ExecutionControlBar::setState(State state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  updateButtonStates();
}

void ExecutionControlBar::updateButtonStates() {
  switch (state_) {
    case State::Idle:
    case State::Finished:
      btn_run_->setEnabled(true);
      btn_pause_->setEnabled(false);
      btn_stop_->setEnabled(false);
      btn_pause_->setText(QStringLiteral("⏸ 暂停"));
      break;
    case State::Running:
      btn_run_->setEnabled(false);
      btn_pause_->setEnabled(true);
      btn_stop_->setEnabled(true);
      btn_pause_->setText(QStringLiteral("⏸ 暂停"));
      break;
    case State::Paused:
      btn_run_->setEnabled(false);
      btn_pause_->setEnabled(true);
      btn_stop_->setEnabled(true);
      btn_pause_->setText(QStringLiteral("▶ 继续"));
      break;
  }
}

void ExecutionControlBar::updateStats(int pass, int fail, int elapsedMs) {
  pass_count_ = pass;
  fail_count_ = fail;
  elapsed_ms_ = elapsedMs;
  refreshStatsLabel();
}

void ExecutionControlBar::setStatusText(const QString& text) {
  if (label_status_) {
    label_status_->setText(text);
  }
}

void ExecutionControlBar::resetStats() {
  pass_count_ = 0;
  fail_count_ = 0;
  elapsed_ms_ = 0;
  refreshStatsLabel();
}

void ExecutionControlBar::refreshStatsLabel() {
  if (!label_stats_) {
    return;
  }
  int secs = elapsed_ms_ / 1000;
  label_stats_->setText(
      QStringLiteral("✅ %1  ❌ %2  ⏱ %3s")
          .arg(pass_count_)
          .arg(fail_count_)
          .arg(secs));
}

}  // namespace etest::app
