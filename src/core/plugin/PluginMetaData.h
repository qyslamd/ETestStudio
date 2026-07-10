#ifndef ETEST_CORE_PLUGIN_PLUGIN_META_DATA_H_
#define ETEST_CORE_PLUGIN_PLUGIN_META_DATA_H_

#include <QString>
#include <QStringList>

namespace etest {
namespace core {
namespace plugin {

struct PluginMetaData {
  QString id;                // 唯一标识，如 "etest.plugin.device.serial"
  QString name;              // 显示名称，如 "串口设备插件"
  QString version;           // 版本号，如 "1.0.0"
  QString description;       // 描述
  QString author;            // 作者
  QString category;          // 分类标签，如 "device"、"icd"、"report"
  QStringList dependencies;  // 依赖的其他插件ID列表

  // 设备插件扩展字段
  QString device_type;       // 设备类型标识，如 "ad"、"da"、"serial"、"a429"、"can"
  int device_channels = 0;   // 设备通道数
  QString device_function;   // 设备功能类型，如 "A429"、"AD"、"DISCRETE"
  QString device_direction;  // 设备方向，默认 "Bidirectional"
  bool is_mock = false;      // 标记此插件是否为 Mock 实现

  bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_PLUGIN_PLUGIN_META_DATA_H_
