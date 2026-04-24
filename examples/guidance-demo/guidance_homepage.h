#ifndef GUIDANCE_HOME_PAGE_H
#define GUIDANCE_HOME_PAGE_H

#include <QWidget>
class WindowMover;

namespace Ui {
class GuidanceHomePage;
}

class QStandardItemModel;
class GuidanceController;
class GuidanceHomePage : public QWidget {
  Q_OBJECT

 public:
  explicit GuidanceHomePage(GuidanceController* controller,
                            QWidget* parent = nullptr);
  ~GuidanceHomePage();
 signals:
  void windowHided();

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 protected slots:
  void actShowAnimation();
  void actHideAnimation();
  void onListViewItemClicked(const QModelIndex& index);
  void onButtonAllClicked();
  void onButtonGoClicked();

 private:
  void initUi();
  void initSignals();

 private:
  Ui::GuidanceHomePage* ui;
  QWidget* widget_ = nullptr;
  WindowMover* mover_ = nullptr;

  QStandardItemModel* model_ = nullptr;
  GuidanceController* controller_ = nullptr;
};

#endif  // GUIDANCE_HOME_PAGE_H
