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
    
    # 检查是否已经patch过（避免重复patch）
    if(CMAKE_CONTENT MATCHES "cmake_minimum_required\\(VERSION 3.15\\)")
        message(STATUS "${description} already patched, skipping")
        return()
    endif()
    
    # 1. 替换版本号
    string(REPLACE "cmake_minimum_required(VERSION 3.6)" 
                   "cmake_minimum_required(VERSION 3.15)" 
                   CMAKE_CONTENT_MODIFIED "${CMAKE_CONTENT}")
    
    # 2. 注释掉find_package(ZLIB REQUIRED)，因为我们自己提供zlib
    string(REPLACE "  find_package(ZLIB REQUIRED)" 
                   "#  find_package(ZLIB REQUIRED)  # 使用项目自己的zlib" 
                   CMAKE_CONTENT_MODIFIED "${CMAKE_CONTENT_MODIFIED}")
    
    # 3. 修改链接目标，使用我们的静态zlib
    # 注意：这里不使用${M_LIBRARY}，因为在patch执行时变量还不存在
    string(REPLACE "target_link_libraries(png_shared PUBLIC ZLIB::ZLIB" 
                   "target_link_libraries(png_shared PUBLIC ZLIB::ZLIBSTATIC" 
                   CMAKE_CONTENT_MODIFIED "${CMAKE_CONTENT_MODIFIED}")
    string(REPLACE "target_link_libraries(png_static PUBLIC ZLIB::ZLIB" 
                   "target_link_libraries(png_static PUBLIC ZLIB::ZLIBSTATIC" 
                   CMAKE_CONTENT_MODIFIED "${CMAKE_CONTENT_MODIFIED}")
    
    # 写入修改后的内容（会覆盖原文件）
    file(WRITE "${cmake_path}" "${CMAKE_CONTENT_MODIFIED}")
    
    message(STATUS "Patched ${description}: cmake_minimum_required updated to 3.15, zlib integration complete")
endfunction()

# 补丁 libpng CMakeLists.txt
patch_libpng_cmake(
    "${CMAKE_SOURCE_DIR}/3rdparty/libpng-1.6.43/CMakeLists.txt"
    "libpng root CMakeLists.txt"
)
