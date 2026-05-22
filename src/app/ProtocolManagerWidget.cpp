#include "ProtocolManagerWidget.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <tl/expected.hpp>

#include <icd/error.hpp>
#include <icd/loader.hpp>
#include <icd/repository.hpp>
#include "icd_utility/src/format/json_serializer.hpp"
#include "icd_utility/src/format/xml_parser.hpp"
#include "icd_utility/src/schema/builder.hpp"
#include "icd_utility/src/schema/schema.hpp"

#include "project/ProjectManager.h"

namespace etest::app {

using namespace etest::core::project;

// ── 工具：帧类型 → 显示名 ──
static QString frameTypeDisplayName(const QString& type) {
  if (type == "cmd")       return QStringLiteral("CMD");
  if (type == "data")      return QStringLiteral("DATA");
  if (type == "datacfg")   return QStringLiteral("DATACFG");
  return type.toUpper();
}

ProtocolManagerWidget::ProtocolManagerWidget(QWidget* parent)
    : QWidget(parent) {
  setupUi();
  initSignals();
}

void ProtocolManagerWidget::setupUi() {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // ── 顶部工具栏 ──
  auto* toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("protocolManagerToolbar"));
  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(8, 4, 8, 4);
  toolbar_layout->setSpacing(4);

  new_btn_ = new QPushButton(QStringLiteral("+ 新建"), this);
  new_btn_->setObjectName(QStringLiteral("protocolNewBtn"));
  new_btn_->setFixedHeight(24);

  import_btn_ = new QPushButton(QStringLiteral("导入XML"), this);
  import_btn_->setObjectName(QStringLiteral("protocolImportBtn"));
  import_btn_->setFixedHeight(24);

  toolbar_layout->addWidget(new_btn_);
  toolbar_layout->addWidget(import_btn_);
  toolbar_layout->addStretch();

  layout->addWidget(toolbar);

  // ── 协议树 ──
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

void ProtocolManagerWidget::initSignals() {
  connect(tree_, &QTreeWidget::itemDoubleClicked,
          this, &ProtocolManagerWidget::onItemDoubleClicked);
  connect(tree_, &QTreeWidget::customContextMenuRequested,
          this, &ProtocolManagerWidget::onCustomContextMenu);

  connect(new_btn_, &QPushButton::clicked,
          this, &ProtocolManagerWidget::onNewProtocol);
  connect(import_btn_, &QPushButton::clicked,
          this, &ProtocolManagerWidget::onImportXml);
}

void ProtocolManagerWidget::refreshList() {
  tree_->clear();

  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) return;

  auto* project = pm.currentProject();
  if (!project) return;

  const auto protocols = project->protocols();
  for (const auto& ref : protocols) {
    // 将相对路径转为绝对路径
    QString absPath = QDir(project->rootPath()).absoluteFilePath(ref.filePath);

    auto* item = new QTreeWidgetItem(tree_);
    item->setText(0, ref.name.isEmpty() ? QFileInfo(absPath).fileName()
                                        : ref.name);
    item->setData(0, Qt::UserRole, absPath);
    item->setToolTip(0, absPath);

    // 加载该文件的帧列表
    QVector<QPair<int, QString>> frames;
    if (parseEprotoFrames(absPath, frames)) {
      for (const auto& pair : frames) {
        auto* frameItem = new QTreeWidgetItem(item);
        frameItem->setText(0, pair.second);
        frameItem->setData(0, Qt::UserRole, absPath);
      }
    }
  }

  tree_->expandAll();
}

void ProtocolManagerWidget::onItemDoubleClicked(QTreeWidgetItem* item,
                                                 int column) {
  Q_UNUSED(column);
  if (!item) return;

  QString filePath = item->data(0, Qt::UserRole).toString();
  if (filePath.isEmpty()) return;

  // 如果是文件节点（没有父节点），打开该文件
  if (!item->parent()) {
    emit openFileRequested(filePath);
  }
}

void ProtocolManagerWidget::onCustomContextMenu(const QPoint& pos) {
  QTreeWidgetItem* item = tree_->itemAt(pos);
  if (!item) return;

  // 只在文件节点（顶层节点）上显示右键菜单
  if (item->parent()) return;

  QString filePath = item->data(0, Qt::UserRole).toString();
  if (filePath.isEmpty()) return;

  auto* menu = new QMenu(this);

  auto* openAction = menu->addAction(QStringLiteral("打开"));
  connect(openAction, &QAction::triggered, this, [this, filePath]() {
    emit openFileRequested(filePath);
  });

  menu->addSeparator();

  auto* renameAction = menu->addAction(QStringLiteral("重命名"));
  connect(renameAction, &QAction::triggered, this, [this, filePath]() {
    renameProtocolFile(filePath);
  });

  auto* removeAction = menu->addAction(QStringLiteral("删除"));
  connect(removeAction, &QAction::triggered, this, [this, filePath]() {
    removeProtocolFile(filePath);
  });

  menu->exec(tree_->mapToGlobal(pos));
  menu->deleteLater();
}

void ProtocolManagerWidget::onNewProtocol() {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先打开项目"));
    return;
  }

  QString rootPath = pm.currentProjectRoot();
  if (rootPath.isEmpty()) return;

  bool ok;
  QString name = QInputDialog::getText(
      this, QStringLiteral("新建协议文件"),
      QStringLiteral("文件名称（不含扩展名）:"),
      QLineEdit::Normal, QStringLiteral("new_protocol"), &ok);
  if (!ok || name.trimmed().isEmpty()) return;

  name = name.trimmed();
  QString fileName = name + QStringLiteral(".eproto");
  QString filePath = QDir(rootPath).absoluteFilePath(
      QStringLiteral("protocol") + QStringLiteral("/") + fileName);

  if (QFile::exists(filePath)) {
    QMessageBox::warning(this, QStringLiteral("新建失败"),
                         QStringLiteral("文件已存在：%1").arg(filePath));
    return;
  }

  // 创建空的 .eproto 文件
  QJsonObject root;
  root["version"] = QStringLiteral("1.0");
  root["frames"] = QJsonArray();

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    QMessageBox::warning(this, QStringLiteral("新建失败"),
                         QStringLiteral("无法创建文件：%1").arg(filePath));
    return;
  }
  file.write(QJsonDocument(root).toJson());
  file.close();

  // 注册到项目
  pm.registerProtocolRef(filePath);

  refreshList();

  // 打开新建的文件
  emit openFileRequested(filePath);
}

void ProtocolManagerWidget::onImportXml() {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先打开项目"));
    return;
  }

  QString xmlPath = QFileDialog::getOpenFileName(
      this, QStringLiteral("选择 XML 文件"), QString(),
      QStringLiteral("XML 文件 (*.xml);;所有文件 (*)"));
  if (xmlPath.isEmpty()) return;

  // ── 读取文件头部，检测 XML 类型 ──
  QFile f(xmlPath);
  if (!f.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
                         QStringLiteral("无法读取文件：%1").arg(xmlPath));
    return;
  }
  QByteArray head = f.read(4096);
  f.close();

  // 匹配 <ICDConfig…> 或 <ICDData…>（可能带 xmlns 属性）
  const bool isConfig = head.contains("<ICDConfig");
  const bool isFrame  = head.contains("<ICDData");

  if (!isConfig && !isFrame) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
        QStringLiteral("无法识别的 XML 格式，需要 <ICDConfig> 或 <ICDData> 根元素。"));
    return;
  }

  // ── 确定输出路径 ──
  QFileInfo fi(xmlPath);
  QString eprotoName = fi.completeBaseName() + QStringLiteral(".eproto");
  QString rootPath = pm.currentProjectRoot();
  QString outputPath = QDir(rootPath).absoluteFilePath(
      QStringLiteral("protocol") + QStringLiteral("/") + eprotoName);

  QDir(rootPath).mkpath(QStringLiteral("protocol"));

  if (QFile::exists(outputPath)) {
    int ret = QMessageBox::question(
        this, QStringLiteral("文件已存在"),
        QStringLiteral("协议文件已存在：%1\n\n是否覆盖？").arg(outputPath),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;
  }

  // ── 使用 icd_utility 转换 ──
  namespace fs = std::filesystem;
  const fs::path inPath  = xmlPath.toStdWString();
  const fs::path outPath = outputPath.toStdWString();

  tl::expected<icd::Repository, icd::Error> result;

  if (isConfig) {
    // ICDConfig 多文件配置 → Loader 自动解析所有引用的帧文件
    result = icd::Loader::init(inPath);
  } else {
    // ICDData 单帧文件 → 直接解析
    auto frameResult = icd::format::parse_xml_frame(inPath);
    if (!frameResult) {
      QMessageBox::warning(this, QStringLiteral("导入失败"),
          QStringLiteral("解析 XML 帧失败：%1")
              .arg(QString::fromStdString(frameResult.error().message)));
      return;
    }
    icd::schema::SchemaConfig config;
    config.frames.push_back(std::move(*frameResult));
    result = icd::schema::build_repository(config);
  }

  if (!result) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
        QStringLiteral("构建协议失败：%1")
            .arg(QString::fromStdString(result.error().message)));
    return;
  }

  auto sr = icd::format::serialize_repository(outPath, *result);
  if (!sr) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
        QStringLiteral("写入协议文件失败：%1")
            .arg(QString::fromStdString(sr.error().message)));
    return;
  }

  // ── 注册并刷新 ──
  pm.registerProtocolRef(outputPath);
  refreshList();
  emit openFileRequested(outputPath);
}

bool ProtocolManagerWidget::removeProtocolFile(const QString& filePath) {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) return false;

  auto* project = const_cast<ProjectInfo*>(pm.currentProject());
  if (!project) return false;

  int ret = QMessageBox::question(
      this, QStringLiteral("确认删除"),
      QStringLiteral("确定要删除协议文件吗？\n%1\n\n此操作将从项目中移除引用，文件将被删除。")
          .arg(filePath),
      QMessageBox::Yes | QMessageBox::No);
  if (ret != QMessageBox::Yes) return false;

  // 从 ProjectInfo 中移除引用
  // 使用绝对路径比较，兼容 ref.filePath 可能为绝对路径的情况
  QString rootPath = project->rootPath();
  const auto protocols = project->protocols();
  for (const auto& ref : protocols) {
    QString refAbsPath = QDir(rootPath).absoluteFilePath(ref.filePath);
    if (QDir(refAbsPath) == QDir(filePath)) {
      pm.removeProtocolRef(ref.id);
      break;
    }
  }

  // 删除文件
  QFile::remove(filePath);

  refreshList();
  return true;
}

bool ProtocolManagerWidget::renameProtocolFile(const QString& oldPath) {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) return false;

  auto* project = const_cast<ProjectInfo*>(pm.currentProject());
  if (!project) return false;

  bool ok;
  QString newName = QInputDialog::getText(
      this, QStringLiteral("重命名"),
      QStringLiteral("新名称（不含扩展名）:"),
      QLineEdit::Normal,
      QFileInfo(oldPath).completeBaseName(), &ok);
  if (!ok || newName.trimmed().isEmpty()) return false;

  newName = newName.trimmed();
  QString newFileName = newName + QStringLiteral(".eproto");
  QFileInfo fi(oldPath);
  QString newPath = fi.absolutePath() + QStringLiteral("/") + newFileName;

  if (oldPath == newPath) return true;

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

  // 更新 ProjectInfo 中的引用
  // 使用绝对路径比较，兼容 ref.filePath 可能为绝对路径的情况（如 fromJson 兼容旧数据）
  QString newRelativePath = QDir(project->rootPath()).relativeFilePath(newPath);
  QString rootPath = project->rootPath();
  const auto protocols = project->protocols();
  for (const auto& ref : protocols) {
    QString refAbsPath = QDir(rootPath).absoluteFilePath(ref.filePath);
    if (QDir(refAbsPath) == QDir(oldPath)) {
      project->removeProtocol(ref.id);
      ProtocolRef newRef = ref;
      newRef.filePath = newRelativePath;
      newRef.name = newFileName;
      project->addProtocol(newRef);
      break;
    }
  }

  pm.saveProject();
  refreshList();
  return true;
}

bool ProtocolManagerWidget::parseEprotoFrames(
    const QString& filePath, QVector<QPair<int, QString>>& frames) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) return false;

  QByteArray data = file.readAll();
  file.close();

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(data, &err);
  if (err.error != QJsonParseError::NoError) return false;

  QJsonObject root = doc.object();
  QJsonArray framesArr = root["frames"].toArray();

  for (const auto& val : framesArr) {
    QJsonObject frameObj = val.toObject();
    int id = frameObj["id"].toInt();
    QString name = frameObj["name"].toString();
    QString type = frameTypeDisplayName(frameObj["type"].toString());
    int length = frameObj["length"].toInt();

    // 格式: "221 CMD 4B — name"
    QString label = QStringLiteral("%1 %2 %3B — %4")
                        .arg(id)
                        .arg(type)
                        .arg(length)
                        .arg(name);
    frames.append({id, label});
  }

  return true;
}

}  // namespace etest::app
