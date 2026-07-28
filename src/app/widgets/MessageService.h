#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <functional>

namespace etest::app {

/// 提示消息数据
struct HintData {
  QString text;
  QString actionLabel;
  std::function<void()> action;
  bool read = false;
};

/// 全局消息服务（单例），继承 QAbstractListModel 既做消息入口又做 QListView 的 model。
/// 任何组件可通过 MessageService::instance().postHint(...) 发送消息。
class MessageService : public QAbstractListModel {
  Q_OBJECT
 public:
  static MessageService& instance();

  enum Roles {
    TextRole = Qt::UserRole + 1,
    ActionLabelRole,
    HasActionRole,
    ReadRole,
  };

  // 消息入口
  void postHint(const QString& text,
                const QString& actionLabel = QString(),
                std::function<void()> action = nullptr);
  void clearAll();
  void markAllRead();
  void removeAt(int row);
  void markRead(int row);
  void triggerAction(int row);

  // QAbstractListModel
  int rowCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  int unreadCount() const { return unread_count_; }

 signals:
  void unreadCountChanged(int count);

 private:
  MessageService(QObject* parent = nullptr);

  QList<HintData> messages_;
  int unread_count_ = 0;

  void updateUnreadCount();
};

}  // namespace etest::app
