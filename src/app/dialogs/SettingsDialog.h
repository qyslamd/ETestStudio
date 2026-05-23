#ifndef ETEST_APP_DIALOGS_SETTINGSDIALOG_H_
#define ETEST_APP_DIALOGS_SETTINGSDIALOG_H_

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeWidget>

namespace etest::app {

class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(QWidget* parent = nullptr);

 private:
  void initUi();
  void initSignals();

  // Page creation
  QWidget* createGeneralPage();
  QWidget* createEditorPage();
  QWidget* createTerminalPage();
  QWidget* createAppearancePage();
  QWidget* createBackupPage();

  // Form row creators — return the control widget for signal wiring
  QSpinBox* addSpinBoxRow(QWidget* parent,
                          const QString& label,
                          const QString& configKey,
                          int min,
                          int max,
                          int step,
                          int defaultVal);
  QCheckBox* addCheckBoxRow(QWidget* parent,
                            const QString& label,
                            const QString& configKey,
                            bool defaultVal);
  QComboBox* addComboBoxRow(QWidget* parent,
                            const QString& label,
                            const QString& configKey,
                            const QStringList& items,
                            const QString& defaultVal);
  QPushButton* addButtonRow(QWidget* parent,
                            const QString& label,
                            const QString& text);

  // Section header label
  QWidget* createSectionHeader(const QString& title);

  // Bidirectional binding helpers
  void spinBoxToConfig(const QString& key, QSpinBox* spin);
  void checkBoxToConfig(const QString& key, QCheckBox* cb);
  void comboBoxToConfig(const QString& key, QComboBox* combo);
  void onConfigChanged(const QString& key);

  QTreeWidget* tree_;
  QStackedWidget* pages_;
  QScrollArea* scroll_area_;

  // Maps config key → control widget for bidirectional sync
  QMap<QString, QSpinBox*> spin_map_;
  QMap<QString, QCheckBox*> check_map_;
  QMap<QString, QComboBox*> combo_map_;
};

}  // namespace etest::app

#endif  // ETEST_APP_DIALOGS_SETTINGSDIALOG_H_
