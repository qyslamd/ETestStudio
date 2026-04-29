# extract_dependencies.cmake - 自动解压第三方依赖库
# 在顶层CMakeLists.txt中调用，用于解压3rdparty目录下的tar.gz文件

# 设置3rdparty目录路径
set(3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty")

# 定义需要解压的依赖库列表
set(DEPENDENCY_ARCHIVES
    "zlib-1.3.2.tar.gz"
    "spdlog-1.17.0.tar.gz"
    "Qt-Advanced-Docking-System-3.8.3.tar.gz"
    "QXlsx-1.5.0.tar.gz"
    "googletest-1.17.0.tar.gz"
    "libharu-2.4.6.tar.gz"
    "QScintilla-2.11.3.tar.gz"
    "lua-5.4.4.tar.gz"
    "libpng-1.6.43.tar.gz"
    "sol2-3.3.0.tar.gz"
)

# 检测tar解压命令是否可用
function(check_tar_command)
    execute_process(COMMAND tar --version
                    OUTPUT_VARIABLE tar_version
                    ERROR_QUIET
                    RESULT_VARIABLE tar_check)
    if(tar_check EQUAL 0)
        set(TAR_FOUND TRUE PARENT_SCOPE)
        message(STATUS "Found tar command, will use tar for extraction")
    else()
        set(TAR_FOUND FALSE PARENT_SCOPE)
        message(STATUS "tar command not found, checking 7z...")
    endif()
endfunction()

# 检测7z解压命令是否可用
function(check_7z_command)
    execute_process(COMMAND 7z
                    OUTPUT_VARIABLE 7z_help
                    ERROR_QUIET
                    RESULT_VARIABLE 7z_check)
    if(7z_check EQUAL 0 OR 7z_check EQUAL 1)
        set(7Z_FOUND TRUE PARENT_SCOPE)
        message(STATUS "Found 7z command, will use 7z for extraction")
    else()
        set(7Z_FOUND FALSE PARENT_SCOPE)
        message(STATUS "7z command not found")
    endif()
endfunction()

# 解压函数
function(extract_if_needed archive_file)
    set(archive_path "${3RDPARTY_DIR}/${archive_file}")
    
    # 检查压缩包是否存在
    if(NOT EXISTS "${archive_path}")
        message(WARNING "Archive not found: ${archive_file}, skipping...")
        return()
    endif()
    
    # 提取目录名（去掉.tar.gz后缀）
    string(REGEX REPLACE "\\.tar\\.gz$" "" dir_name "${archive_file}")
    set(dir_path "${3RDPARTY_DIR}/${dir_name}")
    
    # 检查目录是否已存在
    if(EXISTS "${dir_path}")
        message(STATUS "Already extracted: ${archive_file}")
        return()
    endif()
    
    # 尝试解压
    message(STATUS "Extracting: ${archive_file}")
    
    set(extract_success FALSE)
    
    # 优先使用tar命令
    if(TAR_FOUND)
        execute_process(COMMAND tar -xf "${archive_file}"
                        WORKING_DIRECTORY "${3RDPARTY_DIR}"
                        RESULT_VARIABLE extract_result
                        OUTPUT_QUIET
                        ERROR_QUIET)
        if(extract_result EQUAL 0)
            set(extract_success TRUE)
            message(STATUS "  Successfully extracted using tar")
        else()
            message(WARNING "  tar extraction failed, trying 7z...")
        endif()
    endif()
    
    # 如果tar失败或不存在，尝试7z
    if(NOT extract_success AND 7Z_FOUND)
        execute_process(COMMAND 7z x "${archive_file}" -y
                        WORKING_DIRECTORY "${3RDPARTY_DIR}"
                        RESULT_VARIABLE extract_result
                        OUTPUT_QUIET
                        ERROR_QUIET)
        if(extract_result EQUAL 0)
            set(extract_success TRUE)
            message(STATUS "  Successfully extracted using 7z")
        else()
            message(WARNING "  7z extraction failed")
        endif()
    endif()
    
    # 验证解压结果
    if(extract_success AND EXISTS "${dir_path}")
        message(STATUS "  Extraction complete: ${dir_name}")
    else()
        message(FATAL_ERROR "  Extraction failed for: ${archive_file}")
    endif()
endfunction()

# 主执行逻辑
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Checking 3rdparty dependencies...")
message(STATUS "========================================")

# 检测解压命令
check_tar_command()

if(NOT TAR_FOUND)
    check_7z_command()
    if(NOT 7Z_FOUND)
        message(FATAL_ERROR "No extraction tool found! Please install either tar (Windows 10+ built-in) or 7-Zip")
    endif()
endif()

# 遍历所有依赖库进行解压
foreach(archive_file ${DEPENDENCY_ARCHIVES})
    extract_if_needed(${archive_file})
endforeach()

message(STATUS "========================================")
message(STATUS "Dependency extraction complete!")
message(STATUS "========================================")
message(STATUS "")
