# Git 面板根目录定位问题

## 问题描述

GitWidget 通过 `git rev-parse --is-inside-work-tree` 判断项目目录是否在 git 仓库内。该命令的行为是**向上遍历**查找 `.git` 目录，而非严格限定在项目根目录。

这导致两个问题：

### 1. 父级 git 仓库污染

开发机上的项目目录可能位于某个父级 git 仓库内（例如整个 `D:/trae_workspace/` 是一个 git 仓库）。此时打开一个未执行 `git init` 的 ETest 项目：

```
D:/trae_workspace/                 ← 父级 .git 在这里
  └── etest-demo/                  ← 项目根目录，没有 .git
        ├── demo_mock.etproj
        └── cases/
```

`git rev-parse --is-inside-work-tree` → `"true"`，但实际显示的是父仓库的 git status，而非项目本身的变更，与用户预期不符。

### 2. 新建项目未初始化 git

`ProjectManager::createProject()` 创建项目目录结构后，没有执行 `git init`，项目初始就没有自己的 git 仓库。

## 后续改进计划

### 改进 A：`isGitRepo()` 直检 `.git` 目录

```cpp
bool GitWidget::isGitRepo() const {
  return QDir(project_root_).exists(".git");
}
```

或保持用 `git rev-parse --git-dir` 并将结果与 `project_root_` 比较：

```cpp
bool GitWidget::isGitRepo() const {
  QProcess git;
  git.setWorkingDirectory(project_root_);
  git.start("git", {"rev-parse", "--git-dir"});
  if (!git.waitForFinished(3000)) return false;
  QString gitDir = QString::fromUtf8(git.readAllStandardOutput().trimmed());
  // 校验 git 目录必须严格在 project_root_ 下
  return QDir::isAbsolutePath(gitDir) &&
         QFileInfo(gitDir).absolutePath() == QDir(project_root_).absolutePath();
}
```

### 改进 B：新建项目自动 init

`ProjectManager::createProject()` 尾部或 `MainWindow::onProjectOpened()` 中新增：

```cpp
QProcess::execute("git", {"init", projectDir});
```

### 改进 C：已存在项目手动 init

在 GitWidget 的空状态页（"不是 Git 仓库"）添加一个"初始化 Git 仓库"按钮，调用 `git init` 后刷新。

### 改进 D：打开项目时弹窗确认初始化

在 `MainWindow::onProjectOpened()` 中，完成项目加载后检查项目目录是否已初始化 git：

```cpp
// 伪代码
if (!QDir(projectRoot).exists(".git")) {
  auto result = QMessageBox::question(
      this, QStringLiteral("Git 仓库"),
      QStringLiteral("项目目录尚未初始化 Git 仓库，是否立即初始化？\n\n"
                      "这将在项目根目录创建 .git 目录，"
                      "方便您对测试程序文件进行版本管理。"),
      QMessageBox::Yes | QMessageBox::No);
  if (result == QMessageBox::Yes) {
    QProcess::execute("git", {"init", projectRoot});
    if (auto* gw = sidebar_->gitWidget()) {
      gw->setProjectRoot(projectRoot);
    }
  }
}
```

弹窗时机应在所有初始化完成之后、窗口完全展示之前，避免打断 ICD 加载等关键流程。

## 已实施

四项改进已全部落地：

- **A** `src/app/GitWidget.cpp` `isGitRepo()` 改为 `QDir(project_root_).exists(".git")`
- **B** `src/core/project/ProjectManager.cpp` `createProject()` 末尾自动 `git init`
- **C** `src/app/GitWidget.h/.cpp` 空状态页加"初始化 Git 仓库"按钮，新增 public `initRepository()` 与 `initRepoRequested` 信号
- **D** `src/app/MainWindow.cpp` `onProjectOpened` 中检查无 `.git` 时弹窗询问；`initRepoRequested` 走同一确认流程

新建项目在 `createProject()` 中已 `git init`，因此后续 `onProjectOpened` 被触发时 `.git` 已存在，不会重复弹窗。
