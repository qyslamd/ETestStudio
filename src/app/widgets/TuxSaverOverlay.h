#pragma once

#include <QPixmap>
#include <QVector>
#include <QWidget>

class QPushButton;
class SaverWidgetBase;

/// 覆盖 MainWindow 客户区的叠加层，空闲时显示屏保内容
///
/// 通过 SaverWidgetBase 策略接口支持多种屏保模式切换。
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
  void initModes();
  void switchMode(int direction);
  void repositionButtons();

  QVector<SaverWidgetBase*> modes_;
  int currentMode_ = 0;
  SaverWidgetBase* saver_ = nullptr;
  QPushButton* close_btn_ = nullptr;
  QPushButton* prev_btn_ = nullptr;
  QPushButton* next_btn_ = nullptr;
  QPixmap background_;
};
