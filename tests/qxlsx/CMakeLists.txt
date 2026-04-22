add_etest(
    NAME test_qxlsx_basic
    SOURCES test_qxlsx_basic.cpp
    LIBS QXlsx::QXlsx
    QT_MODULES Core Gui
    LABELS THIRD_PARTY
)
