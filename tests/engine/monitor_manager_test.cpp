#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "MonitorManager.h"

using namespace etest::engine;

// ══════════════════════════════════════════════════════════════════════════════
// 辅助：构建一个简单的拓扑 JSON，含 2 个 monitor 和 3 个 tap
// ══════════════════════════════════════════════════════════════════════════════
static QJsonObject makeTestTopology() {
    QJsonObject root;

    // devices
    QJsonArray devicesArr;
    QJsonObject dev1;
    dev1[QStringLiteral("id")] = QStringLiteral("dev-uuid-1");
    dev1[QStringLiteral("name")] = QStringLiteral("AD卡1");
    dev1[QStringLiteral("type")] = QStringLiteral("ad");
    devicesArr.append(dev1);
    QJsonObject dev2;
    dev2[QStringLiteral("id")] = QStringLiteral("dev-uuid-2");
    dev2[QStringLiteral("name")] = QStringLiteral("串口1");
    dev2[QStringLiteral("type")] = QStringLiteral("serial");
    devicesArr.append(dev2);
    root[QStringLiteral("devices")] = devicesArr;

    // monitors
    QJsonArray monitorsArr;

    // Monitor 0: AD监听器, 2通道
    QJsonObject mon0;
    mon0[QStringLiteral("name")] = QStringLiteral("监听器_AD");
    mon0[QStringLiteral("deviceType")] = QStringLiteral("monitor_ad");
    mon0[QStringLiteral("channelCount")] = 2;
    QJsonArray taps0;
    QJsonObject tap00;
    tap00[QStringLiteral("deviceId")] = QStringLiteral("dev-uuid-1");
    tap00[QStringLiteral("devicePort")] = QStringLiteral("ch0");
    tap00[QStringLiteral("productName")] = QStringLiteral("UUT1");
    tap00[QStringLiteral("portName")] = QStringLiteral("out1");
    tap00[QStringLiteral("deviceName")] = QStringLiteral("AD卡1");
    tap00[QStringLiteral("displayMode")] = QStringLiteral("waveform");
    taps0.append(tap00);
    QJsonObject tap01;
    tap01[QStringLiteral("deviceId")] = QStringLiteral("dev-uuid-1");
    tap01[QStringLiteral("devicePort")] = QStringLiteral("ch1");
    tap01[QStringLiteral("productName")] = QStringLiteral("UUT1");
    tap01[QStringLiteral("portName")] = QStringLiteral("out2");
    tap01[QStringLiteral("deviceName")] = QStringLiteral("AD卡1");
    tap01[QStringLiteral("displayMode")] = QStringLiteral("waveform");
    taps0.append(tap01);
    mon0[QStringLiteral("taps")] = taps0;
    monitorsArr.append(mon0);

    // Monitor 1: 串口监听器, 1通道
    QJsonObject mon1;
    mon1[QStringLiteral("name")] = QStringLiteral("监听器_串口");
    mon1[QStringLiteral("deviceType")] = QStringLiteral("monitor_serial");
    mon1[QStringLiteral("channelCount")] = 1;
    QJsonArray taps1;
    QJsonObject tap10;
    tap10[QStringLiteral("deviceId")] = QStringLiteral("dev-uuid-2");
    tap10[QStringLiteral("devicePort")] = QStringLiteral("tx");
    tap10[QStringLiteral("productName")] = QStringLiteral("UUT1");
    tap10[QStringLiteral("portName")] = QStringLiteral("rx");
    tap10[QStringLiteral("deviceName")] = QStringLiteral("串口1");
    tap10[QStringLiteral("displayMode")] = QStringLiteral("meter");
    taps1.append(tap10);
    mon1[QStringLiteral("taps")] = taps1;
    monitorsArr.append(mon1);

    root[QStringLiteral("monitors")] = monitorsArr;
    return root;
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 1: loadFromTopology 正确创建查表和树缓存
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, LoadFromTopology) {
    MonitorManager mgr;
    QJsonObject topo = makeTestTopology();
    mgr.loadFromTopology(topo);

    // verify monitorTree
    auto tree = mgr.monitorTree();
    ASSERT_EQ(tree.size(), 2);
    EXPECT_EQ(tree[0].name, QStringLiteral("监听器_AD"));
    EXPECT_EQ(tree[0].deviceType, QStringLiteral("monitor_ad"));
    EXPECT_EQ(tree[0].channelCount, 2);
    EXPECT_EQ(tree[1].name, QStringLiteral("监听器_串口"));
    EXPECT_EQ(tree[1].channelCount, 1);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 2: onHardwareOpFinished 命中 tap 时写入 buffer
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, MatchTapAndBuffer) {
    MonitorManager mgr;
    mgr.loadFromTopology(makeTestTopology());

    // 发射与 dev-uuid-1 ch0 匹配的信号（AD 卡的 ch0）
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(),  // 无帧数据（AD 类）
                              2699.0,        // rawValue
                              5.02);         // engValue

    // flush 看结果
    QJsonArray result = mgr.flushSamples();
    ASSERT_EQ(result.size(), 1);  // 只有一个有数据的 monitor

    QJsonObject monObj = result[0].toObject();
    EXPECT_EQ(monObj[QStringLiteral("name")].toString(),
              QStringLiteral("监听器_AD"));

    QJsonArray channels = monObj[QStringLiteral("channels")].toArray();
    ASSERT_EQ(channels.size(), 1);  // 只有 ch0 有数据
    EXPECT_EQ(channels[0].toObject()[QStringLiteral("index")].toInt(), 0);

    QJsonArray samples = channels[0].toObject()[QStringLiteral("samples")].toArray();
    ASSERT_EQ(samples.size(), 1);
    EXPECT_DOUBLE_EQ(samples[0].toObject()[QStringLiteral("eng")].toDouble(), 5.02);
    EXPECT_DOUBLE_EQ(samples[0].toObject()[QStringLiteral("raw")].toDouble(), 2699.0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 3: subscribe 回调在匹配 tap 时被调用
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, SubscribeCallback) {
    MonitorManager mgr;
    mgr.loadFromTopology(makeTestTopology());

    bool called = false;
    MonitorSample captured;
    mgr.subscribe(0, 0, [&](const MonitorSample& sample) {
        called = true;
        captured = sample;
    });

    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 999.0, 12.34);
    mgr.flushNow();  // 定时器批处理改为同步触发

    EXPECT_TRUE(called);
    EXPECT_DOUBLE_EQ(captured.rawValue, 999.0);
    EXPECT_DOUBLE_EQ(captured.engValue, 12.34);
    EXPECT_EQ(captured.monitorIndex, 0);
    EXPECT_EQ(captured.channelIndex, 0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 4: unsubscribe 后不再收到回调
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, Unsubscribe) {
    MonitorManager mgr;
    mgr.loadFromTopology(makeTestTopology());

    int callCount = 0;
    mgr.subscribe(0, 0, [&](const MonitorSample&) { ++callCount; });
    mgr.unsubscribe(0, 0);

    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 100.0, 1.0);
    mgr.flushNow();

    EXPECT_EQ(callCount, 0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 5: 不匹配的 deviceId/port 不会崩溃
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, NoMatchNoCrash) {
    MonitorManager mgr;
    mgr.loadFromTopology(makeTestTopology());

    // 完全不存在的设备端口
    mgr.onHardwareOpFinished(QStringLiteral("dev-nonexistent"),
                              QStringLiteral("port-xyz"),
                              QByteArray(), 0.0, 0.0);

    QJsonArray result = mgr.flushSamples();
    EXPECT_TRUE(result.isEmpty());
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 6: flushSamples 后 buffer 清空
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, FlushClearsBuffer) {
    MonitorManager mgr;
    mgr.loadFromTopology(makeTestTopology());

    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 100.0, 1.0);

    QJsonArray first = mgr.flushSamples();
    EXPECT_EQ(first.size(), 1);

    // 再次 flush 应为空
    QJsonArray second = mgr.flushSamples();
    EXPECT_TRUE(second.isEmpty());
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 7: clearRuntime 清 buffer 但保留结构和订阅
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, ClearRuntime) {
    MonitorManager mgr;
    mgr.loadFromTopology(makeTestTopology());

    // 建立一个订阅（用计数器验证 clearRuntime 后回调仍能触发）
    int callCount = 0;
    mgr.subscribe(0, 0, [&](const MonitorSample&) { ++callCount; });

    // 写入 buffer 数据 + 验证订阅在 clearRuntime 前生效
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 100.0, 1.0);
    mgr.flushNow();
    EXPECT_EQ(callCount, 1);

    // 执行
    mgr.clearRuntime();

    // buffer 应清空
    QJsonArray result = mgr.flushSamples();
    EXPECT_TRUE(result.isEmpty());

    // 树和订阅应保留
    auto tree = mgr.monitorTree();
    EXPECT_EQ(tree.size(), 2);

    // 订阅仍有效（clearRuntime 后发同通道数据，回调应再触发）
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 200.0, 2.0);
    mgr.flushNow();
    EXPECT_EQ(callCount, 2);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 8: clearStructure 清树和订阅但保留 buffer
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, ClearStructure) {
    MonitorManager mgr;
    mgr.loadFromTopology(makeTestTopology());

    // 写入 buffer 数据
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 100.0, 1.0);

    // 建立一个订阅
    mgr.subscribe(0, 0, [&](const MonitorSample&) {});

    // 执行
    mgr.clearStructure();

    // 树应清空
    auto tree = mgr.monitorTree();
    EXPECT_TRUE(tree.isEmpty());

    // 订阅应清空——再发射信号不会崩溃
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 200.0, 2.0);
    // （无断言：只验证不崩溃）

    // buffer 应保留（flush 仍能拿到之前的数据，值不受 clearStructure 影响）
    QJsonArray result = mgr.flushSamples();
    ASSERT_EQ(result.size(), 1);
    QJsonObject monObj = result[0].toObject();
    QJsonArray channels = monObj[QStringLiteral("channels")].toArray();
    ASSERT_EQ(channels.size(), 1);
    QJsonArray samples = channels[0].toObject()[QStringLiteral("samples")].toArray();
    ASSERT_EQ(samples.size(), 1);
    EXPECT_DOUBLE_EQ(samples[0].toObject()[QStringLiteral("eng")].toDouble(), 1.0);
}
