<#
.SYNOPSIS
Git交互式提交工具，完全兼容原有bat逻辑，执行后自动回退到原始工作目录
#>

# 保存执行前的原始目录
$originalLocation = Get-Location

# 切换到项目根目录
$PROJECT_ROOT = Split-Path $MyInvocation.MyCommand.Definition -Parent
Set-Location (Join-Path $PROJECT_ROOT "..")

try {
    # 检查Git状态
    try {
        $gitStatus = git status --short 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[错误] 无法获取Git状态" -ForegroundColor Red
            exit 1
        }
    }
    catch {
        Write-Host "[错误] 无法获取Git状态" -ForegroundColor Red
        exit 1
    }

    # 检查是否有修改
    if (-not $gitStatus) {
        Write-Host "没有可提交的更改，退出"
        exit 0
    }

    # 显示提交类型选项
    Write-Host "请选择提交类型:"
    Write-Host "0. quit    - 退出"
    Write-Host "1. feat    - 新功能"
    Write-Host "2. fix     - Bug修复"
    Write-Host "3. docs    - 文档更新"
    Write-Host "4. chore   - 构建/工具变动"
    Write-Host "5. refactor - 重构"
    Write-Host "6. style   - 代码格式调整"
    Write-Host "7. test    - 测试"
    Write-Host ""

    # 获取用户选择
    $TYPE_NUM = Read-Host "请输入编号(1-7)"
    if ($TYPE_NUM) { $TYPE_NUM = $TYPE_NUM.Trim() }

    if ($TYPE_NUM -eq "0") {
        Write-Host "退出"
        exit 0
    }

    # 映射提交类型
    $TYPE = switch ($TYPE_NUM) {
        "1" { "feat" }
        "2" { "fix" }
        "3" { "docs" }
        "4" { "chore" }
        "5" { "refactor" }
        "6" { "style" }
        "7" { "test" }
        default { $null }
    }

    if (-not $TYPE) {
        Write-Host "无效的选择" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "已选择: [$TYPE]"
    Write-Host ""

    # 获取提交描述
    $DESC = Read-Host "请输入提交描述"
    if ($DESC) { $DESC = $DESC.Trim() }

    if (-not $DESC) {
        Write-Host "提交描述不能为空" -ForegroundColor Red
        exit 1
    }

    # 确认提交信息
    Write-Host ""
    Write-Host "===== 确认信息 ====="
    Write-Host "类型: $TYPE"
    Write-Host "描述: $DESC"
    Write-Host "====================="
    Write-Host ""

    # 执行提交
    git add .
    git commit -m "${TYPE}: $DESC"
}
finally {
    # 无论执行结果如何，强制切回原始执行目录
    Set-Location $originalLocation
}