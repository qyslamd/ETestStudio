#pragma once

#include <QPixmap>
#include <QWidget>

class QPushButton;
class TuxSaverWidget;

/// 覆盖 MainWindow 客户区的叠加层，空闲时显示 Tux 企鹅动画
class TuxSaverOverlay : public QWidget {
  Q_OBJECT
 public:
  explicit TuxSaverOverlay(QWidget* parent = nullptr);

  void activate();
  void deactivate();

 signals:
  void closed();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

 private:
  void repositionCloseButton();

  TuxSaverWidget* saver_ = nullptr;
  QPushButton* close_btn_ = nullptr;
  QPixmap background_;
};
