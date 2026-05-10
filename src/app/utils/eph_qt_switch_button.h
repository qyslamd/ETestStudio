#ifndef ETEST_APP_UTILS_EPH_QT_SWITCH_BUTTON_H_
#define ETEST_APP_UTILS_EPH_QT_SWITCH_BUTTON_H_

#include <QAbstractButton>
#include <QBrush>
#include <QString>

namespace etest::app {

class EphQtSwitchButton : public QAbstractButton {
  Q_OBJECT
 public:
  explicit EphQtSwitchButton(QWidget* parent = nullptr);

  void setOnBackground(const QBrush& brush);
  void setOffBackground(const QBrush& brush);

  void setCheckedText(const QString& text);
  void setUnCheckedText(const QString& text);

 protected:
  void paintEvent(QPaintEvent* event) override;
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 private:
  void paintButton(QPainter* p);
  void fillRoundRect(QPainter* p, const QRectF& rect, const QBrush& brush);
  void paintText(QPainter* p);

  QBrush background_on_ = QColor(Qt::green);
  QBrush background_off_ = QColor(255, 37, 37);
  QString checked_text_ = QStringLiteral("使能");
  QString unchecked_text_ = QStringLiteral("禁用");
};

}  // namespace etest::app

#endif  // ETEST_APP_UTILS_EPH_QT_SWITCH_BUTTON_H_
