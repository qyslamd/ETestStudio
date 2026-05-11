#include <gtest/gtest.h>
#include <sol/sol.hpp>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <string>
#include <thread>
#include <chrono>

// ============================================================
// IATP测试引擎需要的数据结构（模拟阶段5的真实定义）
// ============================================================

enum class DeviceStatus { Offline, Online, Error, Simulated };

enum class FaultType { StuckAt, Bias, CrcError, ParityError, Delay, PacketLoss, SampleRateAnomaly };

struct Tolerance {
    double min = 0.0;
    double max = 0.0;
    Tolerance() = default;
    Tolerance(double min_, double max_) : min(min_), max(max_) {}
};

struct SignalValue {
    QString signalId;
    QVariant value;
    qint64 timestamp = 0;
    bool isValid = true;
    int quality = 0;  // 0=正常, 1=可疑, 2=无效, 3=模拟
    QString sourceInfo;
    SignalValue() = default;
};

struct FaultConfig {
    FaultType type = FaultType::StuckAt;
    QVariant faultValue;
    double delayMs = 0.0;
    double probability = 1.0;
    FaultConfig() = default;
};

// ============================================================
// 实验1-1: QString绑定
// ============================================================

class Sol2QStringTest : public ::testing::Test {
protected:
    sol::state lua;
    void SetUp() override {
        lua.open_libraries(sol::lib::base, sol::lib::string);
    }
};

TEST_F(Sol2QStringTest, SetGetString) {
    // sol2不原生支持QString，需要转std::string
    lua["name"] = QString("温度传感器").toStdString();
    std::string result = lua["name"];
    EXPECT_EQ(result, "温度传感器");
}

TEST_F(Sol2QStringTest, PassQStringToCppFunction) {
    // C++函数接收std::string，Lua传入字符串
    lua["get_length"] = [](const std::string& s) -> int {
        return static_cast<int>(s.size());
    };
    int len = lua.script("return get_length('hello')");
    EXPECT_EQ(len, 5);
}

TEST_F(Sol2QStringTest, ReturnQStringFromCppFunction) {
    // C++函数返回std::string，Lua接收字符串
    lua["make_signal_id"] = []() -> std::string {
        return QString("sig-550e8400-e29b-41d4").toStdString();
    };
    std::string result = lua.script("return make_signal_id()");
    EXPECT_EQ(result, "sig-550e8400-e29b-41d4");
}

TEST_F(Sol2QStringTest, QStringConcatInLua) {
    lua["prefix"] = std::string("sig-");
    std::string result = lua.script("return prefix .. '12345'");
    EXPECT_EQ(result, "sig-12345");
}

// ============================================================
// 实验1-2: QVariant绑定
// ============================================================

class Sol2QVariantTest : public ::testing::Test {
protected:
    sol::state lua;
    void SetUp() override {
        lua.open_libraries(sol::lib::base);
    }
};

TEST_F(Sol2QVariantTest, QVariantAsDouble) {
    // QVariant持有double，通过sol2 table传递
    lua["signal"] = sol::table::create_with(lua.lua_state(),
        "value", 37.5,
        "quality", 0,
        "isValid", true
    );
    double val = lua["signal"]["value"];
    EXPECT_DOUBLE_EQ(val, 37.5);
    int quality = lua["signal"]["quality"];
    EXPECT_EQ(quality, 0);
    bool isValid = lua["signal"]["isValid"];
    EXPECT_TRUE(isValid);
}

TEST_F(Sol2QVariantTest, QVariantMapRoundTrip) {
    // C++端: QVariantMap → Lua table → C++端读取
    QVariantMap map;
    map["name"] = QString("温度");
    map["value"] = 37.5;
    map["unit"] = QString("°C");

    // 写入Lua table
    lua["signal"] = lua.create_table();
    lua["signal"]["name"] = map["name"].toString().toStdString();
    lua["signal"]["value"] = map["value"].toDouble();
    lua["signal"]["unit"] = map["unit"].toString().toStdString();

    // Lua端修改
    lua.script("signal.value = 42.0");

    // C++端读取
    double val = lua["signal"]["value"];
    EXPECT_DOUBLE_EQ(val, 42.0);
}

TEST_F(Sol2QVariantTest, MixedTypeValues) {
    // IATP信号值可能是int/double/string/bool
    lua.create_table("mixed");
    lua["mixed"]["int_val"] = 100;
    lua["mixed"]["double_val"] = 3.14;
    lua["mixed"]["string_val"] = std::string("hello");
    lua["mixed"]["bool_val"] = true;

    EXPECT_EQ(lua["mixed"]["int_val"].get<int>(), 100);
    EXPECT_DOUBLE_EQ(lua["mixed"]["double_val"].get<double>(), 3.14);
    EXPECT_EQ(lua["mixed"]["string_val"].get<std::string>(), "hello");
    EXPECT_TRUE(lua["mixed"]["bool_val"].get<bool>());
}

// ============================================================
// 实验1-3: 自定义Struct绑定（new_usertype）
// ============================================================

class Sol2StructBindingTest : public ::testing::Test {
protected:
    sol::state lua;
    void SetUp() override {
        lua.open_libraries(sol::lib::base);

        // 绑定Tolerance
        lua.new_usertype<Tolerance>("Tolerance",
            sol::constructors<Tolerance(), Tolerance(double, double)>(),
            "min", &Tolerance::min,
            "max", &Tolerance::max
        );

        // 绑定SignalValue（signalId/sourceInfo用std::string代替QString）
        lua.new_usertype<SignalValue>("SignalValue",
            sol::constructors<SignalValue()>(),
            "signalId", sol::property(
                [](SignalValue& sv) -> std::string { return sv.signalId.toStdString(); },
                [](SignalValue& sv, const std::string& s) { sv.signalId = QString::fromStdString(s); }
            ),
            "value", sol::property(
                [](SignalValue& sv) -> double { return sv.value.toDouble(); },
                [](SignalValue& sv, double v) { sv.value = v; }
            ),
            "timestamp", &SignalValue::timestamp,
            "isValid", &SignalValue::isValid,
            "quality", &SignalValue::quality,
            "sourceInfo", sol::property(
                [](SignalValue& sv) -> std::string { return sv.sourceInfo.toStdString(); },
                [](SignalValue& sv, const std::string& s) { sv.sourceInfo = QString::fromStdString(s); }
            )
        );

        // 绑定FaultConfig
        lua.new_usertype<FaultConfig>("FaultConfig",
            sol::constructors<FaultConfig()>(),
            "type", &FaultConfig::type,
            "faultValue", sol::property(
                [](FaultConfig& fc) -> double { return fc.faultValue.toDouble(); },
                [](FaultConfig& fc, double v) { fc.faultValue = v; }
            ),
            "delayMs", &FaultConfig::delayMs,
            "probability", &FaultConfig::probability
        );
    }
};

TEST_F(Sol2StructBindingTest, ToleranceCreateAndAccess) {
    lua.script(R"(
        tol = Tolerance.new(-0.1, 0.1)
    )");
    Tolerance& tol = lua["tol"];
    EXPECT_DOUBLE_EQ(tol.min, -0.1);
    EXPECT_DOUBLE_EQ(tol.max, 0.1);
}

TEST_F(Sol2StructBindingTest, ToleranceModifyFromLua) {
    lua.script(R"(
        tol = Tolerance.new()
        tol.min = -0.5
        tol.max = 0.5
    )");
    Tolerance& tol = lua["tol"];
    EXPECT_DOUBLE_EQ(tol.min, -0.5);
    EXPECT_DOUBLE_EQ(tol.max, 0.5);
}

TEST_F(Sol2StructBindingTest, SignalValueCreateAndAccess) {
    lua.script(R"(
        sv = SignalValue.new()
        sv.signalId = "sig-550e8400"
        sv.value = 37.5
        sv.quality = 0
        sv.isValid = true
        sv.sourceInfo = "AD-CH0"
    )");
    SignalValue& sv = lua["sv"];
    EXPECT_EQ(sv.signalId.toStdString(), "sig-550e8400");
    EXPECT_DOUBLE_EQ(sv.value.toDouble(), 37.5);
    EXPECT_EQ(sv.quality, 0);
    EXPECT_TRUE(sv.isValid);
    EXPECT_EQ(sv.sourceInfo.toStdString(), "AD-CH0");
}

TEST_F(Sol2StructBindingTest, SignalValueRoundTrip) {
    // C++端创建 → Lua端读取修改 → C++端验证
    SignalValue sv;
    sv.signalId = "sig-test";
    sv.value = 100.0;
    sv.quality = 3;  // 模拟模式
    lua["sv"] = &sv;

    lua.script(R"(
        sv.value = 200.0
        sv.quality = 0
    )");

    EXPECT_DOUBLE_EQ(sv.value.toDouble(), 200.0);
    EXPECT_EQ(sv.quality, 0);
}

TEST_F(Sol2StructBindingTest, FaultConfigCreateAndAccess) {
    lua.script(R"(
        fc = FaultConfig.new()
        fc.faultValue = 999.0
        fc.delayMs = 100.0
        fc.probability = 0.5
    )");
    FaultConfig& fc = lua["fc"];
    EXPECT_DOUBLE_EQ(fc.faultValue.toDouble(), 999.0);
    EXPECT_DOUBLE_EQ(fc.delayMs, 100.0);
    EXPECT_DOUBLE_EQ(fc.probability, 0.5);
}

// ============================================================
// 实验1-4: 枚举绑定
// ============================================================

class Sol2EnumBindingTest : public ::testing::Test {
protected:
    sol::state lua;
    void SetUp() override {
        lua.open_libraries(sol::lib::base);

        // 方式1: 将枚举值注册为Lua全局常量
        lua["DeviceStatus_Offline"] = static_cast<int>(DeviceStatus::Offline);
        lua["DeviceStatus_Online"] = static_cast<int>(DeviceStatus::Online);
        lua["DeviceStatus_Error"] = static_cast<int>(DeviceStatus::Error);
        lua["DeviceStatus_Simulated"] = static_cast<int>(DeviceStatus::Simulated);

        lua["FaultType_StuckAt"] = static_cast<int>(FaultType::StuckAt);
        lua["FaultType_Bias"] = static_cast<int>(FaultType::Bias);
        lua["FaultType_CrcError"] = static_cast<int>(FaultType::CrcError);

        // 方式2: 将枚举注册为Lua table（更接近Lua习惯）
        auto deviceStatusTable = lua.create_table();
        deviceStatusTable["Offline"] = static_cast<int>(DeviceStatus::Offline);
        deviceStatusTable["Online"] = static_cast<int>(DeviceStatus::Online);
        deviceStatusTable["Error"] = static_cast<int>(DeviceStatus::Error);
        deviceStatusTable["Simulated"] = static_cast<int>(DeviceStatus::Simulated);
        lua["DeviceStatus"] = deviceStatusTable;

        auto faultTypeTable = lua.create_table();
        faultTypeTable["StuckAt"] = static_cast<int>(FaultType::StuckAt);
        faultTypeTable["Bias"] = static_cast<int>(FaultType::Bias);
        faultTypeTable["CrcError"] = static_cast<int>(FaultType::CrcError);
        faultTypeTable["ParityError"] = static_cast<int>(FaultType::ParityError);
        faultTypeTable["Delay"] = static_cast<int>(FaultType::Delay);
        faultTypeTable["PacketLoss"] = static_cast<int>(FaultType::PacketLoss);
        faultTypeTable["SampleRateAnomaly"] = static_cast<int>(FaultType::SampleRateAnomaly);
        lua["FaultType"] = faultTypeTable;
    }
};

TEST_F(Sol2EnumBindingTest, GlobalConstantStyle) {
    int val = lua.script("return DeviceStatus_Online");
    EXPECT_EQ(val, static_cast<int>(DeviceStatus::Online));
}

TEST_F(Sol2EnumBindingTest, TableStyle) {
    int val = lua.script("return DeviceStatus.Simulated");
    EXPECT_EQ(val, static_cast<int>(DeviceStatus::Simulated));
}

TEST_F(Sol2EnumBindingTest, EnumInCondition) {
    lua.script(R"(
        status = DeviceStatus.Online
        if status == DeviceStatus.Online then
            is_online = true
        else
            is_online = false
        end
    )");
    bool isOnline = lua["is_online"];
    EXPECT_TRUE(isOnline);
}

TEST_F(Sol2EnumBindingTest, EnumAsFunctionParameter) {
    lua["check_status"] = [](int status) -> bool {
        return status == static_cast<int>(DeviceStatus::Simulated);
    };
    bool result = lua.script("return check_status(DeviceStatus.Simulated)");
    EXPECT_TRUE(result);
    result = lua.script("return check_status(DeviceStatus.Online)");
    EXPECT_FALSE(result);
}

// ============================================================
// 实验1-5: Lua回调C++函数（模拟IATP Lua API）
// ============================================================

class Sol2CallbackTest : public ::testing::Test {
protected:
    sol::state lua;
    // 模拟ICD层
    std::vector<std::pair<std::string, double>> setSignalLog;
    std::vector<std::tuple<std::string, double, double, double>> verifyLog;

    void SetUp() override {
        lua.open_libraries(sol::lib::base);

        // 模拟SetDevice API
        lua["SetDevice"] = [this](const std::string& signalId, double value) {
            setSignalLog.emplace_back(signalId, value);
        };

        // 模拟VerifyDevice API
        lua["VerifyDevice"] = [this](const std::string& signalId, double expected,
                                      double tolMin, double tolMax) -> bool {
            verifyLog.emplace_back(signalId, expected, tolMin, tolMax);
            return true;  // 模拟验证通过
        };

        // 模拟Delay
        lua["Delay"] = [](int ms) {
            // 实际测试中不做真正等待
        };

        // 模拟Log
        lua["Log"] = [](const std::string& msg) {
            // 日志输出
        };

        // 模拟InjectFault
        lua["InjectFault"] = [](const std::string& signalId, const std::string& faultType,
                                double faultValue) {
            // 故障注入
        };

        // 模拟ClearFault
        lua["ClearFault"] = [](const std::string& signalId) {
            // 故障清除
        };
    }
};

TEST_F(Sol2CallbackTest, SetDeviceCall) {
    lua.script(R"(
        SetDevice("温度", 37.5)
        SetDevice("压力", 5.0)
    )");
    ASSERT_EQ(setSignalLog.size(), 2u);
    EXPECT_EQ(setSignalLog[0].first, "温度");
    EXPECT_DOUBLE_EQ(setSignalLog[0].second, 37.5);
    EXPECT_EQ(setSignalLog[1].first, "压力");
    EXPECT_DOUBLE_EQ(setSignalLog[1].second, 5.0);
}

TEST_F(Sol2CallbackTest, VerifyDeviceCall) {
    lua.script(R"(
        result = VerifyDevice("温度", 37.5, -0.1, 0.1)
    )");
    ASSERT_EQ(verifyLog.size(), 1u);
    EXPECT_EQ(std::get<0>(verifyLog[0]), "温度");
    EXPECT_DOUBLE_EQ(std::get<1>(verifyLog[0]), 37.5);
    EXPECT_DOUBLE_EQ(std::get<2>(verifyLog[0]), -0.1);
    EXPECT_DOUBLE_EQ(std::get<3>(verifyLog[0]), 0.1);
}

TEST_F(Sol2CallbackTest, FullTestCaseScript) {
    // 模拟一个完整测试用例的Lua脚本
    lua.script(R"(
        -- 设置温度
        SetDevice("温度", 37.5)
        Delay(1000)

        -- 验证温度
        local ok = VerifyDevice("温度", 37.5, -0.1, 0.1)
        if ok then
            Log("温度验证通过")
        end

        -- 注入故障
        InjectFault("温度", "stuck_at", 999)
        Delay(500)
        ClearFault("温度")
    )");
    EXPECT_EQ(setSignalLog.size(), 1u);
    EXPECT_EQ(verifyLog.size(), 1u);
}

// ============================================================
// 实验1-6: safe_script错误恢复
// ============================================================

class Sol2ErrorRecoveryTest : public ::testing::Test {
protected:
    sol::state lua;
    void SetUp() override {
        lua.open_libraries(sol::lib::base, sol::lib::os);
    }
};

TEST_F(Sol2ErrorRecoveryTest, SafeScriptCatchRuntimeError) {
    // 运行时错误：对nil做算术
    auto result = lua.safe_script("local x = nil; return x + 1", sol::script_pass_on_error);
    EXPECT_FALSE(result.valid());
    sol::error err = result;
    EXPECT_NE(std::string(err.what()).find("attempt to perform arithmetic on a nil value"),
              std::string::npos);
}

TEST_F(Sol2ErrorRecoveryTest, SafeScriptCatchSyntaxError) {
    auto result = lua.safe_script("if true then", sol::script_pass_on_error);
    EXPECT_FALSE(result.valid());
}

TEST_F(Sol2ErrorRecoveryTest, VmStillUsableAfterError) {
    // 错误后VM应该仍然可用
    auto bad = lua.safe_script("error('test error')", sol::script_pass_on_error);
    EXPECT_FALSE(bad.valid());

    // 后续正常脚本应该能执行
    int val = lua.script("return 1 + 1");
    EXPECT_EQ(val, 2);
}

TEST_F(Sol2ErrorRecoveryTest, ProtectedFunctionCall) {
    lua.script(R"(
        function might_fail(x)
            if x < 0 then
                error("x must be non-negative")
            end
            return x * 2
        end
    )");

    // 正常调用
    sol::protected_function func = lua["might_fail"];
    auto good = func(5);
    EXPECT_TRUE(good.valid());
    EXPECT_EQ(good.get<int>(), 10);

    // 错误调用
    auto bad = func(-1);
    EXPECT_FALSE(bad.valid());
}

TEST_F(Sol2ErrorRecoveryTest, WhileTimeoutErrorSimulation) {
    // 模拟WHILE指令超时报错
    lua["GetDevice"] = [](const std::string&) -> double { return 10.0; };
    lua["SetDevice"] = [](const std::string&, double) {};
    lua["Delay"] = [](int) {};

    // 模拟WHILE超时错误
    // Delay设为真实等待使os.clock()能检测到时间流逝
    lua["Delay"] = [](int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    };

    auto result = lua.safe_script(R"(
        local _start = os.clock()
        while GetDevice("温度") < 30.0 do
            if (os.clock() - _start) * 1000 >= 50 then
                error("WHILE超时: 温度 < 30.0")
            end
            SetDevice("加热器", 1)
            Delay(10)
        end
    )", sol::script_pass_on_error);

    EXPECT_FALSE(result.valid());
    sol::error err = result;
    EXPECT_NE(std::string(err.what()).find("WHILE超时"), std::string::npos);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
