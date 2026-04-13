@echo off
setlocal enabledelayedexpansion

chcp 65001 >nul

:: 检查是否有修改
git status --short >nul
if errorlevel 1 (
    echo [错误] 无法获取git状态
    exit /b 1
)

for /f "delims=" %%i in ('git status --short') do set HAS_CHANGE=%%i
if not defined HAS_CHANGE (
    echo 没有可提交的更改，退出
    exit /b 0
)

echo 请选择提交类型:
echo 0. quit    - 退出
echo 1. feat    - 新功能
echo 2. fix     - Bug修复
echo 3. docs    - 文档更新
echo 4. chore   - 构建/工具变动
echo 5. refactor - 重构
echo 6. style   - 代码格式调整
echo 7. test    - 测试

set /p TYPE_NUM=请输入编号(1-7):

set "TYPE_NUM=!TYPE_NUM: =!"

if "!TYPE_NUM!"=="0" (
    echo 退出
    exit /b 0
)

if "!TYPE_NUM!"=="1" set TYPE=feat
if "!TYPE_NUM!"=="2" set TYPE=fix
if "!TYPE_NUM!"=="3" set TYPE=docs
if "!TYPE_NUM!"=="4" set TYPE=chore
if "!TYPE_NUM!"=="5" set TYPE=refactor
if "!TYPE_NUM!"=="6" set TYPE=style
if "!TYPE_NUM!"=="7" set TYPE=test

if not defined TYPE (
    echo 无效的选择
    exit /b 1
)

echo.
echo 已选择: [!TYPE!]
echo.
echo 请输入提交描述:
set /p DESC=

if not defined DESC (
    echo 提交描述不能为空
    exit /b 1
)

echo.
echo ===== 确认信息 =====
echo 类型: !TYPE!
echo 描述: !DESC!
echo =====================
echo.

git add .
git commit -m "!TYPE!: !DESC!"