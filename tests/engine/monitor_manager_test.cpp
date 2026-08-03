#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "MonitorManager.h"

using namespace etest::engine;

// ══════════════════════════════════════════════════════════════════════════════
// 辅助：构建一个简单的拓扑 JSON，含 2 个 monitor
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

    // monitors（connectionId + displayMode）
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

// 便捷：把拓扑里的 monitors 数组 + 拓扑 doc 一起 loadMonitors
static void loadMonitorsFromTopo(MonitorManager& mgr,
                                 const QJsonObject& topo) {
    mgr.loadMonitors(topo.value(QStringLiteral("monitors")).toArray(), topo);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 1: loadMonitors 正确创建查表和树缓存（connectionId 作 key）
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, LoadMonitorsBuildsTreeAndLookup) {
    MonitorManager mgr;
    loadMonitorsFromTopo(mgr, makeTestTopology());

    auto tree = mgr.monitorTree();
    ASSERT_EQ(tree.size(), 2);
    EXPECT_EQ(tree[0].connectionId, QStringLiteral("conn-ad"));
    EXPECT_EQ(tree[0].name, QStringLiteral("监听器_AD"));
    EXPECT_EQ(tree[0].deviceType, QStringLiteral("ad"));  // 从 device 派生
    EXPECT_FALSE(tree[0].invalid);
    EXPECT_EQ(tree[1].connectionId, QStringLiteral("conn-serial"));
    EXPECT_EQ(tree[1].name, QStringLiteral("监听器_串口"));
    EXPECT_EQ(tree[1].displayMode, QStringLiteral("meter"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 2: onHardwareOpFinished 命中 tap 时写入 buffer
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, MatchTapAndBuffer) {
    MonitorManager mgr;
    loadMonitorsFromTopo(mgr, makeTestTopology());

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

    QJsonArray samples = monObj[QStringLiteral("samples")].toArray();
    ASSERT_EQ(samples.size(), 1);
    EXPECT_DOUBLE_EQ(samples[0].toObject()[QStringLiteral("eng")].toDouble(), 5.02);
    EXPECT_DOUBLE_EQ(samples[0].toObject()[QStringLiteral("raw")].toDouble(), 2699.0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 3: subscribe 回调在匹配 tap 时被调用（key = connectionId）
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, SubscribeCallback) {
    MonitorManager mgr;
    loadMonitorsFromTopo(mgr, makeTestTopology());

    bool called = false;
    MonitorSample captured;
    mgr.subscribe(QStringLiteral("conn-ad"), [&](const MonitorSample& sample) {
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
    EXPECT_EQ(captured.connectionId, QStringLiteral("conn-ad"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 4: unsubscribe 后不再收到回调
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, Unsubscribe) {
    MonitorManager mgr;
    loadMonitorsFromTopo(mgr, makeTestTopology());

    int callCount = 0;
    mgr.subscribe(QStringLiteral("conn-ad"), [&](const MonitorSample&) { ++callCount; });
    mgr.unsubscribe(QStringLiteral("conn-ad"));

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
    loadMonitorsFromTopo(mgr, makeTestTopology());

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
    loadMonitorsFromTopo(mgr, makeTestTopology());

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
    loadMonitorsFromTopo(mgr, makeTestTopology());

    // 建立一个订阅（用计数器验证 clearRuntime 后回调仍能触发）
    int callCount = 0;
    mgr.subscribe(QStringLiteral("conn-ad"), [&](const MonitorSample&) { ++callCount; });

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
    loadMonitorsFromTopo(mgr, makeTestTopology());

    // 写入 buffer 数据
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 100.0, 1.0);

    // 建立一个订阅
    mgr.subscribe(QStringLiteral("conn-ad"), [&](const MonitorSample&) {});

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
    QJsonArray samples = monObj[QStringLiteral("samples")].toArray();
    ASSERT_EQ(samples.size(), 1);
    EXPECT_DOUBLE_EQ(samples[0].toObject()[QStringLiteral("eng")].toDouble(), 1.0);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 9: loadMonitors 幂等 —— 重复加载不累积
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, IdempotentReload) {
    MonitorManager mgr;
    QJsonObject topo = makeTestTopology();
    auto monitors = topo.value(QStringLiteral("monitors")).toArray();
    mgr.loadMonitors(monitors, topo);
    mgr.loadMonitors(monitors, topo);  // 幂等：不累积

    EXPECT_EQ(mgr.monitorTree().size(), 2);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 10: loadMonitors 单次数组内重复 connectionId 去重（审查 🟡7）
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, DedupWithinLoadMonitors) {
    MonitorManager mgr;
    QJsonObject topo = makeTestTopology();
    QJsonArray monitors = topo.value(QStringLiteral("monitors")).toArray();
    // 追加与首项同 connectionId 的重复项
    QJsonObject dup;
    dup[QStringLiteral("name")] = QStringLiteral("重复项");
    dup[QStringLiteral("connectionId")] = QStringLiteral("conn-ad");
    dup[QStringLiteral("displayMode")] = QStringLiteral("led");
    monitors.append(dup);

    mgr.loadMonitors(monitors, topo);

    auto tree = mgr.monitorTree();
    ASSERT_EQ(tree.size(), 2);  // 重复项被跳过，保留首个
    EXPECT_EQ(tree[0].name, QStringLiteral("监听器_AD"));
    EXPECT_EQ(mgr.displayMode(QStringLiteral("conn-ad")),
              QStringLiteral("waveform"));  // 首条 displayMode 生效
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 11: connectionId 失效 → 标记 invalid、不进 lookup_table_（不订阅路由）
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, InvalidConnectionMarked) {
    MonitorManager mgr;
    QJsonObject topo = makeTestTopology();
    QJsonArray monitors = topo.value(QStringLiteral("monitors")).toArray();
    QJsonObject bad;
    bad[QStringLiteral("name")] = QStringLiteral("失效监听器");
    bad[QStringLiteral("connectionId")] = QStringLiteral("conn-gone");
    bad[QStringLiteral("displayMode")] = QStringLiteral("gauge");
    monitors.append(bad);

    mgr.loadMonitors(monitors, topo);

    auto tree = mgr.monitorTree();
    ASSERT_EQ(tree.size(), 3);
    EXPECT_FALSE(tree[0].invalid);
    EXPECT_TRUE(tree[2].invalid);
    EXPECT_EQ(tree[2].connectionId, QStringLiteral("conn-gone"));

    // 失效监听器的 displayMode 仍可查（供对话框展示类型）
    EXPECT_EQ(mgr.displayMode(QStringLiteral("conn-gone")),
              QStringLiteral("gauge"));

    // 失效监听器不参与数据路由
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-1"),
                              QStringLiteral("ch0"),
                              QByteArray(), 1.0, 1.0);
    QJsonArray result = mgr.flushSamples();
    ASSERT_EQ(result.size(), 1);  // 只有正常监听器有数据
    EXPECT_EQ(result[0].toObject()[QStringLiteral("name")].toString(),
              QStringLiteral("监听器_AD"));
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 11: addMonitor 单条增量添加 + removeMonitor 删除
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, AddRemoveMonitor) {
    MonitorManager mgr;
    loadMonitorsFromTopo(mgr, makeTestTopology());

    MonitorConfig cfg;
    cfg.connectionId = QStringLiteral("conn-new");
    cfg.name = QStringLiteral("新监听器");
    cfg.displayMode = QStringLiteral("led");
    ASSERT_TRUE(mgr.addMonitor(cfg, QStringLiteral("dev-uuid-2"),
                               QStringLiteral("tx"),
                               QStringLiteral("serial")));

    auto tree = mgr.monitorTree();
    ASSERT_EQ(tree.size(), 3);
    EXPECT_EQ(tree[2].name, QStringLiteral("新监听器"));
    EXPECT_EQ(mgr.displayMode(QStringLiteral("conn-new")), QStringLiteral("led"));

    // 数据路由到新监听器
    int called = 0;
    mgr.subscribe(QStringLiteral("conn-new"), [&](const MonitorSample&) { ++called; });
    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-2"), QStringLiteral("tx"),
                             QByteArray(), 1.0, 2.0);
    mgr.flushNow();
    EXPECT_EQ(called, 1);

    // 删除后：树减少、订阅撤销、不再路由
    ASSERT_TRUE(mgr.removeMonitor(QStringLiteral("conn-new")));
    EXPECT_EQ(mgr.monitorTree().size(), 2);
    EXPECT_TRUE(mgr.displayMode(QStringLiteral("conn-new")).isEmpty());

    mgr.onHardwareOpFinished(QStringLiteral("dev-uuid-2"), QStringLiteral("tx"),
                             QByteArray(), 1.0, 2.0);
    mgr.flushNow();
    EXPECT_EQ(called, 1);
}

// ══════════════════════════════════════════════════════════════════════════════
// Test 12: 一连接一监听器 —— 重复 addMonitor 被拒绝
// ══════════════════════════════════════════════════════════════════════════════
TEST(MonitorManagerTest, DuplicateAddRejected) {
    MonitorManager mgr;
    loadMonitorsFromTopo(mgr, makeTestTopology());

    MonitorConfig cfg;
    cfg.connectionId = QStringLiteral("conn-ad");  // 已存在
    cfg.name = QStringLiteral("重复");
    cfg.displayMode = QStringLiteral("gauge");
    EXPECT_FALSE(mgr.addMonitor(cfg, QStringLiteral("dev-uuid-1"),
                                QStringLiteral("ch0"), QStringLiteral("ad")));
    EXPECT_EQ(mgr.monitorTree().size(), 2);
}
