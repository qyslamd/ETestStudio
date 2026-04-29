#ifndef ETEST_APP_ACTIVITY_BAR_WIDGET_H_
#define ETEST_APP_ACTIVITY_BAR_WIDGET_H_

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace etest {
namespace app {

class ActivityBarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ActivityBarWidget(QWidget* parent = nullptr);

  void setActiveIndex(int index);
  int activeIndex() const;

 Q_SIGNALS:
  void activityClicked(int index);
  void sidebarToggleRequested();

 private:
  void setupUi();
  QPushButton* createButton(const QString& tooltip, const QString& iconText);

  QVBoxLayout* layout_;
  QVBoxLayout* top_layout_;
  QVBoxLayout* bottom_layout_;
  QVector<QPushButton*> buttons_;
  int active_index_ = 0;
};

}  // namespace app
}  // namespace etest

#endif  // ETEST_APP_ACTIVITY_BAR_WIDGET_H_
