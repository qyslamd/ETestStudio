<#
.SYNOPSIS
Git interactive commit tool
#>

# Save original working directory
$originalLocation = Get-Location

# Switch to project root
$PROJECT_ROOT = Split-Path $MyInvocation.MyCommand.Definition -Parent
Set-Location (Join-Path $PROJECT_ROOT "..")

try {
    # Check git status
    try {
        $gitStatus = git status --short 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[ERROR] Failed to get git status" -ForegroundColor Red
            exit 1
        }
    }
    catch {
        Write-Host "[ERROR] Failed to get git status" -ForegroundColor Red
        exit 1
    }

    # Check for changes
    if (-not $gitStatus) {
        Write-Host "No changes to commit, exiting"
        exit 0
    }

    # Show commit type options
    Write-Host "Select commit type:"
    Write-Host "0. quit    - Exit"
    Write-Host "1. feat    - New feature"
    Write-Host "2. fix     - Bug fix"
    Write-Host "3. docs    - Documentation update"
    Write-Host "4. chore   - Build/tool changes"
    Write-Host "5. refactor - Code refactoring"
    Write-Host "6. style   - Code formatting"
    Write-Host "7. test    - Test updates"
    Write-Host ""

    # Get user selection
    $TYPE_NUM = Read-Host "Enter number (1-7)"
    if ($TYPE_NUM) { $TYPE_NUM = $TYPE_NUM.Trim() }

    if ($TYPE_NUM -eq "0") {
        Write-Host "Exiting"
        exit 0
    }

    # Map type
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
        Write-Host "Invalid selection" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "Selected: [$TYPE]"
    Write-Host ""

    # Get commit description
    $DESC = Read-Host "Enter commit description"
    if ($DESC) { $DESC = $DESC.Trim() }

    if (-not $DESC) {
        Write-Host "Commit description cannot be empty" -ForegroundColor Red
        exit 1
    }

    # Confirmation
    Write-Host ""
    Write-Host "===== Confirmation ====="
    Write-Host "Type: $TYPE"
    Write-Host "Description: $DESC"
    Write-Host "========================"
    Write-Host ""

    # Execute commit
    git add .
    git commit -m "${TYPE}: $DESC"
}
finally {
    # Return to original directory
    Set-Location $originalLocation
}