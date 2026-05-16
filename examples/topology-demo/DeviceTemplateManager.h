#pragma once

#include <QString>
#include <QJsonObject>

namespace topology {

class TopologyDocument;

class DeviceTemplateManager {
public:
    static bool saveTemplate(const TopologyDocument* doc, int deviceIndex,
                             const QString& filePath);
    static bool loadTemplate(const QString& filePath,
                             QJsonObject& outDeviceType,
                             QJsonArray& outProperties);

    static QString lastError() { return last_error_; }

private:
    static QString last_error_;
};

}  // namespace topology
