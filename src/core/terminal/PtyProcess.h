#ifndef ETEST_CORE_TERMINAL_PTYPROCESS_H_
#define ETEST_CORE_TERMINAL_PTYPROCESS_H_

#include <QByteArray>
#include <QObject>

namespace etest {
namespace core {
namespace terminal {

class PtyProcess : public QObject {
  Q_OBJECT

 public:
  explicit PtyProcess(QObject* parent = nullptr) : QObject(parent) {}
  ~PtyProcess() override;

  static PtyProcess* create(QObject* parent = nullptr);

  virtual bool start(const QString& command, int cols, int rows) = 0;
  virtual void write(const QByteArray& data) = 0;
  virtual QByteArray readAll() = 0;
  virtual void resize(int cols, int rows) = 0;
  virtual void terminate() = 0;
  virtual bool isRunning() const = 0;

 signals:
  void readyRead();
  void processFinished(int exitCode);
};

}  // namespace terminal
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_TERMINAL_PTYPROCESS_H_
