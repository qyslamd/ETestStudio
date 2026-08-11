#pragma once

#include <QLabel>
#include <QPixmap>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

class QListView;
class QStandardItemModel;

namespace etest::app {

class WelcomeRecentDelegate;

// 新双栏启动页（VS Code 风格）：头部品牌区 + 左栏「开始/最近项目」+
// 右栏「快速新建/入门指南」+ 底部状态条。背景沿用 CONFIG_WELCOME_BG_* 配置。
class WelcomeV2Widget : public QWidget {
  Q_OBJECT

 public:
  explicit WelcomeV2Widget(QWidget* parent = nullptr);

  /// 刷新最近项目（容器转发入口）
  void refreshRecentProjects();

 signals:
  void newProjectRequested();
  void openProjectRequested();
  void createFileRequested(const QString& categoryId, const QString& extension,
                           const QString& baseName);
  void projectOpenRequested(const QString& projectPath);
  void settingsRequested();

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void initUi();
  void initSignals();
  void loadBackground();
  void rebuildRecentList();
  void removeRecentProject(const QString& path);
  void showNextTip();
  QWidget* makeStartSection();
  QWidget* makeRecentSection();
  QWidget* makeQuickCreateSection();
  QWidget* makeGuideSection();

  // 最近项目（QListView + model + delegate，固定高度内部滚动）
  QListView* recent_view_ = nullptr;
  QStandardItemModel* recent_model_ = nullptr;
  WelcomeRecentDelegate* recent_delegate_ = nullptr;
  QLabel* recent_empty_ = nullptr;

  // 每日提示
  QLabel* tip_label_ = nullptr;
  QStringList tips_;
  int tip_index_ = -1;

  // 背景
  QPixmap bg_pixmap_;
  QString bg_image_path_;
  QString bg_dir_path_;
  int bg_mode_ = 0;
  QStringList image_filters_{
      "*.png", "*.jpg", "*.jpeg", "*.jfif", "*.bmp", "*.gif", "*.svg"};
};

}  // namespace etest::app
