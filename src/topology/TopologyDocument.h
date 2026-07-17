#pragma once

#include <QObject>
#include <QPair>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QUuid>
#include <QVector>

#include "TopologyPathRouter.h"

namespace etest::topology {

enum class FunctionType {
  A429,
  AD,
  DA,
  DISCRETE,
  SERIAL,
  MIL1553,
  POWER,
  CAMERA,
  OSCILLOSCOPE,
  CUSTOM
};

QString functionTypeToString(FunctionType t);
FunctionType stringToFunctionType(const QString& s);

struct TopologyPort {
  QString name;
  enum class Direction { Input, Output, Bidirectional } direction = Direction::Input;
  QStringList allowedDeviceTypes;
  FunctionType functionType = FunctionType::CUSTOM;
  int positionHint = -1;
  int portStyle = 0;  // PortStyle enum: 0=Circle, 1=Triangle
};

QString directionToString(TopologyPort::Direction d);
TopologyPort::Direction stringToDirection(const QString& s);

struct TopologyDevicePort {
  QString name;
  TopologyPort::Direction direction = TopologyPort::Direction::Output;
  FunctionType functionType = FunctionType::CUSTOM;
  int positionHint = -1;
  int portStyle = 0;  // PortStyle enum: 0=Circle, 1=Triangle

  // ── M2: 该端口绑定的 ICD 帧名 ──
  QStringList boundFrameNames;
};

struct TopologyProduct {
  QString name;
  QVector<TopologyPort> ports;
  QPointF position{0, 0};
  QSizeF size{0, 0};
};

struct TopologyDevice {
  QString id;             // ── M2: 设备实例持久 id（UUID v4），创建时生成，永不变
  QString name;
  QString deviceType;
  QString pluginId;       // 设备插件唯一标识，必填
  QPointF position{0, 0};
  QVector<QPair<QString, QString>> properties;
  QVector<TopologyDevicePort> ports;
  QSizeF size{0, 0};
};

struct TopologyConnection {
  QString productName;
  QString portName;
  QString deviceName;
  QString devicePort;
  PathStyle style = PathStyle::Bezier;
};

// Tap point identifying a connection by its endpoint names
struct TopologyMonitorTap {
  QString productName;
  QString portName;
  QString deviceName;
  QString devicePort;
  QString deviceId;                              // M4: 设备 UUID，挂载时由 deviceName 查 devices[].id 填入
  QString displayMode = QStringLiteral("auto");  // M4: auto/waveform/led/meter/frame
};

// Listener / monitor device — passively taps existing connections
struct TopologyMonitor {
  QString name;
  QString deviceType;
  int channelCount = 1;
  QPointF position{0, 0};
  QSizeF size{0, 0};
  QVector<TopologyMonitorTap> taps;
};

class TopologyDocument : public QObject {
  Q_OBJECT
 public:
  explicit TopologyDocument(QObject* parent = nullptr);

  int addProduct(const TopologyProduct& product);
  int insertProduct(int index, const TopologyProduct& product);
  void removeProduct(int index);
  TopologyProduct* product(int index);
  const TopologyProduct* product(int index) const;
  int productCount() const;
  int findProductIndex(const QString& name) const;
  bool renameProduct(int index, const QString& newName);

  int addDevice(const TopologyDevice& device);
  int insertDevice(int index, const TopologyDevice& device);
  void removeDevice(int index);
  TopologyDevice* device(int index);
  const TopologyDevice* device(int index) const;
  int deviceCount() const;
  int findDeviceIndex(const QString& name) const;
  int findDeviceIndexById(const QString& id) const;  // ── M2 新增

  // ── M2/M3: 端口绑帧访问器（供 DevicePortBindingDialog 使用） ──
  void setDevicePortFrames(int deviceIndex, int portIndex,
                           const QStringList& frames);
  QStringList devicePortFrames(int deviceIndex, int portIndex) const;

  bool renameDevice(int index, const QString& newName);

  // Product (UUT) port management
  void addProductPort(int productIndex, const TopologyPort& port);
  int insertProductPort(int productIndex, int portIndex,
                        const TopologyPort& port);
  void removeProductPort(int productIndex, int portIndex);
  int findProductPortIndex(int productIndex, const QString& name) const;
  bool renameProductPort(int productIndex, int portIndex,
                         const QString& newName);

  void addDevicePort(int deviceIndex, const TopologyDevicePort& port);
  int insertDevicePort(int deviceIndex, int portIndex,
                       const TopologyDevicePort& port);
  void removeDevicePort(int deviceIndex, int portIndex);
  int findDevicePortIndex(int deviceIndex, const QString& name) const;
  bool renameDevicePort(int deviceIndex, int portIndex,
                        const QString& newName);

  int addConnection(const TopologyConnection& conn);
  int insertConnection(int index, const TopologyConnection& conn);
  void removeConnection(int index);
  TopologyConnection* connection(int index);
  const TopologyConnection* connection(int index) const;
  int connectionCount() const;

  // Monitor management
  int addMonitor(const TopologyMonitor& monitor);
  int insertMonitor(int index, const TopologyMonitor& monitor);
  void removeMonitor(int index);
  TopologyMonitor* monitor(int index);
  const TopologyMonitor* monitor(int index) const;
  int monitorCount() const;
  int findMonitorIndex(const QString& name) const;

  // Tap management
  void addTap(int monitorIndex, const TopologyMonitorTap& tap);
  int insertTap(int monitorIndex, int tapIndex, const TopologyMonitorTap& tap);
  void removeTap(int monitorIndex, int tapIndex);

  bool canConnect(const QString& productName,
                  const QString& portName,
                  const QString& deviceName,
                  const QString& devicePortName) const;

  bool isModified() const;

  QUndoStack* undoStack() const { return undo_stack_; }

  void clear();

 signals:
  void productAdded(int index);
  void productRemoved(int index);
  void productChanged(int index);
  void productPortAdded(int productIndex, int portIndex);
  void productPortRemoved(int productIndex, int portIndex);
  void deviceAdded(int index);
  void deviceRemoved(int index);
  void deviceChanged(int index);
  // ── M2/M3 新增 ──
  void devicePortAdded(int deviceIndex, int portIndex);
  void devicePortRemoved(int deviceIndex, int portIndex);
  void devicePortFramesChanged(int deviceIndex, int portIndex);
  void connectionAdded(int index);
  void connectionRemoved(int index);
  void monitorAdded(int index);
  void monitorRemoved(int index);
  void monitorChanged(int index);
  void documentCleared();

 private:
  QUndoStack* undo_stack_;
  QVector<TopologyProduct> products_;
  QVector<TopologyDevice> devices_;
  QVector<TopologyConnection> connections_;
  QVector<TopologyMonitor> monitors_;
};

}  // namespace etest::topology
