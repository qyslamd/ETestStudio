#include "TopologyJsonSerializer.h"
#include "TopologyDocument.h"

#include <QJsonArray>
#include <QJsonObject>

namespace topology {

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

        QJsonArray portsArr;
        for (const auto& port : p->ports) {
            QJsonObject portObj;
            portObj["name"] = port.name;
            portObj["direction"] = (port.direction == TopologyPort::Input)
                                      ? QStringLiteral("input")
                                      : QStringLiteral("output");
            QJsonArray typesArr;
            for (const auto& t : port.allowedDeviceTypes)
                typesArr.append(t);
            portObj["allowedDeviceTypes"] = typesArr;
            portObj["positionHint"] = port.positionHint;
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
        dObj["name"] = d->name;
        dObj["deviceType"] = d->deviceType;
        dObj["positionX"] = d->position.x();
        dObj["positionY"] = d->position.y();

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
        connsArr.append(cObj);
    }
    root["connections"] = connsArr;

    return root;
}

bool TopologyJsonSerializer::deserialize(const QJsonObject& json,
                                         TopologyDocument* doc) {
    if (!doc) {
        last_error_ = "Null document pointer";
        return false;
    }

    doc->clear();

    // Products
    QJsonArray productsArr = json["products"].toArray();
    for (const auto& pVal : productsArr) {
        QJsonObject pObj = pVal.toObject();
        TopologyProduct product;
        product.name = pObj["name"].toString();
        product.position =
            QPointF(pObj["positionX"].toDouble(), pObj["positionY"].toDouble());

        QJsonArray portsArr = pObj["ports"].toArray();
        for (const auto& portVal : portsArr) {
            QJsonObject portObj = portVal.toObject();
            TopologyPort port;
            port.name = portObj["name"].toString();
            port.direction =
                (portObj["direction"].toString() == QStringLiteral("input"))
                    ? TopologyPort::Input
                    : TopologyPort::Output;
            for (const auto& t : portObj["allowedDeviceTypes"].toArray())
                port.allowedDeviceTypes.append(t.toString());
            port.positionHint = portObj["positionHint"].toInt(-1);
            product.ports.append(port);
        }
        doc->addProduct(product);
    }

    // Devices
    QJsonArray devicesArr = json["devices"].toArray();
    for (const auto& dVal : devicesArr) {
        QJsonObject dObj = dVal.toObject();
        TopologyDevice device;
        device.name = dObj["name"].toString();
        device.deviceType = dObj["deviceType"].toString();
        device.position =
            QPointF(dObj["positionX"].toDouble(), dObj["positionY"].toDouble());

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
            dp.functionType = stringToFunctionType(
                portObj["functionType"].toString());
            dp.positionHint = portObj["positionHint"].toInt(-1);
            device.ports.append(dp);
        }

        doc->addDevice(device);
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
        doc->addConnection(conn);
    }

    return true;
}

QString TopologyJsonSerializer::lastError() {
    return last_error_;
}

}  // namespace topology
