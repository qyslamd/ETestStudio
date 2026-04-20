# Lua嵌入开发教程（C++/Qt项目专属）
> 适配环境：Lua 5.4.4 + MSVC2017 + Qt 5.12 + C++17
> 前置基础：熟练C++、了解Python等脚本语言语法、已掌握Lua基础语法（变量、函数、table基础）

---
## 一、Lua核心特性快速入门（只讲独有概念，类比C++/Python）
### 1.1 基础语法差异
| 特性 | Lua规则 | C++/Python类比 |
|------|---------|----------------|
| 变量默认全局 | 不写`local`就是全局变量，全局变量存在`_G`表中 | 相当于C++默认变量都是static全局，`local`相当于局部作用域变量 |
| 类型系统 | 只有8种基础类型：nil/boolean/number/string/table/function/userdata/thread | number统一是double，相当于Python的动态类型 |
| 多重返回值 | 函数可以返回多个值，自动适配接收变量数量 | 相当于Python的多返回值，比C++的tuple更灵活 |
| 可变参数 | 函数参数用`...`接收可变参数，`{...}`转为table | 类比C++可变参数模板/Python的*args |
| 空值判断 | nil和false为假，0/空字符串/空table都是真 | 和Python/JS的假值判断不同，注意踩坑 |

```lua
-- 多重返回值示例
function get_user()
    return "张三", 25, "男"
end
local name, age = get_user() -- 第三个返回值自动丢弃
```

---
### 1.2 Table核心机制（Lua唯一数据结构）
> 类比：相当于C++的std::map + std::vector + Python的dict + 类的结合体
#### 1.2.1 二象性：数组+哈希表
```lua
-- 数组模式（下标从1开始！这是Lua最容易踩的坑）
local arr = {1, 2, 3, 4}
print(arr[1]) -- 输出1，arr[0]是nil

-- 哈希表模式
local map = {name = "张三", age = 25}
print(map.name) -- 等价于map["name"]

-- 混合模式
local mix = {1, 2, name = "张三", 3} -- 数组部分是{1,2,3}，哈希部分是{name="张三"}
```
#### 1.2.2 元表/元方法（Lua的运算符重载+类继承实现）
> 类比：相当于C++的运算符重载、Python的__getitem__/__setitem__等魔法方法
```lua
local mt = {}
-- 定义__index元方法：访问不存在的key时触发
mt.__index = function(table, key)
    return "默认值"
end
-- 定义__add元方法：支持+运算符
mt.__add = function(a, b)
    return a.value + b.value
end

local obj = {value = 10}
setmetatable(obj, mt)

print(obj.nonexist_key) -- 输出"默认值"，触发__index
local obj2 = {value = 20}
print(obj + obj2) -- 输出30，触发__add
```
> **C++交互关联**：元表是实现C++自定义类导出到Lua的核心机制，userdata绑定元表后就可以支持Lua侧用`.`/`:`访问成员和调用方法。

---
### 1.3 函数高级特性
#### 1.3.1 闭包+upvalue
> 类比：相当于C++11的lambda捕获外部变量，Python的闭包
```lua
function create_counter()
    local count = 0 -- upvalue：被闭包捕获的变量，生命周期延长
    return function()
        count = count + 1
        return count
    end
end

local counter = create_counter()
print(counter()) -- 1
print(counter()) -- 2
```
#### 1.3.2 冒号语法糖
```lua
local obj = {name = "张三"}
function obj:say_hello()
    print("你好，我是"..self.name) -- self是第一个参数，自动传入
end
-- 等价于
function obj.say_hello(self)
    print("你好，我是"..self.name)
end

obj:say_hello() -- 自动传obj作为self，等价于obj.say_hello(obj)
```
> **C++交互关联**：导出C++成员函数到Lua时，冒号语法会自动处理this指针传递。

---
### 1.4 协程基础
> 类比：相当于C++20的协程、Python的async/await，是用户态轻量级线程
```lua
local co = coroutine.create(function(a, b)
    print("协程执行："..a, b)
    local sum = a + b
    local yield_ret = coroutine.yield(sum) -- 让出CPU，返回sum，下次唤醒时yield_ret是唤醒参数
    print("唤醒后参数："..yield_ret)
    return "完成"
end)

local ok, res = coroutine.resume(co, 10, 20) -- 输出"协程执行：10 20"
print(res) -- 输出30（yield的返回值）
ok, res = coroutine.resume(co, "唤醒参数") -- 输出"唤醒后参数：唤醒参数"
print(res) -- 输出"完成"
```
> **使用场景**：实现异步业务逻辑、脚本任务调度、状态机。

---
### 1.5 模块化机制
> 类比：相当于C++的#include、Python的import
```lua
-- module.lua
local module = {}
function module.add(a, b)
    return a + b
end
return module

-- main.lua
local mod = require("module") -- require只会加载一次，返回模块return的值
print(mod.add(1,2)) -- 3
```

---
## 二、Lua和C++互操作全流程（基于你项目的原生API实现）
### 2.1 核心原理：Lua栈
> Lua和C++所有交互都通过栈完成，栈是先进后出结构，栈底下标1，栈顶可以用负数表示（-1是栈顶）
```cpp
// 示例：C++调用Lua函数
lua_getglobal(L, "add"); // 把Lua全局函数add压入栈
lua_pushinteger(L, 10); // 参数1压栈
lua_pushinteger(L, 20); // 参数2压栈
lua_pcall(L, 2, 1, 0); // 调用函数：2个参数，1个返回值
int result = lua_tointeger(L, -1); // 取栈顶返回值
lua_pop(L, 1); // 弹出返回值，栈恢复原状
```
#### 2.1.1 栈操作核心规则
1. **谁调用谁清理**：C++压入栈的内容，使用完必须自己弹出，避免栈溢出
2. **类型安全**：调用`lua_toxxx`之前必须用`lua_isxxx`判断类型，避免崩溃
3. **返回值顺序**：Lua函数返回值按顺序压栈，第一个返回值在最上面

---
### 2.2 基础类型交互
| C++类型 | 压栈API | 读取API |
|---------|---------|---------|
| int | `lua_pushinteger(L, val)` | `lua_tointeger(L, idx)` |
| double | `lua_pushnumber(L, val)` | `lua_tonumber(L, idx)` |
| bool | `lua_pushboolean(L, val)` | `lua_toboolean(L, idx)` |
| const char* | `lua_pushstring(L, val)` | `lua_tostring(L, idx)` |
| nil | `lua_pushnil(L)` | `lua_isnil(L, idx)` |

---
### 2.3 Table交互（核心数据结构）
#### 2.3.1 C++读取Lua Table
```cpp
// Lua侧表：local user = {name = "张三", age = 25, tags = {"admin", "user"}}
lua_getglobal(L, "user");

// 读取字符串字段
lua_getfield(L, -1, "name");
const char* name = lua_tostring(L, -1);
lua_pop(L, 1);

// 读取数字字段
lua_getfield(L, -1, "age");
int age = lua_tointeger(L, -1);
lua_pop(L, 1);

// 读取数组字段
lua_getfield(L, -1, "tags");
lua_pushinteger(L, 1);
lua_gettable(L, -2); // 等价于tags[1]
const char* tag1 = lua_tostring(L, -1);
lua_pop(L, 2); // 弹出tag1和tags表
```
#### 2.3.2 C++创建Lua Table
```cpp
lua_newtable(L); // 创建空表压栈

// 设置键值对
lua_pushstring(L, "name");
lua_pushstring(L, "张三");
lua_settable(L, -3); // 等价于table["name"] = "张三"

// 简化写法：lua_setfield
lua_pushinteger(L, 25);
lua_setfield(L, -2, "age"); // 等价于table.age = 25

// 压入全局
lua_setglobal(L, "user"); // 现在Lua侧可以访问global user表
```

---
### 2.4 自定义类型交互（userdata）
> 用来传递C++自定义类对象到Lua侧，类比：相当于C++的void*强转，带类型安全检查
```cpp
// C++侧定义类
class User {
public:
    std::string name;
    int age;
};

// 创建userdata传递到Lua
User* u = new User();
u->name = "张三";
u->age = 25;
User** ud = (User**)lua_newuserdata(L, sizeof(User*)); // 分配userdata内存
*ud = u;

// 绑定元表（用于类型检查和成员访问）
luaL_newmetatable(L, "UserMeta"); // 创建元表
lua_setmetatable(L, -2); // 绑定到userdata

// Lua侧获取userdata
User** ud = (User**)luaL_checkudata(L, 1, "UserMeta"); // 类型安全检查
User* u = *ud;
```

---
### 2.5 错误处理
#### 2.5.1 错误码
| 错误码 | 含义 |
|--------|------|
| LUA_OK(0) | 成功 |
| LUA_ERRRUN | 运行时错误 |
| LUA_ERRSYNTAX | 语法错误 |
| LUA_ERRMEM | 内存分配错误 |
#### 2.5.2 错误捕获
```cpp
if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
    const char* err = lua_tostring(L, -1); // 获取错误信息
    qDebug() << "Lua执行错误：" << err;
    lua_pop(L, 1); // 清理错误信息
}
```
> **安全提示**：所有Lua执行都必须加错误捕获，避免Lua错误直接导致C++程序崩溃。

---
### 2.6 推荐绑定方案：sol2（比原生API简洁90%）
> 项目集成方案：直接下载sol2单头文件，包含到项目即可，无需编译
```cpp
#include <sol/sol.hpp>

// 初始化sol
sol::state lua;
lua.open_libraries(sol::lib::base, sol::lib::string);

// 1. C++调用Lua
lua.script(R"(
    function add(a, b)
        return a + b
    end
)");
int result = lua["add"](10, 20); // 自动处理参数和返回值

// 2. Lua调用C++函数
lua["cpp_add"] = [](int a, int b) { return a + b; };
lua.script("result = cpp_add(10, 20)");

// 3. 导出C++类到Lua
lua.new_usertype<User>("User",
    "name", &User::name,
    "age", &User::age,
    "say_hello", &User::say_hello
);
```
> **集成成本**：只需要把sol2.hpp放到3rdparty目录，在CMake中加包含路径即可，无依赖。

---
### 2.7 Qt类型适配（可直接复用代码）
```cpp
// QString和Lua string互转
sol::state lua;
lua["test_qstring"] = [](const QString& str) {
    qDebug() << "收到Qt字符串：" << str;
    return str.toUpper();
};

// QVariant和Lua类型互转
void push_variant(lua_State* L, const QVariant& var) {
    if (var.type() == QVariant::Int) lua_pushinteger(L, var.toInt());
    else if (var.type() == QVariant::String) lua_pushstring(L, var.toString().toUtf8());
    else if (var.type() == QVariant::Bool) lua_pushboolean(L, var.toBool());
    else if (var.canConvert<QVariantList>()) {
        lua_newtable(L);
        QVariantList list = var.toList();
        for (int i=0; i<list.size(); i++) {
            push_variant(L, list[i]);
            lua_rawseti(L, -2, i+1);
        }
    }
    // 其他类型以此类推
}
```

---
## 三、嵌入工程化实践
### 3.1 调试方案
1. **日志打印**：重定向Lua的print函数到Qt的qDebug，统一日志输出
2. **调试工具**：使用ZeroBrane Studio支持Lua代码断点调试，支持远程attach到C++进程
3. **错误栈**：出错时打印Lua调用栈，用`luaL_traceback`获取完整调用信息

### 3.2 性能优化
1. **避免频繁交互**：减少C++和Lua之间的跨边界调用次数，批量传递数据
2. **缓存Lua函数**：经常调用的Lua函数不要每次都`lua_getglobal`，缓存到C++侧的引用
3. **热点代码优化**：性能敏感的逻辑放到C++实现，Lua只做业务逻辑编排
4. **LuaJIT**：如果需要更高性能，可以切换到LuaJIT，比原生Lua快5~10倍，支持FFI直接调用C函数

### 3.3 安全沙箱
1. **禁用危险库**：初始化Lua时不要打开os/io/debug等危险库，避免脚本访问系统资源
2. **资源配额**：限制Lua脚本内存使用、执行指令数，防止恶意脚本死循环占满CPU
3. **全局环境隔离**：每个业务脚本使用独立的_ENV环境，避免全局变量污染

### 3.4 热更新方案
```cpp
// 热重载核心逻辑
void reload_script(const std::string& path) {
    lua_newtable(L); // 创建新的环境表
    lua_setglobal(L, "_ENV"); // 替换全局环境
    luaL_dofile(L, path.c_str()); // 重新加载脚本
}
```
> **注意**：热更时需要注意upvalue和协程的状态保留，避免出现状态不一致。

### 3.5 工程规范
1. **目录结构**：脚本按业务模块拆分，`scripts/`下分`common/`、`business/`、`config/`
2. **编码规范**：统一用UTF-8编码，和C++风格保持一致，local变量用小写下划线，全局变量大写下划线
3. **版本管理**：脚本和C++代码一起提交git，大版本更新时同步脚本版本号

---
## 四、常见坑点汇总
1. Lua数组下标从1开始，0是nil
2. 变量不写local默认是全局变量，容易污染全局环境
3. 栈操作必须谁压入谁弹出，避免栈溢出
4. 字符串/Table传递都是引用传递，修改会影响原对象
5. Lua的number是double，大整数（超过2^53）会丢失精度
