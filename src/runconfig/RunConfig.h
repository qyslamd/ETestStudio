#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace etest::runconfig {

// ── 运行配置数据（.erun 文件） ──
// 运行编辑器产出：选择测试程序 + 监听器配置 + 运行参数（预留）。
// Monitor 自包含卡片全部状态（id 关联 / 绑定连线 / 类型 / 名称 / 几何）。
// 纯数据模型，与 UI 解耦，便于运行态 / 独立运行程序消费。
struct RunConfig {
  struct Monitor {
    QString id;            // 卡片实例 UUID（进场景分配，layout 关联 key）
    QString connectionId;  // 绑定连线，空 = 未绑定
    QString displayMode;
    QString name;
    double x = 0;  // 几何并入 Monitor（废弃独立 layout 数组）
    double y = 0;
    double w = 0;
    double h = 0;
  };

  QStringList programs;  // 测试程序（相对项目根路径），与监听/布局正交
  QVector<Monitor> monitors;
  QJsonObject runParams;  // 预留

  QJsonObject toJson() const;
  bool fromJson(const QJsonObject& obj);

  // 文件读写助手（统一 JSON 解析/写出，编辑器与运行态共用）
  static bool loadFromFile(const QString& path, RunConfig* out);
  static bool saveToFile(const QString& path, const RunConfig& config);
};

}  // namespace etest::runconfig
