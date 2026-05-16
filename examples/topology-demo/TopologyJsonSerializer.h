#pragma once

#include <QJsonObject>
#include <QString>

namespace etest::topology {

class TopologyDocument;

class TopologyJsonSerializer {
 public:
  static QJsonObject serialize(const TopologyDocument& doc);
  static bool deserialize(const QJsonObject& json, TopologyDocument* doc);
  static QString lastError();

 private:
  static QString last_error_;
};

}  // namespace etest::topology
