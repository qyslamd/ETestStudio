#ifndef ETEST_APP_ACTIVITY_BAR_WIDGET_H_
#define ETEST_APP_ACTIVITY_BAR_WIDGET_H_

#include <QIcon>
#include <QPushButton>
#include <QString>
#include <QVector>
#include <QWidget>

class QVBoxLayout;

namespace etest::app {

struct ActivityBarPageInfo {
  QString id;
  QString iconName;
  QString tooltip;
};

class ActivityBarWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ActivityBarWidget(QWidget* parent = nullptr);

  void addPage(const QString& id, const QString& tooltip, const QString& iconName);
  void setActivePageId(const QString& id);
  QString activePageId() const;
  void reloadIcons();

 signals:
  void pageClicked(const QString& id);
  void settingsTriggered();

 private:
  void setupUi();
  QPushButton* createButton(const QString& tooltip);

  QVector<ActivityBarPageInfo> pages_;
  QVector<QPushButton*> buttons_;
  QVBoxLayout* top_layout_ = nullptr;
  QPushButton* settings_btn_ = nullptr;
  QString active_page_id_;
};

}  // namespace etest::app

#endif  // ETEST_APP_ACTIVITY_BAR_WIDGET_H_
