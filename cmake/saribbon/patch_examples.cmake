# patch_examples.cmake - 为 SARibbon 的 example 生成运行脚本
# 在顶层 CMakeLists.txt 中调用（必须在 add_subdirectory(SARibbon-2.5.7) 之前）
# 使用 file(GENERATE) + $<TARGET_FILE_...> 生成器表达式，
# 自动处理 CMAKE_DEBUG_POSTFIX，确保 exe 路径始终正确

function(_patch_example_run_script example_target)
    set(example_cmake "${CMAKE_SOURCE_DIR}/3rdparty/SARibbon-2.5.7/example/${example_target}/CMakeLists.txt")
    if(NOT EXISTS "${example_cmake}")
        message(WARNING "SARibbon example CMakeLists.txt not found: ${example_cmake}")
        return()
    endif()

    file(READ "${example_cmake}" _content)

    if(_content MATCHES "generate run script")
        message(STATUS "SARibbon example ${example_target} already patched, skipping")
        return()
    endif()

    # 模板使用 @VAR@ 占位，@EXAMPLE_NAME@ 后续用 string(REPLACE) 替换
    # 生成器表达式 $<TARGET_FILE_...> 保留原样，在 CMake generate 阶段求值
    set(_configure_block_template [=[
# generate run script
if(WIN32)
    file(GENERATE OUTPUT "@CMAKE_BINARY_DIR@/bin/run_@EXAMPLE_NAME@.bat" CONTENT
"@echo off
setlocal

set \"PATH=@QT_BIN_DIR@;%PATH%\"
set \"QT_PLUGIN_PATH=@QT_PLUGINS_DIR@\"

chcp 65001 >nul
pushd \"@CMAKE_RUNTIME_OUTPUT_DIRECTORY@\"

$<TARGET_FILE_DIR:@EXAMPLE_NAME@>/$<TARGET_FILE_NAME:@EXAMPLE_NAME@>
popd
")
endif()
]=])
    # 先替换 @EXAMPLE_NAME@（string(CONFIGURE 会吃掉未知的 @VAR@）
    string(REPLACE "@EXAMPLE_NAME@" "${example_target}" _configure_block "${_configure_block_template}")
    # 再替换 @QT_BIN_DIR@, @QT_PLUGINS_DIR@, @CMAKE_BINARY_DIR@
    string(CONFIGURE "${_configure_block}" _configure_block @ONLY)

    set(_install_pattern "install(TARGETS ${example_target}")
    string(FIND "${_content}" "${_install_pattern}" _pos)
    if(_pos GREATER -1)
        string(REPLACE "${_install_pattern}" "${_configure_block}\n${_install_pattern}" _content "${_content}")
    else()
        string(APPEND _content "\n\n${_configure_block}\n")
    endif()

    file(WRITE "${example_cmake}" "${_content}")
    message(STATUS "Patched SARibbon example ${example_target}: added file(GENERATE) for run script")
endfunction()

# ========================================
# 对所有 SARibbon example 打补丁
# ========================================
_patch_example_run_script(MainWindowExample)
_patch_example_run_script(UseNativeFrameExample)
_patch_example_run_script(WidgetWithRibbon)
_patch_example_run_script(NormalMenuBarExample)
_patch_example_run_script(MdiAreaWindowExample)
_patch_example_run_script(MatlabUI)
_patch_example_run_script(StaticExample)
