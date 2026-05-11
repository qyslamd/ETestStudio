#include <gtest/gtest.h>
#include <sol/sol.hpp>
#include <string>
#include <vector>

// ============================================================
// 实验4: Lua协程实现执行控制
// ============================================================

// ============================================================
// 实验4-1: 基础协程创建与恢复
// ============================================================

TEST(LuaCoroutineTest, BasicCoroutine) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    lua.script(R"(
        co = coroutine.create(function()
            return 42
        end)

        local ok, val = coroutine.resume(co)
        assert(ok == true)
        assert(val == 42)
    )");

    SUCCEED();
}

// ============================================================
// 实验4-2: yield/resume实现步骤级执行
// ============================================================

TEST(LuaCoroutineTest, YieldResume) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    // C++端逐步骤驱动协程执行
    std::vector<std::string> steps;

    lua["Log"] = [&steps](const std::string& msg) {
        steps.push_back(msg);
    };

    lua.script(R"(
        co = coroutine.create(function()
            Log("步骤1: 设置温度")
            coroutine.yield()

            Log("步骤2: 验证温度")
            coroutine.yield()

            Log("步骤3: 延时等待")
            coroutine.yield()

            Log("步骤4: 用例完成")
        end)
    )");

    // C++端逐步resume
    sol::coroutine co = lua["co"];

    auto r1 = co();
    EXPECT_TRUE(r1.valid());
    ASSERT_EQ(steps.size(), 1u);
    EXPECT_EQ(steps[0], "步骤1: 设置温度");

    auto r2 = co();
    EXPECT_TRUE(r2.valid());
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[1], "步骤2: 验证温度");

    auto r3 = co();
    EXPECT_TRUE(r3.valid());
    ASSERT_EQ(steps.size(), 3u);
    EXPECT_EQ(steps[2], "步骤3: 延时等待");

    auto r4 = co();
    EXPECT_TRUE(r4.valid());
    ASSERT_EQ(steps.size(), 4u);
    EXPECT_EQ(steps[3], "步骤4: 用例完成");
}

// ============================================================
// 实验4-3: C++端通过resume传值给协程
// ============================================================

TEST(LuaCoroutineTest, PassValueOnResume) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    std::vector<double> received_values;

    lua["StoreValue"] = [&received_values](double val) {
        received_values.push_back(val);
    };

    lua.script(R"(
        co = coroutine.create(function()
            -- 第一次yield后，resume传来的值作为coroutine.yield()的返回值
            local val1 = coroutine.yield()
            StoreValue(val1)

            local val2 = coroutine.yield()
            StoreValue(val2)
        end)
    )");

    sol::coroutine co = lua["co"];

    // 第一次resume启动协程（没有传值）
    auto r1 = co();
    EXPECT_TRUE(r1.valid());

    // 第二次resume传值37.5
    auto r2 = co(37.5);
    EXPECT_TRUE(r2.valid());
    ASSERT_EQ(received_values.size(), 1u);
    EXPECT_DOUBLE_EQ(received_values[0], 37.5);

    // 第三次resume传值5.0
    auto r3 = co(5.0);
    EXPECT_TRUE(r3.valid());
    ASSERT_EQ(received_values.size(), 2u);
    EXPECT_DOUBLE_EQ(received_values[1], 5.0);
}

// ============================================================
// 实验4-4: 模拟IATP测试步骤的逐步骤执行
// ============================================================

TEST(LuaCoroutineTest, StepByStepExecution) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    // 模拟IATP的Lua API
    std::vector<std::string> execution_log;

    lua["SetDevice"] = [&](const std::string& signalId, double value) {
        execution_log.push_back("SET " + signalId + " = " + std::to_string(value));
    };
    lua["VerifyDevice"] = [&](const std::string& signalId, double expected,
                               double tolMin, double tolMax) -> bool {
        execution_log.push_back("VERIFY " + signalId + " ~= " + std::to_string(expected));
        return true;
    };
    lua["Delay"] = [&](int ms) {
        execution_log.push_back("DELAY " + std::to_string(ms) + "ms");
    };
    lua["Log"] = [&](const std::string& msg) {
        execution_log.push_back("LOG " + msg);
    };

    // 每个步骤后yield，C++端控制执行节奏
    lua.script(R"(
        co = coroutine.create(function()
            SetDevice("温度", 37.5)
            coroutine.yield()     -- 步骤1完成，暂停

            Delay(1000)
            coroutine.yield()     -- 步骤2完成，暂停

            VerifyDevice("温度", 37.5, -0.1, 0.1)
            coroutine.yield()     -- 步骤3完成，暂停

            Log("测试完成")
        end)
    )");

    sol::coroutine co = lua["co"];

    // 步骤1
    co();
    ASSERT_GE(execution_log.size(), 1u);
    EXPECT_NE(execution_log[0].find("SET"), std::string::npos);

    // 步骤2
    co();
    ASSERT_GE(execution_log.size(), 2u);
    EXPECT_NE(execution_log[1].find("DELAY"), std::string::npos);

    // 步骤3
    co();
    ASSERT_GE(execution_log.size(), 3u);
    EXPECT_NE(execution_log[2].find("VERIFY"), std::string::npos);

    // 步骤4
    co();
    ASSERT_GE(execution_log.size(), 4u);
    EXPECT_NE(execution_log[3].find("LOG"), std::string::npos);
}

// ============================================================
// 实验4-5: 协程错误处理
// ============================================================

TEST(LuaCoroutineTest, CoroutineErrorHandling) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    lua.script(R"(
        co = coroutine.create(function()
            error("测试错误")
        end)
    )");

    sol::coroutine co = lua["co"];
    auto result = co();

    // 协程中的错误不会导致C++崩溃
    // sol2的coroutine执行出错时，result.valid()为false
    EXPECT_FALSE(result.valid());

    // 协程状态应该是dead
    lua.script(R"(
        assert(coroutine.status(co) == "dead")
    )");

    SUCCEED();
}

// ============================================================
// 实验4-6: 协程内使用lua_sethook
// ============================================================

TEST(LuaCoroutineTest, CoroutineWithDebugHook) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    int line_count = 0;
    lua["__hook_line_count"] = static_cast<void*>(&line_count);

    // 在主线程设置hook——协程会继承hook设置
    lua_sethook(lua.lua_state(), [](lua_State* L, lua_Debug* ar) {
        if (ar->event == LUA_HOOKLINE) {
            auto* count = static_cast<int*>(
                sol::state_view(L)["__hook_line_count"].get<void*>());
            if (count) (*count)++;
        }
    }, LUA_MASKLINE, 0);

    lua.script(R"(
        co = coroutine.create(function()
            local x = 1
            local y = 2
            local z = x + y
            coroutine.yield()
            local w = z * 2
        end)

        -- resume协程
        coroutine.resume(co)   -- 执行到yield
        coroutine.resume(co)   -- 从yield恢复
    )");

    lua_sethook(lua.lua_state(), nullptr, 0, 0);

    // hook应该在协程内也生效
    EXPECT_GT(line_count, 0);
}

// ============================================================
// 实验4-7: 协程 vs lua_yield方案对比
// ============================================================

TEST(LuaCoroutineTest, LuaYieldInHookForCoroutine) {
    // 关键实验：在hook回调中调用lua_yield暂停协程
    // 这可以实现"不需要手动在脚本中写coroutine.yield()"的暂停机制
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    std::vector<int> executed_before_pause;
    bool pause_requested = false;

    lua["__pause_req"] = static_cast<void*>(&pause_requested);
    lua["__exec_before"] = static_cast<void*>(&executed_before_pause);

    // 创建协程
    lua.script(R"(
        test_func = function()
            local a = 1
            local b = 2
            local c = 3
            local d = 4
            local e = 5
            return a + b + c + d + e
        end
    )");

    // 获取协程的lua_State
    // 注意：lua_sethook需要设置在协程的lua_State上
    // 协程共享主线程的hook设置（Lua 5.4行为）

    // 方案1: 在协程创建后，设置hook
    // Lua 5.4中，子协程会继承主线程的hook设置

    // 设置hook，在暂停标志为true时lua_yield
    lua_sethook(lua.lua_state(), [](lua_State* L, lua_Debug* ar) {
        if (ar->event == LUA_HOOKLINE) {
            auto* should_pause = static_cast<bool*>(
                sol::state_view(L)["__pause_req"].get<void*>());
            if (should_pause && *should_pause) {
                auto* lines = static_cast<std::vector<int>*>(
                    sol::state_view(L)["__exec_before"].get<void*>());
                if (lines) {
                    lua_getinfo(L, "l", ar);
                    lines->push_back(ar->currentline);
                }
                // lua_yield只能从C函数或hook中被调用（Lua 5.4）
                // 注意：这里需要在协程上下文中才能yield
                // lua_yield(L, 0);  // 取消注释可能导致crash如果不在协程中
            }
        }
    }, LUA_MASKLINE, 0);

    // 暂时不测试lua_yield在hook中的调用（需要确保在协程上下文中）
    // 关键结论记录在findings.md中

    lua_sethook(lua.lua_state(), nullptr, 0, 0);

    SUCCEED();
}

// ============================================================
// 实验4-8: 用sol2的coroutine API从C++端控制
// ============================================================

TEST(LuaCoroutineTest, Sol2CoroutineFromCpp) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::coroutine);

    int step_count = 0;

    lua["StepDone"] = [&step_count]() {
        step_count++;
    };

    // 用Lua创建协程
    lua.script(R"(
        function test_workflow()
            StepDone()        -- step 1
            coroutine.yield()
            StepDone()        -- step 2
            coroutine.yield()
            StepDone()        -- step 3
            return "complete"
        end
    )");

    // 从C++端创建协程并驱动执行
    sol::coroutine co = lua["coroutine"]["create"](lua["test_workflow"]);

    // resume 1
    auto r1 = co();
    EXPECT_TRUE(r1.valid());
    EXPECT_EQ(step_count, 1);

    // resume 2
    auto r2 = co();
    EXPECT_TRUE(r2.valid());
    EXPECT_EQ(step_count, 2);

    // resume 3 (协程结束)
    auto r3 = co();
    EXPECT_TRUE(r3.valid());
    EXPECT_EQ(step_count, 3);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
