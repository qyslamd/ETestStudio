# patch_msys_env.cmake — MSYS 环境下 MSVC 编译 libpng 的兼容性补丁
#
# 问题：在 MSYS/bash 环境中执行 cmake 时，CMake 会将 UNIX 变量设置为 TRUE，
# 即使编译器是 MSVC。导致 libpng 的 CMakeLists.txt 中 if(UNIX ...) 分支被触发：
#   1. find_library(M_LIBRARY m) — 添加 -lm 链接参数，MSVC 的 link.exe 无法识别
#   2. 使用 Unix 库命名规则（png16.dll）而非 Windows 规则（libpng16.dll）
#   3. 尝试应用链接器版本脚本（-Wl,--version-script）
#
# 修复：在所有 if(UNIX ...) 条件中添加 AND NOT MSVC 防护

function(patch_libpng_msys_env cmake_path description)
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

    if(CMAKE_CONTENT MATCHES "if\\(UNIX")
        string(REPLACE "if(UNIX"
                       "if(UNIX AND NOT MSVC"
                       TMP_CONTENT "${CMAKE_CONTENT_MODIFIED}")
        if(NOT "${TMP_CONTENT}" STREQUAL "${CMAKE_CONTENT_MODIFIED}")
            set(CMAKE_CONTENT_MODIFIED "${TMP_CONTENT}")
            set(PATCH_APPLIED TRUE)
            message(STATUS "  [OK] Patched if(UNIX) to if(UNIX AND NOT MSVC)")
        endif()
    else()
        message(STATUS "  [INFO] if(UNIX) pattern not found, skipping")
    endif()

    if("${CMAKE_CONTENT_MODIFIED}" STREQUAL "")
        message(WARNING "Modified content is empty, aborting write")
        return()
    endif()

    if(PATCH_APPLIED)
        file(WRITE "${cmake_path}" "${CMAKE_CONTENT_MODIFIED}")
        message(STATUS "Patched ${description}: MSYS env compatibility applied")
    else()
        message(STATUS "${description} already patched, no changes needed")
    endif()
endfunction()

if(WIN32)
    patch_libpng_msys_env(
        "${CMAKE_SOURCE_DIR}/3rdparty/libpng-1.6.43/CMakeLists.txt"
        "libpng root CMakeLists.txt"
    )
endif()
