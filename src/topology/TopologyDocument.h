#pragma once

#include <QObject>
#include <QPair>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QVector>

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
  enum Direction { Input, Output, Bidirectional } direction = Input;
  QStringList allowedDeviceTypes;
  FunctionType functionType = FunctionType::CUSTOM;
  int positionHint = -1;
};

QString directionToString(TopologyPort::Direction d);
TopologyPort::Direction stringToDirection(const QString& s);

struct TopologyDevicePort {
  QString name;
  TopologyPort::Direction direction = TopologyPort::Output;
  FunctionType functionType = FunctionType::CUSTOM;
  int positionHint = -1;
};

struct TopologyProduct {
  QString name;
  QVector<TopologyPort> ports;
  QPointF position{0, 0};
};

struct TopologyDevice {
  QString name;
  QString deviceType;
  QPointF position{0, 0};
  QVector<QPair<QString, QString>> properties;
  QVector<TopologyDevicePort> ports;
};

struct TopologyConnection {
  QString productName;
  QString portName;
  QString deviceName;
  QString devicePort;
};

class TopologyDocument : public QObject {
  Q_OBJECT
 public:
  explicit TopologyDocument(QObject* parent = nullptr);

  int addProduct(const TopologyProduct& product);
  void removeProduct(int index);
  TopologyProduct* product(int index);
  const TopologyProduct* product(int index) const;
  int productCount() const;
  int findProductIndex(const QString& name) const;

  int addDevice(const TopologyDevice& device);
  void removeDevice(int index);
  TopologyDevice* device(int index);
  const TopologyDevice* device(int index) const;
  int deviceCount() const;
  int findDeviceIndex(const QString& name) const;

  void addDevicePort(int deviceIndex, const TopologyDevicePort& port);
  void removeDevicePort(int deviceIndex, int portIndex);
  int findDevicePortIndex(int deviceIndex, const QString& name) const;

  int addConnection(const TopologyConnection& conn);
  void removeConnection(int index);
  TopologyConnection* connection(int index);
  const TopologyConnection* connection(int index) const;
  int connectionCount() const;

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
  void deviceAdded(int index);
  void deviceRemoved(int index);
  void deviceChanged(int index);
  void connectionAdded(int index);
  void connectionRemoved(int index);
  void documentCleared();

 private:
  QUndoStack* undo_stack_;
  QVector<TopologyProduct> products_;
  QVector<TopologyDevice> devices_;
  QVector<TopologyConnection> connections_;
};

}  // namespace etest::topology
