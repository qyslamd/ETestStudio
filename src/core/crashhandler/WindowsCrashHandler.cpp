#include <QtGlobal>
#ifdef Q_OS_WIN

#include "WindowsCrashHandler.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QDebug>

#pragma comment(lib, "dbghelp.lib")

namespace etest {
namespace core {
namespace crashhandler {

WindowsCrashHandler* WindowsCrashHandler::s_instance = nullptr;

WindowsCrashHandler::WindowsCrashHandler() 
    : m_prevFilter(nullptr) {
    s_instance = this;
    // 默认崩溃日志路径
    QString docPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    m_dumpPath = docPath + "/etest/crash/";
    QDir().mkpath(m_dumpPath);
}

WindowsCrashHandler::~WindowsCrashHandler() {
    if (m_prevFilter) {
        SetUnhandledExceptionFilter(m_prevFilter);
    }
    s_instance = nullptr;
}

bool WindowsCrashHandler::init() {
    m_prevFilter = SetUnhandledExceptionFilter(exceptionHandler);
    return m_prevFilter != nullptr;
}

void WindowsCrashHandler::setDumpPath(const QString& path) {
    m_dumpPath = path;
    QDir().mkpath(m_dumpPath);
}

void WindowsCrashHandler::setCrashCallback(std::function<void(const QString& crashLog)> callback) {
    m_crashCallback = std::move(callback);
}

LONG WINAPI WindowsCrashHandler::exceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    if (!s_instance) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // 收集崩溃信息
    QString crashLog;
    crashLog += "=== 崩溃信息 ===\n";
    crashLog += QString("异常代码：%1 (%2)\n")
        .arg(pExceptionInfo->ExceptionRecord->ExceptionCode, 8, 16, QChar('0'))
        .arg(getExceptionString(pExceptionInfo->ExceptionRecord->ExceptionCode));
    crashLog += QString("异常地址：0x%1\n")
        .arg(reinterpret_cast<quintptr>(pExceptionInfo->ExceptionRecord->ExceptionAddress), 
             QT_POINTER_SIZE * 2, 16, QChar('0'));
    
    crashLog += "\n=== 寄存器上下文 ===\n";
    crashLog += getRegisterContext(pExceptionInfo->ContextRecord);
    
    crashLog += "\n=== 调用栈 ===\n";
    crashLog += getCallStack(pExceptionInfo->ContextRecord);
    
    crashLog += s_instance->collectCommonInfo();

    // 写入日志文件
    QString fileName = s_instance->generateCrashFileName();
    QString fullPath = s_instance->m_dumpPath + fileName;
    
    QFile file(fullPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(crashLog.toUtf8());
        file.close();
    }

    // 执行回调
    if (s_instance->m_crashCallback) {
        s_instance->m_crashCallback(crashLog);
    }

    // 显示友好提示
    QMessageBox::critical(nullptr, "程序崩溃", 
        QString("程序发生异常，已生成崩溃日志：\n%1\n\n请联系开发者解决问题。").arg(fullPath),
        QMessageBox::Ok);

    // 终止程序
    TerminateProcess(GetCurrentProcess(), pExceptionInfo->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

QString WindowsCrashHandler::getExceptionString(DWORD exceptionCode) {
    switch (exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION: return "内存访问违规";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "数组越界";
        case EXCEPTION_BREAKPOINT: return "断点触发";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "数据类型未对齐";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "浮点操作数异常";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "浮点除零";
        case EXCEPTION_FLT_INEXACT_RESULT: return "浮点结果不精确";
        case EXCEPTION_FLT_INVALID_OPERATION: return "无效浮点操作";
        case EXCEPTION_FLT_OVERFLOW: return "浮点溢出";
        case EXCEPTION_FLT_STACK_CHECK: return "浮点栈检查失败";
        case EXCEPTION_FLT_UNDERFLOW: return "浮点下溢";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "非法指令";
        case EXCEPTION_IN_PAGE_ERROR: return "页错误";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "整数除零";
        case EXCEPTION_INT_OVERFLOW: return "整数溢出";
        case EXCEPTION_INVALID_DISPOSITION: return "无效处置";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "不可继续异常";
        case EXCEPTION_PRIV_INSTRUCTION: return "特权指令";
        case EXCEPTION_SINGLE_STEP: return "单步执行";
        case EXCEPTION_STACK_OVERFLOW: return "栈溢出";
        default: return "未知异常";
    }
}

QString WindowsCrashHandler::getRegisterContext(CONTEXT* context) {
    QString regs;
#ifdef _WIN64
    regs += QString("RAX: 0x%1\n").arg(context->Rax, 16, 16, QChar('0'));
    regs += QString("RBX: 0x%1\n").arg(context->Rbx, 16, 16, QChar('0'));
    regs += QString("RCX: 0x%1\n").arg(context->Rcx, 16, 16, QChar('0'));
    regs += QString("RDX: 0x%1\n").arg(context->Rdx, 16, 16, QChar('0'));
    regs += QString("RSP: 0x%1\n").arg(context->Rsp, 16, 16, QChar('0'));
    regs += QString("RBP: 0x%1\n").arg(context->Rbp, 16, 16, QChar('0'));
    regs += QString("RSI: 0x%1\n").arg(context->Rsi, 16, 16, QChar('0'));
    regs += QString("RDI: 0x%1\n").arg(context->Rdi, 16, 16, QChar('0'));
    regs += QString("RIP: 0x%1\n").arg(context->Rip, 16, 16, QChar('0'));
#else
    regs += QString("EAX: 0x%1\n").arg(context->Eax, 8, 16, QChar('0'));
    regs += QString("EBX: 0x%1\n").arg(context->Ebx, 8, 16, QChar('0'));
    regs += QString("ECX: 0x%1\n").arg(context->Ecx, 8, 16, QChar('0'));
    regs += QString("EDX: 0x%1\n").arg(context->Edx, 8, 16, QChar('0'));
    regs += QString("ESP: 0x%1\n").arg(context->Esp, 8, 16, QChar('0'));
    regs += QString("EBP: 0x%1\n").arg(context->Ebp, 8, 16, QChar('0'));
    regs += QString("ESI: 0x%1\n").arg(context->Esi, 8, 16, QChar('0'));
    regs += QString("EDI: 0x%1\n").arg(context->Edi, 8, 16, QChar('0'));
    regs += QString("EIP: 0x%1\n").arg(context->Eip, 8, 16, QChar('0'));
#endif
    return regs;
}

QString WindowsCrashHandler::getCallStack(CONTEXT* context) {
    QString stack;
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymInitialize(process, nullptr, TRUE);
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

    STACKFRAME64 stackFrame = {0};
#ifdef _WIN64
    stackFrame.AddrPC.Offset = context->Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Rbp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#else
    stackFrame.AddrPC.Offset = context->Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = context->Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = context->Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    DWORD machineType = IMAGE_FILE_MACHINE_I386;
#endif

    char symbolBuffer[sizeof(SYMBOL_INFO) + 256] = {0};
    PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = 255;

    IMAGEHLP_LINE64 lineInfo = {0};
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (int i = 0; i < 64; ++i) {
        if (!StackWalk64(machineType, process, thread, &stackFrame, context, 
                        nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
            break;
        }

        if (stackFrame.AddrPC.Offset == 0) {
            break;
        }

        DWORD64 displacement = 0;
        if (SymFromAddr(process, stackFrame.AddrPC.Offset, &displacement, pSymbol)) {
            stack += QString("%1: 0x%2 %3()")
                .arg(i, 2)
                .arg(stackFrame.AddrPC.Offset, QT_POINTER_SIZE * 2, 16, QChar('0'))
                .arg(pSymbol->Name);
            
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, stackFrame.AddrPC.Offset, &lineDisplacement, &lineInfo)) {
                stack += QString(" %1:%2").arg(lineInfo.FileName).arg(lineInfo.LineNumber);
            }
            stack += "\n";
        } else {
            stack += QString("%1: 0x%2 [未知函数]\n")
                .arg(i, 2)
                .arg(stackFrame.AddrPC.Offset, QT_POINTER_SIZE * 2, 16, QChar('0'));
        }
    }

    SymCleanup(process);
    return stack;
}

}  // namespace crashhandler
}  // namespace core
}  // namespace etest

#endif // Q_OS_WIN
