#include "terminal/PosixPtyProcess.h"

#ifndef Q_OS_WIN

namespace etest {
namespace core {
namespace terminal {

PosixPtyProcess::PosixPtyProcess(QObject* parent) : PtyProcess(parent) {}

PosixPtyProcess::~PosixPtyProcess() {
  terminate();
}

bool PosixPtyProcess::start(const QString& command, int cols, int rows) {
  // TODO: Implement with openpty() + fork() + exec()
  Q_UNUSED(command)
  Q_UNUSED(cols)
  Q_UNUSED(rows)
  return false;
}

void PosixPtyProcess::write(const QByteArray& data) {
  Q_UNUSED(data)
}

QByteArray PosixPtyProcess::readAll() {
  return {};
}

void PosixPtyProcess::resize(int cols, int rows) {
  Q_UNUSED(cols)
  Q_UNUSED(rows)
}

void PosixPtyProcess::terminate() {}

bool PosixPtyProcess::isRunning() const {
  return false;
}

}  // namespace terminal
}  // namespace core
}  // namespace etest

#endif  // !Q_OS_WIN
