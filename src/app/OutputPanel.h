#ifndef ETEST_APP_OUTPUT_PANEL_H_
#define ETEST_APP_OUTPUT_PANEL_H_

#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

class OutputPanel : public QWidget {
  Q_OBJECT

 public:
  explicit OutputPanel(QWidget* parent = nullptr);

  void appendLog(const QString& text);
  void clearLog();

 private:
  void setupUi();

  QTextEdit* text_edit_;
};

#endif  // ETEST_APP_OUTPUT_PANEL_H_
