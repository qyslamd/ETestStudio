#include <gtest/gtest.h>
#include "crashhandler/CrashHandler.h"
#ifdef Q_OS_WIN
#include "crashhandler/WindowsCrashHandler.h"
#include <intrin.h>
#endif
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <functional>

// 普通功能测试（可自动运行，无崩溃）
TEST(CrashHandlerTest, CreateInstance) {
    auto handler = CrashHandler::create();
#ifdef Q_OS_WIN
    EXPECT_NE(handler, nullptr);
#else
    EXPECT_EQ(handler, nullptr);
#endif
}

TEST(CrashHandlerTest, DefaultDumpPath) {
#ifdef Q_OS_WIN
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    
    QString expectedPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/etest/crash/";
    EXPECT_TRUE(QDir(expectedPath).exists());
#endif
}

TEST(CrashHandlerTest, SetCustomDumpPath) {
#ifdef Q_OS_WIN
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    
    QString customPath = QCoreApplication::applicationDirPath() + "/test_crash_dump/";
    handler->setDumpPath(customPath);
    EXPECT_TRUE(QDir(customPath).exists());
    
    // 测试完成清理临时目录
    QDir(customPath).removeRecursively();
#endif
}

TEST(CrashHandlerTest, GenerateFileName) {
    auto handler = CrashHandler::create();
    if (!handler) {
        GTEST_SKIP() << "CrashHandler not implemented on current platform";
    }
    
    QString fileName = handler->generateCrashFileName();
    EXPECT_TRUE(fileName.startsWith("etest_crash_"));
    EXPECT_TRUE(fileName.endsWith(".log"));
    EXPECT_EQ(fileName.size(), 29); // "etest_crash_YYYYMMDD_HHMMSS.log" 长度固定29
}

TEST(CrashHandlerTest, CollectCommonInfo) {
    auto handler = CrashHandler::create();
    if (!handler) {
        GTEST_SKIP() << "CrashHandler not implemented on current platform";
    }
    
    QString info = handler->collectCommonInfo();
    EXPECT_TRUE(info.contains("系统信息"));
    EXPECT_TRUE(info.contains("操作系统"));
    EXPECT_TRUE(info.contains("程序信息"));
    EXPECT_TRUE(info.contains("程序路径"));
    EXPECT_TRUE(info.contains("进程ID"));
}

// 崩溃捕获测试（手动运行，前缀DISABLED_避免自动测试触发崩溃）
TEST(CrashHandlerTest, DISABLED_CatchNullPointerCrash) {
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    handler->init();
    
    // 触发空指针访问异常
    int* p = nullptr;
    *p = 1;
}

TEST(CrashHandlerTest, DISABLED_CatchDivideByZeroCrash) {
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    handler->init();
    
    // 触发除零异常
    int a = 1;
    int b = 0;
    int c = a / b;
}

TEST(CrashHandlerTest, DISABLED_CatchStackOverflowCrash) {
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    handler->init();
    
    // 触发栈溢出异常
    std::function<void()> recursive = [&]() { recursive(); };
    recursive();
}

TEST(CrashHandlerTest, DISABLED_CatchIllegalInstructionCrash) {
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    handler->init();
    
    // 触发非法指令异常（x64平台非法指令
    __ud2();
}

TEST(CrashHandlerTest, DISABLED_CatchArrayOutOfBoundsCrash) {
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    handler->init();
    
    // 触发数组越界访问
    int arr[2] = {1, 2};
    volatile int val = arr[100];
    (void)val;
}

TEST(CrashHandlerTest, DISABLED_CatchPageFaultCrash) {
    auto handler = CrashHandler::create();
    ASSERT_NE(handler, nullptr);
    handler->init();
    
    // 触发页错误异常：访问内核地址空间
    volatile char* p = reinterpret_cast<char*>(0xFFFFFFFFFFFFFFFFULL);
    char val = *p;
    (void)val;
}
