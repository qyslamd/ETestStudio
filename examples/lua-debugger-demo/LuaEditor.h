#ifndef LUAEDITOR_H
#define LUAEDITOR_H

#include <Qsci/qsciscintilla.h>
#include <QString>
#include <QSet>

class LuaEditor : public QsciScintilla {
    Q_OBJECT
public:
    explicit LuaEditor(QWidget *parent = nullptr);

    void setBreakpoint(int line, bool set);
    bool hasBreakpoint(int line) const;
    void clearAllBreakpoints();
    void setExecutionLine(int line);
    void clearExecutionLine();
    QSet<int> breakpointLines() const { return breakpointLines_; }

signals:
    void breakpointToggled(int line, bool set);

private slots:
    void onMarginClicked(int margin, int line, Qt::KeyboardModifiers mods);

private:
    void setupEditor();

    int bpMarker_;
    int execMarker_;
    int lastExecLine_ = -1;
    QSet<int> breakpointLines_;
};

#endif
