#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonObject>

#include "RunConfig.h"

using namespace etest::runconfig;

TEST(RunConfigTest, RoundTripBoundAndUnbound) {
  RunConfig cfg;
  RunConfig::Monitor bound;
  bound.id = QStringLiteral("c1");
  bound.connectionId = QStringLiteral("conn-1");
  bound.displayMode = QStringLiteral("waveform");
  bound.name = QStringLiteral("油压");
  bound.x = 20;
  bound.y = 30;
  bound.w = 320;
  bound.h = 240;
  RunConfig::Monitor unbound;
  unbound.id = QStringLiteral("c2");
  unbound.displayMode = QStringLiteral("gauge");
  unbound.x = 380;
  unbound.y = 30;
  unbound.w = 120;
  unbound.h = 120;
  cfg.monitors.append(bound);
  cfg.monitors.append(unbound);

  RunConfig restored;
  ASSERT_TRUE(restored.fromJson(cfg.toJson()));
  ASSERT_EQ(restored.monitors.size(), 2);
  EXPECT_EQ(restored.monitors[0].id, QStringLiteral("c1"));
  EXPECT_EQ(restored.monitors[0].connectionId, QStringLiteral("conn-1"));
  EXPECT_EQ(restored.monitors[0].displayMode, QStringLiteral("waveform"));
  EXPECT_EQ(restored.monitors[0].x, 20);
  EXPECT_EQ(restored.monitors[0].h, 240);
  // 未绑定（connectionId 空）合法保留
  EXPECT_TRUE(restored.monitors[1].connectionId.isEmpty());
  EXPECT_EQ(restored.monitors[1].displayMode, QStringLiteral("gauge"));
  EXPECT_EQ(restored.monitors[1].x, 380);
  EXPECT_EQ(restored.monitors[1].h, 120);
}

TEST(RunConfigTest, DedupByIdAndConnectionId) {
  QJsonArray arr;
  QJsonObject m1;
  m1[QStringLiteral("id")] = QStringLiteral("c1");
  m1[QStringLiteral("connectionId")] = QStringLiteral("conn-1");
  m1[QStringLiteral("displayMode")] = QStringLiteral("waveform");
  m1[QStringLiteral("x")] = 0;
  QJsonObject m2;  // 同 id → 丢弃
  m2[QStringLiteral("id")] = QStringLiteral("c1");
  m2[QStringLiteral("displayMode")] = QStringLiteral("gauge");
  QJsonObject m3;  // 不同 id 但同非空 connectionId → 丢弃
  m3[QStringLiteral("id")] = QStringLiteral("c3");
  m3[QStringLiteral("connectionId")] = QStringLiteral("conn-1");
  m3[QStringLiteral("displayMode")] = QStringLiteral("led");
  arr.append(m1);
  arr.append(m2);
  arr.append(m3);
  QJsonObject root;
  root[QStringLiteral("monitors")] = arr;

  RunConfig cfg;
  ASSERT_TRUE(cfg.fromJson(root));
  ASSERT_EQ(cfg.monitors.size(), 1);
  EXPECT_EQ(cfg.monitors[0].displayMode, QStringLiteral("waveform"));
}

TEST(RunConfigTest, MissingIdGetsUuid) {
  QJsonArray arr;
  QJsonObject m;
  m[QStringLiteral("displayMode")] = QStringLiteral("waveform");  // 无 id
  arr.append(m);
  QJsonObject root;
  root[QStringLiteral("monitors")] = arr;

  RunConfig cfg;
  ASSERT_TRUE(cfg.fromJson(root));
  ASSERT_EQ(cfg.monitors.size(), 1);
  EXPECT_FALSE(cfg.monitors[0].id.isEmpty());
}
