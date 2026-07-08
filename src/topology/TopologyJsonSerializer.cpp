#include "TopologyJsonSerializer.h"
#include "TopologyDocument.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUndoCommand>

namespace etest::topology {

static QString pathStyleToString(PathStyle s) {
  switch (s) {
    case PathStyle::Bezier:   return QStringLiteral("bezier");
    case PathStyle::Polyline: return QStringLiteral("polyline");
    case PathStyle::Straight: return QStringLiteral("straight");
  }
  return QStringLiteral("bezier");
}

static PathStyle stringToPathStyle(const QString& s) {
  if (s == QStringLiteral("polyline")) return PathStyle::Polyline;
  if (s == QStringLiteral("straight")) return PathStyle::Straight;
  return PathStyle::Bezier;
}

QString TopologyJsonSerializer::last_error_;

QJsonObject TopologyJsonSerializer::serialize(const TopologyDocument& doc) {
  QJsonObject root;

  root["version"] = 1;

  // Products
  QJsonArray productsArr;
  for (int i = 0; i < doc.productCount(); ++i) {
    const auto* p = doc.product(i);
    QJsonObject pObj;
    pObj["name"] = p->name;
    pObj["positionX"] = p->position.x();
    pObj["positionY"] = p->position.y();
    if (p->size.isValid() && p->size.width() > 0)
      pObj["size"] = QJsonArray{p->size.width(), p->size.height()};

    QJsonArray portsArr;
    for (const auto& port : p->ports) {
      QJsonObject portObj;
      portObj["name"] = port.name;
      portObj["direction"] = directionToString(port.direction);
      QJsonArray typesArr;
      for (const auto& t : port.allowedDeviceTypes)
        typesArr.append(t);
      portObj["allowedDeviceTypes"] = typesArr;
      portObj["positionHint"] = port.positionHint;
      portObj["portStyle"] = port.portStyle;
      portObj["functionType"] = functionTypeToString(port.functionType);
      portsArr.append(portObj);
    }
    pObj["ports"] = portsArr;
    productsArr.append(pObj);
  }
  root["products"] = productsArr;

  // Devices
  QJsonArray devicesArr;
  for (int i = 0; i < doc.deviceCount(); ++i) {
    const auto* d = doc.device(i);
    QJsonObject dObj;
    dObj["id"] = d->id;  // ── M2 新增
    dObj["name"] = d->name;
    dObj["deviceType"] = d->deviceType;
    dObj["pluginId"] = d->pluginId;
    dObj["positionX"] = d->position.x();
    dObj["positionY"] = d->position.y();
    if (d->size.isValid() && d->size.width() > 0)
      dObj["size"] = QJsonArray{d->size.width(), d->size.height()};

    QJsonArray propsArr;
    for (const auto& prop : d->properties) {
      QJsonObject propObj;
      propObj["key"] = prop.first;
      propObj["value"] = prop.second;
      propsArr.append(propObj);
    }
    dObj["properties"] = propsArr;

    QJsonArray portsArr;
    for (const auto& dp : d->ports) {
      QJsonObject dpObj;
      dpObj["name"] = dp.name;
      dpObj["direction"] = directionToString(dp.direction);
      dpObj["functionType"] = functionTypeToString(dp.functionType);
      dpObj["positionHint"] = dp.positionHint;
      dpObj["portStyle"] = dp.portStyle;
      // ── M2 新增 ──
      QJsonArray framesArr;
      for (const auto& f : dp.boundFrameNames)
        framesArr.append(f);
      dpObj["boundFrames"] = framesArr;
      portsArr.append(dpObj);
    }
    dObj["ports"] = portsArr;

    devicesArr.append(dObj);
  }
  root["devices"] = devicesArr;

  // Connections
  QJsonArray connsArr;
  for (int i = 0; i < doc.connectionCount(); ++i) {
    const auto* c = doc.connection(i);
    QJsonObject cObj;
    cObj["product"] = c->productName;
    cObj["port"] = c->portName;
    cObj["device"] = c->deviceName;
    cObj["devicePort"] = c->devicePort;
    cObj["style"] = pathStyleToString(c->style);
    connsArr.append(cObj);
  }
  root["connections"] = connsArr;

  // Monitors
  QJsonArray monitorsArr;
  for (int i = 0; i < doc.monitorCount(); ++i) {
    const auto* m = doc.monitor(i);
    QJsonObject mObj;
    mObj["name"] = m->name;
    mObj["deviceType"] = m->deviceType;
    mObj["channelCount"] = m->channelCount;
    mObj["positionX"] = m->position.x();
    mObj["positionY"] = m->position.y();
    if (m->size.isValid() && m->size.width() > 0)
      mObj["size"] = QJsonArray{m->size.width(), m->size.height()};

    QJsonArray tapsArr;
    for (const auto& tap : m->taps) {
      QJsonObject tapObj;
      tapObj["productName"] = tap.productName;
      tapObj["portName"] = tap.portName;
      tapObj["deviceName"] = tap.deviceName;
      tapObj["devicePort"] = tap.devicePort;
      tapsArr.append(tapObj);
    }
    mObj["taps"] = tapsArr;
    monitorsArr.append(mObj);
  }
  root["monitors"] = monitorsArr;

  return root;
}

bool TopologyJsonSerializer::deserialize(const QJsonObject& json,
                                         TopologyDocument* doc) {
  if (!doc) {
    last_error_ = "Null document pointer";
    return false;
  }

  if (!json.contains(QStringLiteral("version")) ||
      !json[QStringLiteral("version")].isDouble()) {
    last_error_ = QStringLiteral("缺少拓扑文件版本号");
    return false;
  }

  const QStringList arrayKeys = {QStringLiteral("products"),
                                 QStringLiteral("devices"),
                                 QStringLiteral("connections"),
                                 QStringLiteral("monitors")};
  for (const auto& key : arrayKeys) {
    if (!json.contains(key) || !json[key].isArray()) {
      last_error_ = QStringLiteral("字段 %1 不是数组").arg(key);
      return false;
    }
  }

  doc->clear();

  // Products
  QJsonArray productsArr = json["products"].toArray();
  for (const auto& pVal : productsArr) {
    if (!pVal.isObject()) {
      last_error_ = QStringLiteral("UUT 节点不是对象");
      return false;
    }
    QJsonObject pObj = pVal.toObject();
    if (!pObj.contains(QStringLiteral("name")) ||
        !pObj[QStringLiteral("name")].isString()) {
      last_error_ = QStringLiteral("UUT 缺少名称");
      return false;
    }
    if (!pObj.contains(QStringLiteral("ports")) ||
        !pObj[QStringLiteral("ports")].isArray()) {
      last_error_ = QStringLiteral("UUT 端口列表无效");
      return false;
    }
    TopologyProduct product;
    product.name = pObj["name"].toString();
    product.position =
        QPointF(pObj["positionX"].toDouble(), pObj["positionY"].toDouble());
    {
      QJsonArray a = pObj["size"].toArray();
      if (a.size() == 2)
        product.size = QSizeF(a[0].toDouble(), a[1].toDouble());
    }

    QJsonArray portsArr = pObj["ports"].toArray();
    for (const auto& portVal : portsArr) {
      QJsonObject portObj = portVal.toObject();
      TopologyPort port;
      port.name = portObj["name"].toString();
      port.direction = stringToDirection(
          portObj["direction"].toString(QStringLiteral("output")));
      for (const auto& t : portObj["allowedDeviceTypes"].toArray())
        port.allowedDeviceTypes.append(t.toString());
      port.positionHint = portObj["positionHint"].toInt(-1);
      port.portStyle = portObj["portStyle"].toInt(0);
      port.functionType =
          stringToFunctionType(portObj["functionType"].toString());
      product.ports.append(port);
    }
    doc->addProduct(product);
  }

  // Devices
  QJsonArray devicesArr = json["devices"].toArray();
  bool migratedId = false;
  for (const auto& dVal : devicesArr) {
    QJsonObject dObj = dVal.toObject();
    TopologyDevice device;
    // M2: id 读取，缺省时生成（老文件迁移）
    device.id = dObj["id"].toString();
    if (device.id.isEmpty()) {
      device.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      migratedId = true;
    }
    device.name = dObj["name"].toString();
    device.deviceType = dObj["deviceType"].toString();
    device.pluginId = dObj["pluginId"].toString();
    device.position =
        QPointF(dObj["positionX"].toDouble(), dObj["positionY"].toDouble());
    {
      QJsonArray a = dObj["size"].toArray();
      if (a.size() == 2)
        device.size = QSizeF(a[0].toDouble(), a[1].toDouble());
    }

    QJsonArray propsArr = dObj["properties"].toArray();
    for (const auto& propVal : propsArr) {
      QJsonObject propObj = propVal.toObject();
      device.properties.append(
          {propObj["key"].toString(), propObj["value"].toString()});
    }

    QJsonArray devPortsArr = dObj["ports"].toArray();
    for (const auto& portVal : devPortsArr) {
      QJsonObject portObj = portVal.toObject();
      TopologyDevicePort dp;
      dp.name = portObj["name"].toString();
      dp.direction = stringToDirection(
          portObj["direction"].toString(QStringLiteral("output")));
      dp.functionType =
          stringToFunctionType(portObj["functionType"].toString());
      dp.positionHint = portObj["positionHint"].toInt(-1);
      dp.portStyle = portObj["portStyle"].toInt(0);
      // ── M2 新增 ──
      QJsonArray framesArr = portObj["boundFrames"].toArray();
      for (const auto& f : framesArr)
        dp.boundFrameNames.append(f.toString());
      device.ports.append(dp);
    }

    doc->addDevice(device);
  }
  // M2: 若发生了 ID 迁移，标记文档已修改（确保用户保存）
  if (migratedId) {
    doc->undoStack()->push(new QUndoCommand(QStringLiteral("迁移设备 ID")));
  }

  // Connections
  QJsonArray connsArr = json["connections"].toArray();
  for (const auto& cVal : connsArr) {
    QJsonObject cObj = cVal.toObject();
    TopologyConnection conn;
    conn.productName = cObj["product"].toString();
    conn.portName = cObj["port"].toString();
    conn.deviceName = cObj["device"].toString();
    conn.devicePort = cObj["devicePort"].toString();
    conn.style = stringToPathStyle(cObj["style"].toString());
    doc->addConnection(conn);
  }

  // Monitors
  QJsonArray monitorsArr = json["monitors"].toArray();
  for (const auto& mVal : monitorsArr) {
    QJsonObject mObj = mVal.toObject();
    TopologyMonitor mon;
    mon.name = mObj["name"].toString();
    mon.deviceType = mObj["deviceType"].toString();
    mon.channelCount = mObj["channelCount"].toInt(1);
    mon.position =
        QPointF(mObj["positionX"].toDouble(), mObj["positionY"].toDouble());
    {
      QJsonArray a = mObj["size"].toArray();
      if (a.size() == 2)
        mon.size = QSizeF(a[0].toDouble(), a[1].toDouble());
    }

    QJsonArray tapsArr = mObj["taps"].toArray();
    for (const auto& tapVal : tapsArr) {
      QJsonObject tapObj = tapVal.toObject();
      TopologyMonitorTap tap;
      tap.productName = tapObj["productName"].toString();
      tap.portName = tapObj["portName"].toString();
      tap.deviceName = tapObj["deviceName"].toString();
      tap.devicePort = tapObj["devicePort"].toString();
      mon.taps.append(tap);
    }
    doc->addMonitor(mon);
  }

  return true;
}

QString TopologyJsonSerializer::lastError() {
  return last_error_;
}

}  // namespace etest::topology
