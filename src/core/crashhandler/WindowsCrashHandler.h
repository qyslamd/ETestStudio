#include <QtGlobal>
#ifdef Q_OS_WIN

#ifndef ETEST_CORE_CRASHHANDLER_WINDOWSCRASHHANDLER_H_
#define ETEST_CORE_CRASHHANDLER_WINDOWSCRASHHANDLER_H_

#include "CrashHandler.h"
#include <windows.h>
#include <dbghelp.h>

namespace etest {
namespace core {
namespace crashhandler {

class WindowsCrashHandler : public CrashHandler {
public:
    WindowsCrashHandler();
    ~WindowsCrashHandler() override;

    bool init() override;
    void setDumpPath(const QString& path) override;
    void setCrashCallback(std::function<void(const QString& crashLog)> callback) override;

private:
    static LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS pExceptionInfo);
    static LONG WINAPI vectoredHandler(PEXCEPTION_POINTERS pExceptionInfo);
    static QString getExceptionString(DWORD exceptionCode);
    static QString getCallStack(CONTEXT* context);
    static QString getRegisterContext(CONTEXT* context);

    static WindowsCrashHandler* s_instance;
    QString m_dumpPath;
    std::function<void(const QString&)> m_crashCallback;
    LPTOP_LEVEL_EXCEPTION_FILTER m_prevFilter;
    PVOID m_vectoredHandler = nullptr;
};

}  // namespace crashhandler
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_CRASHHANDLER_WINDOWSCRASHHANDLER_H_

#endif // Q_OS_WIN
