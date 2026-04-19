#include "MainWindow.h"
#include <Qsci/qscilexercpp.h>
#include <QHeaderView>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include "spdlog/spdlog.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), dock_manager_(nullptr) {
  initUi();
  initSignals();
  spdlog::info("MainWindow(无边框模式)初始化完成");
}

MainWindow::~MainWindow() {
  // QADS会自动管理子控件生命周期，无需手动释放
}

void MainWindow::initUi() {
  setWindowTitle("QADS 停靠布局示例");
  resize(1200, 800);

  // 创建停靠管理器作为中心部件
  // QADS 3.8.3版本无边框功能通过Qt窗口属性自动处理拖拽
  dock_manager_ = new ads::CDockManager(this);
  dock_manager_->setStyleSheet("");

  // ==================== 1. 左侧停靠：项目视图 ====================
  ads::CDockWidget* project_dock = new ads::CDockWidget("项目视图");
  project_dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);
  project_dock->setFeature(ads::CDockWidget::DockWidgetClosable, true);

  QTreeWidget* project_tree = new QTreeWidget();
  project_tree->setHeaderLabel("项目文件");
  QTreeWidgetItem* root = new QTreeWidgetItem(project_tree, {"测试项目"});
  new QTreeWidgetItem(root, {"main.cpp"});
  new QTreeWidgetItem(root, {"test.proto"});
  new QTreeWidgetItem(root, {"test.prot"});
  new QTreeWidgetItem(root, {"test_config.json"});
  root->setExpanded(true);
  project_dock->setWidget(project_tree);

  // 添加到左侧停靠区域
  dock_manager_->addDockWidget(ads::LeftDockWidgetArea, project_dock);

  // ==================== 2. 右侧停靠：属性编辑器 ====================
  ads::CDockWidget* property_dock = new ads::CDockWidget("属性编辑器");
  property_dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);

  QTableWidget* property_table = new QTableWidget();
  property_table->setColumnCount(2);
  property_table->setHorizontalHeaderLabels({"属性", "值"});
  property_table->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  property_table->setRowCount(5);
  property_table->setItem(0, 0, new QTableWidgetItem("窗口宽度"));
  property_table->setItem(0, 1, new QTableWidgetItem("1200"));
  property_table->setItem(1, 0, new QTableWidgetItem("窗口高度"));
  property_table->setItem(1, 1, new QTableWidgetItem("800"));
  property_table->setItem(2, 0, new QTableWidgetItem("主题"));
  property_table->setItem(2, 1, new QTableWidgetItem("浅色"));
  property_table->setItem(3, 0, new QTableWidgetItem("语言"));
  property_table->setItem(3, 1, new QTableWidgetItem("中文"));
  property_table->setItem(4, 0, new QTableWidgetItem("版本"));
  property_table->setItem(4, 1, new QTableWidgetItem("1.0.0"));
  property_dock->setWidget(property_table);

  // 添加到右侧停靠区域
  dock_manager_->addDockWidget(ads::RightDockWidgetArea, property_dock);

  // ==================== 3. 底部停靠：输出面板 ====================
  ads::CDockWidget* output_dock = new ads::CDockWidget("输出面板");
  output_dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);

  QTextEdit* output_edit = new QTextEdit();
  output_edit->setReadOnly(true);
  output_edit->append("[INFO] 程序启动成功");
  output_edit->append("[INFO] QADS停靠框架初始化完成");
  output_edit->append("[INFO] 所有模块加载正常");
  output_dock->setWidget(output_edit);

  // 添加到底部停靠区域
  dock_manager_->addDockWidget(ads::BottomDockWidgetArea, output_dock);

  // ==================== 4. 中心停靠：QScintilla编辑器测试 ====================
  ads::CDockWidget* editor_dock = new ads::CDockWidget("代码编辑器");
  editor_dock->setFeature(ads::CDockWidget::DockWidgetFloatable, true);

  m_editor = new QsciScintilla();
  // 显示行号
  m_editor->setMarginType(0, QsciScintilla::NumberMargin);
  m_editor->setMarginWidth(0, "0000");
  // 开启C++语法高亮，设置经典VS风格配色
  QsciLexerCPP* lexer = new QsciLexerCPP(m_editor);
  lexer->setColor(QColor(0, 0, 255), QsciLexerCPP::Keyword);              // 关键字蓝色
  lexer->setColor(QColor(0, 128, 0), QsciLexerCPP::Comment);              // 注释绿色
  lexer->setColor(QColor(163, 21, 21), QsciLexerCPP::DoubleQuotedString); // 双引号字符串暗红色
  lexer->setColor(QColor(163, 21, 21), QsciLexerCPP::SingleQuotedString); // 单引号字符串暗红色
  lexer->setColor(QColor(0, 0, 255), QsciLexerCPP::PreProcessor);         // 预处理指令蓝色
  lexer->setColor(QColor(43, 145, 175), QsciLexerCPP::Number);            // 数字蓝绿色
  lexer->setColor(QColor(128, 128, 128), QsciLexerCPP::Operator);         // 运算符灰色
  m_editor->setLexer(lexer);
  // 设置测试代码
  m_editor->setText(R"(#include <iostream>

int main() {
    std::cout << "Hello QScintilla!" << std::endl;
    return 0;
})");
  editor_dock->setWidget(m_editor);

  // 添加到中心停靠区域
  dock_manager_->addDockWidget(ads::CenterDockWidgetArea, editor_dock);
}

void MainWindow::initSignals() {
  // 目前暂无需信号槽连接，后续功能扩展可在这里添加
}
