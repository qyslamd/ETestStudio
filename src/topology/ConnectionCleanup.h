#pragma once

#include <QString>
#include <QVector>

namespace etest::topology {

class TopologyDocument;

/// 描述一个无效项的详细信息（无效连线）
struct InvalidEntry {
  enum Type { Connection };
  Type type;
  int index;       ///< connection 下标
  QString description;
};

/// 连接清理工具：扫描文档中无效的连线
class ConnectionCleanup {
 public:
  /// 扫描 doc 并返回所有无效项
  static QVector<InvalidEntry> findInvalid(const TopologyDocument* doc);

  /// 按安全删除顺序排序：连接按易失下标降序
  static void sortForRemoval(QVector<InvalidEntry>* entries);
};

}  // namespace etest::topology
