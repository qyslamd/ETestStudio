#pragma once

#include <QWidget>

class QLabel;
class QListView;
class QStackedWidget;
class QToolButton;

namespace etest::app {

class HintMessageDelegate;

/// 消息提示 popup 窗口。Qt::Popup 模式，顶部工具栏 + QListView。
class HintPopup : public QWidget {
  Q_OBJECT
 public:
  explicit HintPopup(QWidget* parent = nullptr);

  /// 显示在指定全局坐标的右下方（补偿阴影边距使内容框对齐）
  void showBelow(const QPoint& globalPos);

  /// 根据 QListView 内容切换显示（空/非空页面切换）
  void refresh();

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  QListView* list_view_ = nullptr;
  QLabel* empty_label_ = nullptr;
  QStackedWidget* content_stack_ = nullptr;
  HintMessageDelegate* delegate_ = nullptr;
  QToolButton* mark_all_btn_ = nullptr;
  QToolButton* clear_btn_ = nullptr;
  int last_hovered_row_ = -1;
};

}  // namespace etest::app
