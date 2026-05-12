# PDF文本提取操作指南

## 概述

将恩菲特产品发布目录中的PDF手册（用户手册、软件参考手册）批量提取为纯文本文件，保存到对应目录的 `extracted/` 文件夹下。

## 前置条件

Python 环境已安装 `pdfplumber` 库：

```bash
pip install pdfplumber
```

## 脚本位置

```
enpht/extract_pdfs.py
```

## 使用方法

```bash
python enpht/extract_pdfs.py <目标基目录>
```

### 示例

```bash
# 提取 AD 目录
python enpht/extract_pdfs.py "D:/trae_workspace/etest-demo/enpht/AD"

# 提取 DA 目录
python enpht/extract_pdfs.py "D:/trae_workspace/etest-demo/enpht/DA"

# 提取 1553B 目录
python enpht/extract_pdfs.py "D:/trae_workspace/etest-demo/enpht/1553B"

# 提取 CAN 目录
python enpht/extract_pdfs.py "D:/trae_workspace/etest-demo/enpht/CAN"
```

## 脚本工作原理

### 1. 目录扫描

脚本遍历目标基目录下的所有子目录，跳过 `extracted/` 目录，用正则表达式 `发布文件-([\w-]+)-V` 从目录名中提取产品型号。

**示例：**
| 目录名 | 提取的型号 |
|--------|-----------|
| `发布文件-EPH5033A-T-V1.00` | `EPH5033A-T` |
| `发布文件-CAN-V1.20` | `CAN` |
| `发布文件-1394B-V1.11` | `1394B` |
| `发布文件-EPH5175B-V1.03` | `EPH5175B` |

### 2. PDF 查找

在每个发布目录中，按以下优先级查找 `Doc/` 或 `doc/` 文件夹：
1. 直接查找 `{目录}/Doc/` 或 `{目录}/doc/`
2. 如果找不到，递归搜索子目录（处理嵌套结构如 `发布目录/发布目录/Doc/`）

然后递归收集所有 PDF 文件。

### 3. 类型识别

根据 PDF 文件名中的关键词分类：
- **用户手册**（含 "用户手册"）→ 输出为 `_user.txt`
- **软件参考手册 / 参考手册**（含 "软件参考手册" 或 "参考手册"）→ 输出为 `_api.txt`
- **其他类型** → 自动跳过（如 PNP 使用手册等）

### 4. 输出文件命名

输出文件格式：`{型号}{-变体}_{类型}.txt`

**变体提取逻辑：**
1. 去除 PDF 文件名中的中文关键词和 `.pdf` 后缀
2. 将 `EP-H` 统一为 `EPH` 以便匹配
3. 用目录提取的型号名匹配文件名开头，剩余部分作为变体
4. 去除版本号后缀（如 `-V1.00`、`V1.00`）
5. API参考手册始终不使用变体

**命名示例：**

| PDF 文件名 | 目录型号 | 输出文件名 |
|-----------|---------|-----------|
| `用户手册-EPH5272-24T8R-V1.00.pdf` | EPH5272 | `EPH5272-24T8R_user.txt` |
| `用户手册-EPH5272[16T16R]-V1.03.pdf` | EPH5272 | `EPH5272-[16T16R]_user.txt` |
| `用户手册-EPH5272[21T11R]-V1.02.pdf` | EPH5272 | `EPH5272-[21T11R]_user.txt` |
| `软件参考手册-EPH5272-V1.14.pdf` | EPH5272 | `EPH5272_api.txt` |
| `用户手册-EPH5141-T-V1.00.pdf` | EPH5141 | `EPH5141-T_user.txt` |
| `用户手册-EPH5141_2_3-V2.01.pdf` | EPH5141 | `EPH5141-2_3_user.txt` |
| `EP-H5273用户手册.pdf` | EPH5273 | `EPH5273_user.txt` |
| `用户手册-CAN-V1.17.pdf` | CAN | `CAN_user.txt` |

### 5. 文本提取

使用 `pdfplumber` 逐页提取文本，每页添加 `--- Page N ---` 分隔标记，UTF-8 编码保存。

### 6. 重复检查

如果输出文件已存在，自动跳过该 PDF 避免重复提取。

## 已提取的目录

| 基目录 | 提取文件数 | 型号列表 |
|--------|-----------|---------|
| `enpht/AD` | 2 | EPH5033A-T |
| `enpht/DA` | 8 | EPH5172, EPH5173, EPH5175B, EPH5177 |
| `enpht/SerialPort` | 2 | EPH5276H |
| `enpht/A429` | 4 | EPH5272 (含3个变体) |
| `enpht/CAN` | 2 | CAN |
| `enpht/1553B` | 2 | EPH5273 |
| `enpht/FlexRay` | 2 | EPH5285 |
| `enpht/IO` | 9 | EPH5121, EPH5125B, EPH5127, EPH5141 |
| `enpht/1394B` | 2 | 1394B |

## 新增目录的操作步骤

1. 将新的产品发布目录放入对应基目录（如 `enpht/AD/`、`enpht/DA/`）
2. 运行脚本：
   ```bash
   python enpht/extract_pdfs.py "D:/trae_workspace/etest-demo/enpht/对应目录"
   ```
3. 检查 `extracted/` 目录下的输出文件
