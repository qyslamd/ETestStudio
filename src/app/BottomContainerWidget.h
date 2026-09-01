#ifndef ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_
#define ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_

#include <QFrame>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>

namespace etest::app {

class BottomContainerWidget : public QFrame {
  Q_OBJECT

 public:
  explicit BottomContainerWidget(QWidget* parent = nullptr);

  void addPanel(const QString& title,
                QWidget* panel,
                const QString& iconName = {});

  void setPanelVisible(int index, bool visible);
  bool isPanelVisible(int index) const;
  int indexOf(QWidget* panel) const;
  int count() const;

  void setCurrentPanel(int index);
  int currentPanelIndex() const;

 signals:
  void panelVisibilityChanged();

 private:
  void initUi();

  QTabWidget* tab_widget_;
  QStringList tab_icon_names_;
};

}  // namespace etest::app

#endif  // ETEST_APP_BOTTOM_CONTAINER_WIDGET_H_
