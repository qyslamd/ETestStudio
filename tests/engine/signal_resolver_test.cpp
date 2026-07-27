#include <gtest/gtest.h>

#include <QString>
#include <QVector>

#include "SignalResolver.h"

#include "SignalRegistry.h"

#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>

using namespace etest::engine;
using namespace etest::core;

// ══════════════════════════════════════════════════════════════════════════════
//  Helper: 创建测试用的 ICD Repository
// ══════════════════════════════════════════════════════════════════════════════

static std::unique_ptr<icd::Repository> createTestRepository() {
    auto repo = std::make_unique<icd::Repository>();

    // 创建 ICD 帧: "TestFrame", id=1, FrameType::data, LittleEndian
    auto frame = std::make_unique<icd::Frame>(
        1, "TestFrame", "Test ICD frame for unit test",
        icd::FrameType::data, icd::ByteOrder::little_endian);

    // 创建节点属性: 工程值范围 0~100, 缩放系数 a=0.5, b=1.0, 单位 mV
    icd::NodeAttrs sigAttrs;
    sigAttrs.unit = "mV";
    sigAttrs.min = 0.0f;
    sigAttrs.max = 100.0f;
    sigAttrs.scale_a = 0.5f;
    sigAttrs.scale_b = 1.0f;
    sigAttrs.is_scaled = true;

    auto signalNode = std::make_unique<icd::Node>(
        "FuelValve",      // name
        "Fuel valve position sensor",
        0,                // offset (byte offset = 0)
        0,                // bit_offset = 0
        16,               // bit_width = 16 (word)
        icd::ValueType::word,
        icd::Tag::none,
        sigAttrs);

    // 创建中间分组节点，模拟节点树
    icd::NodeAttrs emptyAttrs;
    auto groupNode = std::make_unique<icd::Node>(
        "业务数据", "Business data group",
        0, 0, 0,
        icd::ValueType::unknown,
        icd::Tag::none,
        emptyAttrs);
    groupNode->add_child(std::move(signalNode));

    // 将分组节点添加到帧
    frame->add_root(std::move(groupNode));

    // 将帧添加到仓库
    repo->add_frame(std::move(frame));

    return repo;
}

// ══════════════════════════════════════════════════════════════════════════════
//  Helper: 初始化测试用的 SignalRegistry（QObject 不可拷贝，使用指针）
// ══════════════════════════════════════════════════════════════════════════════

static void initTestRegistry(SignalRegistry* reg) {
    reg->registerDevice("dev-fuel-001", "燃油阀门控制器");
    reg->bindPortToFrames("dev-fuel-001", "ch0", {"TestFrame"});
    reg->registerSignals({{"dev-fuel-001", "ch0", "TestFrame", "业务数据/FuelValve"}});
}

// ══════════════════════════════════════════════════════════════════════════════
//  测试用例
// ══════════════════════════════════════════════════════════════════════════════

// Test 1: 空 registry / 空 repo 返回 invalid
TEST(SignalResolverTest, NullRegistryReturnsInvalid) {
    SignalResolver resolver(nullptr, nullptr);
    auto result = resolver.resolve("some-uuid");
    EXPECT_FALSE(result.valid);
}

TEST(SignalResolverTest, NullRepoReturnsInvalid) {
    SignalRegistry registry;
    SignalResolver resolver(&registry, nullptr);
    auto result = resolver.resolve("some-uuid");
    EXPECT_FALSE(result.valid);
}

TEST(SignalResolverTest, NullRegistryWithRepoReturnsInvalid) {
    auto repo = createTestRepository();
    SignalResolver resolver(nullptr, repo.get());
    auto result = resolver.resolve("some-uuid");
    EXPECT_FALSE(result.valid);
}

// Test 2: 未知 UUID 返回 invalid
TEST(SignalResolverTest, UnknownUuidReturnsInvalid) {
    SignalRegistry registry;
    initTestRegistry(&registry);
    auto repo = createTestRepository();
    SignalResolver resolver(&registry, repo.get());

    auto result = resolver.resolve("00000000000000000000000000000000");
    EXPECT_FALSE(result.valid);
}

// Test 3: 已知 UUID 返回 valid 且 deviceId 正确
TEST(SignalResolverTest, KnownUuidReturnsValid) {
    SignalRegistry registry;
    initTestRegistry(&registry);
    auto repo = createTestRepository();
    SignalResolver resolver(&registry, repo.get());

    // 通过 SignalRegistry 计算 UUID
    QString uuid = SignalRegistry::computeUuid(
        "dev-fuel-001", "ch0", "TestFrame", "业务数据/FuelValve");

    auto result = resolver.resolve(uuid);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.deviceId, "dev-fuel-001");
    EXPECT_EQ(result.deviceName, "燃油阀门控制器");
    EXPECT_EQ(result.portName, "ch0");
    EXPECT_EQ(result.frameName, "TestFrame");
    EXPECT_EQ(result.nodePath, "业务数据/FuelValve");
}

// Test 4: 解析 ICD 节点属性（coeff, offset, unit, engMin, engMax）
TEST(SignalResolverTest, FillsIcdNodeAttributes) {
    SignalRegistry registry;
    initTestRegistry(&registry);
    auto repo = createTestRepository();
    SignalResolver resolver(&registry, repo.get());

    QString uuid = SignalRegistry::computeUuid(
        "dev-fuel-001", "ch0", "TestFrame", "业务数据/FuelValve");

    auto result = resolver.resolve(uuid);
    ASSERT_TRUE(result.valid);

    // ICD 缩放属性
    EXPECT_DOUBLE_EQ(result.coeff, 0.5);
    EXPECT_DOUBLE_EQ(result.offset, 1.0);
    EXPECT_EQ(result.unit, QStringLiteral("mV"));

    // 工程值范围
    EXPECT_DOUBLE_EQ(result.engMin, 0.0);
    EXPECT_DOUBLE_EQ(result.engMax, 100.0);

    // 帧级属性
    EXPECT_EQ(result.frameId, 1u);
    EXPECT_EQ(result.byteOrder, ByteOrder::LittleEndian);

    // 节点级位域属性
    EXPECT_EQ(result.byteOffset, 0);
    EXPECT_EQ(result.bitOffset, 0);
    EXPECT_EQ(result.bitWidth, 16);

    // 信号类型从帧类型推断 (FrameType::data → CAN)
    EXPECT_EQ(result.signalType, SignalType::CAN);
}

// Test 5: Frame 不存在时返回 valid 但 ICD 属性为默认值
TEST(SignalResolverTest, MissingFrameUsesDefaults) {
    SignalRegistry reg;
    reg.registerDevice("dev-01", "TestDev");
    reg.bindPortToFrames("dev-01", "p1", {"NonExistentFrame"});
    reg.registerSignals({{"dev-01", "p1", "NonExistentFrame", "SomeNode"}});

    auto repo = std::make_unique<icd::Repository>();
    SignalResolver resolver(&reg, repo.get());

    QString uuid = SignalRegistry::computeUuid(
        "dev-01", "p1", "NonExistentFrame", "SomeNode");

    auto result = resolver.resolve(uuid);
    // valid=true 因为设备/端口在 SignalRegistry 中找到
    ASSERT_TRUE(result.valid);
    // ICD 属性应该保持默认值
    EXPECT_EQ(result.unit, QString());
    EXPECT_DOUBLE_EQ(result.coeff, 1.0);
    EXPECT_DOUBLE_EQ(result.offset, 0.0);
    EXPECT_EQ(result.frameId, 0u);
}

// Test 6: Node 路径在 ICD 中不存在时返回 valid 但编码属性为默认值
TEST(SignalResolverTest, MissingNodeUsesDefaults) {
    SignalRegistry reg;
    reg.registerDevice("dev-01", "TestDev");
    reg.bindPortToFrames("dev-01", "p1", {"TestFrame"});
    reg.registerSignals(
        {{"dev-01", "p1", "TestFrame", "NonExistentNode"}});

    auto repo = createTestRepository();
    SignalResolver resolver(&reg, repo.get());

    QString uuid = SignalRegistry::computeUuid(
        "dev-01", "p1", "TestFrame", "NonExistentNode");

    auto result = resolver.resolve(uuid);
    ASSERT_TRUE(result.valid);
    // frame 能找到，所以 frameId 被设置
    EXPECT_EQ(result.frameId, 1u);
    // 但 node 找不到，节点级属性保持默认
    EXPECT_EQ(result.byteOffset, 0);
    EXPECT_EQ(result.bitWidth, 0);
    EXPECT_DOUBLE_EQ(result.coeff, 1.0);
}

// Test 7: ByteOrder 映射正确（BigEndian）
TEST(SignalResolverTest, BigEndianByteOrder) {
    auto repo = std::make_unique<icd::Repository>();
    auto frame = std::make_unique<icd::Frame>(
        2, "BEFrame", "Big endian test frame",
        icd::FrameType::data, icd::ByteOrder::big_endian);

    icd::NodeAttrs emptyAttrs;
    auto node = std::make_unique<icd::Node>(
        "SigBE", "", 0, 0, 8,
        icd::ValueType::byte_,
        icd::Tag::none,
        emptyAttrs);
    frame->add_root(std::move(node));
    repo->add_frame(std::move(frame));

    SignalRegistry reg;
    reg.registerDevice("dev-02", "BEDev");
    reg.bindPortToFrames("dev-02", "p1", {"BEFrame"});
    reg.registerSignals({{"dev-02", "p1", "BEFrame", "SigBE"}});

    SignalResolver resolver(&reg, repo.get());
    QString uuid = SignalRegistry::computeUuid(
        "dev-02", "p1", "BEFrame", "SigBE");

    auto result = resolver.resolve(uuid);
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.byteOrder, ByteOrder::BigEndian);
}

// Test 8: SignalType 从 FrameType 推断
TEST(SignalResolverTest, SignalTypeFromFrameType) {
    auto repo = std::make_unique<icd::Repository>();

    // FrameType::data → CAN
    auto dataFrame = std::make_unique<icd::Frame>(
        3, "DataFrame", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    icd::NodeAttrs emptyAttrs;
    dataFrame->add_root(std::make_unique<icd::Node>(
        "Sig1", "", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, emptyAttrs));
    repo->add_frame(std::move(dataFrame));

    // FrameType::cmd → SERIAL
    auto cmdFrame = std::make_unique<icd::Frame>(
        4, "CmdFrame", "", icd::FrameType::cmd, icd::ByteOrder::little_endian);
    cmdFrame->add_root(std::make_unique<icd::Node>(
        "Sig2", "", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, emptyAttrs));
    repo->add_frame(std::move(cmdFrame));

    // FrameType::data_cmd → CAN
    auto dcFrame = std::make_unique<icd::Frame>(
        5, "DCFrame", "", icd::FrameType::data_cmd, icd::ByteOrder::little_endian);
    dcFrame->add_root(std::make_unique<icd::Node>(
        "Sig3", "", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, emptyAttrs));
    repo->add_frame(std::move(dcFrame));

    SignalRegistry reg;
    reg.registerDevice("dev-03", "TypeDev");
    reg.bindPortToFrames("dev-03", "p1", {"DataFrame", "CmdFrame", "DCFrame"});
    reg.registerSignals({{"dev-03", "p1", "DataFrame", "Sig1"},
                          {"dev-03", "p1", "CmdFrame", "Sig2"},
                          {"dev-03", "p1", "DCFrame", "Sig3"}});

    SignalResolver resolver(&reg, repo.get());

    QString uuid1 = SignalRegistry::computeUuid("dev-03", "p1", "DataFrame", "Sig1");
    EXPECT_EQ(resolver.resolve(uuid1).signalType, SignalType::CAN);

    QString uuid2 = SignalRegistry::computeUuid("dev-03", "p1", "CmdFrame", "Sig2");
    EXPECT_EQ(resolver.resolve(uuid2).signalType, SignalType::SERIAL);

    QString uuid3 = SignalRegistry::computeUuid("dev-03", "p1", "DCFrame", "Sig3");
    EXPECT_EQ(resolver.resolve(uuid3).signalType, SignalType::CAN);
}

// Test 9: 简单节点路径（无 "/" 分隔）也能正常解析
TEST(SignalResolverTest, FlatNodePathResolution) {
    auto repo = std::make_unique<icd::Repository>();
    auto frame = std::make_unique<icd::Frame>(
        6, "FlatFrame", "", icd::FrameType::data, icd::ByteOrder::little_endian);

    icd::NodeAttrs attrs;
    attrs.unit = "degC";
    attrs.is_scaled = true;
    attrs.scale_a = 0.1f;

    frame->add_root(std::make_unique<icd::Node>(
        "Temperature", "", 4, 0, 16,
        icd::ValueType::word, icd::Tag::none, attrs));
    repo->add_frame(std::move(frame));

    SignalRegistry reg;
    reg.registerDevice("dev-04", "TempDev");
    reg.bindPortToFrames("dev-04", "p1", {"FlatFrame"});
    reg.registerSignals({{"dev-04", "p1", "FlatFrame", "Temperature"}});

    SignalResolver resolver(&reg, repo.get());
    QString uuid = SignalRegistry::computeUuid(
        "dev-04", "p1", "FlatFrame", "Temperature");

    auto result = resolver.resolve(uuid);
    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.unit, QStringLiteral("degC"));
    EXPECT_NEAR(result.coeff, 0.1, 1e-6);
    EXPECT_EQ(result.byteOffset, 4);
    EXPECT_EQ(result.bitWidth, 16);
}

// Test 10: 多个信号批量解析一致性
TEST(SignalResolverTest, MultipleSignalsResolveCorrectly) {
    auto repo = std::make_unique<icd::Repository>();

    icd::NodeAttrs emptyAttrs;
    auto frame = std::make_unique<icd::Frame>(
        10, "MultiFrame", "", icd::FrameType::data, icd::ByteOrder::little_endian);

    icd::NodeAttrs attrs1;
    attrs1.unit = "kPa";
    attrs1.scale_a = 2.0f;
    attrs1.scale_b = -1.0f;
    attrs1.is_scaled = true;

    icd::NodeAttrs attrs2;
    attrs2.unit = "rpm";
    attrs2.scale_a = 1.0f;
    attrs2.is_scaled = true;

    frame->add_root(std::make_unique<icd::Node>(
        "Pressure", "", 0, 0, 16,
        icd::ValueType::word, icd::Tag::none, attrs1));
    frame->add_root(std::make_unique<icd::Node>(
        "Speed", "", 2, 0, 16,
        icd::ValueType::word, icd::Tag::none, attrs2));
    repo->add_frame(std::move(frame));

    SignalRegistry reg;
    reg.registerDevice("dev-05", "MultiDev");
    reg.bindPortToFrames("dev-05", "p1", {"MultiFrame"});
    reg.registerSignals({{"dev-05", "p1", "MultiFrame", "Pressure"},
                          {"dev-05", "p1", "MultiFrame", "Speed"}});

    SignalResolver resolver(&reg, repo.get());

    QString uuidP = SignalRegistry::computeUuid("dev-05", "p1", "MultiFrame", "Pressure");
    auto resultP = resolver.resolve(uuidP);
    ASSERT_TRUE(resultP.valid);
    EXPECT_EQ(resultP.unit, QStringLiteral("kPa"));
    EXPECT_DOUBLE_EQ(resultP.coeff, 2.0);
    EXPECT_DOUBLE_EQ(resultP.offset, -1.0);
    EXPECT_EQ(resultP.byteOffset, 0);

    QString uuidS = SignalRegistry::computeUuid("dev-05", "p1", "MultiFrame", "Speed");
    auto resultS = resolver.resolve(uuidS);
    ASSERT_TRUE(resultS.valid);
    EXPECT_EQ(resultS.unit, QStringLiteral("rpm"));
    EXPECT_DOUBLE_EQ(resultS.coeff, 1.0);
    EXPECT_DOUBLE_EQ(resultS.offset, 0.0);
    EXPECT_EQ(resultS.byteOffset, 2);
}
