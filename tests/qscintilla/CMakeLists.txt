add_etest(
    NAME qscintilla_highlight_test
    SOURCES test_qscintilla_highlight.cpp
    LIBS qsci
    QT_MODULES Core Gui Widgets PrintSupport
    INCLUDES ${CMAKE_SOURCE_DIR}/3rdparty/QScintilla-2.11.3/Qt4Qt5
    LABELS THIRD_PARTY
)