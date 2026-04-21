#include "CrashHandler.h"
#include <QDateTime>
#include <QSysInfo>
#include <QCoreApplication>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include "WindowsCrashHandler.h"
#endif

std::unique_ptr<CrashHandler> CrashHandler::create() {
#ifdef Q_OS_WIN
    return std::make_unique<WindowsCrashHandler>();
#else
    // TODO: Linux平台实现
    return nullptr;
#endif
}

QString CrashHandler::generateCrashFileName() const {
    return QString("etest_crash_%1.log")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
}

QString CrashHandler::collectCommonInfo() const {
    QString info;
    info += "=== 系统信息 ===\n";
    info += QString("操作系统：%1 %2\n")
        .arg(QSysInfo::prettyProductName())
        .arg(QSysInfo::currentCpuArchitecture());
    info += QString("内核版本：%1\n").arg(QSysInfo::kernelVersion());
    info += "\n=== 程序信息 ===\n";
    info += QString("程序版本：%1\n").arg(QCoreApplication::applicationVersion());
    info += QString("程序路径：%1\n").arg(QCoreApplication::applicationFilePath());
    info += QString("进程ID：%1\n").arg(QCoreApplication::applicationPid());
    info += "\n";
    return info;
}
