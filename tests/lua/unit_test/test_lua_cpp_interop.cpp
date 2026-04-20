#include <gtest/gtest.h>
#include <lua.hpp>
#include <string>

// 测试用C++函数：加法，注册到Lua
int lua_add(lua_State* L) {
    int a = lua_tointeger(L, 1);
    int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a + b);
    return 1;
}

// 测试用C++函数：获取table中指定key的值
int lua_get_table_value(lua_State* L) {
    const char* key = lua_tostring(L, 2);
    lua_getfield(L, 1, key);
    return 1;
}

class LuaCppInteropTest : public ::testing::Test {
protected:
    void SetUp() override {
        L = luaL_newstate();
        luaL_openlibs(L);
    }

    void TearDown() override {
        lua_close(L);
    }

    lua_State* L;
};

// 测试1：基础双向调用
TEST_F(LuaCppInteropTest, BasicInterop) {
    // 1. C++调用Lua函数
    const char* lua_script = R"(
        function lua_multiply(a, b)
            return a * b
        end
    )";
    ASSERT_EQ(luaL_loadstring(L, lua_script), LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 0, 0), LUA_OK);

    lua_getglobal(L, "lua_multiply");
    lua_pushinteger(L, 5);
    lua_pushinteger(L, 6);
    ASSERT_EQ(lua_pcall(L, 2, 1, 0), LUA_OK);
    ASSERT_EQ(lua_tointeger(L, -1), 30);
    lua_pop(L, 1);

    // 2. Lua调用C++函数
    lua_pushcfunction(L, lua_add);
    lua_setglobal(L, "cpp_add");

    const char* call_cpp_script = R"(
        return cpp_add(10, 20)
    )";
    ASSERT_EQ(luaL_loadstring(L, call_cpp_script), LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), LUA_OK);
    ASSERT_EQ(lua_tointeger(L, -1), 30);
    lua_pop(L, 1);
}

// 测试2：Lua Table传递
TEST_F(LuaCppInteropTest, TablePassing) {
    // C++创建Lua Table
    lua_newtable(L);
    lua_pushstring(L, "name");
    lua_pushstring(L, "test_user");
    lua_settable(L, -3);
    lua_pushstring(L, "age");
    lua_pushinteger(L, 25);
    lua_settable(L, -3);
    lua_setglobal(L, "user");

    // Lua读取并修改Table
    const char* modify_table_script = R"(
        user.age = user.age + 5
        user.email = "test@example.com"
        return user
    )";
    ASSERT_EQ(luaL_loadstring(L, modify_table_script), LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), LUA_OK);

    // C++验证修改结果
    lua_getfield(L, -1, "age");
    ASSERT_EQ(lua_tointeger(L, -1), 30);
    lua_pop(L, 1);
    
    lua_getfield(L, -1, "email");
    ASSERT_STREQ(lua_tostring(L, -1), "test@example.com");
    lua_pop(L, 2);

    // C++注册函数操作Table
    lua_pushcfunction(L, lua_get_table_value);
    lua_setglobal(L, "get_table_value");

    const char* test_table_func = R"(
        local test_table = {key1 = "value1", key2 = "value2"}
        return get_table_value(test_table, "key1")
    )";
    ASSERT_EQ(luaL_loadstring(L, test_table_func), LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), LUA_OK);
    ASSERT_STREQ(lua_tostring(L, -1), "value1");
    lua_pop(L, 1);
}

// 测试3：错误处理
TEST_F(LuaCppInteropTest, ErrorHandling) {
    // 场景1：Lua语法错误
    const char* bad_syntax = R"(
        function test()
            if true then
                -- 缺少end
    )";
    ASSERT_NE(luaL_loadstring(L, bad_syntax), LUA_OK);
    ASSERT_TRUE(lua_isstring(L, -1));
    lua_pop(L, 1);

    // 场景2：调用不存在的函数
    const char* call_nonexist = R"(
        return nonexist_function(1, 2, 3)
    )";
    ASSERT_EQ(luaL_loadstring(L, call_nonexist), LUA_OK);
    ASSERT_NE(lua_pcall(L, 0, 1, 0), LUA_OK);
    ASSERT_TRUE(lua_isstring(L, -1));
    lua_pop(L, 1);

    // 场景3：参数类型错误
    lua_pushcfunction(L, lua_add);
    lua_setglobal(L, "cpp_add");

    const char* wrong_param = R"(
        return cpp_add("string", 2)
    )";
    ASSERT_EQ(luaL_loadstring(L, wrong_param), LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 1, 0), LUA_OK); // lua_add没有类型检查，会得到0+2=2
    ASSERT_EQ(lua_tointeger(L, -1), 2);
    lua_pop(L, 1);
}

// 测试4：复杂数据交互
TEST_F(LuaCppInteropTest, ComplexInteraction) {
    // C++传递多个不同类型参数到Lua
    const char* complex_script = R"(
        function process_data(str, num, flag)
            local result = {}
            result.upper_str = string.upper(str)
            result.double_num = num * 2
            result.reverse_flag = not flag
            return result
        end
    )";
    ASSERT_EQ(luaL_loadstring(L, complex_script), LUA_OK);
    ASSERT_EQ(lua_pcall(L, 0, 0, 0), LUA_OK);

    lua_getglobal(L, "process_data");
    lua_pushstring(L, "hello lua");
    lua_pushinteger(L, 123);
    lua_pushboolean(L, true);
    ASSERT_EQ(lua_pcall(L, 3, 1, 0), LUA_OK);

    // 验证返回结果
    lua_getfield(L, -1, "upper_str");
    ASSERT_STREQ(lua_tostring(L, -1), "HELLO LUA");
    lua_pop(L, 1);

    lua_getfield(L, -1, "double_num");
    ASSERT_EQ(lua_tointeger(L, -1), 246);
    lua_pop(L, 1);

    lua_getfield(L, -1, "reverse_flag");
    ASSERT_EQ(lua_toboolean(L, -1), false);
    lua_pop(L, 2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
