#include "DeviceTemplateManager.h"
#include "TopologyDocument.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace topology {

QString DeviceTemplateManager::last_error_;

bool DeviceTemplateManager::saveTemplate(const TopologyDocument* doc,
                                          int deviceIndex,
                                          const QString& filePath) {
    const auto* dev = doc->device(deviceIndex);
    if (!dev) {
        last_error_ = QStringLiteral("设备索引无效");
        return false;
    }

    QJsonObject root;
    root["deviceType"] = dev->deviceType;

    QJsonArray propsArr;
    for (const auto& prop : dev->properties) {
        QJsonObject pObj;
        pObj["key"] = prop.first;
        pObj["value"] = prop.second;
        propsArr.append(pObj);
    }
    root["properties"] = propsArr;

    QJsonDocument jdoc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        last_error_ = QStringLiteral("无法写入文件");
        return false;
    }
    file.write(jdoc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool DeviceTemplateManager::loadTemplate(const QString& filePath,
                                          QJsonObject& outDeviceType,
                                          QJsonArray& outProperties) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        last_error_ = QStringLiteral("无法打开文件");
        return false;
    }

    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(file.readAll(), &err);
    file.close();

    if (err.error != QJsonParseError::NoError) {
        last_error_ = err.errorString();
        return false;
    }

    QJsonObject root = jdoc.object();
    outDeviceType = root;
    outProperties = root["properties"].toArray();
    return true;
}

}  // namespace topology
