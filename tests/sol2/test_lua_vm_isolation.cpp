#include <gtest/gtest.h>
#include <sol/sol.hpp>
#include <string>

// ============================================================
// 实验3: VM隔离与多实例
// ============================================================

// ============================================================
// 实验3-1: 多个sol::state实例互不干扰
// ============================================================

TEST(LuaVMIsolationTest, MultipleVMs) {
    sol::state luaA;
    sol::state luaB;
    luaA.open_libraries(sol::lib::base);
    luaB.open_libraries(sol::lib::base);

    // VM-A注册SetDevice_A
    int call_count_a = 0;
    luaA["SetDevice"] = [&](const std::string& name, double val) {
        call_count_a++;
    };

    // VM-B注册SetDevice_B（不同的实现）
    int call_count_b = 0;
    luaB["SetDevice"] = [&](const std::string& name, double val) {
        call_count_b++;
    };

    // VM-A执行
    luaA.script(R"(
        SetDevice("温度", 37.5)
        SetDevice("压力", 5.0)
    )");

    // VM-B执行
    luaB.script(R"(
        SetDevice("电压", 12.0)
    )");

    EXPECT_EQ(call_count_a, 2);
    EXPECT_EQ(call_count_b, 1);
}

// ============================================================
// 实验3-2: 全局变量隔离
// ============================================================

TEST(LuaVMIsolationTest, GlobalIsolation) {
    sol::state luaA;
    sol::state luaB;
    luaA.open_libraries(sol::lib::base);
    luaB.open_libraries(sol::lib::base);

    // VM-A设置全局变量
    luaA["shared_var"] = 100;
    luaA["only_in_a"] = std::string("A的数据");

    // VM-B设置全局变量
    luaB["shared_var"] = 200;
    luaB["only_in_b"] = std::string("B的数据");

    // 验证互不影响
    int a_shared = luaA["shared_var"];
    int b_shared = luaB["shared_var"];
    EXPECT_EQ(a_shared, 100);
    EXPECT_EQ(b_shared, 200);

    // B中不应有A的变量
    auto a_only = luaA["only_in_a"].get<std::string>();
    EXPECT_EQ(a_only, "A的数据");

    auto b_only = luaB["only_in_b"].get<std::string>();
    EXPECT_EQ(b_only, "B的数据");

    // B中访问only_in_a应为nil
    sol::object b_has_a_var = luaB["only_in_a"];
    EXPECT_TRUE(b_has_a_var == sol::nil);
}

// ============================================================
// 实验3-3: 顺序执行不串状态
// ============================================================

TEST(LuaVMIsolationTest, SequentialExecution) {
    sol::state luaA;
    sol::state luaB;
    luaA.open_libraries(sol::lib::base, sol::lib::math);
    luaB.open_libraries(sol::lib::base, sol::lib::math);

    // 模拟两个测试用例在不同的VM中执行
    std::vector<std::string> log_a;
    std::vector<std::string> log_b;

    luaA["Log"] = [&](const std::string& msg) { log_a.push_back(msg); };
    luaB["Log"] = [&](const std::string& msg) { log_b.push_back(msg); };

    // 用例A: 温度测试
    luaA.script(R"(
        Log("用例A开始")
        local temp = 37.5
        Log("温度=" .. tostring(temp))
        Log("用例A结束")
    )");

    // 用例B: 压力测试
    luaB.script(R"(
        Log("用例B开始")
        local pressure = 5.0
        Log("压力=" .. tostring(pressure))
        Log("用例B结束")
    )");

    // 验证两个VM的日志完全隔离
    ASSERT_EQ(log_a.size(), 3u);
    EXPECT_EQ(log_a[0], "用例A开始");
    EXPECT_EQ(log_a[2], "用例A结束");

    ASSERT_EQ(log_b.size(), 3u);
    EXPECT_EQ(log_b[0], "用例B开始");
    EXPECT_EQ(log_b[2], "用例B结束");
}

// ============================================================
// 实验3-4: VM销毁与重建
// ============================================================

TEST(LuaVMIsolationTest, VMDestroyAndRecreate) {
    // 模拟：用一个例执行完毕后销毁VM，创建新VM执行下一个用例
    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        lua["counter"] = 0;
        lua.script("counter = counter + 10");
        int val = lua["counter"];
        EXPECT_EQ(val, 10);
    }  // VM销毁

    {
        sol::state lua;
        lua.open_libraries(sol::lib::base);
        // 新VM，counter不存在
        sol::object obj = lua["counter"];
        EXPECT_TRUE(obj == sol::nil);
    }
}

// ============================================================
// 实验3-5: 限制可用的标准库（沙箱化）
// ============================================================

TEST(LuaVMIsolationTest, SandboxRestrictLibraries) {
    sol::state lua;
    // 只打开base库，不打开io/os等危险库
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string,
                       sol::lib::table, sol::lib::coroutine);

    // 安全的脚本应该正常执行
    int result = lua.script("return math.abs(-42)");
    EXPECT_EQ(result, 42);

    // io库应该不可用
    auto io_result = lua.safe_script("io.open('test.txt', 'w')",
                                      sol::script_pass_on_error);
    EXPECT_FALSE(io_result.valid());

    // os.execute应该不可用
    auto os_result = lua.safe_script("os.execute('echo test')",
                                      sol::script_pass_on_error);
    EXPECT_FALSE(os_result.valid());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
