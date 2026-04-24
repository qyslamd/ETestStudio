#ifndef ETEST_APP_TERMINAL_PANEL_H_
#define ETEST_APP_TERMINAL_PANEL_H_

#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace etest {
namespace app {

class TerminalPanel : public QWidget {
  Q_OBJECT

 public:
  explicit TerminalPanel(QWidget* parent = nullptr);

 private:
  void setupUi();

  QPushButton* open_terminal_btn_;
};

}  // namespace app
}  // namespace etest

#endif  // ETEST_APP_TERMINAL_PANEL_H_
