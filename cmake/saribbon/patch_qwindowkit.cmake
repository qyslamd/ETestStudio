# patch_qwindowkit.cmake - SARibbon QWindowKit 依赖补丁
# 在顶层 CMakeLists.txt 中调用，用于修改 SARibbon 的 CMakeLists.txt
# 使其直接使用项目中已集成的 QWindowKit 目标，而不是通过 find_package 查找

# 补丁1: SARibbon 顶层 CMakeLists.txt - 替换 find_package(QWindowKit) 查找块
set(_saribbon_root_cmake "${CMAKE_SOURCE_DIR}/3rdparty/SARibbon-2.5.7/CMakeLists.txt")
if(EXISTS "${_saribbon_root_cmake}")
    file(READ "${_saribbon_root_cmake}" _root_content)

    if("${_root_content}" MATCHES "find_package\\(QWindowKit QUIET")
        set(_old_block [=[
if(_SARIBBON_USE_FRAMELESS_LIB)
    # 明确记录尝试查找的路径
    set(_qwk_search_paths)

    # 如果用户已指定QWindowKit_DIR，优先使用
    if(QWindowKit_DIR)
        list(APPEND _qwk_search_paths ${QWindowKit_DIR})
        message(STATUS "Using user-specified QWindowKit_DIR: ${QWindowKit_DIR}")
    endif()

    # 添加项目本地安装目录作为备选路径
    set(_local_qwk_dir ${SARIBBON_LOCAL_INSTALL_BIN_DIR}/lib/cmake/QWindowKit)
    if(EXISTS "${_local_qwk_dir}")
        list(APPEND _qwk_search_paths ${_local_qwk_dir})
        message(STATUS "Adding local install path for QWindowKit: ${_local_qwk_dir}")
    endif()

    # 尝试查找QWindowKit包
    find_package(QWindowKit QUIET
        PATHS ${_qwk_search_paths}
        NO_DEFAULT_PATH  # 禁止在系统默认路径查找
    )

    # 如果未找到，尝试系统路径作为最后手段
    if(NOT QWindowKit_FOUND)
        message(STATUS "QWindowKit not found in specified paths, trying system default...")
        find_package(QWindowKit QUIET)
    endif()

    # 最终确认是否找到
    if(QWindowKit_FOUND)
        message(STATUS "QWindowKit found at: ${QWindowKit_DIR}")
    else()
        set(_SARIBBON_USE_FRAMELESS_LIB OFF)
        message(WARNING "QWindowKit not found - disabling frameless support. Searched paths:\n  ${_qwk_search_paths}")
    endif()
endif()]=])

        set(_new_block [=[
if(_SARIBBON_USE_FRAMELESS_LIB)
    # QWindowKit 已通过项目 add_subdirectory 集成，直接检查目标可用性
    if(TARGET QWindowKit::Widgets)
        message(STATUS "QWindowKit found (integrated via project add_subdirectory)")
    else()
        set(_SARIBBON_USE_FRAMELESS_LIB OFF)
        message(WARNING "QWindowKit not found - disabling frameless support.")
    endif()
endif()]=])

        set(_root_content_before "${_root_content}")
        string(REPLACE "${_old_block}" "${_new_block}" _root_content "${_root_content}")
        if(NOT "${_root_content}" STREQUAL "${_root_content_before}")
            file(WRITE "${_saribbon_root_cmake}" "${_root_content}")
            message(STATUS "Patched SARibbon root CMakeLists.txt: find_package(QWindowKit) -> direct target check")
        else()
            message(WARNING "SARibbon root CMakeLists.txt: find_package(QWindowKit) block found but string(REPLACE) did not match!")
        endif()
    else()
        message(STATUS "SARibbon root CMakeLists.txt: find_package(QWindowKit) already patched, skipping")
    endif()
else()
    message(WARNING "SARibbon root CMakeLists.txt not found, skipping patch 1")
endif()


# 补丁2: SARibbonBar CMakeLists.txt - 移除 find_package(QWindowKit REQUIRED) + install EXPORT
set(_saribbon_bar_cmake "${CMAKE_SOURCE_DIR}/3rdparty/SARibbon-2.5.7/src/SARibbonBar/CMakeLists.txt")
if(NOT EXISTS "${_saribbon_bar_cmake}")
    message(WARNING "SARibbonBar CMakeLists.txt not found, skipping patches 2/3")
else()
    file(READ "${_saribbon_bar_cmake}" _bar_content)
    set(_bar_modified FALSE)

    # 补丁2a: 移除 find_package(QWindowKit REQUIRED)
    if("${_bar_content}" MATCHES "find_package\\(QWindowKit REQUIRED\\)")
        string(REPLACE "    find_package(QWindowKit REQUIRED)\n    target_link_libraries"
                       "    target_link_libraries"
                       _bar_content "${_bar_content}")
        set(_bar_modified TRUE)
        message(STATUS "  Removed find_package(QWindowKit REQUIRED) from SARibbonBar")
    endif()

    # 补丁2b: 移除 install EXPORT - 避免 QWKWidgets 不在 export set 中的错误
    if("${_bar_content}" MATCHES "EXPORT \\$\\{SARIBBON_LIB_NAME\\}Targets")
        string(REPLACE "    EXPORT \${SARIBBON_LIB_NAME}Targets" "" _bar_content "${_bar_content}")
        string(REPLACE "install(EXPORT \${SARIBBON_LIB_NAME}Targets
    FILE \${SARIBBON_LIB_NAME}Targets.cmake
    NAMESPACE \${SARIBBON_LIB_NAME}::
    DESTINATION \${CMAKE_INSTALL_LIBDIR}/cmake/\${SARIBBON_LIB_NAME}
)" "" _bar_content "${_bar_content}")
        set(_bar_modified TRUE)
        message(STATUS "  Removed install EXPORT from SARibbonBar")
    endif()

    if(_bar_modified)
        file(WRITE "${_saribbon_bar_cmake}" "${_bar_content}")
        message(STATUS "Patched SARibbonBar CMakeLists.txt")
    else()
        message(STATUS "SARibbonBar CMakeLists.txt already patched, skipping")
    endif()
endif()


# 补丁4: QWindowKit CMakeLists.txt - 将 qmsetup 源码路径从内部相对路径改为 3rdparty 归档路径
# QWindowKit 默认在 qmsetup/ 子目录下查找，但 qmsetup 现在作为独立 3rdparty 依赖存在
set(_qwindowkit_cmake "${CMAKE_SOURCE_DIR}/3rdparty/qwindowkit-1.5.0/CMakeLists.txt")
if(EXISTS "${_qwindowkit_cmake}")
    file(READ "${_qwindowkit_cmake}" _qwk_content)
    if("${_qwk_content}" MATCHES "CMAKE_CURRENT_SOURCE_DIR}/qmsetup")
        string(REPLACE
            "    set(_source_dir \${CMAKE_CURRENT_SOURCE_DIR}/qmsetup)"
            "    set(_source_dir \${CMAKE_SOURCE_DIR}/3rdparty/qmsetup-4a3ff82)"
            _qwk_content "${_qwk_content}")
        file(WRITE "${_qwindowkit_cmake}" "${_qwk_content}")
        message(STATUS "Patched QWindowKit CMakeLists.txt: qmsetup source dir -> 3rdparty/qmsetup-4a3ff82")
    else()
        message(STATUS "QWindowKit CMakeLists.txt: qmsetup path already patched, skipping")
    endif()
else()
    message(WARNING "QWindowKit CMakeLists.txt not found, skipping patch 4")
endif()
