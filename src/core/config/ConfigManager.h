#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QVariant>
#include <memory>
#include "ConfigDefs.h"

namespace etest::core::config {

class ConfigManager : public QObject {
  Q_OBJECT
 public:
  static ConfigManager& instance();
  ~ConfigManager() override;

  // 禁止拷贝
  ConfigManager(const ConfigManager&) = delete;
  ConfigManager& operator=(const ConfigManager&) = delete;

  // 通用配置读写接口
  template <typename T>
  T get(const QString& key, const T& defaultValue = T()) const;

  template <typename T>
  void set(const QString& key, const T& value);

  // 导入导出功能
  bool exportToJson(const QString& filePath) const;
  bool importFromJson(const QString& filePath, bool overrideExisting = true);

  // 立即同步到磁盘
  void sync();

  // 重置功能
  void resetAllToDefault();
  void resetKeyToDefault(const QString& key);

 signals:
  void configChanged(const QString& key);  // 配置变更通知信号

 private:
  ConfigManager();
  class Impl;
  std::unique_ptr<Impl> m_impl;

  // 获取默认值的内部接口
  QVariant getDefaultValue(const QString& key) const;
};

// 模板实现
template <typename T>
T ConfigManager::get(const QString& key, const T& defaultValue) const {
  return get(key, QVariant::fromValue(defaultValue)).template value<T>();
}

template <typename T>
void ConfigManager::set(const QString& key, const T& value) {
  set(key, QVariant::fromValue(value));
}

// 模板特化
template <>
QVariant ConfigManager::get<QVariant>(const QString& key,
                                      const QVariant& defaultValue) const;
template <>
void ConfigManager::set<QVariant>(const QString& key, const QVariant& value);
}  // namespace etest::core::config

#endif  // CONFIGMANAGER_H
