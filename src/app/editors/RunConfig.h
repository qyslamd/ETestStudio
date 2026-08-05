#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace etest::app {

// ── 运行配置数据（.erun 文件） ──
// 运行编辑器产出：选择测试程序 + 监听器配置 + 布局 + 运行参数（预留）。
// 纯数据模型，与 UI 解耦，便于运行态 / 独立运行程序消费。
struct RunConfig {
  struct Monitor {
    QString connectionId;
    QString displayMode;
    QString name;
  };
  struct LayoutItem {
    QString connectionId;
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
  };

  QStringList programs;  // 测试程序（相对项目根路径），与监听/布局正交
  QVector<Monitor> monitors;
  QVector<LayoutItem> layout;
  QJsonObject runParams;  // 预留

  QJsonObject toJson() const;
  bool fromJson(const QJsonObject& obj);

  // 文件读写助手（统一 JSON 解析/写出，编辑器与运行态共用）
  static bool loadFromFile(const QString& path, RunConfig* out);
  static bool saveToFile(const QString& path, const RunConfig& config);
};

}  // namespace etest::app
