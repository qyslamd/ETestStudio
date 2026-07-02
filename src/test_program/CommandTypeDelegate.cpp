#include "CommandTypeDelegate.h"

#include <QComboBox>

CommandTypeDelegate::CommandTypeDelegate(Mode mode, QObject* parent)
    : QStyledItemDelegate(parent) {
  if (mode == Full) {
    items_ << QStringLiteral("SET") << QStringLiteral("VERIFY")
           << QStringLiteral("WAIT") << QStringLiteral("DELAY")
           << QStringLiteral("ACTION") << QStringLiteral("LOG")
           << QStringLiteral("LOOP") << QStringLiteral("WHILE")
           << QStringLiteral("IF") << QStringLiteral("INJECT_FAULT")
           << QStringLiteral("CLEAR_FAULT") << QStringLiteral("PHOTO")
           << QStringLiteral("RECORD");
  } else {
    // FlatOnly — 排除控制流命令
    items_ << QStringLiteral("SET") << QStringLiteral("VERIFY")
           << QStringLiteral("WAIT") << QStringLiteral("DELAY")
           << QStringLiteral("ACTION") << QStringLiteral("LOG")
           << QStringLiteral("INJECT_FAULT") << QStringLiteral("CLEAR_FAULT")
           << QStringLiteral("PHOTO") << QStringLiteral("RECORD");
  }
}

QWidget* CommandTypeDelegate::createEditor(QWidget* parent,
                                           const QStyleOptionViewItem& option,
                                           const QModelIndex& index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);

  auto* editor = new QComboBox(parent);
  editor->setEditable(true);
  editor->setInsertPolicy(QComboBox::NoInsert);
  editor->addItems(items_);
  return editor;
}

void CommandTypeDelegate::setEditorData(QWidget* editor,
                                        const QModelIndex& index) const {
  auto* comboBox = qobject_cast<QComboBox*>(editor);
  if (!comboBox) {
    return;
  }

  QString currentText = index.data(Qt::DisplayRole).toString();
  int idx = comboBox->findText(currentText);
  if (idx >= 0) {
    comboBox->setCurrentIndex(idx);
  } else {
    comboBox->setEditText(currentText);
  }
}

void CommandTypeDelegate::setModelData(QWidget* editor,
                                       QAbstractItemModel* model,
                                       const QModelIndex& index) const {
  auto* comboBox = qobject_cast<QComboBox*>(editor);
  if (!comboBox) {
    return;
  }

  QString oldValue = index.data(Qt::DisplayRole).toString();
  QString newValue = comboBox->currentText();
  if (newValue == oldValue) {
    return;
  }

  model->setData(index, newValue, Qt::EditRole);

  // 通过 const_cast 发出信号（Qt 模式：delegate 发出 signal 让 table 响应）
  auto* self = const_cast<CommandTypeDelegate*>(this);
  emit self->commandChanged(oldValue, newValue, index);
}

void CommandTypeDelegate::updateEditorGeometry(
    QWidget* editor, const QStyleOptionViewItem& option,
    const QModelIndex& index) const {
  Q_UNUSED(index);
  editor->setGeometry(option.rect);
}
