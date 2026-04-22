add_etest(
    NAME test_libharu_basic
    SOURCES test_libharu_basic.cpp
    LIBS hpdf zlibstatic
    INCLUDES 
        ${CMAKE_SOURCE_DIR}/3rdparty/libharu-2.4.6/include
        ${CMAKE_BINARY_DIR}/3rdparty/libharu-2.4.6/include
    LABELS THIRD_PARTY
)
