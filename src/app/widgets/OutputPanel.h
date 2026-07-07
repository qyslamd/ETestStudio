#ifndef ETEST_APP_OUTPUT_PANEL_H_
#define ETEST_APP_OUTPUT_PANEL_H_

#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include "logger/LogHistoryBuffer.h"

namespace etest::app {

class OutputPanel : public QWidget {
  Q_OBJECT

 public:
  explicit OutputPanel(QWidget* parent = nullptr);

  void appendLog(int level, const QString& text);
  void clearLog();

 private:
  void initUi();
  QString levelColor(int level) const;
  QString levelName(int level) const;

  QTextEdit* text_edit_;
  static constexpr int kMaxLines = 5000;

 private slots:
  void onHistoricalLogs(const QList<etest::core::logger::LogEntry>& entries);
};

}  // namespace etest::app

#endif  // ETEST_APP_OUTPUT_PANEL_H_
