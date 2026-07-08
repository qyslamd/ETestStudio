#ifndef ETEST_PROGRAM_EXECUTION_CONTROL_BAR_H_
#define ETEST_PROGRAM_EXECUTION_CONTROL_BAR_H_

#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace etest::app {

class ExecutionControlBar : public QWidget {
  Q_OBJECT

 public:
  enum class State { Idle, Running, Paused, Finished };

  explicit ExecutionControlBar(QWidget* parent = nullptr);
  ~ExecutionControlBar() override = default;

  // State management
  void setState(State state);
  State state() const { return state_; }

  // Stats update
  void updateStats(int pass, int fail, int elapsedMs);
  void setStatusText(const QString& text);
  void resetStats();

  // Accessors for wiring
  QPushButton* runButton() const { return btn_run_; }
  QPushButton* pauseButton() const { return btn_pause_; }
  QPushButton* stopButton() const { return btn_stop_; }

 signals:
  void runClicked();
  void pauseClicked();
  void stopClicked();

 private:
  void initUi();
  void initSignals();
  void updateButtonStates();
  void refreshStatsLabel();

  State state_ = State::Idle;
  int pass_count_ = 0;
  int fail_count_ = 0;
  int elapsed_ms_ = 0;

  QPushButton* btn_run_ = nullptr;
  QPushButton* btn_pause_ = nullptr;
  QPushButton* btn_stop_ = nullptr;
  QLabel* label_stats_ = nullptr;
  QLabel* label_status_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_EXECUTION_CONTROL_BAR_H_
