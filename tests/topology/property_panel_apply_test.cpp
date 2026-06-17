#include <gtest/gtest.h>
#include <QApplication>
#include <QStandardItemModel>
#include <QTableWidgetItem>
#include <memory>

#define private public
#include "topology/PropertyPanelWidget.h"
#undef private
#include "topology/topology_items.h"

using namespace etest::topology;

namespace {

class TestEnv : public ::testing::Environment {
 public:
  void SetUp() override {
    int argc = 1;
    char name[] = "test";
    char* argv = name;
    app_ = std::make_unique<QApplication>(argc, &argv);
  }

  void TearDown() override { app_.reset(); }

 private:
  std::unique_ptr<QApplication> app_;
};

testing::Environment* const env =
    testing::AddGlobalTestEnvironment(new TestEnv);

}  // namespace

TEST(PropertyPanelApplyTest, ApplyUutPortsPreservesHiddenFields) {
  TopologyDocument doc;
  TopologyProduct product;
  product.name = QStringLiteral("UUT");
  TopologyPort port;
  port.name = QStringLiteral("OldPort");
  port.direction = TopologyPort::Input;
  port.functionType = FunctionType::A429;
  port.allowedDeviceTypes << QStringLiteral("EPH6272T")
                          << QStringLiteral("A429");
  port.positionHint = 7;
  port.portStyle = 1;
  product.ports.append(port);
  doc.addProduct(product);

  TopologyDevice device;
  device.name = QStringLiteral("Device");
  doc.addDevice(device);

  PropertyPanelWidget panel(&doc);
  UutItem uutItem(0, &doc);
  DeviceItem deviceItem(0, &doc);
  panel.showPropertiesFor(&uutItem);
  panel.uut_port_table_->setItem(0, 0,
                                 new QTableWidgetItem(QStringLiteral("NewPort")));
  panel.uut_port_table_->setItem(0, 1,
                                 new QTableWidgetItem(QStringLiteral("Output")));
  panel.uut_port_table_->setItem(0, 2,
                                 new QTableWidgetItem(QStringLiteral("POWER")));

  panel.showPropertiesFor(&deviceItem);

  ASSERT_EQ(doc.product(0)->ports.size(), 1);
  const auto& updated = doc.product(0)->ports[0];
  EXPECT_EQ(updated.name, QStringLiteral("NewPort"));
  EXPECT_EQ(updated.allowedDeviceTypes, port.allowedDeviceTypes);
  EXPECT_EQ(updated.positionHint, 7);
  EXPECT_EQ(updated.portStyle, 1);
}

TEST(PropertyPanelApplyTest, ApplyDevicePortsPreservesHiddenFields) {
  TopologyDocument doc;
  TopologyDevice device;
  device.name = QStringLiteral("Device");
  TopologyDevicePort port;
  port.name = QStringLiteral("OldPort");
  port.direction = TopologyPort::Output;
  port.functionType = FunctionType::A429;
  port.positionHint = 5;
  port.portStyle = 1;
  device.ports.append(port);
  doc.addDevice(device);

  TopologyProduct product;
  product.name = QStringLiteral("UUT");
  doc.addProduct(product);

  PropertyPanelWidget panel(&doc);
  DeviceItem deviceItem(0, &doc);
  UutItem uutItem(0, &doc);
  panel.showPropertiesFor(&deviceItem);
  panel.device_port_model_->setData(panel.device_port_model_->index(0, 0),
                                    QStringLiteral("NewPort"));
  panel.device_port_model_->setData(panel.device_port_model_->index(0, 1),
                                    QStringLiteral("Input"));
  panel.device_port_model_->setData(panel.device_port_model_->index(0, 2),
                                    QStringLiteral("POWER"));

  panel.showPropertiesFor(&uutItem);

  ASSERT_EQ(doc.device(0)->ports.size(), 1);
  const auto& updated = doc.device(0)->ports[0];
  EXPECT_EQ(updated.name, QStringLiteral("NewPort"));
  EXPECT_EQ(updated.positionHint, 5);
  EXPECT_EQ(updated.portStyle, 1);
}
