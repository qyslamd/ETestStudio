#include <gtest/gtest.h>

#include "topology/TopologyDocument.h"
#include "topology/UndoCommands.h"

using namespace etest::topology;

namespace {

TopologyDevice makeDevice() {
  TopologyDevice device;
  device.name = QStringLiteral("EPH5272");
  device.deviceType = QStringLiteral("A429");
  device.pluginId = QStringLiteral("plugin_eph5272");
  TopologyDevicePort port;
  port.name = QStringLiteral("ch0");
  port.direction = TopologyPort::Direction::Output;
  device.ports.append(port);
  return device;
}

}  // namespace

TEST(TopologyM3Test, DevicePortFramesAccessor) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice();
  int devIdx = doc.addDevice(device);

  // 初始为空
  QStringList frames = doc.devicePortFrames(devIdx, 0);
  EXPECT_TRUE(frames.isEmpty());

  // 设置
  QStringList expected;
  expected << QStringLiteral("A429_发送") << QStringLiteral("A429_接收");
  doc.setDevicePortFrames(devIdx, 0, expected);

  frames = doc.devicePortFrames(devIdx, 0);
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[0], "A429_发送");
  EXPECT_EQ(frames[1], "A429_接收");
}

TEST(TopologyM3Test, SetDevicePortFramesUndoRedo) {
  TopologyDocument doc;
  TopologyDevice device = makeDevice();
  int devIdx = doc.addDevice(device);

  auto* stack = doc.undoStack();

  // 设置第一组
  QStringList frames1;
  frames1 << QStringLiteral("帧A");
  stack->push(
      new SetDevicePortFramesCommand(&doc, devIdx, 0, frames1));
  EXPECT_EQ(doc.devicePortFrames(devIdx, 0).size(), 1);

  // 设置第二组
  QStringList frames2;
  frames2 << QStringLiteral("帧B") << QStringLiteral("帧C");
  stack->push(
      new SetDevicePortFramesCommand(&doc, devIdx, 0, frames2));
  EXPECT_EQ(doc.devicePortFrames(devIdx, 0).size(), 2);

  // Undo → 回到帧A
  stack->undo();
  EXPECT_EQ(doc.devicePortFrames(devIdx, 0).size(), 1);
  EXPECT_EQ(doc.devicePortFrames(devIdx, 0)[0], "帧A");

  // Redo → 回到帧B+帧C
  stack->redo();
  EXPECT_EQ(doc.devicePortFrames(devIdx, 0).size(), 2);
}
