#include <gtest/gtest.h>
#include <sol/sol.hpp>
#include <string>
#include <vector>

// 测试基本的Lua代码执行
TEST(Sol2BasicTest, ExecuteLuaCode) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // 执行简单加法
    int result = lua.script("return 1 + 2");
    EXPECT_EQ(result, 3);

    // 执行字符串操作
    std::string str = lua.script("return 'Hello ' .. 'Sol2'");
    EXPECT_EQ(str, "Hello Sol2");
}

// 测试C++变量暴露给Lua
TEST(Sol2BasicTest, ExposeCppVariable) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // 暴露整数
    lua["my_number"] = 42;
    int value = lua["my_number"];
    EXPECT_EQ(value, 42);

    // 暴露字符串
    lua["my_string"] = "Test String";
    std::string str = lua["my_string"];
    EXPECT_EQ(str, "Test String");

    // 暴露布尔值
    lua["my_bool"] = true;
    bool b = lua["my_bool"];
    EXPECT_TRUE(b);
}

// 测试C++函数注册到Lua并调用
int add(int a, int b) {
    return a + b;
}

std::string greet(const std::string& name) {
    return "Hello, " + name + "!";
}

TEST(Sol2BasicTest, RegisterCppFunction) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // 注册简单函数
    lua["add"] = &add;
    lua["greet"] = &greet;

    // 在Lua中调用C++函数
    int sum = lua.script("return add(10, 20)");
    EXPECT_EQ(sum, 30);

    std::string greeting = lua.script("return greet('World')");
    EXPECT_EQ(greeting, "Hello, World!");
}

// 测试调用Lua函数
TEST(Sol2BasicTest, CallLuaFunction) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // 定义Lua函数
    lua.script(R"(
        function multiply(a, b)
            return a * b
        end

        function concatenate(str1, str2)
            return str1 .. " " .. str2
        end
    )");

    // 调用Lua函数
    auto multiply = lua["multiply"];
    int product = multiply(5, 6);
    EXPECT_EQ(product, 30);

    auto concatenate = lua["concatenate"];
    std::string result = concatenate("Lua", "Function");
    EXPECT_EQ(result, "Lua Function");
}

// 测试表操作
TEST(Sol2BasicTest, TableOperations) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // 创建表
    lua["user"] = sol::table::create_with(lua.lua_state(),
        "name", "Zhang San",
        "age", 25,
        "is_active", true
    );

    // 读取表字段
    std::string name = lua["user"]["name"];
    int age = lua["user"]["age"];
    bool is_active = lua["user"]["is_active"];

    EXPECT_EQ(name, "Zhang San");
    EXPECT_EQ(age, 25);
    EXPECT_TRUE(is_active);

    // 修改表字段
    lua["user"]["age"] = 26;
    int new_age = lua["user"]["age"];
    EXPECT_EQ(new_age, 26);

    // 遍历表
    lua.script(R"(
        fruits = {"apple", "banana", "orange"}
    )");

    sol::table fruits = lua["fruits"];
    std::vector<std::string> fruit_list;
    for (auto& pair : fruits) {
        fruit_list.push_back(pair.second.as<std::string>());
    }

    EXPECT_EQ(fruit_list.size(), 3);
    EXPECT_EQ(fruit_list[0], "apple");
    EXPECT_EQ(fruit_list[1], "banana");
    EXPECT_EQ(fruit_list[2], "orange");
}

// 测试错误处理
TEST(Sol2BasicTest, ErrorHandling) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // 测试执行错误代码
    sol::protected_function_result result = lua.safe_script("return nil + 1", sol::script_pass_on_error);
    EXPECT_FALSE(result.valid());
    if (!result.valid()) {
        sol::error err = result;
        EXPECT_NE(std::string(err.what()).find("attempt to perform arithmetic on a nil value"), std::string::npos);
    }

    // 测试调用不存在的函数
    sol::protected_function bad_func = lua["nonexistent_function"];
    auto bad_result = bad_func();
    EXPECT_FALSE(bad_result.valid());

    sol::error err = bad_result;
    EXPECT_NE(std::string(err.what()).find("attempt to call a nil value"), std::string::npos);
}

// 测试C++类绑定（简单示例）
class Person {
public:
    Person(std::string name, int age) : name_(std::move(name)), age_(age) {}

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    int getAge() const { return age_; }
    void setAge(int age) { age_ = age; }

    std::string introduce() const {
        return "I'm " + name_ + ", " + std::to_string(age_) + " years old.";
    }

private:
    std::string name_;
    int age_;
};

TEST(Sol2BasicTest, ClassBinding) {
    sol::state lua;
    lua.open_libraries(sol::lib::base);

    // 绑定Person类
    lua.new_usertype<Person>("Person",
        sol::constructors<Person(std::string, int)>(),
        "name", sol::property(&Person::getName, &Person::setName),
        "age", sol::property(&Person::getAge, &Person::setAge),
        "introduce", &Person::introduce
    );

    // 在Lua中创建Person对象并使用
    lua.script(R"(
        p = Person.new("Li Si", 30)
        print(p:introduce())
        p.age = 31
        p.name = "Li Si Updated"
    )");

    // 获取Lua中创建的对象
    Person& p = lua["p"];
    EXPECT_EQ(p.getName(), "Li Si Updated");
    EXPECT_EQ(p.getAge(), 31);

    // 在C++中修改，Lua中也能看到变化
    p.setName("Wang Wu");
    std::string name = lua["p"]["name"];
    EXPECT_EQ(name, "Wang Wu");
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}