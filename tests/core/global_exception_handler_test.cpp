#include "common/GlobalExceptionHandler.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <csignal>

#include "logger/Logger.h"

using namespace etest::core::common;

class GlobalExceptionHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override { etest::core::logger::Logger::init(); }

  void TearDown() override {
    GlobalExceptionHandler::instance().shutdown();
    etest::core::logger::Logger::shutdown();
  }
};

// 生命周期测试

TEST_F(GlobalExceptionHandlerTest, InitAndShutdown) {
  EXPECT_NO_THROW(GlobalExceptionHandler::instance().init());
  EXPECT_NO_THROW(GlobalExceptionHandler::instance().shutdown());
}

TEST_F(GlobalExceptionHandlerTest, DoubleInit) {
  GlobalExceptionHandler::instance().init();
  EXPECT_NO_THROW(GlobalExceptionHandler::instance().init());
}

TEST_F(GlobalExceptionHandlerTest, ShutdownWithoutInit) {
  // shutdown 未 init 时应安全无操作
  EXPECT_NO_THROW(GlobalExceptionHandler::instance().shutdown());
}

TEST_F(GlobalExceptionHandlerTest, DoubleShutdown) {
  GlobalExceptionHandler::instance().init();
  GlobalExceptionHandler::instance().shutdown();
  EXPECT_NO_THROW(GlobalExceptionHandler::instance().shutdown());
}

// 信号捕获测试（DISABLED_，手动运行）

TEST_F(GlobalExceptionHandlerTest, DISABLED_CatchSigAbrt) {
  GlobalExceptionHandler::instance().init();
  std::raise(SIGABRT);
}

TEST_F(GlobalExceptionHandlerTest, DISABLED_CatchSigSegv) {
  GlobalExceptionHandler::instance().init();
  int* p = nullptr;
  *p = 1;
}

TEST_F(GlobalExceptionHandlerTest, DISABLED_CatchSigFpe) {
  GlobalExceptionHandler::instance().init();
  volatile int a = 1;
  volatile int b = 0;
  volatile int c = a / b;
  (void)c;
}

TEST_F(GlobalExceptionHandlerTest, DISABLED_CatchSigInt) {
  GlobalExceptionHandler::instance().init();
  std::raise(SIGINT);
}

TEST_F(GlobalExceptionHandlerTest, DISABLED_CatchSigTerm) {
  GlobalExceptionHandler::instance().init();
  std::raise(SIGTERM);
}
