# patch_cmake_version.cmake - libpng CMake版本和zlib集成补丁
# 在顶层CMakeLists.txt中调用，用于修改libpng的cmake_minimum_required版本和zlib集成

# 补丁函数：统一处理单个CMakeLists.txt的版本替换
function(patch_libpng_cmake cmake_path description)
    if(NOT EXISTS "${cmake_path}")
        message(WARNING "CMake file not found: ${cmake_path}, skipping patch")
        return()
    endif()
    
    # 读取原文件内容
    file(READ "${cmake_path}" CMAKE_CONTENT)
    if("${CMAKE_CONTENT}" STREQUAL "")
        message(WARNING "Read ${cmake_path} failed, content is empty, skipping patch")
        return()
    endif()
    
    # 强制初始化修改后的内容为原始内容，避免空变量问题
    set(CMAKE_CONTENT_MODIFIED "${CMAKE_CONTENT}")
    set(PATCH_APPLIED FALSE)
    
    # 1. 替换版本号
    if(NOT CMAKE_CONTENT MATCHES "cmake_minimum_required\\(VERSION 3.15\\)")
        string(REPLACE "cmake_minimum_required(VERSION 3.6)" 
                "cmake_minimum_required(VERSION 3.15)" 
                TMP_CONTENT "${CMAKE_CONTENT_MODIFIED}")
        if(NOT "${TMP_CONTENT}" STREQUAL "${CMAKE_CONTENT_MODIFIED}")
            set(CMAKE_CONTENT_MODIFIED "${TMP_CONTENT}")
            set(PATCH_APPLIED TRUE)
            message(STATUS "  [OK] Patched cmake_minimum_required version to 3.15")
        else()
            message(WARNING "  [WARN] Failed to patch cmake_minimum_required version, pattern not found")
        endif()
    else()
        message(STATUS "  [INFO] cmake_minimum_required already at 3.15, skipping")
    endif()
    
    # 2. 注释掉find_package(ZLIB REQUIRED)，因为我们自己提供zlib
    if(CMAKE_CONTENT MATCHES "  find_package\\(ZLIB REQUIRED\\)")
        string(REPLACE "  find_package(ZLIB REQUIRED)" 
                       "#  find_package(ZLIB REQUIRED)  # 使用项目自己的zlib" 
                       TMP_CONTENT "${CMAKE_CONTENT_MODIFIED}")
        if(NOT "${TMP_CONTENT}" STREQUAL "${CMAKE_CONTENT_MODIFIED}")
            set(CMAKE_CONTENT_MODIFIED "${TMP_CONTENT}")
            set(PATCH_APPLIED TRUE)
            message(STATUS "  [OK] Patched find_package(ZLIB) comment")
        endif()
    else()
        message(STATUS "  [INFO] find_package(ZLIB) already commented, skipping")
    endif()
    
    # 4. 添加CMP0194政策设置 + 移除不必要的ASM语言声明，解决MSVC下ASM编译器错误
    if(CMAKE_CONTENT MATCHES "LANGUAGES C ASM")
        string(REPLACE "project(libpng
        VERSION \${PNGLIB_VERSION}
        LANGUAGES C ASM)"
"if(POLICY CMP0194)
  cmake_policy(SET CMP0194 NEW)
endif()
project(libpng
        VERSION \${PNGLIB_VERSION}
        LANGUAGES C)"
        TMP_CONTENT "${CMAKE_CONTENT_MODIFIED}")
        if(NOT "${TMP_CONTENT}" STREQUAL "${CMAKE_CONTENT_MODIFIED}")
            set(CMAKE_CONTENT_MODIFIED "${TMP_CONTENT}")
            set(PATCH_APPLIED TRUE)
            message(STATUS "  [OK] Added CMP0194 policy and removed ASM language requirement")
        else()
            message(WARNING "  [WARN] Failed to patch project declaration, pattern not found")
        endif()
    else()
        message(STATUS "  [INFO] Project declaration already patched (ASM removed), skipping")
    endif()
    
    # 写入前有效性校验，避免写入空内容损坏文件
    if("${CMAKE_CONTENT_MODIFIED}" STREQUAL "")
        message(WARNING "Modified content is empty, aborting write to avoid damaging original file")
        return()
    endif()
    
    # 只有实际有修改的时候才写入文件，避免不必要的文件IO和时间戳变更
    if(PATCH_APPLIED)
        # 写入修改后的内容（会覆盖原文件）
        file(WRITE "${cmake_path}" "${CMAKE_CONTENT_MODIFIED}")
        message(STATUS "Patched ${description}: all patches applied successfully")
    else()
        message(STATUS "${description} already fully patched, no changes needed")
    endif()
endfunction()

# 补丁 libpng CMakeLists.txt
if(WIN32)
    patch_libpng_cmake(
        "${CMAKE_SOURCE_DIR}/3rdparty/libpng-1.6.43/CMakeLists.txt"
        "libpng root CMakeLists.txt"
    )
endif()