#ifndef ETEST_CORE_TERMINAL_POSIXPTYPROCESS_H_
#define ETEST_CORE_TERMINAL_POSIXPTYPROCESS_H_

#include "terminal/PtyProcess.h"

#ifndef Q_OS_WIN

namespace etest {
namespace core {
namespace terminal {

class PosixPtyProcess : public PtyProcess {
  Q_OBJECT

 public:
  explicit PosixPtyProcess(QObject* parent = nullptr);
  ~PosixPtyProcess() override;

  bool start(const QString& command, int cols, int rows) override;
  void write(const QByteArray& data) override;
  QByteArray readAll() override;
  void resize(int cols, int rows) override;
  void terminate() override;
  bool isRunning() const override;
};

}  // namespace terminal
}  // namespace core
}  // namespace etest

#endif  // !Q_OS_WIN

#endif  // ETEST_CORE_TERMINAL_POSIXPTYPROCESS_H_
