#ifndef ETEST_APP_EXECUTION_OUTPUT_PANEL_H_
#define ETEST_APP_EXECUTION_OUTPUT_PANEL_H_

#include <QWidget>

#include "engine/StepRunner.h"

class QTextEdit;

namespace etest::app {

class ExecutionOutputPanel : public QWidget {
  Q_OBJECT

 public:
  explicit ExecutionOutputPanel(QWidget* parent = nullptr);

  void appendResult(const etest::engine::StepResult& result);
  void appendText(const QString& text);
  void clearOutput();

 private:
  void initUi();
  void trimToMaxLines();
  void scrollToBottom();
  QString statusIcon(etest::engine::StepStatus status) const;

  QTextEdit* text_edit_;
  static constexpr int kMaxLines = 10000;
};

}  // namespace etest::app

#endif  // ETEST_APP_EXECUTION_OUTPUT_PANEL_H_
