#pragma once

#include <QPixmap>
#include <QWidget>

class QPushButton;
class SaverWidgetBase;

/// 覆盖 MainWindow 客户区的叠加层，空闲时显示屏保内容
///
/// 通过 SaverWidgetBase 策略接口支持多种屏保模式，
/// 模式由配置键 tuxsaver/mode 控制（"tux" / "wisdom"）。
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
  void initSaver();
  QString saverModeFromConfig() const;

  SaverWidgetBase* saver_ = nullptr;
  QString last_mode_;
  QPushButton* close_btn_ = nullptr;
  QPixmap background_;
};
