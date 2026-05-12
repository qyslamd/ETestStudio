# PDF提取任务提示词

以下是为 agent 准备的提示词，复制后直接发给 agent 即可。

---

## 提示词模板

```
请将 {目标基目录} 下的PDF文件内容提取到 {目标基目录}/extracted/ 目录中。

## 目录结构
{目标基目录}/ 下包含多个以 "发布文件-{型号}-V{版本}" 命名的子目录，
每个子目录内都有 Doc/ 或 doc/ 文件夹存放PDF手册。

## 提取规则

### 1. 扫描
- 遍历所有子目录，跳过 extracted/
- 从目录名提取型号：正则 发布文件-([\w-]+)-V

### 2. 找PDF
- 在每个发布目录中找 Doc/ 或 doc/ 文件夹
- 找不到则递归搜索（有些目录会嵌套一层同名目录）
- 收集所有PDF文件

### 3. 类型识别
- 文件名含"用户手册" → user
- 文件名含"软件参考手册"或"参考手册" → api
- 其他 → 跳过

### 4. 输出文件命名
格式：{型号}{-变体}_{类型}.txt

API参考手册始终不用变体，格式固定为 {型号}_api.txt

变体提取步骤：
1. 从PDF文件名中去掉中文关键词和 .pdf
2. 去掉首尾空白和横线
3. 将 EP-H 替换为 EPH
4. 用目录型号匹配清理后的文件名开头
5. 匹配成功则剩余部分去掉版本号后作为变体
6. 变体为空或含另一个型号则丢弃
7. 型号不匹配文件名开头时，用型号中的数字部分去文件名中定位

命名示例对照表：
| PDF文件名 | 目录型号 | 输出文件名 |
|---|---|---|
| 用户手册-EPH5272-24T8R-V1.00.pdf | EPH5272 | EPH5272-24T8R_user.txt |
| 用户手册-EPH5272[16T16R]-V1.03.pdf | EPH5272 | EPH5272-[16T16R]_user.txt |
| 用户手册-EPH5141_2_3-V2.01.pdf | EPH5141 | EPH5141-2_3_user.txt |
| 用户手册-EPH5033A-T-V100.pdf | EPH5033A-T | EPH5033A-T_user.txt |
| EP-H5273用户手册.pdf | EPH5273 | EPH5273_user.txt |
| EP-H5173用户手册v1.00.pdf | EPH5173 | EPH5173_user.txt |
| 用户手册-EPH5175B-V1.03.pdf | EPH5175B | EPH5175B_user.txt |
| 软件参考手册-EPH5175-V1.01.pdf | EPH5175B | EPH5175B_api.txt |
| EP-H5125软件参考手册.pdf | EPH5125B | EPH5125B_api.txt |
| EPH1553-API软件参考手册.pdf | EPH5273 | EPH5273_api.txt |
| 软件参考手册-EPH5272-V1.14.pdf | EPH5272 | EPH5272_api.txt |
| 用户手册-CAN-V1.17.pdf | CAN | CAN_user.txt |
| 用户手册-1394B-V1.02.pdf | 1394B | 1394B_user.txt |

### 5. 文本提取
- 使用 pdfplumber 逐页提取，每页前加 --- Page N --- 分隔
- UTF-8 保存为 .txt

### 6. 重复检查
输出文件已存在则跳过。

## 技术选型
Python + pdfplumber，未安装则先 pip install pdfplumber
```

---

## 使用示例

发给 agent 时，将 `{目标基目录}` 替换为实际路径。例如：

```
请将 D:\trae_workspace\etest-demo\enpht\AD 下的PDF文件内容提取到 D:\trae_workspace\etest-demo\enpht\AD/extracted/ 目录中。

## 目录结构
D:\trae_workspace\etest-demo\enpht\AD/ 下包含多个以 "发布文件-{型号}-V{版本}" 命名的子目录，
每个子目录内都有 Doc/ 或 doc/ 文件夹存放PDF手册。

## 提取规则
【...后面粘贴上面的提取规则...】
```

或者简化为：

```
参考 docs/02-研究/PDF提取任务提示词.md 中的规则，
将 D:\trae_workspace\etest-demo\enpht\AD 下的PDF提取到对应 extracted/ 目录中。
```
