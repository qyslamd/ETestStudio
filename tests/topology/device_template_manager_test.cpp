#include <gtest/gtest.h>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "topology/DeviceTemplateManager.h"
#include "topology/TopologyDocument.h"

using namespace etest::topology;

TEST(DeviceTemplateManagerTest, SaveTemplatePreservesPortLayoutAndStyle) {
  TopologyDocument doc;
  TopologyDevice device;
  device.name = QStringLiteral("Device");
  device.deviceType = QStringLiteral("EPH6272T");
  TopologyDevicePort port;
  port.name = QStringLiteral("CH01");
  port.direction = TopologyPort::Output;
  port.functionType = FunctionType::A429;
  port.positionHint = 8;
  port.portStyle = 1;
  device.ports.append(port);
  doc.addDevice(device);

  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath(QStringLiteral("device.dvt"));

  ASSERT_TRUE(DeviceTemplateManager::saveTemplate(&doc, 0, path));

  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  QJsonDocument json = QJsonDocument::fromJson(file.readAll());
  ASSERT_TRUE(json.isObject());
  QJsonArray ports = json.object()[QStringLiteral("ports")].toArray();
  ASSERT_EQ(ports.size(), 1);
  QJsonObject savedPort = ports[0].toObject();
  EXPECT_EQ(savedPort[QStringLiteral("positionHint")].toInt(), 8);
  EXPECT_EQ(savedPort[QStringLiteral("portStyle")].toInt(), 1);
}

TEST(DeviceTemplateManagerTest, LoadTemplateRejectsInvalidRoot) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath(QStringLiteral("bad.dvt"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(QJsonDocument(QJsonArray()).toJson());
  file.close();

  QJsonObject root;
  QJsonArray properties;
  QJsonArray ports;
  EXPECT_FALSE(DeviceTemplateManager::loadTemplate(path, root, properties, ports));
}

TEST(DeviceTemplateManagerTest, LoadTemplateRejectsMissingPortsArray) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString path = dir.filePath(QStringLiteral("bad.dvt"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  QJsonObject root;
  root[QStringLiteral("deviceType")] = QStringLiteral("EPH6272T");
  root[QStringLiteral("properties")] = QJsonArray();
  file.write(QJsonDocument(root).toJson());
  file.close();

  QJsonObject outRoot;
  QJsonArray properties;
  QJsonArray ports;
  EXPECT_FALSE(DeviceTemplateManager::loadTemplate(path, outRoot, properties,
                                                   ports));
}
