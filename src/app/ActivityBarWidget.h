#ifndef ETEST_APP_ACTIVITY_BAR_WIDGET_H_
#define ETEST_APP_ACTIVITY_BAR_WIDGET_H_

#include <QPushButton>
#include <QVector>
#include <QWidget>

class QVBoxLayout;

namespace etest::app {

class ActivityBarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ActivityBarWidget(QWidget* parent = nullptr);

  void setActiveIndex(int index);
  int activeIndex() const;

 signals:
  void pageClicked(int index);
  void settingsTriggered();

 private:
  void setupUi();
  QPushButton* createButton(const QString& tooltip,
                            const QString& darkIconPath,
                            const QString& lightIconPath);

  QVector<QPushButton*> buttons_;
  int active_index_ = 0;
};

}  // namespace etest::app

#endif  // ETEST_APP_ACTIVITY_BAR_WIDGET_H_
