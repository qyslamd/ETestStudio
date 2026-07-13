#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QWidget>

#include "api/IEditor.h"

class QLabel;
class QListWidget;
class QScrollArea;
class QSplitter;
class QStandardItem;
class QStandardItemModel;
class QTextEdit;
class QTreeView;

namespace etest::app {

class EtlogViewerWidget : public QWidget, public IEditor {
  Q_OBJECT

 public:
  explicit EtlogViewerWidget(const QString& filePath,
                             QWidget* parent = nullptr);

  QString displayName() const override;
  bool isModified() const override;
  bool save() override;
  bool saveAs(const QString& path) override;
  QString filePath() const override;
  QString editorId() const override;
  QWidget* widget() override;
  QString editorType() const override;
  QObject* signalObject() override;
  bool canUndo() const override;
  bool canRedo() const override;
  void undo() override;
  void redo() override;
  void openFile(const QString& filePath) override;

 private:
  void initUi();
  void populateSummary(const QJsonObject& root);
  void populateCaseList(const QJsonObject& root);
  void populateStepTree(const QJsonObject& caseObj);
  void populateStepDetail(const QJsonObject& stepObj);
  void buildStepTreeRecursive(QStandardItem* parent,
                              const QJsonArray& steps,
                              const QString& prefix);
  QString aggregatedStatus(const QJsonArray& steps);
  QList<QStandardItem*> createStepRow(const QJsonObject& step,
                                      const QString& path);
  void showEmptyState(const QString& message);
  void clearDetail();
  void applyThemeColors();
  static QColor colorForStatus(const QString& status);

  QString file_path_;
  QJsonDocument doc_;
  bool has_error_ = false;

  QLabel* title_label_ = nullptr;
  QLabel* summary_label_ = nullptr;
  QLabel* footer_label_ = nullptr;
  QLabel* empty_label_ = nullptr;
  QWidget* content_widget_ = nullptr;

  QListWidget* case_list_ = nullptr;
  QTreeView* step_tree_ = nullptr;
  QStandardItemModel* step_model_ = nullptr;
  QScrollArea* detail_scroll_ = nullptr;
  QLabel* detail_command_ = nullptr;
  QLabel* detail_target_ = nullptr;
  QLabel* detail_status_ = nullptr;
  QLabel* detail_expected_ = nullptr;
  QLabel* detail_actual_ = nullptr;
  QTextEdit* detail_message_ = nullptr;
  QLabel* detail_timestamp_ = nullptr;
  QLabel* detail_elapsed_ = nullptr;
};

}  // namespace etest::app
