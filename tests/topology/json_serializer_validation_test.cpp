#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonObject>

#include "topology/TopologyDocument.h"
#include "topology/TopologyJsonSerializer.h"

using namespace etest::topology;

TEST(JsonSerializerValidationTest, RejectsMissingVersion) {
  TopologyDocument doc;
  QJsonObject json;
  json[QStringLiteral("products")] = QJsonArray();
  json[QStringLiteral("devices")] = QJsonArray();
  json[QStringLiteral("connections")] = QJsonArray();
  json[QStringLiteral("monitors")] = QJsonArray();

  EXPECT_FALSE(TopologyJsonSerializer::deserialize(json, &doc));
  EXPECT_FALSE(TopologyJsonSerializer::lastError().isEmpty());
}

TEST(JsonSerializerValidationTest, RejectsNonArrayProducts) {
  TopologyDocument doc;
  QJsonObject json;
  json[QStringLiteral("version")] = 1;
  json[QStringLiteral("products")] = QJsonObject();
  json[QStringLiteral("devices")] = QJsonArray();
  json[QStringLiteral("connections")] = QJsonArray();
  json[QStringLiteral("monitors")] = QJsonArray();

  EXPECT_FALSE(TopologyJsonSerializer::deserialize(json, &doc));
}

TEST(JsonSerializerValidationTest, RejectsProductWithoutName) {
  TopologyDocument doc;
  QJsonObject json;
  json[QStringLiteral("version")] = 1;
  QJsonObject product;
  product[QStringLiteral("ports")] = QJsonArray();
  json[QStringLiteral("products")] = QJsonArray{product};
  json[QStringLiteral("devices")] = QJsonArray();
  json[QStringLiteral("connections")] = QJsonArray();
  json[QStringLiteral("monitors")] = QJsonArray();

  EXPECT_FALSE(TopologyJsonSerializer::deserialize(json, &doc));
}
