#ifndef ETEST_PROGRAM_SIGNAL_SELECTION_INTERFACE_H_
#define ETEST_PROGRAM_SIGNAL_SELECTION_INTERFACE_H_

#include <QInputDialog>
#include <QString>
#include <QWidget>

namespace etest::app {

// 信号选择抽象接口
// 当 ICD 层建成后，由 ICD 感知的实现替换默认实现
class ISignalSelection {
 public:
  virtual ~ISignalSelection() = default;

  // 打开选择对话框，返回信号 UUID（或名称作为降级）。
  // 取消返回空字符串。
  virtual QString selectSignal(QWidget* parent) = 0;
};

// 默认实现：使用 QInputDialog::getText 手动输入
class DefaultSignalSelection : public ISignalSelection {
 public:
  QString selectSignal(QWidget* parent) override {
    bool ok;
    QString result = QInputDialog::getText(
        parent, QStringLiteral("选择信号"),
        QStringLiteral("信号 UUID 或名称:"), QLineEdit::Normal,
        QString(), &ok);
    return ok ? result.trimmed() : QString();
  }
};

}  // namespace etest::app

#endif  // ETEST_PROGRAM_SIGNAL_SELECTION_INTERFACE_H_
