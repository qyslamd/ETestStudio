# ETest Demo — 任务计划

## 项目概述
仿凯云ETest测试系统，Qt/C++实现，CMake构建。目标覆盖6个开发阶段。

---

## 当前进度总览

| 阶段 | 状态 | 完成度 |
|------|------|--------|
| 1.1 开发环境搭建 | ✅ 已完成 | 100% |
| 1.2 核心基础设施 | ✅ 已完成 | 100% |
| 1.3 主窗口框架 | ✅ 已完成 | 100% |
| 2 设备管理 | ❌ 未开始 | 0% |
| 3 ICD协议管理 | ❌ 未开始 | 0% |
| 4 测试用例编辑 | ❌ 未开始 | 0% |
| 5 测试执行 | ❌ 未开始 | 0% |
| 6 测试与优化 | ❌ 未开始 | 0% |

---

## 阶段1.3 完成总结

### 已完成项目
- [x] 通用插件框架（PluginManager + IPlugin）
- [x] 主窗口6区布局（QADS dock: 活动栏/侧边栏/编辑区/底部面板/属性面板/状态栏）
- [x] 项目管理（.etproj格式、创建/打开/关闭/最近项目）
- [x] 文件浏览器（QFileSystemModel、右键CRUD、双击打开、**文件类型图标**）
- [x] 多标签编辑器（QScintilla、语法高亮、脏标记、拖拽tab）
- [x] 活动栏与视图切换（7按钮、toggle显隐）
- [x] 底部三面板（OutputPanel日志、ProblemsPanel、**TerminalPanel嵌入式PTY终端**）
- [x] **全局搜索**（SearchWidget、项目文本搜索、点击跳转行、Ctrl+Shift+F）
- [x] **设置页面**（SettingsWidget独立对话框、分类树+表单、ConfigManager双向绑定）
- [x] **文件类型图标**（FileTypeIconProvider、11类Seti风格彩色SVG图标）
- [x] **会话持久化**（captureSessionData/writeSessionFile/restoreSession，恢复编辑器/侧边栏/面板状态）
- [x] 窗口大小/位置/布局状态持久化（ConfigManager）
- [x] 单元测试（10+测试文件）

### 遗留可选项（不影响阶段完成）
- [ ] SidebarWidget Git/调试/扩展页仍为占位符（P2，后续阶段按需实现）
- [x] 异常框架补全（1.2遗留）— GlobalExceptionHandler已实现
- [x] 自动备份功能（1.2遗留）— BackupManager已实现

---

## 阶段2 设备管理（下一阶段）

### 核心任务
1. 串口设备插件（RS232/422/485）— 基于ISerialDevicePlugin接口
2. 网络设备插件（TCP Client/Server, UDP）
3. 模拟量/开关量设备插件（AI/AO/DI/DO）— 基于IADevicePlugin/IDADevicePlugin
4. CAN设备插件（占位，需SDK集成）
5. 设备管理器侧边栏视图
6. 设备自检功能
7. 设备配置导入导出

### 预估工期：2.5周

---

## 阶段3 ICD协议管理

### 核心任务
1. ICD层级数据模型（协议→消息→字段）
2. 表格化图形编辑器（类Excel）
3. XML/JSON/YAML导入导出
4. Excel导入导出（对齐ETest模板）
5. C/C++头文件导出
6. 打包/解包接口
7. CRC/校验和/异或校验

### 预估工期：3周

---

## 阶段4 测试用例编辑

### 核心任务
1. .etcase JSON格式定义
2. 左右布局：动作库 + 表格编辑器
3. 动作类型：基本动作、IO动作、测试断言、流程控制
4. Lua引擎集成（隔离VM）
5. 参数化 {{param}} 语法
6. 拖拽、复制粘贴、撤销重做

### 预估工期：2.8周

---

## 阶段5 测试执行

### 核心任务
1. 单线程执行引擎 + 后台Worker
2. 断点与单步调试
3. 实时监控面板（日志/通道数据/变量/进度）
4. SQLite存储执行数据
5. HTML/Excel/PDF报告生成

### 预估工期：2.7周

---

## 阶段6 测试与优化

### 核心任务
1. 全模块回归测试
2. 全链路集成测试
3. 跨平台兼容（Windows 7/10/11, Ubuntu）
4. 压力/稳定性测试（24h, 1000次迭代）
5. 用户体验优化
6. 打包与文档

### 预估工期：2.4周

---

## 决策记录

| 日期 | 决策 | 原因 |
|------|------|------|
| 2026-05-07 | 全局字体设为微软雅黑过渡 | 默认宋体不好看，后续可替换为自定义ttf |
| 2026-05-07 | QADS标题栏按钮按objectName单独隐藏 | 整个titleBar()->hide()会连同tab一起隐藏 |
| 2026-05-07 | ActivityBar与Sidebar保持为独立CDockWidget | 合并后toggle行为异常 |
| 2026-05-08 | 设置页面改为独立QDialog而非侧边栏嵌入 | 类似VS Code以独立Tab打开设置，简化交互 |
| 2026-05-08 | 文件图标采用Seti风格（填充+底部彩色标签） | 纯描边图标在16px下难以区分，彩色标签一目了然 |
| 2026-05-08 | 全局搜索为主线程同步搜索（MVP） | 项目<10K文件够用，后续可改为QThreadPool |
| 2026-05-09 | 会话持久化采用JSON格式存入AppData/Local/session.json | 轻量、可读、与ConfigManager分离，关闭确认后才写盘 |
| 2026-05-09 | 崩溃/日志路径从Documents迁移到AppData/Local | 崩溃dump和日志是机器相关数据，不适合放用户文档目录 |
| 2026-05-09 | CrashHandler添加VEH+MiniDump | SetUnhandledExceptionFilter在gtest等SEH环境中不够优先，VEH在链最前端 |
| 2026-05-09 | 发布采用RelWithDebInfo而非Release | 带PDB的优化构建，崩溃dump可在任意机器用WinDbg/VS调试 |
| 2026-05-09 | disabled测试脚本加--gtest_catch_exceptions=0 | gtest默认SEH拦截崩溃，导致CrashHandler无法触发 |
