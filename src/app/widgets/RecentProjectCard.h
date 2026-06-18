#ifndef ETEST_APP_RECENT_PROJECT_CARD_H_
#define ETEST_APP_RECENT_PROJECT_CARD_H_

#include <QString>
#include <QWidget>

class QLabel;

namespace etest::app {

// 最近项目卡片，自包含点击/右键菜单交互。
// 默认 objectName 为 WelcomeProjectCardContent，调用方可自行 setObjectName
// 和 setAttribute 来适配不同场景的 QSS 样式。
class RecentProjectCard : public QWidget {
  Q_OBJECT

 public:
  RecentProjectCard(const QString& projectPath, const QString& displayName,
                    const QString& dirPath, const QString& timeStr,
                    QWidget* parent = nullptr);

  void setTimeStr(const QString& timeStr);
  void setRemoveActionText(const QString& text);

 signals:
  void openRequested(const QString& projectPath);
  void removeRequested(const QString& projectPath);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  void initUi(const QString& displayName, const QString& dirPath,
              const QString& timeStr);
  void initSignals();

  QString project_path_;
  QString remove_action_text_ = QStringLiteral("从列表中移除");
  QLabel* name_label_ = nullptr;
  QLabel* path_label_ = nullptr;
  QLabel* time_label_ = nullptr;
};

}  // namespace etest::app

#endif  // ETEST_APP_RECENT_PROJECT_CARD_H_
