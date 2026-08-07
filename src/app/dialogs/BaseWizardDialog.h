#pragma once

#include "AnimationDialog.h"

#include <QPixmap>

class QCloseEvent;
class QFrame;
class QKeyEvent;
class QLabel;
class QPushButton;
class QStackedWidget;
class QVariantAnimation;

namespace etest::app {

class WizardStepBar;

// 向导页基类：提供校验能力，供 BaseWizardDialog 控制导航。
class WizardPage : public QWidget {
  Q_OBJECT

 public:
  explicit WizardPage(QWidget* parent = nullptr);

  /// 步骤条标签（默认取 title()）
  virtual QString stepLabel() const { return title(); }
  /// 兼容旧接口的页眉标题；v2 由 setHeader 提供固定页头
  virtual QString title() const { return QString(); }
  /// 当前输入是否满足进入下一步/完成的条件
  virtual bool isComplete() const { return true; }
  /// 下一步前校验；返回 false 时页面自行提示并阻止前进
  virtual bool validatePage() { return true; }

 signals:
  /// 页面输入变化导致摘要/校验结果变化时发出
  void completeChanged();
};

// 通用向导容器：QDialog + QStackedWidget 自绘，继承 AnimationDialog 复用
// 阴影/圆角/飞入动画。Fluent 风格布局：页头 + 步骤条 + 淡入上滑转场 + 页脚。
class BaseWizardDialog : public AnimationDialog {
  Q_OBJECT

 public:
  explicit BaseWizardDialog(QWidget* parent = nullptr);

  /// 追加一页，返回页索引
  int addPage(WizardPage* page);
  void setCurrentPage(int index);
  int currentPageIndex() const;
  int pageCount() const;

  /// 前进/后退一页（带淡入上滑转场）
  void next();
  void back();

  /// 设置固定页头：图标名 + 标题 + 副标题
  void setHeader(const QString& iconName, const QString& title,
                 const QString& subtitle);

 signals:
  /// 当前页切换后发出（index 为新页索引）
  void currentPageChanged(int index);

 protected:
  void initUi();
  void initSignals();
  /// 根据当前页位置与校验结果同步导航按钮状态
  void updateNavButtons();
  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  /// 取消二次确认；子类可覆写定制文案
  virtual void confirmCancel();

 private:
  /// 淡入上滑转场：旧页原地淡出，新页自下方 8px 上滑淡入
  void animatePageChange(int fromIndex, int toIndex);
  /// 按当前页面集合重建步骤条
  void rebuildStepBar();
  /// 弹出取消二次确认，返回用户是否确认取消
  bool requestCancelConfirmation();

  class PageTransitionOverlay;
  PageTransitionOverlay* transition_overlay_ = nullptr;
  QVariantAnimation* page_transition_animation_ = nullptr;
  QPixmap transition_old_pixmap_;
  QPixmap transition_new_pixmap_;
  int transition_to_ = -1;

  QStackedWidget* page_stack_ = nullptr;
  QLabel* header_icon_label_ = nullptr;
  QLabel* header_title_label_ = nullptr;
  QLabel* header_subtitle_label_ = nullptr;
  WizardStepBar* step_bar_ = nullptr;
  QPushButton* back_btn_ = nullptr;
  QPushButton* next_btn_ = nullptr;
  QPushButton* create_btn_ = nullptr;
  QPushButton* cancel_btn_ = nullptr;
  QList<WizardPage*> pages_;
};

}  // namespace etest::app
