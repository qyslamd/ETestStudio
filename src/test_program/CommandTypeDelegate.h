#ifndef ETEST_PROGRAM_COMMAND_TYPE_DELEGATE_H_
#define ETEST_PROGRAM_COMMAND_TYPE_DELEGATE_H_

#include <QStyledItemDelegate>

// 测试步骤命令列 ComboBox 委托
// Full 模式：全部 14 种命令
// FlatOnly 模式：排除 LOOP / WHILE / IF（用于嵌套子步骤）
class CommandTypeDelegate : public QStyledItemDelegate {
  Q_OBJECT

 public:
  enum Mode { Full, FlatOnly };

  explicit CommandTypeDelegate(Mode mode = Full, QObject* parent = nullptr);

  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override;
  void setEditorData(QWidget* editor, const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
  void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const override;

 signals:
  // 命令变更时发出，便于表格更新列头
  void commandChanged(const QString& oldCmd, const QString& newCmd,
                      const QModelIndex& index);

 private:
  QStringList items_;
};

#endif  // ETEST_PROGRAM_COMMAND_TYPE_DELEGATE_H_
