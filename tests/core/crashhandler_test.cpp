#include "crashhandler/CrashHandler.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <functional>

#ifdef Q_OS_WIN
#include <intrin.h>
#include "crashhandler/WindowsCrashHandler.h"
#else
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#endif

using namespace etest::core::crashhandler;

// ═══════════════════════════════════════════════════════════════════════════════
// 单元测试：跨平台功能验证
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CrashHandlerTest, CreateInstance) {
  auto handler = CrashHandler::create();
  EXPECT_NE(handler, nullptr);
}

TEST(CrashHandlerTest, DefaultDumpPath) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  QString expectedPath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      "/crash/";
  EXPECT_TRUE(QDir(expectedPath).exists());
}

TEST(CrashHandlerTest, SetCustomDumpPath) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  QString customPath =
      QCoreApplication::applicationDirPath() + "/test_crash_dump/";
  handler->setDumpPath(customPath);
  EXPECT_TRUE(QDir(customPath).exists());

  QDir(customPath).removeRecursively();
}

TEST(CrashHandlerTest, InitReturnsTrue) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  EXPECT_TRUE(handler->init());
}

TEST(CrashHandlerTest, GenerateFileName) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  QString fileName = handler->generateCrashFileName();
  EXPECT_TRUE(fileName.startsWith("etest_crash_"));
  EXPECT_TRUE(fileName.endsWith(".log"));
  // "etest_crash_YYYYMMDD_HHmmss.log" = 31 chars
  EXPECT_EQ(fileName.size(), 31);
}

TEST(CrashHandlerTest, CollectCommonInfo) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);

  QString info = handler->collectCommonInfo();
  EXPECT_TRUE(info.contains("系统信息"));
  EXPECT_TRUE(info.contains("操作系统"));
  EXPECT_TRUE(info.contains("程序信息"));
  EXPECT_TRUE(info.contains("程序路径"));
  EXPECT_TRUE(info.contains("进程ID"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// 集成测试：fork 子进程触发崩溃，验证日志生成（仅 Linux）
// ═══════════════════════════════════════════════════════════════════════════════

#ifndef Q_OS_WIN

/// 裸函数递归触发栈溢出，避免 std::function 的堆操作干扰
__attribute__((noinline)) static void crashRecurse(volatile int* depth) {
  (*depth)++;
  crashRecurse(depth);
}

/// fork 子进程触发 SIGSEGV，验证崩溃日志生成
TEST(CrashHandlerTest, ForkSegfaultGeneratesCrashLog) {
  QTemporaryDir tmpDir;
  ASSERT_TRUE(tmpDir.isValid());
  QString dumpPath = tmpDir.path() + "/crash";

  pid_t pid = fork();
  ASSERT_GE(pid, 0) << "fork failed";

  if (pid == 0) {
    // 子进程：初始化 crash handler 后触发 SIGSEGV
    auto handler = CrashHandler::create();
    handler->setDumpPath(dumpPath);
    handler->init();

    // 触发空指针解引用 -> SIGSEGV
    int* p = nullptr;
    *p = 42;
    _exit(1);  // 不应该到达这里
  }

  // 父进程：等待子进程退出
  int status = 0;
  waitpid(pid, &status, 0);
  // _exit(128+SIGSEGV) 是正常退出（exit code 139），不是被信号终止
  EXPECT_TRUE(WIFEXITED(status)) << "子进程应正常退出（_exit）";
  EXPECT_EQ(WEXITSTATUS(status), 128 + SIGSEGV)
      << "退出码应为 128+SIGSEGV=139";

  // 验证崩溃日志文件已生成
  QDir crashDir(dumpPath);
  EXPECT_TRUE(crashDir.exists()) << "crash 目录应存在";

  QStringList filters = {"etest_crash_*.log"};
  QStringList logFiles = crashDir.entryList(filters, QDir::Files);
  EXPECT_FALSE(logFiles.isEmpty()) << "应生成至少一个崩溃日志文件";

  if (!logFiles.isEmpty()) {
    // 读取日志内容，验证包含关键信息
    QString logPath = crashDir.absoluteFilePath(logFiles.first());
    QFile logFile(logPath);
    ASSERT_TRUE(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(logFile.readAll());
    logFile.close();

    EXPECT_TRUE(content.contains("SIGSEGV")) << "日志应包含信号名称";
    EXPECT_TRUE(content.contains("操作系统")) << "日志应包含系统信息";
  }
}

/// fork 子进程无限递归触发栈溢出，验证 sigaltstack 生效
TEST(CrashHandlerTest, ForkStackOverflowGeneratesCrashLog) {
  QTemporaryDir tmpDir;
  ASSERT_TRUE(tmpDir.isValid());
  QString dumpPath = tmpDir.path() + "/crash";

  pid_t pid = fork();
  ASSERT_GE(pid, 0) << "fork failed";

  if (pid == 0) {
    // 子进程：初始化 crash handler 后触发栈溢出
    auto handler = CrashHandler::create();
    handler->setDumpPath(dumpPath);
    handler->init();

    // 无限递归 -> 栈耗尽 -> SIGSEGV
    // 用裸函数递归避免 std::function 的堆操作干扰
    volatile int depth = 0;
    crashRecurse(&depth);
    _exit(1);
  }

  // 父进程：等待子进程退出
  int status = 0;
  waitpid(pid, &status, 0);
  EXPECT_TRUE(WIFEXITED(status)) << "子进程应正常退出（_exit）";
  EXPECT_EQ(WEXITSTATUS(status), 128 + SIGSEGV)
      << "退出码应为 128+SIGSEGV=139";

  // 验证崩溃日志文件已生成（sigaltstack 使信号处理器在栈溢出时仍可执行）
  QDir crashDir(dumpPath);
  EXPECT_TRUE(crashDir.exists()) << "crash 目录应存在";

  QStringList filters = {"etest_crash_*.log"};
  QStringList logFiles = crashDir.entryList(filters, QDir::Files);
  EXPECT_FALSE(logFiles.isEmpty())
      << "栈溢出场景下也应生成崩溃日志（sigaltstack 生效）";

  if (!logFiles.isEmpty()) {
    QString logPath = crashDir.absoluteFilePath(logFiles.first());
    QFile logFile(logPath);
    ASSERT_TRUE(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString content = QString::fromUtf8(logFile.readAll());
    logFile.close();

    EXPECT_TRUE(content.contains("SIGSEGV"))
        << "栈溢出日志应包含 SIGSEGV";
  }
}

#endif  // !Q_OS_WIN

// ═══════════════════════════════════════════════════════════════════════════════
// 手动崩溃测试（DISABLED_，不自动运行）
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CrashHandlerTest, DISABLED_CatchNullPointerCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  int* p = nullptr;
  *p = 1;
}

TEST(CrashHandlerTest, DISABLED_CatchDivideByZeroCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  int a = 1;
  int b = 0;
  int c = a / b;
  (void)c;
}

TEST(CrashHandlerTest, DISABLED_CatchStackOverflowCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  std::function<void()> recursive = [&]() { recursive(); };
  recursive();
}

#ifdef Q_OS_WIN
TEST(CrashHandlerTest, DISABLED_CatchIllegalInstructionCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  __ud2();
}
#else
TEST(CrashHandlerTest, DISABLED_CatchIllegalInstructionCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  // __builtin_trap() 生成非法指令 -> SIGILL
  __builtin_trap();
}
#endif

TEST(CrashHandlerTest, DISABLED_CatchArrayOutOfBoundsCrash) {
  auto handler = CrashHandler::create();
  ASSERT_NE(handler, nullptr);
  handler->init();

  int arr[2] = {1, 2};
  volatile int val = arr[100];
  (void)val;
}

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
