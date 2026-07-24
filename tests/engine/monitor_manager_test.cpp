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
    dev1[QStringLiteral("deviceType")] = QStringLiteral("ad");
    QJsonArray ports1;
    QJsonObject port1;
    port1["name"] = QStringLiteral("ch0");
    ports1.append(port1);
    dev1["ports"] = ports1;
    devicesArr.append(dev1);
    QJsonObject dev2;
    dev2[QStringLiteral("id")] = QStringLiteral("dev-uuid-2");
    dev2[QStringLiteral("name")] = QStringLiteral("串口1");
    dev2[QStringLiteral("deviceType")] = QStringLiteral("serial");
    QJsonArray ports2;
    QJsonObject port2;
    port2["name"] = QStringLiteral("tx");
    ports2.append(port2);
    dev2["ports"] = ports2;
    devicesArr.append(dev2);
    root[QStringLiteral("devices")] = devicesArr;

    // connections
    QJsonArray connsArr;
    QJsonObject conn0;
    conn0["id"] = QStringLiteral("conn-ad");
    conn0["device"] = QStringLiteral("AD卡1");
    conn0["devicePort"] = QStringLiteral("ch0");
    conn0["product"] = QStringLiteral("UUT1");
    conn0["port"] = QStringLiteral("out1");
    connsArr.append(conn0);
    QJsonObject conn1;
    conn1["id"] = QStringLiteral("conn-serial");
    conn1["device"] = QStringLiteral("串口1");
    conn1["devicePort"] = QStringLiteral("tx");
    conn1["product"] = QStringLiteral("UUT1");
    conn1["port"] = QStringLiteral("rx");
    connsArr.append(conn1);
    root["connections"] = connsArr;

    // monitors（新格式：connectionId + displayMode）
    QJsonArray monitorsArr;
    QJsonObject mon0;
    mon0[QStringLiteral("name")] = QStringLiteral("监听器_AD");
    mon0[QStringLiteral("connectionId")] = QStringLiteral("conn-ad");
    mon0[QStringLiteral("displayMode")] = QStringLiteral("waveform");
    monitorsArr.append(mon0);
    QJsonObject mon1;
    mon1[QStringLiteral("name")] = QStringLiteral("监听器_串口");
    mon1[QStringLiteral("connectionId")] = QStringLiteral("conn-serial");
    mon1[QStringLiteral("displayMode")] = QStringLiteral("meter");
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
    EXPECT_EQ(tree[0].deviceType, QStringLiteral("ad"));  // 从 device 派生
    EXPECT_EQ(tree[0].channelCount, 1);  // 一个 connection = 一个通道
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

// ══════════════════════════════════════════════════════════════════════════════
// Test 9: appendFromTopology 新格式 connectionId + displayMode 路径
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, NewFormatConnectionId) {
    // 构建新格式拓扑 JSON：connections 带 id，monitors 用 connectionId 引用
    QJsonObject root;

    // devices（需要一个 AD 设备）
    QJsonArray devicesArr;
    QJsonObject dev;
    dev["id"] = QStringLiteral("ad-device-uuid");
    dev["name"] = QStringLiteral("AD卡");
    dev["deviceType"] = QStringLiteral("ad");
    dev["mock"] = true;
    QJsonArray portsArr;
    QJsonObject port;
    port["name"] = QStringLiteral("ch0");
    port["boundFrames"] = QJsonArray{QStringLiteral("电压采集")};
    portsArr.append(port);
    dev["ports"] = portsArr;
    devicesArr.append(dev);
    root["devices"] = devicesArr;

    // connections（带 id）
    QJsonArray connsArr;
    QJsonObject conn;
    conn["id"] = QStringLiteral("conn-ad-001");
    conn["device"] = QStringLiteral("AD卡");
    conn["devicePort"] = QStringLiteral("ch0");
    conn["product"] = QStringLiteral("测试UUT");
    conn["port"] = QStringLiteral("AD口");
    connsArr.append(conn);
    root["connections"] = connsArr;

    // monitors（新格式：connectionId + displayMode）
    QJsonArray monitorsArr;
    QJsonObject mon;
    mon["name"] = QStringLiteral("Monitor-AD");
    mon["connectionId"] = QStringLiteral("conn-ad-001");
    mon["displayMode"] = QStringLiteral("waveform");
    monitorsArr.append(mon);
    root["monitors"] = monitorsArr;

    MonitorManager mgr;
    mgr.appendFromTopology(root);

    // 验证 tree_cache_
    auto tree = mgr.monitorTree();
    ASSERT_EQ(tree.size(), 1);
    EXPECT_EQ(tree[0].name, QStringLiteral("Monitor-AD"));
    EXPECT_EQ(tree[0].deviceType, QStringLiteral("ad"));

    // 验证 displayMode 查找
    QString mode = mgr.displayMode(0, 0);
    EXPECT_EQ(mode, QStringLiteral("waveform"));

    // 验证数据路由：发射匹配的设备/端口信号应能写入 CVT buffer_
    mgr.onHardwareOpFinished(QStringLiteral("ad-device-uuid"),
                              QStringLiteral("ch0"),
                              QByteArray(), 100.0, 5.0);

    // flush 确认收到数据
    QJsonArray result = mgr.flushSamples();
    ASSERT_EQ(result.size(), 1);
    QJsonObject monObj = result[0].toObject();
    QJsonArray channels = monObj[QStringLiteral("channels")].toArray();
    ASSERT_EQ(channels.size(), 1);
    QJsonArray samples = channels[0].toObject()[QStringLiteral("samples")].toArray();
    ASSERT_EQ(samples.size(), 1);
    EXPECT_DOUBLE_EQ(samples[0].toObject()[QStringLiteral("eng")].toDouble(), 5.0);
}
