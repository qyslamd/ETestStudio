#pragma once

#include <QAbstractButton>
#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>

#include <functional>

class QLabel;

#include "dialogs/OverlayDialog.h"

namespace etest::app {

class SettingsDialog : public OverlayDialog {
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
  QWidget* createProjectPage();
  QWidget* createBackupPage();

  // Fluent 页面头（大标题 + 副标题）
  void addPageHeader(QVBoxLayout* layout, const QString& title,
                     const QString& subtitle);
  QWidget* createAboutPage();

  // VS Code style setting row — returns the right-side layout for controls
  QHBoxLayout* addSettingRow(QWidget* parent,
                              const QString& title,
                              const QString& description);

  // Form row creators — return the control widget for signal wiring
  QSpinBox* addSpinBoxRow(QWidget* parent,
                           const QString& title,
                           const QString& description,
                           const QString& configKey,
                           int min,
                           int max,
                           int step,
                           int defaultVal);
  QAbstractButton* addCheckBoxRow(QWidget* parent,
                                  const QString& title,
                                  const QString& description,
                                  const QString& configKey,
                                  bool defaultVal);
  QComboBox* addComboBoxRow(QWidget* parent,
                             const QString& title,
                             const QString& description,
                             const QString& configKey,
                             const QStringList& items,
                             const QString& defaultVal);
  QPushButton* addButtonRow(QWidget* parent,
                             const QString& title,
                             const QString& description,
                             const QString& text,
                             std::function<void()> callback = nullptr);

  // Settings card container — returns the card widget for adding rows
  QWidget* createSettingsCard(QWidget* parent, const QString& title);

  // Bidirectional binding helpers
  void spinBoxToConfig(const QString& key, QSpinBox* spin);
  void checkBoxToConfig(const QString& key, QAbstractButton* cb);
  void comboBoxToConfig(const QString& key, QComboBox* combo);
  void onConfigChanged(const QString& key);

  QListWidget* list_;
  QStackedWidget* pages_;
  QPushButton* btn_close_;
  QLabel* title_icon_ = nullptr;

  // Maps config key -> control widget for bidirectional sync
  QMap<QString, QSpinBox*> spin_map_;
  QMap<QString, QAbstractButton*> check_map_;
  QMap<QString, QComboBox*> combo_map_;
};

}  // namespace etest::app
