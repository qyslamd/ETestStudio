#ifndef ETEST_APP_PROBLEMS_PANEL_H_
#define ETEST_APP_PROBLEMS_PANEL_H_

#include <QMetaType>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace etest::app {

/// 验证错误来源枚举，用于问题项跳转导航
/// 对应 verify() 的 6 条检查项，点击问题项时透传给 MainWindow 执行跳转
enum class NavTarget {
  Project,   ///< 未打开项目
  Icd,       ///< ICD 协议未加载
  Topology,  ///< 未找到拓扑文件
  Signal,    ///< 拓扑未绑定信号
  Program,   ///< 无可用测试程序
  Hardware   ///< 硬件/Mock 未配置
};

class ProblemsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit ProblemsPanel(QWidget* parent = nullptr);

  void clearProblems();
  /// @param target 跳转目标枚举，存入列 0 的 Qt::UserRole
  /// @param display_source 列 0「来源」显示文本（如「ICD 协议」「拓扑」）
  /// @param type 列 2「类型」文本（「错误」/「警告」）
  /// @param message 列 1「描述」文本
  void addProblem(NavTarget target, const QString& display_source,
                  const QString& type, const QString& message);
  void showSummary(int errors, int warnings);

 signals:
  /// 双击问题项时发出，携带跳转目标枚举
  void problemActivated(NavTarget target);

 private slots:
  void onCellDoubleClicked(int row, int column);

 private:
  void initUi();
  void initSignals();

  QTableWidget* table_;
};

}  // namespace etest::app

Q_DECLARE_METATYPE(etest::app::NavTarget)

#endif  // ETEST_APP_PROBLEMS_PANEL_H_
