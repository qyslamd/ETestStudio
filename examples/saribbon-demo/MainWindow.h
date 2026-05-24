#pragma once

#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QToolButton>
#include <QVector>
#include <QAction>

#include "SARibbonMainWindow.h"

/// 活动栏（左侧 48px 按钮列）
class ActivityBar : public QWidget {
    Q_OBJECT
public:
    explicit ActivityBar(QWidget* parent = nullptr);
    int activeIndex() const { return active_index_; }
    void setActiveIndex(int index);

signals:
    void pageClicked(int index);
    void settingsTriggered();

private:
    void updateButtonStates();
    QVector<QToolButton*> buttons_;
    int active_index_ = 0;
};

/// 主窗口 — 基于 SARibbonMainWindow 复刻 ETest 布局
class MainWindow : public SARibbonMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void setupRibbon();
    void setupCentralLayout();
    void setupStatusBar();

    void onActivityBarClicked(int index);
    void toggleBottomPanel();
    void toggleAuxSidebar();

    // ---------- Ribbon actions ----------
    QAction* act_new_project_;
    QAction* act_open_project_;
    QAction* act_save_;
    QAction* act_save_as_;
    QAction* act_undo_;
    QAction* act_redo_;
    QAction* act_cut_;
    QAction* act_copy_;
    QAction* act_paste_;
    QAction* act_welcome_;
    QAction* act_toggle_output_;
    QAction* act_toggle_aux_;
    QAction* act_about_;

    // ---------- Layout widgets ----------
    ActivityBar* activity_bar_;
    QLabel* sidebar_title_;
    QStackedWidget* sidebar_stack_;
    QWidget* sidebar_content_panel_;
    QSplitter* h_splitter_;
    QSplitter* v_splitter_;
    QTextEdit* central_editor_;
    QTabWidget* bottom_tab_;
    QWidget* bottom_container_;
    QWidget* aux_sidebar_widget_;

    // ---------- State ----------
    int active_activity_index_ = 0;
    int sidebar_width_ = 280;
    int bottom_height_ = 200;
};
