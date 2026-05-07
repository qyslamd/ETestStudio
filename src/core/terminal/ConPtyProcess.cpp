#include "terminal/ConPtyProcess.h"

#ifdef Q_OS_WIN

#include <QWinEventNotifier>

#include <QDebug>

namespace etest {
namespace core {
namespace terminal {

// Static ConPTY function pointers
HRESULT(WINAPI* ConPtyProcess::pCreatePseudoConsole)(COORD, HANDLE, HANDLE,
                                                     DWORD, HPCON*) = nullptr;
HRESULT(WINAPI* ConPtyProcess::pResizePseudoConsole)(HPCON, COORD) = nullptr;
void(WINAPI* ConPtyProcess::pClosePseudoConsole)(HPCON) = nullptr;
bool ConPtyProcess::conptyLoaded_ = false;

bool ConPtyProcess::loadConPtyFunctions() {
  if (conptyLoaded_) {
    return pCreatePseudoConsole != nullptr;
  }
  conptyLoaded_ = true;

  HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
  if (!hKernel32) return false;

  pCreatePseudoConsole = reinterpret_cast<decltype(pCreatePseudoConsole)>(
      GetProcAddress(hKernel32, "CreatePseudoConsole"));
  pResizePseudoConsole = reinterpret_cast<decltype(pResizePseudoConsole)>(
      GetProcAddress(hKernel32, "ResizePseudoConsole"));
  pClosePseudoConsole = reinterpret_cast<decltype(pClosePseudoConsole)>(
      GetProcAddress(hKernel32, "ClosePseudoConsole"));

  return pCreatePseudoConsole != nullptr && pResizePseudoConsole != nullptr &&
         pClosePseudoConsole != nullptr;
}

ConPtyProcess::ConPtyProcess(QObject* parent) : PtyProcess(parent) {}

ConPtyProcess::~ConPtyProcess() {
  terminate();
}

bool ConPtyProcess::start(const QString& command, int cols, int rows) {
  if (!loadConPtyFunctions()) {
    qWarning() << "ConPTY not available (requires Windows 10 1809+)";
    return false;
  }

  // Create two pipes: one for ConPTY input, one for ConPTY output
  HANDLE hPipeInRead, hPipeInWrite;
  HANDLE hPipeOutRead, hPipeOutWrite;

  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

  if (!CreatePipe(&hPipeInRead, &hPipeInWrite, &sa, 0)) return false;
  if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, &sa, 0)) {
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeInWrite);
    return false;
  }

  // App writes to hPipeInWrite, reads from hPipeOutRead
  // ConPTY reads from hPipeInRead, writes to hPipeOutWrite
  hPipeIn_ = hPipeInWrite;
  hPipeOut_ = hPipeOutRead;

  // Create the pseudo console
  COORD size;
  size.X = static_cast<SHORT>(cols);
  size.Y = static_cast<SHORT>(rows);

  HRESULT hr = pCreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hpc_);
  if (FAILED(hr)) {
    qWarning() << "CreatePseudoConsole failed: 0x" << QString::number(hr, 16);
    CloseHandle(hPipeInRead);
    CloseHandle(hPipeInWrite);
    CloseHandle(hPipeOutRead);
    CloseHandle(hPipeOutWrite);
    hPipeIn_ = hPipeOut_ = nullptr;
    return false;
  }

  // ConPTY now owns hPipeInRead and hPipeOutWrite; we keep hPipeInWrite and
  // hPipeOutRead. But we must NOT close hPipeInRead/hPipeOutWrite here because
  // ConPTY needs them — they will be closed by ClosePseudoConsole.

  // Initialize startup info with ConPTY attribute
  STARTUPINFOEXW siex = {};
  siex.StartupInfo.cb = sizeof(STARTUPINFOEXW);

  SIZE_T attrListSize = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);

  auto* attrList =
      reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));
  if (!attrList) {
    cleanup();
    return false;
  }

  if (!InitializeProcThreadAttributeList(attrList, 1, 0, &attrListSize)) {
    free(attrList);
    cleanup();
    return false;
  }

  if (!UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                  hpc_, sizeof(hpc_), nullptr, nullptr)) {
    DeleteProcThreadAttributeList(attrList);
    free(attrList);
    cleanup();
    return false;
  }

  siex.lpAttributeList = attrList;

  // Build command line
  WCHAR cmdLine[MAX_PATH] = {};
  wcscpy_s(cmdLine, command.toStdWString().c_str());

  PROCESS_INFORMATION pi = {};
  DWORD creationFlags = EXTENDED_STARTUPINFO_PRESENT;

  BOOL ok = CreateProcessW(
      nullptr, cmdLine, nullptr, nullptr,
      FALSE,  // Don't inherit handles
      creationFlags, nullptr, nullptr, &siex.StartupInfo, &pi);

  DeleteProcThreadAttributeList(attrList);
  free(attrList);

  if (!ok) {
    qWarning() << "CreateProcess failed:" << GetLastError();
    cleanup();
    return false;
  }

  hProcess_ = pi.hProcess;
  CloseHandle(pi.hThread);  // Don't need thread handle

  // Monitor output pipe for readable data
  notifier_ = new QWinEventNotifier(hPipeOut_, this);
  connect(notifier_, &QWinEventNotifier::activated, this,
          [this](HANDLE) { onPipeReadable(); });

  return true;
}

void ConPtyProcess::write(const QByteArray& data) {
  if (!hPipeIn_) return;
  DWORD written = 0;
  WriteFile(hPipeIn_, data.constData(), static_cast<DWORD>(data.size()), &written,
            nullptr);
}

QByteArray ConPtyProcess::readAll() {
  if (!hPipeOut_) return {};

  QByteArray result;
  char buffer[4096];
  DWORD bytesRead = 0;

  while (PeekNamedPipe(hPipeOut_, nullptr, 0, nullptr, &bytesRead, nullptr) &&
         bytesRead > 0) {
    DWORD toRead = qMin<DWORD>(sizeof(buffer), bytesRead);
    if (!ReadFile(hPipeOut_, buffer, toRead, &bytesRead, nullptr) || bytesRead == 0)
      break;
    result.append(buffer, bytesRead);
  }

  return result;
}

void ConPtyProcess::resize(int cols, int rows) {
  if (!hpc_ || !pResizePseudoConsole) return;
  COORD size;
  size.X = static_cast<SHORT>(cols);
  size.Y = static_cast<SHORT>(rows);
  pResizePseudoConsole(hpc_, size);
}

void ConPtyProcess::terminate() {
  cleanup();
}

bool ConPtyProcess::isRunning() const {
  if (!hProcess_) return false;
  DWORD exitCode = 0;
  if (!GetExitCodeProcess(hProcess_, &exitCode)) return false;
  return exitCode == STILL_ACTIVE;
}

void ConPtyProcess::cleanup() {
  delete notifier_;
  notifier_ = nullptr;

  if (hProcess_) {
    if (isRunning()) {
      TerminateProcess(hProcess_, 0);
      WaitForSingleObject(hProcess_, 3000);
    }
    CloseHandle(hProcess_);
    hProcess_ = nullptr;
  }

  if (hpc_) {
    if (pClosePseudoConsole) {
      pClosePseudoConsole(hpc_);
    }
    hpc_ = nullptr;
  }

  // hPipeInRead and hPipeOutWrite are owned by ConPTY and closed by
  // ClosePseudoConsole, so we only close our own ends
  if (hPipeIn_) {
    CloseHandle(hPipeIn_);
    hPipeIn_ = nullptr;
  }
  if (hPipeOut_) {
    CloseHandle(hPipeOut_);
    hPipeOut_ = nullptr;
  }
}

void ConPtyProcess::onPipeReadable() {
  // Check if the child process has exited
  if (hProcess_) {
    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess_, &exitCode) && exitCode != STILL_ACTIVE) {
      // Read any remaining output before signaling
      QByteArray remaining = readAll();
      if (!remaining.isEmpty()) {
        emit readyRead();
      }
      emit processFinished(static_cast<int>(exitCode));
      cleanup();
      return;
    }
  }

  emit readyRead();
}

}  // namespace terminal
}  // namespace core
}  // namespace etest

#endif  // Q_OS_WIN
