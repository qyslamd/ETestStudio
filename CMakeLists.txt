cmake_minimum_required(VERSION 3.15)
project(qt_gradient_demo
    VERSION 1.0.0
    DESCRIPTION "Cpp demo projects"
    LANGUAGES CXX)

#=======================================
# CMake全局变量
set(CMAKE_CXX_STANDARD 17)  # C++ 标准，影响全局
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    # 设置Unicode字符集
    add_compile_definitions(
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
endif()

#=======================================
# 添加可执行文件
set(TARGET_NAME "qt_gradient_demo")

add_executable(${TARGET_NAME}
    main.cpp
)

#=======================================
# cmake 自动Qt MOC UIC RCC
set_target_properties(${TARGET_NAME} PROPERTIES 
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

#=======================================
# 查找Qt库
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

#=======================================
# 链接Qt库
target_link_libraries(${TARGET_NAME}
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    )