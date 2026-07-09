# patch_utf8.cmake - libharu UTF-8 编码补丁
# libharu 源文件为无 BOM 的 UTF-8，MSVC 默认用系统代码页(GBK)解析会报 C4819
if(MSVC)
    target_compile_options(hpdf PRIVATE /utf-8)
    message(STATUS "Patched hpdf: added /utf-8 for C4819")
endif()
