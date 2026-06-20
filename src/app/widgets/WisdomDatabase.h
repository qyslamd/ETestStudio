#pragma once

#include <QString>
#include <QVector>

/// 一条诗词记录
struct PoemData {
  QString sentence;    // 金句正文
  QString source;      // 出处（如 "苏轼《赤壁赋》"）
  QString commentary;  // 简短赏析
  QString tag;         // 标签：哲理/人生/自然/情感/励志
  QString dynasty;     // 朝代
};

/// 诗词数据库单例
///
/// 负责从预构建的 SQLite 数据库（嵌入 Qt 资源）初始化连接，
/// 首次运行时复制到 AppDataLocation/wisdom/poems.db。
class WisdomDatabase {
 public:
  static WisdomDatabase& instance();

  /// 初始化数据库连接并返回全部诗词数据
  /// @param writablePath QStandardPaths::AppDataLocation 路径
  /// @return 全部诗词记录
  QVector<PoemData> initDatabase(const QString& writablePath);

 private:
  WisdomDatabase() = default;
  ~WisdomDatabase();
  WisdomDatabase(const WisdomDatabase&) = delete;
  WisdomDatabase& operator=(const WisdomDatabase&) = delete;

  class Impl;
  Impl* impl_ = nullptr;
  QVector<PoemData> poems_;
};
