#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fstream>
#include <thread>
#include <vector>
#include <string>

// 测试日志级别基础功能
TEST(SpdlogTest, LogLevelTest) {
    auto console = spdlog::stdout_color_mt("console");
    console->set_level(spdlog::level::trace);
    
    EXPECT_NO_THROW(console->trace("trace message"));
    EXPECT_NO_THROW(console->debug("debug message"));
    EXPECT_NO_THROW(console->info("info message"));
    EXPECT_NO_THROW(console->warn("warn message"));
    EXPECT_NO_THROW(console->error("error message"));
    EXPECT_NO_THROW(console->critical("critical message"));
    
    spdlog::drop("console");
}

// 测试文件输出功能
TEST(SpdlogTest, FileSinkTest) {
    const std::string test_file = "spdlog_test_output.log";
    auto file_logger = spdlog::basic_logger_mt("file_logger", test_file);
    
    const std::string test_msg = "test file log message";
    file_logger->info(test_msg);
    spdlog::drop("file_logger");
    spdlog::shutdown();
    
    // 验证文件内容
    std::ifstream infile(test_file);
    std::string content;
    std::getline(infile, content);
    EXPECT_NE(content.find(test_msg), std::string::npos);
    
    // 删除测试文件
    std::remove(test_file.c_str());
}

// 测试格式化功能
TEST(SpdlogTest, FormatTest) {
    auto console = spdlog::stdout_color_mt("format_test");
    int int_val = 123;
    double double_val = 3.14159;
    std::string str_val = "test string";
    
    EXPECT_NO_THROW(console->info("int: {}, double: {:.2f}, string: {}", 
                                  int_val, double_val, str_val));
    spdlog::drop("format_test");
}

// 测试多线程并发写
TEST(SpdlogTest, MultiThreadTest) {
    const int thread_count = 10;
    const int msg_per_thread = 100;
    auto logger = spdlog::stdout_color_mt("multi_thread_test");
    logger->set_pattern("[%t] %v");
    
    std::vector<std::thread> threads;
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([logger, i, msg_per_thread]() {
            for (int j = 0; j < msg_per_thread; ++j) {
                logger->info("thread {} message {}", i, j);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    spdlog::drop("multi_thread_test");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
