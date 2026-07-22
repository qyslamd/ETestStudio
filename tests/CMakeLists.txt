# 统一配置Qt测试运行环境变量
if(Qt5_FOUND)
    if(WIN32)
        get_filename_component(QT_BIN_DIR "${QT_LIB_DIR}/../bin" ABSOLUTE)
        set(TEST_ENV_QT
            "PATH=${QT_BIN_DIR};$ENV{PATH}"
            "QT_PLUGIN_PATH=${QT_BIN_DIR}/../plugins"
        )
    else()
        set(TEST_ENV_QT
            "LD_LIBRARY_PATH=${QT_LIB_DIR}:$ENV{LD_LIBRARY_PATH}"
            "QT_PLUGIN_PATH=${QT_LIB_DIR}/../plugins"
        )
    endif()

    # 辅助函数：批量给指定目录下所有测试设置Qt运行时的环境变量（保留原有兼容）
    function(set_test_env_for_dir dir)
        get_property(tests DIRECTORY "${dir}" PROPERTY TESTS)
        if(tests)
            set_tests_properties(${tests} PROPERTIES ENVIRONMENT "${TEST_ENV_QT}")
        endif()
    endfunction()
endif()

# ============== 全局通用测试配置 ==============
# 公共头文件路径，所有测试通用
set(ETEST_COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googletest/include
    ${CMAKE_SOURCE_DIR}/3rdparty/googletest-1.17.0/googlemock/include
)
# 公共链接库，所有测试通用
set(ETEST_COMMON_LIBS gtest gtest_main gmock)
# ✅ 先备份顶层CMakeLists.txt定义的全局输出目录（Lua可执行文件会输出到这里）
set(ETEST_GLOBAL_RUNTIME_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
# 测试产物专属输出目录，和生产代码完全隔离
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)
# 默认测试超时10秒，死循环自动杀进程
set(ETEST_DEFAULT_TIMEOUT 10 CACHE STRING "Default test timeout in seconds")

# ============== 通用测试构建宏（核心，兼容所有现有特性） ==============
# 参数说明：
#   NAME [必填]: 测试目标名
#   SOURCES [必填]: 源文件列表
#   LIBS [选填]: 额外需要链接的库
#   INCLUDES [选填]: 额外头文件路径
#   QT_MODULES [选填]: 依赖的Qt模块（比如Core Widgets，自动加Qt5::前缀）
#   LABELS [选填]: 测试分组标签，方便过滤测试
#   NO_RUN_SCRIPT [选填]: 不生成Windows的run_test.bat脚本
macro(add_etest)
    cmake_parse_arguments(ETEST "NO_RUN_SCRIPT" "NAME" "SOURCES;LIBS;INCLUDES;QT_MODULES;LABELS" ${ARGN})

    # 1. 创建测试目标
    add_executable(${ETEST_NAME} ${ETEST_SOURCES})

    # 2. 统一配置C++标准，和核心库生产代码完全对齐
    set_target_properties(${ETEST_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ON
    )

    # 3. Qt依赖自动配置：只有传了QT_MODULES参数才开启AUTOMOC，非Qt测试无冗余
    if(ETEST_QT_MODULES)
        set_target_properties(${ETEST_NAME} PROPERTIES AUTOMOC ON)
        # 自动拼接Qt模块前缀
        list(TRANSFORM ETEST_QT_MODULES PREPEND "Qt5::")
        list(APPEND ETEST_LIBS ${ETEST_QT_MODULES})
    endif()

    # 4. 统一配置头文件路径
    target_include_directories(${ETEST_NAME} PRIVATE ${ETEST_COMMON_INCLUDES} ${ETEST_INCLUDES})

    # 5. 统一配置链接库：公共库+用户指定库
    target_link_libraries(${ETEST_NAME} PRIVATE ${ETEST_COMMON_LIBS} ${ETEST_LIBS})

    # 6. 统一配置MSVC编译选项，和生产代码完全一致
    if(MSVC)
        target_compile_options(${ETEST_NAME} PRIVATE /utf-8)
        target_compile_definitions(${ETEST_NAME} PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
    endif()

    # 7. 注册到CTest
    add_test(NAME ${ETEST_NAME} COMMAND ${ETEST_NAME} WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    # 8. 自动设置测试属性：超时+分组+环境变量
    set_tests_properties(${ETEST_NAME} PROPERTIES
        TIMEOUT ${ETEST_DEFAULT_TIMEOUT}
        LABELS "${ETEST_LABELS}"
        ENVIRONMENT "${TEST_ENV_QT}"
    )

    # 9. 自动生成Windows下的run_test.bat，完全保留原有特性
    if(WIN32 AND NOT ETEST_NO_RUN_SCRIPT)
        set(TARGET_NAME ${ETEST_NAME})
        configure_file("${CMAKE_SOURCE_DIR}/cmake/run_app.bat.in" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/run_${ETEST_NAME}.bat" @ONLY)
    endif()
endmacro()

# ============== 全局开关配置 ==============
option(BUILD_TESTS_THIRD_PARTY "Build third party library tests" ON)
option(BUILD_TEST_COVERAGE "Generate test coverage report" OFF)
option(ENABLE_MEMCHECK "Enable memory check support for CTest" OFF)

# 覆盖率配置
if(BUILD_TEST_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fprofile-arcs -ftest-coverage)
    link_libraries(gcov)
endif()

# ============== 子目录注册 ==============
add_subdirectory(lua/batch_smoke_test)
add_subdirectory(lua/unit_test)

if(BUILD_TESTS_THIRD_PARTY)
    add_subdirectory(libharu)
    add_subdirectory(qscintilla)
    add_subdirectory(qxlsx)
    add_subdirectory(spdlog)
    add_subdirectory(sol2)
endif()

add_subdirectory(icd_utility)
add_subdirectory(core)
add_subdirectory(engine)
add_subdirectory(topology)
add_subdirectory(test_program)
add_subdirectory(protocol)
add_subdirectory(app)
