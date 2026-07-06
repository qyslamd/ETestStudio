#include "TopologyManagerWidget.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>

#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include "AppIconProvider.h"

#include "project/ProjectManager.h"

namespace etest::app {

using namespace etest::core::project;

// ── 工具：方向枚举 → 简短显示文本 ──
static QString directionShortText(const QString& dir) {
  if (dir == QStringLiteral("input"))
    return QStringLiteral("IN");
  if (dir == QStringLiteral("output"))
    return QStringLiteral("OUT");
  if (dir == QStringLiteral("bidirectional"))
    return QStringLiteral("BIDIR");
  return dir;
}

// ── 工具：功能类型 → 显示文本（过长时截断） ──
static QString functionTypeDisplay(const QString& ft) {
  if (ft.isEmpty())
    return QString();
  // CamelCase 转 friendly 名称；太长就截取
  if (ft.length() > 12)
    return ft.left(10) + QStringLiteral("..");
  return ft;
}

TopologyManagerWidget::TopologyManagerWidget(QWidget* parent)
    : QWidget(parent) {
  setupUi();
  initSignals();
}

void TopologyManagerWidget::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // ── 搜索框 ──
  search_edit_ = new QLineEdit(this);
  search_edit_->setPlaceholderText(QStringLiteral("搜索拓扑文件…"));
  search_edit_->setClearButtonEnabled(true);
  search_edit_->setFixedHeight(28);
  search_edit_->setObjectName(QStringLiteral("topologySearchEdit"));
  layout->addWidget(search_edit_);

  // ── 顶部工具栏 ──
  auto* toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("topologyManagerToolbar"));
  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(8, 4, 8, 4);
  toolbar_layout->setSpacing(4);

  new_btn_ = new QPushButton(QStringLiteral("+ 新建"), this);
  new_btn_->setObjectName(QStringLiteral("topologyNewBtn"));

  toolbar_layout->addWidget(new_btn_);
  toolbar_layout->addStretch();

  layout->addWidget(toolbar);

  // ── 拓扑树 ──
  tree_ = new QTreeWidget(this);
  tree_->setHeaderHidden(true);
  tree_->setRootIsDecorated(true);
  tree_->setIndentation(16);
  tree_->setAnimated(false);
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_->header()->setStretchLastSection(true);
  tree_->setExpandsOnDoubleClick(false);

  layout->addWidget(tree_);
}

void TopologyManagerWidget::initSignals() {
  connect(tree_, &QTreeWidget::itemDoubleClicked, this,
          &TopologyManagerWidget::onItemDoubleClicked);
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &TopologyManagerWidget::onCustomContextMenu);

  connect(new_btn_, &QPushButton::clicked, this,
          &TopologyManagerWidget::onNewTopology);

  // 搜索防抖
  search_timer_ = new QTimer(this);
  search_timer_->setSingleShot(true);
  connect(search_edit_, &QLineEdit::textChanged, this,
          [this]() { search_timer_->start(200); });
  connect(search_timer_, &QTimer::timeout, this,
          [this]() { applySearchFilter(search_edit_->text()); });
}

void TopologyManagerWidget::refreshList() {
  tree_->clear();

  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen())
    return;

  auto* project = pm.currentProject();
  if (!project)
    return;

  const QStringList topoFiles = project->scanDirectory(
      QStringLiteral("topology"), QStringLiteral("etopo"));
  for (const QString& absPath : topoFiles) {
    auto* item = new QTreeWidgetItem(tree_);
    item->setText(0, QFileInfo(absPath).completeBaseName());
    item->setData(0, Qt::UserRole, absPath);
    item->setToolTip(0, absPath);
    item->setIcon(0,
                  AppIconProvider::instance().icon(QStringLiteral("topology")));

    // 解析 JSON 加预览子节点
    addPreviewNodes(item, absPath);
  }

  tree_->expandAll();

  // 如果搜索框有内容，重新应用过滤
  if (!search_edit_->text().isEmpty()) {
    applySearchFilter(search_edit_->text());
  }
}

void TopologyManagerWidget::addPreviewNodes(QTreeWidgetItem* fileItem,
                                            const QString& absPath) {
  QFile file(absPath);
  if (!file.open(QIODevice::ReadOnly)) {
    auto* errItem = new QTreeWidgetItem(fileItem);
    errItem->setText(0, QStringLiteral("(解析失败)"));
    errItem->setForeground(0, QColor(0xbb, 0xbb, 0xbb));
    return;
  }

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
  file.close();

  if (err.error != QJsonParseError::NoError || !doc.isObject()) {
    auto* errItem = new QTreeWidgetItem(fileItem);
    errItem->setText(0, QStringLiteral("(解析失败)"));
    errItem->setForeground(0, QColor(0xbb, 0xbb, 0xbb));
    return;
  }

  QJsonObject root = doc.object();

  QIcon uutIcon = AppIconProvider::instance().icon(QStringLiteral("topo_uut"));
  QIcon deviceIcon =
      AppIconProvider::instance().icon(QStringLiteral("topo_device"));

  // ── Products (UUT) ──
  QJsonArray products = root[QStringLiteral("products")].toArray();
  for (const auto& val : products) {
    QJsonObject pObj = val.toObject();
    QString name = pObj[QStringLiteral("name")].toString();
    auto* child = new QTreeWidgetItem(fileItem);
    child->setText(0, QStringLiteral("UUT: %1").arg(name));
    child->setIcon(0, uutIcon);

    // 端口作为孙子节点，保证可搜索
    QJsonArray ports = pObj[QStringLiteral("ports")].toArray();
    for (const auto& pv : ports) {
      QJsonObject portObj = pv.toObject();
      QString portName = portObj[QStringLiteral("name")].toString();
      QString dir =
          directionShortText(portObj[QStringLiteral("direction")].toString());
      QString ft = functionTypeDisplay(
          portObj[QStringLiteral("functionType")].toString());
      QString portText =
          QStringLiteral("  端口: %1 (%2%3)")
              .arg(portName, dir,
                   ft.isEmpty() ? QString() : QStringLiteral(", ") + ft);
      auto* portItem = new QTreeWidgetItem(child);
      portItem->setText(0, portText);
    }
  }

  // ── Devices ──
  QJsonArray devices = root[QStringLiteral("devices")].toArray();
  for (const auto& val : devices) {
    QJsonObject dObj = val.toObject();
    QString name = dObj[QStringLiteral("name")].toString();
    QString devType = dObj[QStringLiteral("deviceType")].toString();
    auto* child = new QTreeWidgetItem(fileItem);
    child->setText(0, QStringLiteral("%1 (%2)").arg(name, devType));
    child->setIcon(0, deviceIcon);

    // 端口
    QJsonArray ports = dObj[QStringLiteral("ports")].toArray();
    for (const auto& pv : ports) {
      QJsonObject portObj = pv.toObject();
      QString portName = portObj[QStringLiteral("name")].toString();
      QString dir =
          directionShortText(portObj[QStringLiteral("direction")].toString());
      QString ft = functionTypeDisplay(
          portObj[QStringLiteral("functionType")].toString());
      QString portText =
          QStringLiteral("  端口: %1 (%2%3)")
              .arg(portName, dir,
                   ft.isEmpty() ? QString() : QStringLiteral(", ") + ft);
      auto* portItem = new QTreeWidgetItem(child);
      portItem->setText(0, portText);
    }
  }

  // ── Monitors ──
  QJsonArray monitors = root[QStringLiteral("monitors")].toArray();
  for (const auto& val : monitors) {
    QJsonObject mObj = val.toObject();
    QString name = mObj[QStringLiteral("name")].toString();
    QString devType = mObj[QStringLiteral("deviceType")].toString();
    int ch = mObj[QStringLiteral("channelCount")].toInt(1);
    auto* child = new QTreeWidgetItem(fileItem);
    child->setText(
        0, QStringLiteral("监视: %1 (%2, %3CH)").arg(name, devType).arg(ch));
  }

  // ── Connections ──
  QJsonArray conns = root[QStringLiteral("connections")].toArray();
  for (const auto& val : conns) {
    QJsonObject cObj = val.toObject();
    QString device = cObj[QStringLiteral("device")].toString();
    QString product = cObj[QStringLiteral("product")].toString();
    QString port = cObj[QStringLiteral("port")].toString();
    auto* child = new QTreeWidgetItem(fileItem);
    child->setText(
        0, QStringLiteral("连接: %1 → %2.%3").arg(device, product, port));
  }
}

void TopologyManagerWidget::applySearchFilter(const QString& keyword) {
  if (keyword.isEmpty()) {
    // 全部显示
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
      auto* topItem = tree_->topLevelItem(i);
      topItem->setHidden(false);
      for (int j = 0; j < topItem->childCount(); ++j) {
        topItem->child(j)->setHidden(false);
      }
    }
    return;
  }

  for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
    auto* fileItem = tree_->topLevelItem(i);
    bool fileMatch = fileItem->text(0).contains(keyword, Qt::CaseInsensitive);
    bool childMatch = false;

    for (int j = 0; j < fileItem->childCount(); ++j) {
      auto* child = fileItem->child(j);
      bool matches = child->text(0).contains(keyword, Qt::CaseInsensitive);
      childMatch |= matches;
      child->setHidden(!matches);

      // 如果有孙子节点，也检查
      for (int k = 0; k < child->childCount(); ++k) {
        auto* grandChild = child->child(k);
        bool gm = grandChild->text(0).contains(keyword, Qt::CaseInsensitive);
        childMatch |= gm;
        grandChild->setHidden(!gm);
      }
    }

    fileItem->setHidden(!fileMatch && !childMatch);
  }
}

void TopologyManagerWidget::onItemDoubleClicked(QTreeWidgetItem* item,
                                                int column) {
  Q_UNUSED(column);
  if (!item)
    return;

  QString filePath = item->data(0, Qt::UserRole).toString();
  if (filePath.isEmpty())
    return;

  // 只在文件节点（顶层节点）上打开编辑器
  if (!item->parent()) {
    emit openFileRequested(filePath);
  }
}

void TopologyManagerWidget::onCustomContextMenu(const QPoint& pos) {
  QTreeWidgetItem* item = tree_->itemAt(pos);
  if (!item)
    return;

  // 只在文件节点（顶层节点）上显示右键菜单
  if (item->parent())
    return;

  QString filePath = item->data(0, Qt::UserRole).toString();
  if (filePath.isEmpty())
    return;

  auto* menu = new QMenu(this);

  auto* openAction = menu->addAction(QStringLiteral("打开"));
  connect(openAction, &QAction::triggered, this,
          [this, filePath]() { emit openFileRequested(filePath); });

  menu->addSeparator();

  auto* renameAction = menu->addAction(QStringLiteral("重命名"));
  connect(renameAction, &QAction::triggered, this,
          [this, filePath]() { renameTopologyFile(filePath); });

  auto* removeAction = menu->addAction(QStringLiteral("删除"));
  connect(removeAction, &QAction::triggered, this,
          [this, filePath]() { removeTopologyFile(filePath); });

  menu->exec(tree_->mapToGlobal(pos));
  menu->deleteLater();
}

void TopologyManagerWidget::onNewTopology() {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先打开项目"));
    return;
  }

  QString rootPath = pm.currentProjectRoot();
  if (rootPath.isEmpty())
    return;

  bool ok;
  QString name = QInputDialog::getText(
      this, QStringLiteral("新建拓扑文件"),
      QStringLiteral("文件名称（不含扩展名）:"), QLineEdit::Normal,
      QStringLiteral("new_topology"), &ok);
  if (!ok || name.trimmed().isEmpty())
    return;

  name = name.trimmed();
  QString fileName = name + QStringLiteral(".etopo");
  QString dirPath = QDir(rootPath).absoluteFilePath(QStringLiteral("topology"));
  QDir topoDir(dirPath);
  if (!topoDir.exists()) {
    topoDir.mkpath(QStringLiteral("."));
  }
  QString filePath = topoDir.absoluteFilePath(fileName);

  if (QFile::exists(filePath)) {
    QMessageBox::warning(this, QStringLiteral("新建失败"),
                         QStringLiteral("文件已存在：%1").arg(filePath));
    return;
  }

  // 创建空的 .etopo 文件
  QJsonObject root;
  root[QStringLiteral("version")] = 1;
  root[QStringLiteral("products")] = QJsonArray();
  root[QStringLiteral("devices")] = QJsonArray();
  root[QStringLiteral("connections")] = QJsonArray();
  root[QStringLiteral("monitors")] = QJsonArray();

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::warning(this, QStringLiteral("新建失败"),
                         QStringLiteral("无法创建文件：%1").arg(filePath));
    return;
  }
  file.write(QJsonDocument(root).toJson());
  file.close();

  refreshList();

  // 打开新建的文件
  emit openFileRequested(filePath);
}

bool TopologyManagerWidget::renameTopologyFile(const QString& oldPath) {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen())
    return false;

  bool ok;
  QString newName = QInputDialog::getText(
      this, QStringLiteral("重命名"), QStringLiteral("新名称（不含扩展名）:"),
      QLineEdit::Normal, QFileInfo(oldPath).completeBaseName(), &ok);
  if (!ok || newName.trimmed().isEmpty())
    return false;

  newName = newName.trimmed();
  QString newFileName = newName + QStringLiteral(".etopo");
  QFileInfo fi(oldPath);
  QString newPath = fi.absolutePath() + QStringLiteral("/") + newFileName;

  if (oldPath == newPath)
    return true;

  if (QFile::exists(newPath)) {
    QMessageBox::warning(this, QStringLiteral("重命名失败"),
                         QStringLiteral("文件已存在：%1").arg(newPath));
    return false;
  }

  // 重命名文件
  if (!QFile::rename(oldPath, newPath)) {
    QMessageBox::warning(this, QStringLiteral("重命名失败"),
                         QStringLiteral("无法重命名文件"));
    return false;
  }

  refreshList();
  return true;
}

bool TopologyManagerWidget::removeTopologyFile(const QString& filePath) {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen())
    return false;

  int ret = QMessageBox::question(
      this, QStringLiteral("确认删除"),
      QStringLiteral("确定要删除拓扑文件吗？\n%1\n\n文件将被删除。")
          .arg(filePath),
      QMessageBox::Yes | QMessageBox::No);
  if (ret != QMessageBox::Yes)
    return false;

  // 删除文件
  QFile::remove(filePath);

  refreshList();
  return true;
}

}  // namespace etest::app
