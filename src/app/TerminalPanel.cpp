#include "TerminalPanel.h"
#include <QProcess>

namespace etest {
namespace app {

TerminalPanel::TerminalPanel(QWidget* parent) : QWidget(parent) {
  setupUi();
}

void TerminalPanel::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);

  open_terminal_btn_ =
      new QPushButton(QStringLiteral("打开系统终端"), this);
  layout->addWidget(open_terminal_btn_);
  layout->addStretch();

  connect(open_terminal_btn_, &QPushButton::clicked, this, []() {
#ifdef Q_OS_WIN
    QProcess::startDetached("cmd.exe", QStringList());
#elif defined(Q_OS_LINUX)
    QProcess::startDetached("x-terminal-emulator", QStringList());
#endif
  });
}

}  // namespace app
}  // namespace etest
