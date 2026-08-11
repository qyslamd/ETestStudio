#pragma once

#include <QWidget>

#include "dialogs/OverlayDialog.h"

class QCheckBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPushButton;
class QShowEvent;
class QToolButton;

namespace etest::app {

class GuidanceController;
class GuidanceFlow;

// 引导首页主题卡片：图标 + 名称 + 描述。
// 卡片带 objectName "guidanceCard"，背景/边框/hover/选中由 QSS（#guidanceCard）覆盖；
// 无 QSS 时由自绘兜底（读 ThemeManager token，明暗自适应）。
class GuidanceCard : public QWidget {
  Q_OBJECT
 public:
  explicit GuidanceCard(GuidanceFlow* flow, QWidget* parent = nullptr);

  GuidanceFlow* flow() const { return flow_; }
  bool isPlaceholder() const { return placeholder_; }
  void setSelected(bool selected);
  bool isSelected() const { return selected_; }

 signals:
  void clicked();

 protected:
  void enterEvent(QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

 private:
  void initUi();

 private:
  QLabel* icon_label_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* desc_label_ = nullptr;
  GuidanceFlow* flow_ = nullptr;
  bool placeholder_ = false;  // 无步骤（stepCount==0）的占位主题：置灰"敬请期待"
  bool hovered_ = false;
  bool selected_ = false;
  bool pressed_ = false;
};

// 引导首页：复用 OverlayDialog 覆盖 MainWindow（含 ribbon）的遮罩 + 居中卡片，
// 遮罩色设为黑色半透明（引导聚焦）。模态 exec()，点遮罩空白不做退出。
class GuidanceHomePage : public OverlayDialog {
  Q_OBJECT

 public:
  explicit GuidanceHomePage(GuidanceController* controller,
                            QWidget* parent = nullptr);

 protected:
  void showEvent(QShowEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void rebuildCards();
  void hideHomePage();
  void startFlow(GuidanceFlow* flow);

 private:
  QFrame* panel_ = nullptr;
  QLabel* title_label_ = nullptr;
  QLabel* sub_label_ = nullptr;
  QToolButton* close_btn_ = nullptr;
  QPushButton* all_btn_ = nullptr;
  QCheckBox* auto_check_ = nullptr;
  QHBoxLayout* cards_layout_ = nullptr;
  GuidanceController* controller_ = nullptr;
};

}  // namespace etest::app
