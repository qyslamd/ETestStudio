#include <QApplication>
#include <QMessageBox>
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include "SARibbonMainWindow.h"
#include "SARibbonPanel.h"
#include "SARibbonPanelItem.h"
#include "SARibbonToolButton.h"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);

  auto* win = new SARibbonMainWindow();
  win->setWindowTitle(QStringLiteral("SARibbon Demo"));
  win->resize(800, 600);

  // 获取 Ribbon 栏
  auto* ribbon = win->ribbonBar();

  // 添加一个 Category
  auto* cat = ribbon->addCategoryPage(QStringLiteral("主页"));
  auto* panel = cat->addPanel(QStringLiteral("基本"));

  auto* btn = new SARibbonToolButton();
  btn->setText(QStringLiteral("Hello SARibbon"));
  btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
  panel->addWidget(btn, SARibbonPanelItem::Large);

  QObject::connect(btn, &QPushButton::clicked, [win]() {
    QMessageBox::information(
        win, QStringLiteral("SARibbon"),
        QStringLiteral("SARibbon 集成成功！"));
  });

  win->show();
  return app.exec();
}
