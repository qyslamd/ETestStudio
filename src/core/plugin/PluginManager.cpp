#include "PluginManager.h"
#include "config/ConfigDefs.h"
#include "config/ConfigManager.h"
#include "logger/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPluginLoader>

namespace etest::core::plugin {

using namespace etest::core::config;
using namespace etest::core::logger;

class PluginManager::Impl {
 public:
  // 已扫描到的插件库路径: pluginId -> libPath
  QMap<QString, QString> scanned_libs_;

  // 已加载的插件: pluginId -> IPlugin*
  QMap<QString, IPlugin*> loaded_plugins_;

  // QPluginLoader实例: pluginId -> loader
  QMap<QString, QPluginLoader*> loaders_;

  // 插件搜索路径
  QStringList search_paths_;
};

PluginManager::PluginManager() : QObject(nullptr), m_impl(new Impl()) {
  // 默认搜索路径：可执行文件旁的plugins/目录
  QString defaultPath = QCoreApplication::applicationDirPath() + "/plugins";
  m_impl->search_paths_.append(defaultPath);

  // 从ConfigManager读取自定义搜索路径
  ConfigManager& cfg = ConfigManager::instance();
  QString customPaths = cfg.get<QString>(CONFIG_PLUGIN_SEARCH_PATHS);
  if (!customPaths.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const auto parts = customPaths.split(';', Qt::SkipEmptyParts);
#else
    const auto parts = customPaths.split(';', QString::SkipEmptyParts);
#endif
    for (const QString& p : parts) {
      QString trimmed = p.trimmed();
      if (!trimmed.isEmpty() && !m_impl->search_paths_.contains(trimmed)) {
        m_impl->search_paths_.append(trimmed);
      }
    }
  }
}

PluginManager::~PluginManager() {
  unloadAll();
}

PluginManager& PluginManager::instance() {
  static PluginManager inst;
  return inst;
}

void PluginManager::loadAll() {
  scanPluginDirs();

  for (const QString& id : m_impl->scanned_libs_.keys()) {
    loadPlugin(id);
  }
}

void PluginManager::unloadAll() {
  // 逆序卸载：先stop，再uninitialize，再卸载库
  QStringList ids = m_impl->loaded_plugins_.keys();
  for (int i = ids.size() - 1; i >= 0; --i) {
    unloadPlugin(ids[i]);
  }
}

bool PluginManager::loadPlugin(const QString& pluginId) {
  if (m_impl->loaded_plugins_.contains(pluginId)) {
    LOG_WARN("PLUGIN", "插件 {} 已加载，跳过重复加载", pluginId.toStdString());
    return true;
  }

  QString libPath = m_impl->scanned_libs_.value(pluginId);
  if (libPath.isEmpty()) {
    // 尝试重新扫描
    scanPluginDirs();
    libPath = m_impl->scanned_libs_.value(pluginId);
    if (libPath.isEmpty()) {
      emit pluginLoadFailed(pluginId, "未找到插件库文件");
      return false;
    }
  }

  QPluginLoader* loader = new QPluginLoader(libPath, this);

  // 先检查依赖
  PluginMetaData meta = parseMetaDataFromLib(libPath);
  for (const QString& dep : meta.dependencies) {
    if (!m_impl->loaded_plugins_.contains(dep)) {
      emit pluginLoadFailed(pluginId, QString("依赖插件 %1 未加载").arg(dep));
      delete loader;
      return false;
    }
  }

  QObject* obj = loader->instance();
  if (!obj) {
    QString error = loader->errorString();
    LOG_ERROR("PLUGIN", "加载插件 {} 失败: {}", pluginId.toStdString(),
              error.toStdString());
    emit pluginLoadFailed(pluginId, error);
    delete loader;
    return false;
  }

  IPlugin* plugin = qobject_cast<IPlugin*>(obj);
  if (!plugin) {
    LOG_ERROR("PLUGIN", "插件 {} 未实现IPlugin接口", pluginId.toStdString());
    emit pluginLoadFailed(pluginId, "未实现IPlugin接口");
    loader->unload();
    delete loader;
    return false;
  }

  // 初始化
  if (!plugin->initialize()) {
    LOG_ERROR("PLUGIN", "插件 {} 初始化失败", pluginId.toStdString());
    emit pluginLoadFailed(pluginId, "初始化失败");
    loader->unload();
    delete loader;
    return false;
  }

  // 启动
  if (!plugin->start()) {
    LOG_WARN("PLUGIN", "插件 {} 启动失败，但已加载", pluginId.toStdString());
  }

  m_impl->loaders_[pluginId] = loader;
  m_impl->loaded_plugins_[pluginId] = plugin;

  LOG_INFO("PLUGIN", "插件 {} v{} 加载成功", meta.name.toStdString(),
           meta.version.toStdString());
  emit pluginLoaded(pluginId);
  return true;
}

bool PluginManager::unloadPlugin(const QString& pluginId) {
  IPlugin* p = m_impl->loaded_plugins_.value(pluginId);
  if (!p)
    return false;

  p->stop();
  p->uninitialize();

  QPluginLoader* loader = m_impl->loaders_.take(pluginId);
  m_impl->loaded_plugins_.remove(pluginId);

  if (loader) {
    loader->unload();
    loader->deleteLater();
  }

  LOG_INFO("PLUGIN", "插件 {} 已卸载", pluginId.toStdString());
  emit pluginUnloaded(pluginId);
  return true;
}

QList<PluginMetaData> PluginManager::loadedPlugins() const {
  QList<PluginMetaData> result;
  for (IPlugin* p : m_impl->loaded_plugins_.values()) {
    result.append(p->metaData());
  }
  return result;
}

IPlugin* PluginManager::plugin(const QString& pluginId) const {
  return m_impl->loaded_plugins_.value(pluginId, nullptr);
}

void PluginManager::addSearchPath(const QString& path) {
  if (!m_impl->search_paths_.contains(path)) {
    m_impl->search_paths_.append(path);
  }
}

QStringList PluginManager::searchPaths() const {
  return m_impl->search_paths_;
}

void PluginManager::scanPluginDirs() {
  for (const QString& dirPath : m_impl->search_paths_) {
    QDir dir(dirPath);
    if (!dir.exists())
      continue;

    QStringList filters;
#ifdef Q_OS_WIN
    filters << "*.dll";
#elif defined(Q_OS_LINUX)
    filters << "*.so";
#elif defined(Q_OS_MAC)
    filters << "*.dylib";
#endif

    for (const QFileInfo& fi : dir.entryInfoList(filters, QDir::Files)) {
      QString libPath = fi.absoluteFilePath();

      // 利用QPluginLoader读取元数据，不需要加载库
      PluginMetaData meta = parseMetaDataFromLib(libPath);
      if (meta.isValid()) {
        m_impl->scanned_libs_[meta.id] = libPath;
      }
    }
  }
}

PluginMetaData PluginManager::parseMetaDataFromLib(
    const QString& libPath) const {
  PluginMetaData meta;

  QPluginLoader loader(libPath);
  QJsonObject jsonMeta = loader.metaData();

  // Qt的metaData()返回的JSON结构：
  // { "IID": "...", "className": "...", "MetaData": { ...我们的JSON内容... } }
  QJsonObject metaDataObj = jsonMeta.value("MetaData").toObject();

  meta.id = metaDataObj.value("id").toString();
  meta.name = metaDataObj.value("name").toString();
  meta.version = metaDataObj.value("version").toString();
  meta.description = metaDataObj.value("description").toString();
  meta.author = metaDataObj.value("author").toString();
  meta.category = metaDataObj.value("category").toString();
  meta.device_type = metaDataObj.value("device_type").toString();
  meta.device_channels = metaDataObj.value("device_channels").toInt(0);
  meta.device_function = metaDataObj.value("device_function").toString();
  meta.device_direction = metaDataObj.value("device_direction").toString(
      QStringLiteral("Bidirectional"));
  meta.is_mock = metaDataObj.value("is_mock").toBool(false);

  QJsonArray deps = metaDataObj.value("dependencies").toArray();
  for (const QJsonValue& v : deps) {
    meta.dependencies.append(v.toString());
  }

  return meta;
}

QList<PluginMetaData> PluginManager::devicesByType(const QString& deviceType) const {
  QList<PluginMetaData> result;
  for (IPlugin* p : m_impl->loaded_plugins_.values()) {
    PluginMetaData meta = p->metaData();
    if (meta.device_type == deviceType) {
      result.append(meta);
    }
  }
  return result;
}

QList<PluginMetaData> PluginManager::devicesByMockType(
    const QString& deviceType, bool mock) const {
  QList<PluginMetaData> result;
  for (IPlugin* p : m_impl->loaded_plugins_.values()) {
    PluginMetaData meta = p->metaData();
    if (meta.device_type == deviceType && meta.is_mock == mock) {
      result.append(meta);
    }
  }
  return result;
}

}  // namespace etest::core::plugin