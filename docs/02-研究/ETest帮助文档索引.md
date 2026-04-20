# ETest帮助文档索引

本目录包含从凯云ETest官方帮助文档整理的详细技术资料，原文档位于 `../ETest_Help_Docs/` 目录。

## 文档结构概览

ETest帮助文档按以下主题组织：

### 概述
- [简介](ETest_Help_Docs/概述/简介.md) - ETest基本介绍
- [系统组成](ETest_Help_Docs/概述/系统组成.md) - 系统架构和组件
- [主要功能](ETest_Help_Docs/概述/主要功能.md) - 核心功能说明
- [安装配置](ETest_Help_Docs/概述/安装配置.md) - 安装和配置指南
- [快速入门](ETest_Help_Docs/概述/快速入门.md) - 快速开始使用
- [常见问题](ETest_Help_Docs/概述/常见问题.md) - 常见问题解答

### 项目及文件管理
- [项目管理](ETest_Help_Docs/项目及文件管理/项目管理.md) - 项目创建和管理
- [源代码管理](ETest_Help_Docs/项目及文件管理/源代码管理.md) - Git集成和版本控制
- [复用库管理](ETest_Help_Docs/项目及文件管理/复用库管理.md) - 资源复用管理
- [文件管理](ETest_Help_Docs/项目及文件管理/文件管理.md) - 文件操作和管理
- [常见问题](ETest_Help_Docs/项目及文件管理/常见问题.md) - 管理相关问题

### 环境配置
#### 设备配置
- [FlexRay](ETest_Help_Docs/环境配置/设备/FlexRay.md) - FlexRay总线配置
- [1553B总线](ETest_Help_Docs/环境配置/设备/1553B/) - 1553B总线配置
  - [简介](ETest_Help_Docs/环境配置/设备/1553B/简介.md)
  - [1553BC配置](ETest_Help_Docs/环境配置/设备/1553B/1553BC配置.md)
  - [1553RT配置](ETest_Help_Docs/环境配置/设备/1553B/1553RT配置.md)
- [1394总线](ETest_Help_Docs/环境配置/设备/1394/) - 1394总线配置
  - [基础](ETest_Help_Docs/环境配置/设备/1394/基础.md)
  - [配置](ETest_Help_Docs/环境配置/设备/1394/配置.md)
- [其他设备类型](ETest_Help_Docs/环境配置/设备/其他设备类型.md) - 其他设备配置

#### 通信协议
- [简介](ETest_Help_Docs/环境配置/通信协议/简介.md) - 协议基本概念
- [术语](ETest_Help_Docs/环境配置/通信协议/术语.md) - 协议相关术语
- [协议字段](ETest_Help_Docs/环境配置/通信协议/协议字段.md) - 字段定义和配置
- [附加属性](ETest_Help_Docs/环境配置/通信协议/附加属性.md) - 高级属性设置
- [协议API和相关函数](ETest_Help_Docs/环境配置/通信协议/协议API和相关函数.md) - API参考

### 执行程序开发
#### 执行程序类型
- [Lua脚本](ETest_Help_Docs/执行程序开发/执行程序/Lua.md) - Lua脚本开发
- [Python脚本](ETest_Help_Docs/执行程序开发/执行程序/Python.md) - Python脚本开发
- [ETL语言](ETest_Help_Docs/执行程序开发/执行程序/etl.md) - ETL语言开发
- 状态图开发
- 流程图开发
- 表格开发

#### API参考
- [Lua API](ETest_Help_Docs/执行程序开发/API/Lua%20API.md) - Lua API文档
- [Python API](ETest_Help_Docs/执行程序开发/API/Python%20API.md) - Python API文档
- 通道使用API
  - UDP、TCP、串口、CAN、1553B等通道使用

### 可视化界面开发
- [RUI界面](ETest_Help_Docs/可视化界面开发/RUI界面/) - 响应式用户界面
  - [概述](ETest_Help_Docs/可视化界面开发/RUI界面/概述.md)
  - [布局组件](ETest_Help_Docs/可视化界面开发/RUI界面/布局组件.md)
  - [通用组件](ETest_Help_Docs/可视化界面开发/RUI界面/通用组件.md)
  - [输出组件](ETest_Help_Docs/可视化界面开发/RUI界面/输出组件.md)
  - [输入组件](ETest_Help_Docs/可视化界面开发/RUI界面/输入组件.md)
  - [复杂组件](ETest_Help_Docs/可视化界面开发/RUI界面/复杂组件.md)
- [界面API](ETest_Help_Docs/可视化界面开发/界面API.md) - 界面相关API
- [网络变量](ETest_Help_Docs/可视化界面开发/网络变量.md) - 网络变量配置
- [菜单配置](ETest_Help_Docs/可视化界面开发/菜单配置.md) - 菜单系统配置
- [打包输出](ETest_Help_Docs/可视化界面开发/打包输出.md) - 应用打包发布

### 测试用例生成器
- [因果图模型](ETest_Help_Docs/测试用例生成器/因果图模型/) - 因果图测试生成
- [测试流程模型](ETest_Help_Docs/测试用例生成器/测试流程模型/) - 流程测试生成
- [组合对模型](ETest_Help_Docs/测试用例生成器/组合对模型/) - 组合测试生成
- [模糊测试](ETest_Help_Docs/测试用例生成器/模糊测试/) - 模糊测试生成

### 其他模块
- 调试与执行监控
- 实用工具
- 进阶应用
- 帮助系统

## 使用说明

1. **文档用途**：这些文档是凯云ETest的官方帮助文档，用于参考和学习
2. **文件编码**：部分文档可能存在编码问题，建议使用UTF-8编码查看
3. **图片资源**：文档中的图片位于各目录下的`images/`子目录中
4. **API参考**：API文档包含详细的函数说明和示例代码

## 统计信息

- 总目录数：10个主要分类
- 总文档数：122篇文档
- 总API数：354个API接口
- 生成时间：2026-04-18

## 注意事项

1. 这些文档仅供参考，具体实现可能需要根据实际情况调整
2. 部分功能可能依赖特定硬件或授权
3. 建议结合[ETest功能分析.md](ETest功能分析.md)一起阅读，理解整体架构

---

*原文档路径：`../ETest_Help_Docs/`，如需查看原始文件结构，请访问该目录。*