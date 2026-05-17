#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QTextEdit;

namespace etest::protocal {

class IcdPropertyPanel : public QWidget {
  Q_OBJECT
 public:
  explicit IcdPropertyPanel(QWidget* parent = nullptr);

 private:
  void initUi();
  template <typename T>
  T* addRow(const QString& label, QWidget* parent);

  QWidget* form_widget_ = nullptr;
};

}  // namespace etest::protocal
