#pragma once

#include <QFrame>
#include <QMap>
#include <QStackedWidget>
#include <QVBoxLayout>

class QLabel;

namespace etest::app {

class HardwareTreeWidget;
class ProtocolManagerWidget;
class SearchWidget;
class GitWidget;
class TestProgramManagerWidget;
class TopologyManagerWidget;

// ── 预定义页面 ID 常量 ──
namespace PageId {
constexpr auto kProjectOverview = "project_overview";
constexpr auto kTopology = "topology";
constexpr auto kHardware = "hardware";
constexpr auto kProtocol = "protocol";
constexpr auto kTestProgram = "testprogram";
constexpr auto kReport = "report";
constexpr auto kSearch = "search";
constexpr auto kGit = "git";
}  // namespace PageId

class SidebarContentWidget : public QFrame {
  Q_OBJECT

 public:
  explicit SidebarContentWidget(QWidget* parent = nullptr);

  void addPage(const QString& id, QWidget* page, const QString& title);
  void switchPage(const QString& id);
  QString currentPageId() const;
  QWidget* pageById(const QString& id) const;
  int pageCount() const;

  void showContent();
  void hideContent();
  bool isContentVisible() const;

  // 类型安全的访问器（过渡期保留，后续逐步替换为 pageById）
  HardwareTreeWidget* hardwareTree() const;
  ProtocolManagerWidget* protocolManager() const;
  SearchWidget* searchWidget() const;
  GitWidget* gitWidget() const;
  TestProgramManagerWidget* testProgramManager() const;
  TopologyManagerWidget* topologyManager() const;

 private:
  void initUi();

  QStackedWidget* stack_;
  QLabel* title_label_;

  QMap<QString, int> id_to_index_;
  QMap<QString, QString> id_to_title_;
  QStringList id_order_;
  QString current_page_id_;

  // 类型安全访问器用指针
  HardwareTreeWidget* hardware_tree_ = nullptr;
  ProtocolManagerWidget* protocol_manager_ = nullptr;
  SearchWidget* search_widget_ = nullptr;
  GitWidget* git_widget_ = nullptr;
  TestProgramManagerWidget* test_program_manager_ = nullptr;
  TopologyManagerWidget* topology_manager_ = nullptr;
};

}  // namespace etest::app
