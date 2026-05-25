# patch_cmake_version.cmake - SARibbon CMake版本补丁
# 在顶层CMakeLists.txt中调用，用于修改SARibbon的cmake_minimum_required版本

# 补丁函数：统一处理单个CMakeLists.txt的版本替换
function(patch_saribbon_cmake cmake_path description)
    if(NOT EXISTS "${cmake_path}")
        message(WARNING "CMake file not found: ${cmake_path}, skipping patch")
        return()
    endif()

    file(READ "${cmake_path}" CMAKE_CONTENT)
    if("${CMAKE_CONTENT}" STREQUAL "")
        message(WARNING "Read ${cmake_path} failed, content is empty, skipping patch")
        return()
    endif()

    set(CMAKE_CONTENT_MODIFIED "${CMAKE_CONTENT}")
    set(PATCH_APPLIED FALSE)

    if(NOT CMAKE_CONTENT MATCHES "cmake_minimum_required\\(VERSION 3\\.15\\)")
        string(REPLACE "cmake_minimum_required(VERSION 3.5)"
                       "cmake_minimum_required(VERSION 3.15)"
                       TMP_CONTENT "${CMAKE_CONTENT_MODIFIED}")
        if(NOT "${TMP_CONTENT}" STREQUAL "${CMAKE_CONTENT_MODIFIED}")
            set(CMAKE_CONTENT_MODIFIED "${TMP_CONTENT}")
            set(PATCH_APPLIED TRUE)
            message(STATUS "  [SARibbon] Patched cmake_minimum_required version to 3.15")
        else()
            message(WARNING "  [SARibbon] Failed to patch cmake_minimum_required version, pattern not found")
        endif()
    else()
        message(STATUS "  [SARibbon] cmake_minimum_required already at 3.15, skipping")
    endif()

    if("${CMAKE_CONTENT_MODIFIED}" STREQUAL "")
        message(WARNING "Modified content is empty, aborting write")
        return()
    endif()

    if(PATCH_APPLIED)
        file(WRITE "${cmake_path}" "${CMAKE_CONTENT_MODIFIED}")
        message(STATUS "Patched ${description}: cmake_minimum_required updated to 3.15")
    else()
        message(STATUS "${description} already fully patched, no changes needed")
    endif()
endfunction()

# 1. 补丁 SARibbon 顶层 CMakeLists.txt
patch_saribbon_cmake(
    "${CMAKE_SOURCE_DIR}/3rdparty/SARibbon-2.5.7/CMakeLists.txt"
    "SARibbon root CMakeLists.txt"
)

# 2. 补丁 SARibbonBar 子目录 CMakeLists.txt
patch_saribbon_cmake(
    "${CMAKE_SOURCE_DIR}/3rdparty/SARibbon-2.5.7/src/SARibbonBar/CMakeLists.txt"
    "SARibbonBar src CMakeLists.txt"
)
