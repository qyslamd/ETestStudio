# 想法清单

## 待实现

- [ ] **全局 IconProvider + ThemeManager**
  统一图标加载和主题管理，带信号机制和缓存
  -> [设计文档](docs/01-规划/全局IconProvider和ThemeManager设计.md)
  -> [任务计划](task_plan.md) 阶段 1.6

## 已完成

- [x] **UI 布局重构**
  全 QADS → QSplitter 混合布局，活动栏/侧边栏/底部面板独立为普通 QWidget
  -> [设计文档](docs/01-规划/UI布局重构方案.md)
  (commit 3b526e1)

- [x] **SARibbon 主窗口改造**
  QMainWindow → SARibbonMainWindow，Ribbon 功能区替代传统菜单栏/工具栏
  (commit 7e15c75)

- [x] **core → etest_core 重命名**
  统一 CMake 目标命名规范，涉及 12 个 CMakeLists.txt
  (commit 466f6ce)

- [x] **topology-demo 浅色主题 + SVG 图标修复**
  添加 resource.qrc、AUTORCC、setDarkTheme(false)
  (commit 7e15c75)
