# patch_cmake_version.cmake - Qt-Advanced-Docking-System CMake版本补丁
# 在顶层CMakeLists.txt中调用，用于修改Qt-Advanced-Docking-System的cmake_minimum_required版本

# 补丁函数：统一处理单个CMakeLists.txt的版本替换
function(patch_ads_cmake cmake_path description)
    if(NOT EXISTS "${cmake_path}")
        message(WARNING "CMake file not found: ${cmake_path}, skipping patch")
        return()
    endif()
    
    # 读取原文件内容
    file(READ "${cmake_path}" CMAKE_CONTENT)
    
    # 替换版本号
    string(REPLACE "cmake_minimum_required(VERSION 3.5)" 
                   "cmake_minimum_required(VERSION 3.15)" 
                   CMAKE_CONTENT_MODIFIED "${CMAKE_CONTENT}")
    
    # 写入修改后的内容（会覆盖原文件）
    file(WRITE "${cmake_path}" "${CMAKE_CONTENT_MODIFIED}")
    
    message(STATUS "Patched ${description}: cmake_minimum_required updated to 3.15")
endfunction()

# 1. 补丁顶层 Qt-Advanced-Docking-System CMakeLists.txt
patch_ads_cmake(
    "${CMAKE_SOURCE_DIR}/3rdparty/Qt-Advanced-Docking-System-3.8.3/CMakeLists.txt"
    "Qt-Advanced-Docking-System root CMakeLists.txt"
)

# 2. 补丁 src 目录下的 CMakeLists.txt
patch_ads_cmake(
    "${CMAKE_SOURCE_DIR}/3rdparty/Qt-Advanced-Docking-System-3.8.3/src/CMakeLists.txt"
    "Qt-Advanced-Docking-System src CMakeLists.txt"
)
