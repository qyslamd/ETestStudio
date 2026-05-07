#include "terminal/PtyProcess.h"

#ifdef Q_OS_WIN
#include "terminal/ConPtyProcess.h"
#else
#include "terminal/PosixPtyProcess.h"
#endif

namespace etest {
namespace core {
namespace terminal {

PtyProcess::~PtyProcess() = default;

PtyProcess* PtyProcess::create(QObject* parent) {
#ifdef Q_OS_WIN
  return new ConPtyProcess(parent);
#else
  return new PosixPtyProcess(parent);
#endif
}

}  // namespace terminal
}  // namespace core
}  // namespace etest
