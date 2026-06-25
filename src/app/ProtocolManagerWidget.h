#ifndef ETEST_APP_PROTOCOL_MANAGER_WIDGET_H_
#define ETEST_APP_PROTOCOL_MANAGER_WIDGET_H_

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTreeWidget>
#include <QWidget>

class QPushButton;
class QToolButton;

namespace etest::app {

class ProtocolManagerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ProtocolManagerWidget(QWidget* parent = nullptr);

  void refreshList();

 signals:
  void openFileRequested(const QString& filePath);

 private:
  void setupUi();
  void initSignals();

  void onItemDoubleClicked(QTreeWidgetItem* item, int column);
  void onCustomContextMenu(const QPoint& pos);
  void onNewProtocol();
  void onImportXml();

  bool removeProtocolFile(const QString& filePath);
  bool renameProtocolFile(const QString& oldPath);
  [[deprecated("Use icd::format::deserialize_repository instead")]]
  bool parseEprotoFrames(const QString& filePath,
                         QVector<QPair<int, QString>>& frames);

  QTreeWidget* tree_ = nullptr;
  QPushButton* new_btn_ = nullptr;
  QPushButton* import_btn_ = nullptr;

};

}  // namespace etest::app

#endif  // ETEST_APP_PROTOCOL_MANAGER_WIDGET_H_
