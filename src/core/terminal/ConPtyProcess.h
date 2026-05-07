#ifndef ETEST_CORE_TERMINAL_CONPTYPROCESS_H_
#define ETEST_CORE_TERMINAL_CONPTYPROCESS_H_

#include "terminal/PtyProcess.h"

#ifdef Q_OS_WIN

#include <windows.h>

class QWinEventNotifier;

namespace etest {
namespace core {
namespace terminal {

class ConPtyProcess : public PtyProcess {
  Q_OBJECT

 public:
  explicit ConPtyProcess(QObject* parent = nullptr);
  ~ConPtyProcess() override;

  bool start(const QString& command, int cols, int rows) override;
  void write(const QByteArray& data) override;
  QByteArray readAll() override;
  void resize(int cols, int rows) override;
  void terminate() override;
  bool isRunning() const override;

 private:
  void cleanup();
  void onPipeReadable();

  HPCON hpc_ = nullptr;
  HANDLE hPipeIn_ = nullptr;   // App writes → ConPTY reads
  HANDLE hPipeOut_ = nullptr;  // ConPTY writes → App reads
  HANDLE hProcess_ = nullptr;
  QWinEventNotifier* notifier_ = nullptr;

  // Dynamically loaded ConPTY functions
  static HRESULT(WINAPI* pCreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD,
                                               HPCON*);
  static HRESULT(WINAPI* pResizePseudoConsole)(HPCON, COORD);
  static void(WINAPI* pClosePseudoConsole)(HPCON);
  static bool conptyLoaded_;
  static bool loadConPtyFunctions();
};

}  // namespace terminal
}  // namespace core
}  // namespace etest

#endif  // Q_OS_WIN

#endif  // ETEST_CORE_TERMINAL_CONPTYPROCESS_H_
