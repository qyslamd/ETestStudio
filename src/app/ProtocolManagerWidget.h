#ifndef ETEST_APP_PROTOCOL_MANAGER_WIDGET_H_
#define ETEST_APP_PROTOCOL_MANAGER_WIDGET_H_

#include <QString>
#include <QTreeWidget>
#include <QWidget>

#include <filesystem>
#include <memory>
#include <vector>

#include <icd/file_entry.hpp>
#include <icd/frame.hpp>
#include <icd/loader.hpp>
#include <icd/repository.hpp>

class QLabel;
class QToolButton;
class QPushButton;

namespace etest::app {

// ProtocolManagerWidget — 以 ICDConfig 为中心的项目级协议管理视图。
//
// 显示项目 <root>/protocol/ICDConfig.xml 或 ICDConfig.json 中的所有帧条目，
// 展示并可编辑每帧的配置属性（启用/禁用、类型、字类型、字节序）。
// 双击帧条目 → 打开 ProtocolEditorWidget 进入 ConfigDriven 模式。
class ProtocolManagerWidget : public QWidget {
  Q_OBJECT

 public:
  explicit ProtocolManagerWidget(QWidget* parent = nullptr);
  ~ProtocolManagerWidget() override;

 public slots:
  // 重新加载当前项目的 ICDConfig 并刷新视图
  void refreshList();

  // 暴露 Repository 给上层（MainWindow 用于同步到 SignalRegistry）
  std::shared_ptr<icd::Repository> repository() const { return repo_; }

 signals:
  // 双击帧条目 / 打开配置 时发出，请求主窗口的 EditorManager 打开 ICDConfig
  void openFileRequested(const QString& filePath);
  // 双击具体帧条目时附带帧 id，便于编辑器自动定位
  void openFrameRequested(const QString& configPath, int frameId);

 private slots:
  // 切换帧条目的 Enable 状态（由 QTreeWidget 内置复选框触发）
  void onItemChanged(QTreeWidgetItem* item, int column);
  // 工具栏按钮槽函数
  void onNewIcdConfig();
  void onNewFrame();
  void onImportXml();
  void onRefresh();
  // 上下文菜单动作槽函数
  void onContextMenu(const QPoint& pos);
  void onOpenFrame(QTreeWidgetItem* item);
  void onRenameFrame(QTreeWidgetItem* item);
  void onRemoveFrame(QTreeWidgetItem* item);
  void onToggleEnable(QTreeWidgetItem* item);

 private:
  // 装配 UI 控件与分层布局
  void initUi();
  // 连接信号槽
  void initSignals();
  // refreshList 的实际实现（外面包了 try-catch 防止 filesystem
  // 异常导致应用崩溃）
  void refreshListImpl();

  // 加载项目的 ICDConfig 并填充到 tree_，返回是否成功
  bool loadIcdConfig();
  // 将 icd::LoadResult 渲染到 tree_（清空 + 重建所有条目）
  void populateTree();
  // 计算帧长度（字节数）
  static int calcFrameLength(const icd::Frame& frame);
  // 类型代码 → 显示名（1=CMD, 2=DATA, 4=DATACFG）
  static QString frameTypeDisplayName(icd::FrameType type);
  // 字节序 → 显示名
  static QString byteOrderDisplayName(icd::ByteOrder order);

  // 在项目 protocol/ 目录下查找 ICDConfig.xml 或 ICDConfig.json
  static QString findIcdConfigPath(const QString& projectRoot);
  // 创建空白 ICDConfig.xml
  bool createEmptyIcdConfig(const QString& path);
  // 将 file_entries 写回 ICDConfig 文件
  bool saveIcdConfig();

  // 状态栏文本：共 N 帧，启用 M
  void updateStatusLabel();

  // 树形结构：根 = ICDConfig 容器，叶子 = 帧条目
  QTreeWidget* tree_ = nullptr;
  // ICDConfig 配置项聚合节点（顶层根项）
  QTreeWidgetItem* config_root_item_ = nullptr;
  // 无 ICDConfig 时的占位提示
  QWidget* empty_state_ = nullptr;
  // 工具栏按钮
  QPushButton* new_frame_btn_ = nullptr;
  QPushButton* import_btn_ = nullptr;
  QToolButton* refresh_btn_ = nullptr;
  // 顶部标题（显示当前 ICDConfig 文件名）
  QLabel* config_label_ = nullptr;
  // 底部状态栏（显示帧总数、启用数）
  QLabel* status_label_ = nullptr;

  // 当前加载的 ICDConfig 数据
  std::filesystem::path config_path_;
  icd::Format config_format_ = icd::Format::auto_detect;
  std::shared_ptr<icd::Repository> repo_;
  std::vector<icd::FrameFileInfo> file_entries_;
  // 加载错误信息（部分帧失败时填充）
  QString load_error_;
};

}  // namespace etest::app

#endif  // ETEST_APP_PROTOCOL_MANAGER_WIDGET_H_
