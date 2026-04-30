#ifndef ETEST_CORE_PLUGIN_PLUGIN_MANAGER_H_
#define ETEST_CORE_PLUGIN_PLUGIN_MANAGER_H_

#include <QList>
#include <QObject>
#include <QStringList>
#include <memory>
#include "IPlugin.h"

namespace etest {
namespace core {
namespace plugin {

class PluginManager : public QObject {
  Q_OBJECT

 public:
  static PluginManager& instance();

  // 扫描并加载所有搜索路径下的插件
  void loadAll();

  // 卸载所有已加载插件
  void unloadAll();

  // 按ID加载/卸载单个插件（从已扫描的库中查找）
  bool loadPlugin(const QString& pluginId);
  bool unloadPlugin(const QString& pluginId);

  // 查询
  QList<PluginMetaData> loadedPlugins() const;
  IPlugin* plugin(const QString& pluginId) const;

  template <typename T>
  T* pluginAs(const QString& pluginId) const {
    return static_cast<T*>(plugin(pluginId));
  }

  // 按设备类型查询已加载的设备插件
  QList<PluginMetaData> devicesByType(const QString& deviceType) const;

  // 插件搜索路径
  void addSearchPath(const QString& path);
  QStringList searchPaths() const;

 Q_SIGNALS:
  void pluginLoaded(const QString& pluginId);
  void pluginUnloaded(const QString& pluginId);
  void pluginLoadFailed(const QString& pluginId, const QString& error);

 private:
  PluginManager();
  ~PluginManager() override;
  PluginManager(const PluginManager&) = delete;
  PluginManager& operator=(const PluginManager&) = delete;

  // 扫描目录下的插件库文件（不立即加载）
  void scanPluginDirs();

  // 从JSON文件解析元数据
  PluginMetaData parseMetaData(const QString& jsonPath) const;

  // 解析QPluginLoader读取的元数据（从动态库内嵌JSON）
  PluginMetaData parseMetaDataFromLib(const QString& libPath) const;

  class Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace plugin
}  // namespace core
}  // namespace etest

#endif  // ETEST_CORE_PLUGIN_PLUGIN_MANAGER_H_
