#pragma once

#include <QEvent>
#include <QIcon>
#include <QString>
#include <QToolButton>
#include <QVector>
#include <QWidget>

class QVBoxLayout;

namespace etest::app {

struct SidebarNavPageInfo {
  QString id;
  QString iconName;
  QString tooltip;
};

class SidebarNavWidget : public QWidget {
  Q_OBJECT

 public:
  explicit SidebarNavWidget(QWidget* parent = nullptr);

  void addPage(const QString& id,
               const QString& tooltip,
               const QString& iconName);
  void setActivePageId(const QString& id);
  void clearActivePage();
  QString activePageId() const;
  void reloadIcons();
  void setLoginState(bool loggedIn,
                     const QString& userName,
                     const QString& role);
  void setLoginActive(bool active);
  void setSettingsActive(bool active);

 signals:
  void pageClicked(const QString& id);
  void settingsTriggered();
  void loginTriggered();

 private:
  void initUi();
  QToolButton* createButton(const QString& tooltip);
  void updateActiveIconSize();
  bool eventFilter(QObject* obj, QEvent* event) override;

  enum { kNormalIconSize = 24, kActiveIconSize = 36 };

  QVector<SidebarNavPageInfo> pages_;
  QVector<QToolButton*> buttons_;
  QVBoxLayout* top_layout_ = nullptr;
  QString active_page_id_;
};

}  // namespace etest::app
