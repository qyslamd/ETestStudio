#pragma once

#include <QStringList>
#include <QWidget>

namespace etest::app {

// Fluent 风格横向步骤条：圆点 + 内联标签 + 连接线。
// 三态：已完成（绿底勾）/ 进行中（主题色底数字+光晕）/ 未到达（灰）。
// 颜色取自 ThemeManager 语义色，跟随主题切换自动刷新。
// 布局与绘制参考 NavProgress（JD 样式）自绘思路。
class WizardStepBar : public QWidget {
  Q_OBJECT
 public:
  explicit WizardStepBar(QWidget* parent = nullptr);

  /// 设置各步标签，个数即步数
  void setLabels(const QStringList& labels);
  /// 设置当前步（0 起），驱动三态
  void setCurrentStep(int currentStep);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  struct StepColors {
    QColor circleBg;      // 未到达圆底
    QColor circleText;    // 未到达数字/标签
    QColor labelActive;   // 进行中/已完成标签
    QColor lineIdle;      // 连接线
    QColor accent;        // 进行中
    QColor success;       // 已完成
  };
  StepColors colors() const;

  QStringList labels_;
  int current_step_ = 0;
};

}  // namespace etest::app
