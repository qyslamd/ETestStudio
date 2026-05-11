#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "LuaDebugger.h"
#include "LuaEditor.h"

QT_BEGIN_NAMESPACE
class QToolBar;
class QAction;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidget;
class QTextEdit;
class QSplitter;
class QTabWidget;
class QLabel;
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onRun();
    void onPause();
    void onStop();
    void onStepInto();
    void onStepOver();
    void onStepOut();
    void onReset();
    void onBreakpointToggled(int line, bool set);
    void onLineChanged(int line);
    void onPaused(const DebugSnapshot &snapshot);
    void onFinished();
    void onError(const QString &message);
    void onOutput(const QString &text);
    void onStackFrameClicked(int index);

private:
    void setupUi();
    void setupToolbar();
    void setupEditorPanel();
    void setupDebugPanels();
    void setupConnections();
    void updateButtonStates();
    void setDefaultScript();

    LuaDebugger *debugger_;
    LuaEditor *editor_;

    QToolBar *toolbar_;
    QAction *actRun_;
    QAction *actPause_;
    QAction *actStop_;
    QAction *actStepInto_;
    QAction *actStepOver_;
    QAction *actStepOut_;
    QAction *actReset_;

    QSplitter *mainSplitter_;
    QTabWidget *debugTabs_;
    QTreeWidget *varTree_;
    QListWidget *callStack_;
    QTextEdit *output_;
    QLabel *statusLabel_;
};

#endif
