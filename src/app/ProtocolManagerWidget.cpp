#include "ProtocolManagerWidget.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <tl/expected.hpp>

#include <algorithm>
#include <filesystem>
#include <functional>

#include <icd/error.hpp>
#include <icd/frame.hpp>
#include <icd/loader.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>

#include "icd_utility/src/format/json_parser.hpp"
#include "icd_utility/src/format/json_serializer.hpp"
#include "icd_utility/src/format/xml_parser.hpp"
#include "icd_utility/src/format/xml_serializer.hpp"
#include "icd_utility/src/schema/builder.hpp"
#include "icd_utility/src/schema/schema.hpp"

#include "AppIconProvider.h"
#include "ThemeManager.h"

#include "project/ProjectManager.h"

namespace etest::app {

using etest::core::project::ProjectManager;

namespace {

// Qt::UserRole + N 存储 frame_id (int) 供右键菜单回调使用
constexpr int kRoleFrameId = Qt::UserRole + 1;
// Qt::UserRole + N 存储 FrameFileInfo 索引
constexpr int kRoleEntryIndex = Qt::UserRole + 2;

}  // namespace

// ── 静态助手函数 ─────────────────────────────────────────────
int ProtocolManagerWidget::calcFrameLength(const icd::Frame& frame) {
  if (frame.roots().empty()) {
    return 0;
  }
  int max_bits = 0;
  std::function<void(const icd::Node&)> update_max = [&](const icd::Node& n) {
    int end = (n.offset() * 8) + n.bit_offset() + n.bit_width();
    if (end > max_bits) {
      max_bits = end;
    }
    for (const auto& c : n.children()) {
      update_max(*c);
    }
  };
  for (const auto& root : frame.roots()) {
    update_max(*root);
  }
  return (max_bits + 7) / 8;
}

QString ProtocolManagerWidget::frameTypeDisplayName(icd::FrameType type) {
  switch (type) {
    case icd::FrameType::cmd:
      return QStringLiteral("CMD");
    case icd::FrameType::data_cmd:
      return QStringLiteral("DATACFG");
    case icd::FrameType::data:
    default:
      return QStringLiteral("DATA");
  }
}

QString ProtocolManagerWidget::byteOrderDisplayName(icd::ByteOrder order) {
  switch (order) {
    case icd::ByteOrder::big_endian:
      return QStringLiteral("BE");
    case icd::ByteOrder::little_endian:
    default:
      return QStringLiteral("LE");
  }
}

QString ProtocolManagerWidget::findIcdConfigPath(const QString& projectRoot) {
  if (projectRoot.isEmpty()) {
    return {};
  }
  QDir protocol_dir(
      QDir(projectRoot).absoluteFilePath(QStringLiteral("protocol")));
  if (!protocol_dir.exists()) {
    return {};
  }
  // 优先 XML，回退 JSON
  const QString xml_path =
      protocol_dir.absoluteFilePath(QStringLiteral("ICDConfig.xml"));
  if (QFile::exists(xml_path)) {
    return xml_path;
  }
  const QString json_path =
      protocol_dir.absoluteFilePath(QStringLiteral("ICDConfig.json"));
  if (QFile::exists(json_path)) {
    return json_path;
  }
  return {};
}

// ── 构造/析构 ────────────────────────────────────────────────
ProtocolManagerWidget::ProtocolManagerWidget(QWidget* parent)
    : QWidget(parent) {
  initUi();
  initSignals();
}

ProtocolManagerWidget::~ProtocolManagerWidget() = default;

// ── UI 装配 ──────────────────────────────────────────────────
void ProtocolManagerWidget::initUi() {
  auto* root_layout = new QVBoxLayout(this);
  root_layout->setContentsMargins(0, 0, 0, 0);
  root_layout->setSpacing(0);

  // ── 顶部配置标题行 ──
  auto* title_bar = new QWidget(this);
  title_bar->setObjectName(QStringLiteral("protocolManagerTitle"));
  title_bar->setFixedHeight(28);
  auto* title_layout = new QHBoxLayout(title_bar);
  title_layout->setContentsMargins(10, 0, 6, 0);
  title_layout->setSpacing(6);

  config_label_ = new QLabel(this);
  config_label_->setObjectName(QStringLiteral("protocolManagerConfigLabel"));
  config_label_->setText(QStringLiteral("ICDConfig（未加载）"));
  title_layout->addWidget(config_label_);
  title_layout->addStretch();

  refresh_btn_ = new QToolButton(this);
  refresh_btn_->setObjectName(QStringLiteral("protocolManagerRefreshBtn"));
  refresh_btn_->setFixedSize(26, 26);
  refresh_btn_->setToolTip(QStringLiteral("刷新"));
  refresh_btn_->setIcon(AppIconProvider::instance().icon("refresh"));
  refresh_btn_->setIconSize(QSize(16, 16));
  title_layout->addWidget(refresh_btn_);

  root_layout->addWidget(title_bar);

  // ── 工具栏（新建帧 / 导入XML） ──
  auto* toolbar = new QWidget(this);
  toolbar->setObjectName(QStringLiteral("protocolManagerToolbar"));
  auto* toolbar_layout = new QHBoxLayout(toolbar);
  toolbar_layout->setContentsMargins(8, 4, 8, 4);
  toolbar_layout->setSpacing(4);

  new_frame_btn_ = new QPushButton(this);
  new_frame_btn_->setText(QStringLiteral("+ 新建帧"));

  import_btn_ = new QPushButton(this);
  import_btn_->setText(QStringLiteral("导入XML"));

  toolbar_layout->addWidget(new_frame_btn_);
  toolbar_layout->addWidget(import_btn_);
  toolbar_layout->addStretch();

  root_layout->addWidget(toolbar);

  // ── 协议树 ──
  tree_ = new QTreeWidget(this);
  tree_->setObjectName(QStringLiteral("protocolManagerTree"));
  tree_->setHeaderHidden(false);
  tree_->setHeaderLabels(QStringList{
      QStringLiteral("帧条目"),
      QStringLiteral("类型"),
      QStringLiteral("长度"),
  });
  tree_->setRootIsDecorated(true);
  tree_->setIndentation(14);
  tree_->setAnimated(false);
  tree_->setContextMenuPolicy(Qt::CustomContextMenu);
  tree_->setUniformRowHeights(true);
  tree_->setAlternatingRowColors(false);
  auto* header = tree_->header();
  header->setStretchLastSection(false);
  header->setSectionResizeMode(0, QHeaderView::Stretch);
  header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
  tree_->setColumnWidth(0, 280);
  root_layout->addWidget(tree_, 1);

  // ── 空状态占位：没有项目或没有 ICDConfig 时显示 ──
  empty_state_ = new QWidget(this);
  empty_state_->setObjectName(QStringLiteral("protocolManagerEmpty"));
  auto* empty_layout = new QVBoxLayout(empty_state_);
  empty_layout->setContentsMargins(16, 32, 16, 16);
  empty_layout->setSpacing(8);
  empty_layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

  auto* empty_text = new QLabel(empty_state_);
  empty_text->setObjectName(QStringLiteral("protocolManagerEmptyText"));
  empty_text->setWordWrap(true);
  empty_text->setAlignment(Qt::AlignCenter);
  empty_layout->addWidget(empty_text);

  auto* create_btn =
      new QPushButton(QStringLiteral("创建 ICDConfig"), empty_state_);
  create_btn->setObjectName(QStringLiteral("protocolManagerCreateBtn"));
  create_btn->setFixedHeight(24);
  empty_layout->addWidget(create_btn, 0, Qt::AlignHCenter);

  empty_state_->hide();
  root_layout->addWidget(empty_state_);

  // 连接 create_btn 的 clicked 到 onNewIcdConfig
  connect(create_btn, &QPushButton::clicked, this,
          &ProtocolManagerWidget::onNewIcdConfig);

  // ── 底部状态栏 ──
  auto* status_bar = new QWidget(this);
  status_bar->setObjectName(QStringLiteral("protocolManagerStatus"));
  status_bar->setFixedHeight(22);
  auto* status_layout = new QHBoxLayout(status_bar);
  status_layout->setContentsMargins(10, 0, 10, 0);
  status_layout->setSpacing(6);

  status_label_ = new QLabel(this);
  status_label_->setObjectName(QStringLiteral("protocolManagerStatusLabel"));
  status_layout->addWidget(status_label_);
  status_layout->addStretch();

  root_layout->addWidget(status_bar);
}

void ProtocolManagerWidget::initSignals() {
  // 双击帧条目打开 ICDConfig
  connect(tree_, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int /*column*/) { onOpenFrame(item); });
  // 复选框状态改变（Enable 切换）
  connect(tree_, &QTreeWidget::itemChanged, this,
          &ProtocolManagerWidget::onItemChanged);
  // 右键菜单
  connect(tree_, &QTreeWidget::customContextMenuRequested, this,
          &ProtocolManagerWidget::onContextMenu);

  // 工具栏
  connect(new_frame_btn_, &QAbstractButton::clicked, this,
          &ProtocolManagerWidget::onNewFrame);
  connect(import_btn_, &QAbstractButton::clicked, this,
          &ProtocolManagerWidget::onImportXml);
  connect(refresh_btn_, &QAbstractButton::clicked, this,
          &ProtocolManagerWidget::onRefresh);

  // 主题切换：刷新图标
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this](bool) {
            refresh_btn_->setIcon(AppIconProvider::instance().icon("refresh"));
          });
}

// ── 公开槽：刷新整个列表 ──────────────────────────────────────
void ProtocolManagerWidget::refreshList() {
  // 防御性 try-catch：refreshList 走 std::filesystem，任何编码/路径异常
  // 都不能让应用崩——只把错误显示出来
  try {
    refreshListImpl();
  } catch (const std::exception& e) {
    config_label_->setText(QStringLiteral("ICDConfig（异常）"));
    tree_->hide();
    empty_state_->show();
    auto* text = empty_state_->findChild<QLabel*>(
        QStringLiteral("protocolManagerEmptyText"));
    if (text) {
      text->setText(QStringLiteral("加载 ICDConfig 抛出异常：\n%1")
                        .arg(QString::fromUtf8(e.what())));
    }
    file_entries_.clear();
    repo_.reset();
    updateStatusLabel();
  } catch (...) {
    config_label_->setText(QStringLiteral("ICDConfig（异常）"));
    tree_->hide();
    empty_state_->show();
    auto* text = empty_state_->findChild<QLabel*>(
        QStringLiteral("protocolManagerEmptyText"));
    if (text) {
      text->setText(QStringLiteral("加载 ICDConfig 抛出未知异常"));
    }
    file_entries_.clear();
    repo_.reset();
    updateStatusLabel();
  }
}

void ProtocolManagerWidget::refreshListImpl() {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    // 没有打开项目：显示空状态
    config_label_->setText(QStringLiteral("ICDConfig（请先打开项目）"));
    tree_->hide();
    empty_state_->show();
    auto* text = empty_state_->findChild<QLabel*>(
        QStringLiteral("protocolManagerEmptyText"));
    if (text) {
      text->setText(QStringLiteral("请先打开一个项目。"));
    }
    file_entries_.clear();
    repo_.reset();
    config_path_.clear();
    updateStatusLabel();
    return;
  }

  const QString root = pm.currentProjectRoot();
  const QString path = findIcdConfigPath(root);
  if (path.isEmpty()) {
    // 项目没有 ICDConfig：显示空状态，提示创建
    config_label_->setText(QStringLiteral("ICDConfig（未找到）"));
    tree_->hide();
    empty_state_->show();
    auto* text = empty_state_->findChild<QLabel*>(
        QStringLiteral("protocolManagerEmptyText"));
    if (text) {
      text->setText(QStringLiteral(
          "此项目尚无 ICDConfig.xml/json。\n点击下方按钮创建一个空白配置。"));
    }
    file_entries_.clear();
    repo_.reset();
    config_path_.clear();
    updateStatusLabel();
    return;
  }

  // 找到了：尝试加载
  config_path_ = std::filesystem::path(path.toStdWString());
  if (!loadIcdConfig()) {
    // 加载失败：显示空状态
    config_label_->setText(QStringLiteral("ICDConfig（加载失败）"));
    tree_->hide();
    empty_state_->show();
    auto* text = empty_state_->findChild<QLabel*>(
        QStringLiteral("protocolManagerEmptyText"));
    if (text) {
      const QString detail =
          load_error_.isEmpty() ? QStringLiteral("未知错误") : load_error_;
      text->setText(
          QStringLiteral("无法加载 ICDConfig。\n%1\n%2").arg(detail, path));
    }
    file_entries_.clear();
    repo_.reset();
    updateStatusLabel();
    return;
  }

  // 加载成功（部分帧可能缺失）：渲染
  tree_->show();
  empty_state_->hide();
  // 标题后追加警告标记
  QString title_suffix;
  if (!load_error_.isEmpty()) {
    title_suffix = QStringLiteral(" ⚠");
  }
  config_label_->setText(QStringLiteral("ICDConfig: %1%2")
                             .arg(QFileInfo(path).fileName(), title_suffix));
  if (!load_error_.isEmpty()) {
    config_label_->setToolTip(load_error_);
  } else {
    config_label_->setToolTip(path);
  }
  populateTree();
  updateStatusLabel();
}

// ── 加载 ICDConfig ───────────────────────────────────────────
// 健壮加载：先解析 config XML 得到 file_entries_，再 best-effort 构造
// Repository。 即使部分帧文件缺失也能展示列表（长度为 "?"），只有关键的 config
// XML 本身无法解析时才返回 false。
bool ProtocolManagerWidget::loadIcdConfig() {
  load_error_ = QString();
  if (config_path_.empty()) {
    load_error_ = QStringLiteral("配置路径为空");
    return false;
  }

  // 1) 解析 config XML（只解析 FileInfo 列表，不加载帧文件）
  tl::expected<icd::schema::SchemaConfig, icd::Error> config_result;
  if (config_format_ == icd::Format::json) {
    // 注：format/json_parser.hpp 没有 parse_json_config 等价物？
    // Loader 才会用 json 路径；这里走通用 Loader。
    auto load_result = icd::Loader::init_with_metadata(config_path_);
    if (load_result) {
      config_format_ = load_result->format;
      repo_ =
          std::make_shared<icd::Repository>(std::move(load_result->repository));
      file_entries_ = std::move(load_result->file_entries);
      return true;
    }
    load_error_ = QString::fromStdString(load_result.error().message);
    return false;
  }

  config_result = icd::format::parse_xml_config(config_path_);
  if (!config_result) {
    load_error_ =
        QStringLiteral("解析 ICDConfig.xml 失败：%1")
            .arg(QString::fromStdString(config_result.error().message));
    return false;
  }
  config_format_ = icd::Format::xml;

  // 2) 转换 SchemaFileEntry → FrameFileInfo
  file_entries_.clear();
  file_entries_.reserve(config_result->files.size());
  for (const auto& sf : config_result->files) {
    icd::FrameFileInfo info;
    info.id = sf.id.value_or(0);
    info.name = sf.logical_name;
    info.description = sf.description;
    info.path = sf.path;
    info.type = sf.type.value_or(icd::FrameType::data);
    info.order = sf.order.value_or(icd::ByteOrder::little_endian);
    info.format =
        (sf.format == icd::Format::auto_detect) ? icd::Format::xml : sf.format;
    info.enable = sf.enable.value_or(true);
    info.word_type = sf.word_type.value_or(0u);
    file_entries_.push_back(std::move(info));
  }

  // 3) best-effort 加载 Repository：逐帧 parse，失败则跳过（不影响列表展示）
  std::vector<icd::schema::SchemaFrameDef> frames;
  frames.reserve(file_entries_.size());
  int loaded = 0;
  int failed = 0;
  QStringList missing_paths;
  for (const auto& entry : file_entries_) {
    // 注意：entry.path 是 std::string (UTF-8)。MSVC 上 path::operator/ 重载
    // 用 ANSI 码页，中文路径会失效并 throw system_error。
    // 必须先转 std::wstring。
    const std::filesystem::path entry_wpath(
        std::wstring(entry.path.begin(), entry.path.end()));
    const std::filesystem::path abs_path =
        config_path_.parent_path() / entry_wpath;
    if (!std::filesystem::exists(abs_path)) {
      failed++;
      missing_paths << QString::fromStdString(entry.path);
      // 仍然追加一个占位 FrameDef（带 id/name，roots 为空）
      icd::schema::SchemaFrameDef placeholder;
      placeholder.id = entry.id;
      placeholder.name = entry.name;
      placeholder.description = entry.description;
      placeholder.type = entry.type;
      placeholder.order = entry.order;
      frames.push_back(std::move(placeholder));
      continue;
    }
    auto fmt = (entry.format == icd::Format::auto_detect) ? icd::Format::xml
                                                          : entry.format;
    tl::expected<icd::schema::SchemaFrameDef, icd::Error> fr;
    if (fmt == icd::Format::json) {
      fr = icd::format::parse_json_frame(abs_path);
    } else {
      fr = icd::format::parse_xml_frame(abs_path);
    }
    if (!fr) {
      failed++;
      continue;
    }
    // 用 entry 的元数据覆盖（保留 config 中的 logical_name 等）
    fr->id = entry.id;
    if (!entry.name.empty()) {
      fr->name = entry.name;
    }
    if (!entry.description.empty()) {
      fr->description = entry.description;
    }
    fr->type = entry.type;
    fr->order = entry.order;
    frames.push_back(std::move(*fr));
    loaded++;
  }

  // 4) 用收集好的 frames 构造 Repository
  icd::schema::SchemaConfig merged;
  merged.files = config_result->files;
  merged.frames = std::move(frames);
  auto repo_result = icd::schema::build_repository(merged);
  if (repo_result) {
    repo_ = std::make_shared<icd::Repository>(std::move(*repo_result));
  } else {
    // Repository 构建失败也容忍：repo_ 留空，长度列显示 "?"
    repo_.reset();
    load_error_ =
        QStringLiteral("部分帧解析失败：%1 个成功，%2 个失败\n%3")
            .arg(loaded)
            .arg(failed)
            .arg(missing_paths.isEmpty()
                     ? QString()
                     : QStringLiteral("缺失：%1").arg(missing_paths.size()));
  }
  return true;
}

// ── 写入 ICDConfig ───────────────────────────────────────────
bool ProtocolManagerWidget::saveIcdConfig() {
  if (config_path_.empty()) {
    return false;
  }
  tl::expected<void, icd::Error> result;
  if (config_format_ == icd::Format::json) {
    result = icd::format::serialize_json_config(config_path_, file_entries_);
  } else {
    result = icd::format::serialize_xml_config(config_path_, file_entries_);
  }
  return result.has_value();
}

// ── 渲染 LoadResult 到树 ─────────────────────────────────────
void ProtocolManagerWidget::populateTree() {
  // 屏蔽 itemChanged 避免重建过程中触发
  tree_->blockSignals(true);
  tree_->clear();
  config_root_item_ = nullptr;

  if (!repo_ || file_entries_.empty()) {
    tree_->blockSignals(false);
    return;
  }

  // 根节点：ICDConfig 配置项聚合
  config_root_item_ = new QTreeWidgetItem(tree_);
  config_root_item_->setText(0, QStringLiteral("ICDConfig"));
  config_root_item_->setData(0, Qt::UserRole,
                             QString::fromStdString(config_path_.string()));
  config_root_item_->setExpanded(true);
  QFont f = config_root_item_->font(0);
  f.setBold(true);
  config_root_item_->setFont(0, f);
  // 根节点禁用复选框
  config_root_item_->setFlags(config_root_item_->flags() &
                              ~Qt::ItemIsUserCheckable);

  // 按 file_entries_ 顺序生成子节点
  for (size_t i = 0; i < file_entries_.size(); ++i) {
    const auto& entry = file_entries_[i];

    // 找对应的 Frame 来计算长度（repo 可能为空 -> 显示 "?"）
    QString length_str = QStringLiteral("?");
    if (repo_) {
      const icd::Frame* frame = repo_->find(entry.id);
      if (frame != nullptr) {
        length_str = QStringLiteral("%1B").arg(calcFrameLength(*frame));
      } else {
        length_str = QStringLiteral("缺失");
      }
    }

    auto* item = new QTreeWidgetItem(config_root_item_);
    QString label =
        QString::fromStdString(entry.name.empty() ? entry.path : entry.name);
    item->setText(0, label);
    item->setToolTip(0, QString::fromStdString(entry.path));
    item->setData(0, kRoleFrameId, entry.id);
    item->setData(0, kRoleEntryIndex, static_cast<int>(i));
    item->setText(1, frameTypeDisplayName(entry.type));
    item->setText(2, length_str);
    // 复选框：Enable
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, entry.enable ? Qt::Checked : Qt::Unchecked);
    // 禁用帧视觉降级
    if (!entry.enable) {
      for (int c = 0; c < 3; ++c) {
        QFont df = item->font(c);
        df.setStrikeOut(true);
        item->setFont(c, df);
        item->setForeground(c, QBrush(QColor(120, 120, 120)));
      }
    }
    // 第 1/2 列不可编辑
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  }

  tree_->blockSignals(false);
}

// ── Enable 切换槽 ───────────────────────────────────────────
void ProtocolManagerWidget::onItemChanged(QTreeWidgetItem* item, int column) {
  if (!item || column != 0) {
    return;
  }
  if (!config_root_item_ || item == config_root_item_) {
    return;
  }
  // 取出对应 entry
  int idx = item->data(0, kRoleEntryIndex).toInt();
  if (idx < 0 || idx >= static_cast<int>(file_entries_.size())) {
    return;
  }
  bool new_enable = (item->checkState(0) == Qt::Checked);
  if (file_entries_[idx].enable == new_enable) {
    return;  // 未变化
  }
  file_entries_[idx].enable = new_enable;
  // 视觉同步
  for (int c = 0; c < 3; ++c) {
    QFont f = item->font(c);
    f.setStrikeOut(!new_enable);
    item->setFont(c, f);
    item->setForeground(
        c, QBrush(new_enable ? QColor(Qt::white) : QColor(120, 120, 120)));
  }
  // 写回
  if (!saveIcdConfig()) {
    QMessageBox::warning(this, QStringLiteral("保存失败"),
                         QStringLiteral("无法写回 ICDConfig 文件。"));
    // 回滚 UI
    item->setCheckState(
        0, file_entries_[idx].enable ? Qt::Checked : Qt::Unchecked);
  } else {
    updateStatusLabel();
  }
}

// ── 状态栏 ───────────────────────────────────────────────────
void ProtocolManagerWidget::updateStatusLabel() {
  if (file_entries_.empty()) {
    status_label_->setText(QString());
    return;
  }
  int enabled = 0;
  for (const auto& e : file_entries_) {
    if (e.enable) {
      ++enabled;
    }
  }
  status_label_->setText(QStringLiteral("共 %1 帧，启用 %2")
                             .arg(file_entries_.size())
                             .arg(enabled));
}

// ── 创建空白 ICDConfig ───────────────────────────────────────
void ProtocolManagerWidget::onNewIcdConfig() {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先打开项目。"));
    return;
  }
  const QString root = pm.currentProjectRoot();
  if (root.isEmpty()) {
    return;
  }
  // protocol 目录可能还不存在
  QDir protocol_dir(QDir(root).absoluteFilePath(QStringLiteral("protocol")));
  if (!protocol_dir.exists() &&
      !QDir(root).mkpath(QStringLiteral("protocol"))) {
    QMessageBox::warning(this, QStringLiteral("创建失败"),
                         QStringLiteral("无法创建 protocol 目录。"));
    return;
  }
  const QString target =
      protocol_dir.absoluteFilePath(QStringLiteral("ICDConfig.xml"));
  if (QFile::exists(target)) {
    QMessageBox::information(
        this, QStringLiteral("已存在"),
        QStringLiteral("ICDConfig.xml 已存在：%1").arg(target));
    refreshList();
    return;
  }
  if (!createEmptyIcdConfig(target)) {
    QMessageBox::warning(this, QStringLiteral("创建失败"),
                         QStringLiteral("无法写入 %1").arg(target));
    return;
  }
  refreshList();
  emit openFileRequested(target);
}

bool ProtocolManagerWidget::createEmptyIcdConfig(const QString& path) {
  // 写一个 XML 根壳
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  const QByteArray content =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<ICDConfig>\n"
      "  <Files>\n"
      "  </Files>\n"
      "</ICDConfig>\n";
  f.write(content);
  f.close();
  return true;
}

// ── 工具栏槽：新建帧 / 导入XML / 刷新 ──────────────────────
void ProtocolManagerWidget::onNewFrame() {
  if (config_path_.empty() || !repo_) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先创建并加载 ICDConfig。"));
    return;
  }

  // 选择帧类型
  QStringList types{
      QStringLiteral("CMD"),
      QStringLiteral("DATA"),
      QStringLiteral("DATACFG"),
  };
  bool ok = false;
  const QString type_str =
      QInputDialog::getItem(this, QStringLiteral("新建帧"),
                            QStringLiteral("帧类型："), types, 1, false, &ok);
  if (!ok) {
    return;
  }

  bool ok2 = false;
  const QString name = QInputDialog::getText(
      this, QStringLiteral("新建帧"), QStringLiteral("帧名称："),
      QLineEdit::Normal, QStringLiteral("NewFrame"), &ok2);
  if (!ok2 || name.trimmed().isEmpty()) {
    return;
  }

  // 找下一个未占用的 id
  int new_id = 0;
  for (const auto& e : file_entries_) {
    if (e.id >= new_id) {
      new_id = e.id + 1;
    }
  }

  icd::FrameType ft = icd::FrameType::data;
  if (type_str == QStringLiteral("CMD")) {
    ft = icd::FrameType::cmd;
  } else if (type_str == QStringLiteral("DATACFG")) {
    ft = icd::FrameType::data_cmd;
  }

  // 构造相对路径：sanitize 名称后加 .xml
  QString safe_name = name.trimmed();
  // 简单 sanitize：替换 Windows 非法文件名字符
  for (QChar& ch : safe_name) {
    if (ch == QLatin1Char('/') || ch == QLatin1Char('\\') ||
        ch == QLatin1Char(':') || ch == QLatin1Char('*') ||
        ch == QLatin1Char('?') || ch == QLatin1Char('"') ||
        ch == QLatin1Char('<') || ch == QLatin1Char('>') ||
        ch == QLatin1Char('|')) {
      ch = QLatin1Char('_');
    }
  }
  // 新建帧的 rel 路径：name 是用户输入，可能含中文，统一用 wstring
  const std::filesystem::path rel_path(safe_name.toStdWString() + L".xml");
  const std::filesystem::path abs_path = config_path_.parent_path() / rel_path;

  // 创建空 Frame，写入 frame file
  icd::Frame frame(new_id, safe_name.toStdString(), "", ft,
                   icd::ByteOrder::little_endian);
  auto ser = icd::format::serialize_xml_frame_file(abs_path, frame);
  if (!ser) {
    QMessageBox::warning(this, QStringLiteral("新建失败"),
                         QStringLiteral("无法写入帧文件：%1\n%2")
                             .arg(QString::fromStdString(abs_path.string()))
                             .arg(QString::fromStdString(ser.error().message)));
    return;
  }

  // 更新 file_entries_
  icd::FrameFileInfo info;
  info.id = new_id;
  info.name = safe_name.toStdString();
  info.description = "";
  info.path = rel_path.string();
  info.type = ft;
  info.order = icd::ByteOrder::little_endian;
  info.format = icd::Format::xml;
  info.enable = true;
  info.word_type = 0;
  file_entries_.push_back(std::move(info));

  // 写回 ICDConfig
  if (!saveIcdConfig()) {
    QMessageBox::warning(this, QStringLiteral("保存失败"),
                         QStringLiteral("无法更新 ICDConfig。"));
    return;
  }

  // 重新加载并刷新（保持 repo 和 file_entries_ 同步）
  loadIcdConfig();
  populateTree();
  updateStatusLabel();
}

void ProtocolManagerWidget::onImportXml() {
  auto& pm = ProjectManager::instance();
  if (!pm.isProjectOpen()) {
    QMessageBox::information(this, QStringLiteral("提示"),
                             QStringLiteral("请先打开项目。"));
    return;
  }
  const QString xml_path = QFileDialog::getOpenFileName(
      this, QStringLiteral("选择 XML 文件"), QString(),
      QStringLiteral("XML 文件 (*.xml);;所有文件 (*)"));
  if (xml_path.isEmpty()) {
    return;
  }

  // 读前 4096 字节检测根标签
  QFile f(xml_path);
  if (!f.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
                         QStringLiteral("无法读取：%1").arg(xml_path));
    return;
  }
  const QByteArray head = f.read(4096);
  f.close();
  const bool is_config = head.contains("<ICDConfig");
  const bool is_frame = head.contains("<ICDData");

  if (!is_config && !is_frame) {
    QMessageBox::warning(
        this, QStringLiteral("导入失败"),
        QStringLiteral(
            "无法识别的 XML 格式，需要 <ICDConfig> 或 <ICDData> 根元素。"));
    return;
  }

  // 如果没有 ICDConfig，单帧导入后直接创建一个空配置并写回
  if (config_path_.empty() || !repo_) {
    QMessageBox::information(
        this, QStringLiteral("提示"),
        QStringLiteral("请先创建 ICDConfig，然后再导入帧。"));
    return;
  }

  namespace fs = std::filesystem;
  const fs::path in_path(xml_path.toStdWString());

  // 单帧导入：把 frame 文件复制到 protocol 目录，并加入 file_entries_
  if (is_frame) {
    auto frame_result = icd::format::parse_xml_frame(in_path);
    if (!frame_result) {
      QMessageBox::warning(
          this, QStringLiteral("导入失败"),
          QStringLiteral("解析 XML 帧失败：%1")
              .arg(QString::fromStdString(frame_result.error().message)));
      return;
    }
    // 用 frame 名作为 rel 路径
    const QFileInfo fi(xml_path);
    const QString rel_qstr = fi.fileName();
    // 转 wstring 避免 MSVC path::operator/ 的 ANSI 编码问题
    const std::filesystem::path abs_path =
        config_path_.parent_path() / rel_qstr.toStdWString();

    // 目标文件已存在：覆盖前先删除
    if (std::filesystem::exists(abs_path)) {
      std::error_code ec;
      std::filesystem::remove(abs_path, ec);
    }
    // 复制源文件到 protocol 目录（保持原始 XML 内容）
    if (!QFile::copy(xml_path, QString::fromStdString(abs_path.string()))) {
      QMessageBox::warning(this, QStringLiteral("导入失败"),
                           QStringLiteral("复制失败：%1")
                               .arg(QString::fromStdString(abs_path.string())));
      return;
    }

    // 计算新 id
    int new_id = 0;
    for (const auto& e : file_entries_) {
      if (e.id >= new_id) {
        new_id = e.id + 1;
      }
    }

    icd::FrameFileInfo info;
    info.id = new_id;
    info.name = frame_result->name;
    info.description = frame_result->description;
    info.path = rel_qstr.toStdString();
    info.type = frame_result->type;
    info.order = frame_result->order;
    info.format = icd::Format::xml;
    info.enable = true;
    info.word_type = 0;
    file_entries_.push_back(std::move(info));

    if (!saveIcdConfig()) {
      QMessageBox::warning(this, QStringLiteral("保存失败"),
                           QStringLiteral("无法更新 ICDConfig。"));
      return;
    }
    loadIcdConfig();
    populateTree();
    updateStatusLabel();
    return;
  }

  // ICDConfig 导入：复制到当前 ICDConfig 路径并刷新
  QFileInfo fi(xml_path);
  const QString target =
      QDir(QString::fromStdString(config_path_.parent_path().string()))
          .absoluteFilePath(fi.fileName());
  if (QFile::exists(target)) {
    int ret = QMessageBox::question(
        this, QStringLiteral("文件已存在"),
        QStringLiteral("%1 已存在，是否覆盖？").arg(target),
        QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return;
    }
  }
  if (!QFile::copy(xml_path, target)) {
    QMessageBox::warning(this, QStringLiteral("导入失败"),
                         QStringLiteral("复制失败：%1").arg(target));
    return;
  }
  config_path_ = fs::path(target.toStdWString());
  loadIcdConfig();
  populateTree();
  updateStatusLabel();
  emit openFileRequested(target);
}

void ProtocolManagerWidget::onRefresh() {
  if (!config_path_.empty()) {
    loadIcdConfig();
  }
  populateTree();
  updateStatusLabel();
}

// ── 右键菜单 ────────────────────────────────────────────────
void ProtocolManagerWidget::onContextMenu(const QPoint& pos) {
  QTreeWidgetItem* item = tree_->itemAt(pos);
  if (!item) {
    return;
  }
  // 根节点或非帧条目：不显示菜单
  if (!item->parent() || item == config_root_item_) {
    return;
  }

  auto* menu = new QMenu(this);

  auto* open_act = menu->addAction(QStringLiteral("打开编辑"));
  connect(open_act, &QAction::triggered, this,
          [this, item]() { onOpenFrame(item); });

  auto* toggle_act = menu->addAction(QStringLiteral("切换启用"));
  connect(toggle_act, &QAction::triggered, this,
          [this, item]() { onToggleEnable(item); });

  menu->addSeparator();

  auto* rename_act = menu->addAction(QStringLiteral("重命名"));
  connect(rename_act, &QAction::triggered, this,
          [this, item]() { onRenameFrame(item); });

  auto* remove_act = menu->addAction(QStringLiteral("删除"));
  connect(remove_act, &QAction::triggered, this,
          [this, item]() { onRemoveFrame(item); });

  menu->exec(tree_->mapToGlobal(pos));
  menu->deleteLater();
}

void ProtocolManagerWidget::onOpenFrame(QTreeWidgetItem* item) {
  if (!item || config_path_.empty()) {
    return;
  }
  // 根节点：直接打开 ICDConfig
  if (item == config_root_item_) {
    emit openFileRequested(QString::fromStdString(config_path_.string()));
    return;
  }
  // 帧节点：发出 openFrameRequested 让主窗口能定位到具体帧
  const QString config_str = QString::fromStdString(config_path_.string());
  const int fid = item->data(0, kRoleFrameId).toInt();
  emit openFrameRequested(config_str, fid);
  // 同时也打开文件（向后兼容）
  emit openFileRequested(config_str);
}

void ProtocolManagerWidget::onRenameFrame(QTreeWidgetItem* item) {
  if (!item) {
    return;
  }
  int idx = item->data(0, kRoleEntryIndex).toInt();
  if (idx < 0 || idx >= static_cast<int>(file_entries_.size())) {
    return;
  }
  bool ok = false;
  const QString new_name = QInputDialog::getText(
      this, QStringLiteral("重命名帧"), QStringLiteral("新名称："),
      QLineEdit::Normal, QString::fromStdString(file_entries_[idx].name), &ok);
  if (!ok || new_name.trimmed().isEmpty()) {
    return;
  }
  file_entries_[idx].name = new_name.trimmed().toStdString();
  if (!saveIcdConfig()) {
    QMessageBox::warning(this, QStringLiteral("保存失败"),
                         QStringLiteral("无法写回 ICDConfig。"));
    return;
  }
  item->setText(0, new_name.trimmed());
  // 注：repo 中 Frame::name 是常量，加载自文件后不变。
  // 这里只更新 ICDConfig 中的显示名，下次 ProtocolEditorWidget 重新打开时
  // 会通过 file_entries 的 logical_name 覆盖 Frame.name（见 Loader 逻辑）。
}

void ProtocolManagerWidget::onRemoveFrame(QTreeWidgetItem* item) {
  if (!item) {
    return;
  }
  int idx = item->data(0, kRoleEntryIndex).toInt();
  if (idx < 0 || idx >= static_cast<int>(file_entries_.size())) {
    return;
  }
  const auto& entry = file_entries_[idx];
  int ret = QMessageBox::question(
      this, QStringLiteral("确认删除"),
      QStringLiteral("确定要删除帧「%1」吗？\n"
                     "该操作会删除关联的帧文件并从 ICDConfig 中移除。")
          .arg(QString::fromStdString(entry.name)),
      QMessageBox::Yes | QMessageBox::No);
  if (ret != QMessageBox::Yes) {
    return;
  }

  // 删除帧文件（仅当路径有效）
  if (!entry.path.empty() && !config_path_.empty()) {
    // 注意：entry.path 是 std::string (UTF-8)，必须转 wstring
    // 否则 MSVC 上 path::operator/ 用 ANSI 码页会触发异常
    const std::filesystem::path entry_wpath(
        std::wstring(entry.path.begin(), entry.path.end()));
    const std::filesystem::path abs_path =
        config_path_.parent_path() / entry_wpath;
    std::error_code ec;
    std::filesystem::remove(abs_path, ec);
  }
  // 从 repo 中移除
  if (repo_) {
    repo_->remove_frame(entry.id);
  }
  // 从 file_entries_ 中移除
  file_entries_.erase(file_entries_.begin() + idx);
  // 写回 ICDConfig
  if (!saveIcdConfig()) {
    QMessageBox::warning(this, QStringLiteral("保存失败"),
                         QStringLiteral("无法写回 ICDConfig。"));
  }
  // 重建树（条目索引已失效）
  populateTree();
  updateStatusLabel();
}

void ProtocolManagerWidget::onToggleEnable(QTreeWidgetItem* item) {
  if (!item) {
    return;
  }
  // 切换复选框状态
  const bool was_checked = (item->checkState(0) == Qt::Checked);
  item->setCheckState(0, was_checked ? Qt::Unchecked : Qt::Checked);
  // onItemChanged 会负责写回
}

}  // namespace etest::app
