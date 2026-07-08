#ifndef ETEST_APP_ICD_SIGNAL_SELECTION_H_
#define ETEST_APP_ICD_SIGNAL_SELECTION_H_

#include <QString>
#include <QWidget>

#include "SignalSelectionInterface.h"
#include "dialogs/SignalSelectionDialog.h"
#include "logger/Logger.h"

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace icd {
class Repository;
}  // namespace icd

namespace etest::app {

// ISignalSelection 的 ICD 感知实现。
// 弹出 SignalSelectionDialog 供用户可视化选择信号。
// 无 registry 或 repository 时降级为 DefaultSignalSelection（文本输入）。
class IcdSignalSelection : public ISignalSelection {
 public:
  explicit IcdSignalSelection(etest::core::SignalRegistry* registry,
                              const icd::Repository* repository)
      : registry_(registry), repository_(repository) {}

  QString selectSignal(QWidget* parent) override {
    if (registry_ && repository_) {
      LOG_DEBUG("UUID", "IcdSignalSelection: registry+repository ok -> SignalSelectionDialog");
      SignalSelectionDialog dlg(registry_, repository_, parent);
      if (dlg.exec() == QDialog::Accepted)
        return dlg.selectedUuid();
      return {};
    }
    LOG_DEBUG("UUID", "IcdSignalSelection: registry or repository is NULL, fallback to DefaultSignalSelection");
    // 降级为文本输入
    DefaultSignalSelection fallback;
    return fallback.selectSignal(parent);
  }

 private:
  etest::core::SignalRegistry* registry_;
  const icd::Repository* repository_;
};

}  // namespace etest::app

#endif  // ETEST_APP_ICD_SIGNAL_SELECTION_H_
