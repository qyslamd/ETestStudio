#include "LuaEditor.h"
#include <Qsci/qscilexerlua.h>
#include <QFont>
#include <QColor>

LuaEditor::LuaEditor(QWidget *parent) : QsciScintilla(parent) {
    setupEditor();
}

void LuaEditor::setupEditor() {
    QFont font("Consolas", 11);
    setFont(font);
    setMarginsFont(font);

    setLexer(new QsciLexerLua(this));

    setBraceMatching(SloppyBraceMatch);
    setMatchedBraceBackgroundColor(QColor(180, 220, 180));
    setUnmatchedBraceBackgroundColor(QColor(255, 180, 180));

    setAutoIndent(true);
    setIndentationWidth(4);
    setIndentationsUseTabs(false);
    setTabWidth(4);
    setBackspaceUnindents(true);

    setMarginType(0, NumberMargin);
    setMarginLineNumbers(0, true);
    setMarginWidth(0, 40);

    setMarginType(1, SymbolMargin);
    setMarginWidth(1, 20);
    setMarginSensitivity(1, true);

    bpMarker_ = markerDefine(Circle);
    setMarkerBackgroundColor(QColor(200, 50, 50), bpMarker_);

    execMarker_ = markerDefine(RightArrow);
    setMarkerBackgroundColor(QColor(220, 180, 50), execMarker_);

    setMarginType(2, SymbolMargin);
    setFolding(BoxedTreeFoldStyle, 2);
    setFoldMarginColors(QColor(240, 240, 240), QColor(220, 220, 220));

    setCaretLineVisible(true);
    setCaretLineBackgroundColor(QColor(240, 245, 255));

    setEolMode(EolUnix);
    setEolVisibility(false);

    connect(this, SIGNAL(marginClicked(int, int, Qt::KeyboardModifiers)),
            this, SLOT(onMarginClicked(int, int, Qt::KeyboardModifiers)));
}

void LuaEditor::onMarginClicked(int margin, int line, Qt::KeyboardModifiers) {
    if (margin != 1) return;
    setBreakpoint(line, !hasBreakpoint(line));
}

void LuaEditor::setBreakpoint(int line, bool set) {
    if (set == hasBreakpoint(line)) return;
    if (set) {
        markerAdd(line, bpMarker_);
        breakpointLines_.insert(line);
    } else {
        markerDelete(line, bpMarker_);
        breakpointLines_.remove(line);
    }
    emit breakpointToggled(line, set);
}

bool LuaEditor::hasBreakpoint(int line) const {
    return breakpointLines_.contains(line);
}

void LuaEditor::clearAllBreakpoints() {
    for (int line : breakpointLines_)
        markerDelete(line, bpMarker_);
    breakpointLines_.clear();
}

void LuaEditor::setExecutionLine(int line) {
    clearExecutionLine();
    if (line >= 1) {
        int zeroLine = line - 1;
        markerAdd(zeroLine, execMarker_);
        lastExecLine_ = zeroLine;
        setCursorPosition(zeroLine, 0);
    }
}

void LuaEditor::clearExecutionLine() {
    if (lastExecLine_ >= 0) {
        markerDelete(lastExecLine_, execMarker_);
        lastExecLine_ = -1;
    }
}
