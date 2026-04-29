# sol2 由浅入深使用教程

## 目录
1. [入门篇](#入门篇)
   - [sol2简介](#sol2简介)
   - [环境说明](#环境说明)
   - [第一个sol2程序](#第一个sol2程序)
2. [基础篇](#基础篇)
   - [基本数据类型交互](#基本数据类型交互)
   - [C++调用Lua函数](#C++调用Lua函数)
   - [Lua调用C++函数](#Lua调用C++函数)
   - [表操作](#表操作)
   - [错误处理](#错误处理)
3. [进阶篇](#进阶篇)
   - [自定义类型绑定](#自定义类型绑定)
   - [继承与多态](#继承与多态)
   - [函数重载与默认参数](#函数重载与默认参数)
   - [STL容器适配](#STL容器适配)
   - [协程（Coroutine）支持](#协程coroutine支持)
4. [高级篇](#高级篇)
   - [用户数据（Userdata）](#用户数据userdata)
   - [内存管理与生命周期](#内存管理与生命周期)
   - [与原生Lua API混合使用](#与原生Lua-API混合使用)
   - [性能优化](#性能优化)
5. [最佳实践与问题排查](#最佳实践与问题排查)
   - [常见使用误区](#常见使用误区)
   - [调试技巧](#调试技巧)
   - [常见错误解决方案](#常见错误解决方案)

---

## 入门篇

### sol2简介
sol2是目前最流行、功能最完善的C++到Lua绑定库，它具有以下优势：
- **纯头文件库**：只需要包含头文件即可使用，不需要编译额外的库
- **零配置成本**：几乎不需要写胶水代码，绑定逻辑简洁清晰
- **高性能**：模板元编程实现，运行时开销极低
- **功能完整**：支持几乎所有Lua特性，包括最新的Lua 5.4版本
- **类型安全**：编译期自动检查类型匹配，减少运行时错误

### 环境说明
本教程基于sol2 3.3.0 + Lua 5.4.4版本，在我们项目中已经完成了集成，只需要在需要使用的文件中包含头文件：
```cpp
#include <sol/sol.hpp>
```
并在CMake中链接sol2目标：
```cmake
target_link_libraries(your_target PRIVATE sol2::sol2)
```

### 第一个sol2程序
```cpp
#include <sol/sol.hpp>
#include <iostream>

int main() {
    // 创建Lua状态机
    sol::state lua;
    // 打开Lua标准库
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::math);

    // 执行Lua代码
    lua.script("print('Hello from Lua!')");

    // 从Lua获取值
    int result = lua.script("return 1 + 2 * 3");
    std::cout << "1 + 2 * 3 = " << result << std::endl;

    return 0;
}
```
输出：
```
Hello from Lua!
1 + 2 * 3 = 7
```

---

## 基础篇

### 基本数据类型交互
sol2会自动在C++类型和Lua类型之间进行转换，支持的基本类型包括：
- 数字：int, float, double等
- 字符串：std::string, const char*
- 布尔值：bool
- 空值：sol::nil

#### 示例：变量传递
```cpp
sol::state lua;
lua.open_libraries(sol::lib::base);

// C++变量传递到Lua
lua["number"] = 42;
lua["str"] = "Hello sol2";
lua["flag"] = true;

// 从Lua读取变量
int num = lua["number"];
std::string s = lua["str"];
bool b = lua["flag"];

// 如果变量不存在，可以设置默认值
int not_exist = lua["non_existent_key"].value_or(-1); // 返回-1
```

### C++调用Lua函数
#### 示例：调用无返回值函数
```cpp
lua.script(R"(
    function greet(name)
        print("Hello, " .. name .. "!")
    end
)");

// 获取函数对象
sol::function greet = lua["greet"];
// 调用函数
greet("World"); // 输出: Hello, World!
```

#### 示例：调用有返回值函数
```cpp
lua.script(R"(
    function add(a, b)
        return a + b
    end
)");

sol::function add = lua["add"];
int sum = add(10, 20); // sum = 30
```

#### 示例：多返回值处理
```cpp
lua.script(R"(
    function get_user_info()
        return "Zhang San", 25, true
    end
)");

sol::function get_user = lua["get_user_info"];
auto [name, age, is_active] = get_user<std::string, int, bool>();
// name = "Zhang San", age = 25, is_active = true
```

### Lua调用C++函数
#### 示例：注册普通函数
```cpp
// C++函数
int add(int a, int b) {
    return a + b;
}

std::string get_greeting(const std::string& name) {
    return "Hello, " + name;
}

// 注册到Lua
lua["add"] = &add;
lua["greet"] = &get_greeting;

// 在Lua中调用
lua.script(R"(
    print(add(1, 2)) -- 输出: 3
    print(greet("Li Si")) -- 输出: Hello, Li Si
)");
```

#### 示例：注册Lambda函数
```cpp
lua["multiply"] = [](int a, int b) {
    return a * b;
};

lua.script("print(multiply(3, 4))"); // 输出: 12
```

### 表操作
Lua的表是最常用的数据结构，sol2提供了非常便捷的操作方式。

#### 示例：创建和访问表
```cpp
// 创建空表
sol::table user = lua.create_table();
// 添加字段
user["name"] = "Wang Wu";
user["age"] = 30;
user["is_active"] = true;

// 或者直接创建带初始值的表
sol::table user2 = lua.create_table_with(
    "name", "Zhao Liu",
    "age", 28,
    "is_active", false
);

// 访问表字段
std::string name = user["name"];
int age = user["age"];

// 嵌套表操作
sol::table address = lua.create_table();
address["city"] = "Beijing";
address["street"] = "Main Street";
user["address"] = address;

std::string city = user["address"]["city"]; // "Beijing"
```

#### 示例：遍历表
```cpp
lua.script(R"(
    fruits = { "apple", "banana", "orange" }
    user = { name = "Zhang San", age = 25 }
)");

// 遍历数组型表
sol::table fruits = lua["fruits"];
for (auto& pair : fruits) {
    int index = pair.first.as<int>();
    std::string value = pair.second.as<std::string>();
    std::cout << index << ": " << value << std::endl;
}
// 输出:
// 1: apple
// 2: banana
// 3: orange

// 遍历对象型表
sol::table user = lua["user"];
for (auto& pair : user) {
    std::string key = pair.first.as<std::string>();
    sol::object value = pair.second;
    std::cout << key << ": " << value.as<std::string>() << std::endl;
}
```

### 错误处理
sol2提供了多种错误处理方式，避免程序崩溃。

#### 示例：安全执行脚本
```cpp
// 方式1：使用safe_script，返回结果可以检查是否有效
sol::protected_function_result result = lua.safe_script("return nil + 1", sol::script_pass_on_error);
if (!result.valid()) {
    sol::error err = result;
    std::cout << "Lua script error: " << err.what() << std::endl;
}

// 方式2：使用自定义错误处理函数
auto result2 = lua.safe_script("return nil + 1", [](lua_State* L, sol::protected_function_result pfr) {
    std::cout << "Custom error handler called" << std::endl;
    // 返回默认值
    return sol::make_object(L, -1);
});
int value = result2; // value = -1
```

#### 示例：安全调用函数
```cpp
sol::protected_function bad_func = lua["non_existent_function"];
auto result = bad_func();
if (!result.valid()) {
    sol::error err = result;
    std::cout << "Function call error: " << err.what() << std::endl;
}
```

---

## 进阶篇

### 自定义类型绑定
sol2可以非常方便地将C++类绑定到Lua中使用。

#### 示例：绑定简单类
```cpp
// C++类定义
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

// 绑定到Lua
lua.new_usertype<Person>("Person",
    // 构造函数
    sol::constructors<Person(std::string, int)>(),
    // 属性
    "name", sol::property(&Person::getName, &Person::setName),
    "age", sol::property(&Person::getAge, &Person::setAge),
    // 成员函数
    "introduce", &Person::introduce
);
```

在Lua中使用：
```lua
-- 创建对象
local p = Person.new("Li Si", 30)
print(p:introduce()) -- 输出: I'm Li Si, 30 years old.

-- 访问属性
print(p.name) -- 输出: Li Si
p.age = 31
print(p.age) -- 输出: 31
```

### 继承与多态
sol2支持类继承关系的绑定。

#### 示例：继承绑定
```cpp
class Animal {
public:
    Animal(std::string name) : name_(std::move(name)) {}
    virtual ~Animal() = default;

    virtual std::string makeSound() const = 0;
    const std::string& getName() const { return name_; }

private:
    std::string name_;
};

class Dog : public Animal {
public:
    Dog(std::string name) : Animal(std::move(name)) {}

    std::string makeSound() const override {
        return "Woof!";
    }

    void wagTail() {
        std::cout << getName() << " is wagging tail." << std::endl;
    }
};

// 绑定基类
lua.new_usertype<Animal>("Animal",
    "getName", &Animal::getName,
    "makeSound", &Animal::makeSound
);

// 绑定派生类，指定基类
lua.new_usertype<Dog>("Dog",
    sol::base_classes, sol::bases<Animal>(),
    sol::constructors<Dog(std::string)>(),
    "makeSound", &Dog::makeSound,
    "wagTail", &Dog::wagTail
);
```

在Lua中使用：
```lua
local dog = Dog.new("Wang Cai")
print(dog:getName()) -- 输出: Wang Cai
print(dog:makeSound()) -- 输出: Woof!
dog:wagTail() -- 输出: Wang Cai is wagging tail.
```

### 函数重载与默认参数
sol2支持函数重载和默认参数的绑定。

#### 示例：函数重载
```cpp
// 多个同名函数
void print(int num) {
    std::cout << "Number: " << num << std::endl;
}

void print(const std::string& str) {
    std::cout << "String: " << str << std::endl;
}

// 绑定重载函数
lua["print"] = sol::overload(
    static_cast<void(*)(int)>(&print),
    static_cast<void(*)(const std::string&)>(&print)
);
```

在Lua中使用：
```lua
print(42) -- 输出: Number: 42
print("Hello") -- 输出: String: Hello
```

#### 示例：默认参数
```cpp
// 有默认参数的函数
void greet(const std::string& name, const std::string& greeting = "Hello") {
    std::cout << greeting << ", " << name << "!" << std::endl;
}

// 绑定时指定默认参数
lua["greet"] = [](const std::string& name, sol::optional<std::string> greeting) {
    return greet(name, greeting.value_or("Hello"));
};
```

在Lua中使用：
```lua
greet("Zhang San") -- 输出: Hello, Zhang San!
greet("Li Si", "Hi") -- 输出: Hi, Li Si!
```

### STL容器适配
sol2可以自动在STL容器和Lua表之间进行转换。

#### 示例：vector和表互转
```cpp
// C++ vector转Lua表
std::vector<int> vec = {1, 2, 3, 4, 5};
lua["vec"] = vec;

lua.script(R"(
    for i, v in ipairs(vec) do
        print(i, v)
    end
)");

// Lua表转C++ vector
std::vector<int> vec2 = lua["vec"];
// vec2 = {1, 2, 3, 4, 5}
```

#### 示例：map和表互转
```cpp
// C++ map转Lua表
std::map<std::string, int> map = {{"a", 1}, {"b", 2}, {"c", 3}};
lua["map"] = map;

// Lua表转C++ map
std::map<std::string, int> map2 = lua["map"];
```

### 协程（Coroutine）支持
sol2完全支持Lua协程。

#### 示例：协程使用
```cpp
lua.script(R"(
    function count(max)
        for i = 1, max do
            coroutine.yield(i)
        end
        return "done"
    end
)");

// 创建协程
sol::coroutine count = lua["count"];

// 执行协程
while (count) {
    int value = count(5);
    if (count.runnable()) {
        std::cout << "Yielded: " << value << std::endl;
    } else {
        std::cout << "Completed: " << value << std::endl;
    }
}
```
输出：
```
Yielded: 1
Yielded: 2
Yielded: 3
Yielded: 4
Yielded: 5
Completed: done
```

---

## 高级篇

### 用户数据（Userdata）
sol2支持三种用户数据类型：
1. **普通用户数据**：完全由Lua管理内存的C++对象
2. **轻量级用户数据**：只存储指针，内存由C++管理
3. **唯一用户数据**：sol2扩展的类型，支持值语义

#### 示例：轻量级用户数据
```cpp
class HeavyObject {
public:
    HeavyObject(int id) : id_(id) {}
    int getId() const { return id_; }
private:
    int id_;
};

// C++创建对象
HeavyObject obj(1001);

// 将指针作为轻量级用户数据传递给Lua
lua["obj"] = &obj;

// 在C++中取回
HeavyObject* ptr = lua["obj"].as<HeavyObject*>();
```

### 内存管理与生命周期
#### 对象所有权控制
sol2提供了多种方式控制对象的所有权：
```cpp
// 方式1：Lua完全拥有对象，Lua垃圾回收时自动删除
lua["p"] = std::make_unique<Person>("Zhang San", 25);

// 方式2：C++保留所有权，Lua只持有引用
Person p("Li Si", 30);
lua["p"] = &p; // 注意：必须保证p的生命周期比Lua状态机长

// 方式3：共享所有权
auto shared_p = std::make_shared<Person>("Wang Wu", 35);
lua["p"] = shared_p; // Lua和C++共享所有权
```

### 与原生Lua API混合使用
sol2和原生Lua API可以无缝混合使用。

#### 示例：获取原生Lua状态机
```cpp
sol::state lua;
lua_State* L = lua.lua_state(); // 获取原生lua_State指针

// 调用原生Lua API
lua_pushinteger(L, 42);
lua_setglobal(L, "num");

// 用sol2读取
int num = lua["num"]; // num = 42
```

#### 示例：在原生API回调中使用sol2
```cpp
static int l_my_function(lua_State* L) {
    // 用sol2包装原生状态机
    sol::state_view lua(L);
    
    // 获取参数
    int a = lua[1];
    int b = lua[2];
    
    // 返回结果
    lua.push(a + b);
    return 1;
}

// 注册原生函数
lua_register(lua.lua_state(), "my_function", l_my_function);
```

### 性能优化
sol2本身性能很高，但使用时注意以下几点可以进一步提升性能：

1. **缓存常用的函数和表**
   ```cpp
   // 不好的写法：每次都从全局表查找
   for (int i = 0; i < 1000; i++) {
       lua.script("my_function()");
   }

   // 好的写法：缓存函数对象
   sol::function func = lua["my_function"];
   for (int i = 0; i < 1000; i++) {
       func();
   }
   ```

2. **使用`sol::unsafe_function`替代`sol::protected_function`**
   如果你确定函数调用不会出错，使用不安全的函数可以跳过错误检查，提升性能。

3. **避免频繁的C++/Lua交互**
   尽量批量传递数据，减少跨语言调用次数。

4. **使用编译期字符串哈希**
   sol3支持编译期字符串哈希，查找全局变量和表字段更快。

---

## 最佳实践与问题排查

### 常见使用误区
1. **不要在Lua回调中持有C++对象的裸指针**
   除非你能保证对象的生命周期比Lua状态机长，否则应该使用智能指针。

2. **不要跨线程使用同一个Lua状态机**
   Lua状态机不是线程安全的，多线程访问需要加锁，或者每个线程使用独立的状态机。

3. **不要在Lua中存储C++对象的引用，然后在C++中删除对象**
   这会导致Lua访问悬空指针，程序崩溃。

4. **避免在Lua中使用过多的全局变量**
   全局变量查找慢，而且容易造成命名冲突。

### 调试技巧
1. **使用`sol::state_view`的`dump`方法打印表内容**
   ```cpp
   sol::table t = lua["my_table"];
   t.dump(); // 打印表的所有内容到标准输出
   ```

2. **使用Lua的debug库进行调试**
   ```cpp
   lua.open_libraries(sol::lib::debug);
   lua.script("debug.traceback()"); // 打印调用栈
   ```

3. **在错误处理中打印调用栈**
   ```cpp
   auto result = lua.safe_script("error('test')", [](lua_State* L, sol::protected_function_result pfr) {
       sol::state_view lua(L);
       sol::function traceback = lua["debug"]["traceback"];
       std::string stack = traceback();
       std::cout << "Error stack: " << stack << std::endl;
       return pfr;
   });
   ```

### 常见错误解决方案

#### 1. 编译错误："无法打开包括文件: lua.h"
**原因**：sol2找不到Lua头文件
**解决方案**：确保liblua目标正确导出了头文件路径，或者在CMake中手动添加Lua头文件目录。

#### 2. 运行时错误："attempt to index a nil value"
**原因**：访问了不存在的表字段
**解决方案**：检查字段名是否拼写正确，确保在访问前已经定义了该字段。

#### 3. 运行时错误："invalid cast from type X to Y"
**原因**：类型转换失败，C++和Lua类型不匹配
**解决方案**：检查两边的类型是否一致，使用`sol::optional`处理可能为空的情况。

#### 4. 编译错误：模板实例化失败
**原因**：通常是因为函数签名不匹配，或者类型不支持自动转换
**解决方案**：检查绑定的函数签名是否正确，复杂类型可以使用`sol::as_function`包装。

#### 5. 内存泄漏
**原因**：Lua持有C++对象的所有权，但C++这边又删除了对象，或者反之
**解决方案**：明确对象所有权，优先使用智能指针管理对象生命周期。
