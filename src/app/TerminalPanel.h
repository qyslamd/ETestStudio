#ifndef ETEST_APP_TERMINAL_PANEL_H_
#define ETEST_APP_TERMINAL_PANEL_H_

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class TerminalPanel : public QWidget {
  Q_OBJECT

 public:
  explicit TerminalPanel(QWidget* parent = nullptr);

 private:
  void setupUi();

  QPushButton* open_terminal_btn_;
};

#endif  // ETEST_APP_TERMINAL_PANEL_H_
