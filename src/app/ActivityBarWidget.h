#ifndef ETEST_APP_ACTIVITY_BAR_WIDGET_H_
#define ETEST_APP_ACTIVITY_BAR_WIDGET_H_

#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class ActivityBarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ActivityBarWidget(QWidget* parent = nullptr);

  void setActiveIndex(int index);
  int activeIndex() const;

 Q_SIGNALS:
  void activityClicked(int index);

 private:
  void setupUi();
  QPushButton* createButton(const QString& tooltip, const QString& iconText);

  QVBoxLayout* layout_;
  QVector<QPushButton*> buttons_;
  int active_index_ = 0;
};

#endif  // ETEST_APP_ACTIVITY_BAR_WIDGET_H_
