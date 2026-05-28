#ifndef ETEST_APP_WELCOME_WIDGET_H_
#define ETEST_APP_WELCOME_WIDGET_H_

#include <QEvent>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QStringList>
#include <QWidget>

namespace etest::app {

class WelcomeWidget : public QWidget {
  Q_OBJECT

 public:
  explicit WelcomeWidget(QWidget* parent = nullptr);

  void refreshRecentProjects();
  void loadBackground();

 signals:
  void newProjectRequested();
  void openProjectRequested();
  void projectOpenRequested(const QString& projectPath);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void rebuildRecentCards();
  void showRandomTip();

  QPushButton* btn_new_project_ = nullptr;
  QPushButton* btn_open_project_ = nullptr;
  QWidget* center_widget_ = nullptr;
  QScrollArea* recent_scroll_ = nullptr;
  QWidget* recent_container_ = nullptr;
  QLabel* tip_label_ = nullptr;

  // 背景图片
  QPixmap bg_pixmap_;
  QString bg_image_path_;
  QString bg_dir_path_;
  int bg_mode_ = 0;  // 0=center, 1=tile, 2=stretch
  QStringList image_filters_{
      "*.png", "*.jpg", "*.jpeg", "*.jfif", "*.bmp", "*.gif", "*.svg"};

  // 每日提示
  QStringList tips_;
};

}  // namespace etest::app

#endif  // ETEST_APP_WELCOME_WIDGET_H_
