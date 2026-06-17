#include "DeviceTemplateManager.h"
#include "TopologyDocument.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace etest::topology {

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

  QJsonArray portsArr;
  for (const auto& dp : dev->ports) {
    QJsonObject pObj;
    pObj["name"] = dp.name;
    pObj["direction"] = directionToString(dp.direction);
    pObj["functionType"] = functionTypeToString(dp.functionType);
    pObj["positionHint"] = dp.positionHint;
    pObj["portStyle"] = dp.portStyle;
    portsArr.append(pObj);
  }
  root["ports"] = portsArr;

  QJsonDocument jdoc(root);
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    last_error_ = QStringLiteral("无法写入文件");
    return false;
  }
  QByteArray data = jdoc.toJson(QJsonDocument::Indented);
  if (file.write(data) != data.size()) {
    last_error_ = QStringLiteral("写入文件失败");
    return false;
  }
  file.close();
  return true;
}

bool DeviceTemplateManager::loadTemplate(const QString& filePath,
                                         QJsonObject& outDeviceType,
                                         QJsonArray& outProperties,
                                         QJsonArray& outPorts) {
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

  if (!jdoc.isObject()) {
    last_error_ = QStringLiteral("模板根节点不是对象");
    return false;
  }

  QJsonObject root = jdoc.object();
  if (!root.contains(QStringLiteral("deviceType")) ||
      !root[QStringLiteral("deviceType")].isString()) {
    last_error_ = QStringLiteral("模板缺少设备类型");
    return false;
  }
  if (!root.contains(QStringLiteral("properties")) ||
      !root[QStringLiteral("properties")].isArray()) {
    last_error_ = QStringLiteral("模板属性列表无效");
    return false;
  }
  if (!root.contains(QStringLiteral("ports")) ||
      !root[QStringLiteral("ports")].isArray()) {
    last_error_ = QStringLiteral("模板端口列表无效");
    return false;
  }

  outDeviceType = root;
  outProperties = root["properties"].toArray();
  outPorts = root["ports"].toArray();
  return true;
}

}  // namespace etest::topology
