#include "ComboBoxDelegate.h"

#include <QComboBox>

ComboBoxDelegate::ComboBoxDelegate(const QStringList& items, QObject* parent)
    : QStyledItemDelegate(parent), items_(items) {}

QWidget* ComboBoxDelegate::createEditor(QWidget* parent,
                                         const QStyleOptionViewItem& /*option*/,
                                         const QModelIndex& /*index*/) const {
  auto* editor = new QComboBox(parent);
  editor->addItems(items_);
  return editor;
}

void ComboBoxDelegate::setEditorData(QWidget* editor,
                                      const QModelIndex& index) const {
  auto* combo = qobject_cast<QComboBox*>(editor);
  if (combo)
    combo->setCurrentText(index.data(Qt::DisplayRole).toString());
}

void ComboBoxDelegate::setModelData(QWidget* editor,
                                     QAbstractItemModel* model,
                                     const QModelIndex& index) const {
  auto* combo = qobject_cast<QComboBox*>(editor);
  if (combo)
    model->setData(index, combo->currentText(), Qt::DisplayRole);
}
