#include "MessageService.h"

namespace etest::app {

MessageService& MessageService::instance() {
  static MessageService instance;
  return instance;
}

MessageService::MessageService(QObject* parent) : QAbstractListModel(parent) {}

void MessageService::postHint(const QString& text,
                               const QString& actionLabel,
                               std::function<void()> action) {
  int row = messages_.size();
  beginInsertRows(QModelIndex(), row, row);
  messages_.append({text, actionLabel, std::move(action), false});
  endInsertRows();
  updateUnreadCount();
}

void MessageService::clearAll() {
  if (messages_.isEmpty()) {
    return;
  }
  beginResetModel();
  messages_.clear();
  endResetModel();
  updateUnreadCount();
}

void MessageService::markAllRead() {
  bool changed = false;
  for (auto& m : messages_) {
    if (!m.read) {
      m.read = true;
      changed = true;
    }
  }
  if (changed) {
    emit dataChanged(index(0), index(messages_.size() - 1), {ReadRole});
    updateUnreadCount();
  }
}

void MessageService::removeAt(int row) {
  if (row < 0 || row >= messages_.size()) {
    return;
  }
  beginRemoveRows(QModelIndex(), row, row);
  messages_.removeAt(row);
  endRemoveRows();
  updateUnreadCount();
}

void MessageService::markRead(int row) {
  if (row < 0 || row >= messages_.size()) {
    return;
  }
  if (messages_[row].read) {
    return;
  }
  messages_[row].read = true;
  emit dataChanged(index(row), index(row), {ReadRole});
  updateUnreadCount();
}

void MessageService::triggerAction(int row) {
  if (row < 0 || row >= messages_.size()) {
    return;
  }
  // 先标记已读，再拷贝 action 后调用（回调可能修改消息列表导致 row 失效）
  markRead(row);
  auto action = messages_[row].action;
  if (action) {
    action();
  }
}

int MessageService::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return messages_.size();
}

QVariant MessageService::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() >= messages_.size()) {
    return {};
  }
  const auto& m = messages_[index.row()];
  switch (role) {
    case TextRole:
      return m.text;
    case ActionLabelRole:
      return m.actionLabel;
    case HasActionRole:
      return m.action != nullptr;
    case ReadRole:
      return m.read;
    default:
      return {};
  }
}

QHash<int, QByteArray> MessageService::roleNames() const {
  return {
      {TextRole, "text"},
      {ActionLabelRole, "actionLabel"},
      {HasActionRole, "hasAction"},
      {ReadRole, "read"},
  };
}

void MessageService::updateUnreadCount() {
  int count = 0;
  for (const auto& m : messages_) {
    if (!m.read) {
      ++count;
    }
  }
  if (count != unread_count_) {
    unread_count_ = count;
    emit unreadCountChanged(unread_count_);
  }
}

}  // namespace etest::app
