#include <gtest/gtest.h>
#include <QStringList>
#include <QVector>

#include "SignalRegistry.h"

using namespace etest::core;

// ══════════════════════════════════════════════════════════════════════════════
//  computeUuid — 确定性纯函数
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalRegistryTest, ComputeUuidDeterministic) {
  auto u1 = SignalRegistry::computeUuid("dev-id-001", "ch0", "A429_发送",
                                        "业务数据/燃油阀门1");
  auto u2 = SignalRegistry::computeUuid("dev-id-001", "ch0", "A429_发送",
                                        "业务数据/燃油阀门1");
  EXPECT_EQ(u1, u2);
  EXPECT_EQ(u1.size(), 32);
  // 验证是合法的 hex 字符
  for (const QChar& c : u1) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  }
}

TEST(SignalRegistryTest, ComputeUuidDifferentDeviceDifferentUuid) {
  // 多台同型号设备：deviceId 不同 → UUID 不同
  auto u1 = SignalRegistry::computeUuid("dev-001", "ch0", "A429_发送",
                                        "业务数据/燃油阀门1");
  auto u2 = SignalRegistry::computeUuid("dev-002", "ch0", "A429_发送",
                                        "业务数据/燃油阀门1");
  EXPECT_NE(u1, u2);
}

TEST(SignalRegistryTest, ComputeUuidDifferentPortDifferentUuid) {
  auto u1 = SignalRegistry::computeUuid("dev-001", "ch0", "A429_发送",
                                        "业务数据/燃油阀门1");
  auto u2 = SignalRegistry::computeUuid("dev-001", "ch1", "A429_发送",
                                        "业务数据/燃油阀门1");
  EXPECT_NE(u1, u2);
}

TEST(SignalRegistryTest, ComputeUuidDifferentFrameDifferentUuid) {
  auto u1 = SignalRegistry::computeUuid("dev-001", "ch0", "A429_发送",
                                        "业务数据/燃油阀门1");
  auto u2 = SignalRegistry::computeUuid("dev-001", "ch0", "A429_接收",
                                        "业务数据/燃油阀门1");
  EXPECT_NE(u1, u2);
}

TEST(SignalRegistryTest, ComputeUuidDifferentNodePathDifferentUuid) {
  auto u1 = SignalRegistry::computeUuid("dev-001", "ch0", "A429_发送",
                                        "业务数据/燃油阀门1");
  auto u2 = SignalRegistry::computeUuid("dev-001", "ch0", "A429_发送",
                                        "业务数据/燃油阀门2");
  EXPECT_NE(u1, u2);
}

// ══════════════════════════════════════════════════════════════════════════════
//  设备注册
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalRegistryTest, RegisterDevice) {
  SignalRegistry reg;
  EXPECT_TRUE(reg.registeredDeviceIds().isEmpty());

  reg.registerDevice("dev-001", "EPH5272-1");
  reg.registerDevice("dev-002", "EPH5272-2");

  QStringList ids = reg.registeredDeviceIds();
  EXPECT_EQ(ids.size(), 2);
  EXPECT_TRUE(ids.contains("dev-001"));
  EXPECT_TRUE(ids.contains("dev-002"));
  EXPECT_EQ(reg.deviceName("dev-001"), "EPH5272-1");
  EXPECT_EQ(reg.deviceName("dev-002"), "EPH5272-2");
  // 未注册的设备应该返回空字符串
  EXPECT_TRUE(reg.deviceName("dev-999").isEmpty());
}

// ══════════════════════════════════════════════════════════════════════════════
//  端口绑定
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalRegistryTest, BindAndForEachPortBinding) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送", "A429_接收"});

  QVector<QString> capturedDevices;
  QVector<QString> capturedPorts;
  reg.forEachPortBinding([&](const QString& deviceId, const QString& portName,
                              const QStringList& frameNames) {
    capturedDevices.append(deviceId);
    capturedPorts.append(portName);
    EXPECT_EQ(frameNames.size(), 2);
  });
  ASSERT_EQ(capturedDevices.size(), 1);
  EXPECT_EQ(capturedDevices[0], "dev-001");
  EXPECT_EQ(capturedPorts[0], "ch0");
}

TEST(SignalRegistryTest, UnbindPort) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送"});
  reg.unbindPort("dev-001", "ch0");

  int count = 0;
  reg.forEachPortBinding(
      [&](const QString&, const QString&, const QStringList&) { ++count; });
  EXPECT_EQ(count, 0);
}

// ══════════════════════════════════════════════════════════════════════════════
//  信号注册与查询
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalRegistryTest, RegisterAndResolve) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送"});

  reg.registerSignals({{"dev-001", "ch0", "A429_发送", "业务数据/燃油阀门1"},
                       {"dev-001", "ch0", "A429_发送", "业务数据/燃油阀门2"}});

  QString uuid = SignalRegistry::computeUuid("dev-001", "ch0", "A429_发送",
                                              "业务数据/燃油阀门1");
  auto r = reg.resolve(uuid);
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(r->deviceId, "dev-001");
  EXPECT_EQ(r->deviceName, "EPH5272-1");
  EXPECT_EQ(r->portName, "ch0");
  EXPECT_EQ(r->frameName, "A429_发送");
  EXPECT_EQ(r->nodePath, "业务数据/燃油阀门1");
  EXPECT_EQ(r->uuid, uuid);
}

TEST(SignalRegistryTest, ResolveNonExistent) {
  SignalRegistry reg;
  auto r = reg.resolve("00000000000000000000000000000000");
  EXPECT_FALSE(r.has_value());
}

TEST(SignalRegistryTest, FindByNode) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.registerDevice("dev-002", "EPH5272-2");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送"});
  reg.bindPortToFrames("dev-002", "ch0", {"A429_发送"});

  // 两台设备的同一 frame+nodePath 都注册
  reg.registerSignals({{"dev-001", "ch0", "A429_发送", "root/temp"},
                       {"dev-002", "ch0", "A429_发送", "root/temp"}});

  auto results = reg.findByNode("A429_发送", "root/temp");
  EXPECT_EQ(results.size(), 2);
}

TEST(SignalRegistryTest, FindByPort) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送", "A429_接收"});

  reg.registerSignals({{"dev-001", "ch0", "A429_发送", "root/temp"},
                       {"dev-001", "ch0", "A429_接收", "root/pressure"}});

  auto results = reg.findByPort("dev-001", "ch0");
  EXPECT_EQ(results.size(), 2);

  // 不存在的端口
  auto empty = reg.findByPort("dev-001", "ch1");
  EXPECT_TRUE(empty.isEmpty());
}

// ══════════════════════════════════════════════════════════════════════════════
//  清理
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalRegistryTest, ClearSignals) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送"});
  reg.registerSignals({{"dev-001", "ch0", "A429_发送", "root/sig1"}});
  EXPECT_TRUE(reg.resolve(SignalRegistry::computeUuid(
                               "dev-001", "ch0", "A429_发送", "root/sig1"))
                  .has_value());

  // clearSignals 只清信号索引，保留设备和端口绑定
  reg.clearSignals();
  EXPECT_FALSE(reg.resolve(SignalRegistry::computeUuid(
                                "dev-001", "ch0", "A429_发送", "root/sig1"))
                   .has_value());
  EXPECT_EQ(reg.deviceName("dev-001"), "EPH5272-1");  // 设备名还在
  int bindingCount = 0;
  reg.forEachPortBinding(
      [&](const QString&, const QString&, const QStringList&) {
        ++bindingCount;
      });
  EXPECT_EQ(bindingCount, 1);  // 绑定还在
}

TEST(SignalRegistryTest, ClearAll) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送"});
  reg.registerSignals({{"dev-001", "ch0", "A429_发送", "root/sig1"}});

  reg.clear();
  EXPECT_TRUE(reg.registeredDeviceIds().isEmpty());
  int bindingCount = 0;
  reg.forEachPortBinding(
      [&](const QString&, const QString&, const QStringList&) {
        ++bindingCount;
      });
  EXPECT_EQ(bindingCount, 0);
}

// ══════════════════════════════════════════════════════════════════════════════
//  resolveByTuple — 纯计算 + 索引
// ══════════════════════════════════════════════════════════════════════════════

TEST(SignalRegistryTest, ResolveByTuple) {
  SignalRegistry reg;
  reg.registerDevice("dev-001", "EPH5272-1");
  reg.bindPortToFrames("dev-001", "ch0", {"A429_发送"});
  reg.registerSignals({{"dev-001", "ch0", "A429_发送", "root/sig1"}});

  QString uuid = reg.resolveByTuple("dev-001", "ch0", "A429_发送", "root/sig1");
  EXPECT_EQ(uuid.size(), 32);

  // 与 computeUuid 等价
  QString expected =
      SignalRegistry::computeUuid("dev-001", "ch0", "A429_发送", "root/sig1");
  EXPECT_EQ(uuid, expected);
}
