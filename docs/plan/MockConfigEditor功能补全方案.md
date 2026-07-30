# MockConfigEditor 功能补全方案

## 问题陈述

MockConfigEditor 当前只覆盖了"编辑已有数据"的路径，缺乏"创建新配置"的能力。用户无法在编辑器内新增端口配置、新增/删除帧响应、或从 ICD 库引导式地选择字段路径。整体 UX 断裂，信息不透明，存在静默失败的场景。

## 架构回顾

MockConfigEditor 是一个独立编辑器（`IEditor`），数据源来自两个文件：

| 文件 | 来源 | 用途 |
|------|------|------|
| `topology.etopo` | 引擎加载的唯一拓扑 | 产物/UUT/端口/设备关系、设备端口 boundFrames |
| `MockResponses.emock` | Mock 响应配置 | `portBehaviors[]` — 每个端口的行为描述 |

**数据结构链：**

```
topology.etopo
  └── products[] → UUT
        └── ports[] → port 定义
              └── (connection) → device
                    └── ports[].boundFrames → 该端口允许的帧列表

MockResponses.emock
  └── portBehaviors[]
        ├── productName + deviceId + port → 定位
        ├── AD: mode + fixedValue|waveform|series
        ├── DA: fixedValue
        └── responses[] → serial/can/a429 帧响应
              ├── frameName → 触发帧
              ├── replyFrameName → 回复帧
              └── fieldValues[] → 字段工程值
```

**导航树映射：**

```
UUT (product)
  └── 端口 (port)
        ├── AD/DA → 直接编辑端口行为
        └── serial/can/a429
              └── response (frameName -> replyFrameName)
```

## 方案选项及理由

### 方案 A：渐进补全（推荐）

在当前架构上**新增**缺失的功能模块，不改动现有数据结构。每个补全模块独立，不破坏现有编辑流程。

**理由：** 改动最小、风险最可控。当前数据结构无变更需求，主要是交互层的补全。

### 方案 B：重构编辑器

推翻当前 QSplitter + QTreeWidget + QStackedWidget 布局，采用更成熟的配置编辑器模式。

**理由：** 虽然架构上更干净，但改动量大，当前阶段收益不足以抵消风险。不选。

---

## 决策记录

1. **采用方案 A** — 渐进补全
2. **新增"从 ICD 树选择"对话框** — 复用 `src/icd_utility/` 的数据结构，不做重复造轮
3. **新建配置的入口放在"端口节点选中 + toolbar 按钮"** — 不使用右键菜单（导航树本质是目录结构，右键应保持为文件操作）

---

## 设计方案

### 1. 端口级"创建配置/删除配置"

**现状：** 若拓扑中有一端口但在 emock 中无对应 behavior，导航树会显示该端口但编辑无效且静默失败。

**改动：**

1a. `buildNavTree()` 中，若当前端口在 `port_behaviors_` 中无匹配条目，端口文本尾部追加 `(未配置)` 标记。

1b. 选中"未配置"端口节点时，`edit_area_` 切换到提示页，包含两个操作：
   - "为此端口创建 Mock 配置"按钮
   - "不配置，忽略 Mock 此端口"说明文字

1c. 创建操作实现：在 `port_behaviors_` 中追加一个新条目，按端口类型写入默认值，在导航树对应 UUT 节点下原位添加子节点，自然保留展开/选中状态，选中新创建的配置。

各端口类型的默认值：

| 端口类型 | 字段 | 默认值 |
|----------|------|--------|
| DA | fixedValue | 0.0 |
| AD | mode | "fixed" |
| AD | fixedValue | 0.0 |
| serial/can/a429 | responses | [] |

1d. 已配置端口增加"删除此配置"按钮（放在编辑页底部或右侧）。

**交互流：**

```
用户选中 "AD采集口 (ad, 固定值: 0)" → 右边显示 AD 编辑页
用户选中 "DA输出口 (da, 未配置)" → 右边显示创建提示页
   └─ 点击"创建配置"→ 新建 behavior 条目 → 导航树刷新 → 选中 AD 编辑页
```

1e. **serial/can/a429 端口编辑页**（审查 #🔴1 决议）

当前 `onCurrentItemChanged()` 对 serial/can/a429 类型端口无处理，选中端口节点时编辑区保持上一个页面不变。新增端口页：

```
┌─────────────────────────────────────────────┐
│ CAN控制口 (can) — Mock CAN卡                  │
│─────────────────────────────────────────────│
│ 已有响应：                                    │
│  ┌─────────────────────────────────────────┐ │
│  │ ◉ 控制指令 -> 状态回传   2 字段  [编辑]  │ │
│  │ ◉ 电压采集 -> 电流回传   1 字段  [编辑]  │ │
│  └─────────────────────────────────────────┘ │
│                                             │
│  [+ 新建响应]     [删除此端口配置]            │
└─────────────────────────────────────────────┘
```

- 响应列表：从 `behavior["responses"]` 渲染，每行显示 frameName → replyFrameName + 字段数 + "[编辑]"
- "[编辑]"按钮等效于导航到对应 response 子节点（切换到 `frame_response_page_`）
- "新建响应"追加空响应，导航树添加子节点并选中
- "删除此端口配置"删除整个 behavior 条目，从导航树移除对应端口节点（含子节点）
- 每次端口页显示时（`onCurrentItemChanged` port 分支）重新从 `behavior["responses"]` 生成摘要列表，确保新建/删除响应后最新

### 2. Response 级"新建帧响应/删除帧响应"

**现状：** serial/can/a429 端口下响应条目只能编辑已有数据，无法新增或删除。

**改动：**

2a. 端口编辑页增加"新建响应"按钮（放在响应列表下方）。点击后追加一条空响应到 `behavior["responses"]`，导航树添加子节点，选中新节点。

2b. 帧响应编辑页增加"删除此响应"按钮（放在标题栏区域）。

2c. 新建响应的默认值：
   - `frameName` = boundFrames[0]（有 boundFrames 时）或空字符串
   - `replyFrameName` = boundFrames[1]（如果有第二个）或空字符串
   - `fieldValues` = 空数组

### 3. 字段路径从 ICD 树选择

**现状：** `fr_field_table_` 的 column 0（字段路径）是纯文本输入，无引导、无补全、不回写。

**改动：**

3a. column 0 不再可编辑。双击 column 0 弹出"从 ICD 选择信号"对话框。

3b. 对话框：
   - 左侧 ICD 帧树（`icd_repo_->frames()`），展开为 `frame → signal → member` 三级
   - 右侧预览：选中节点后显示其完整路径（如 `signals.voltage`）
   - "确定"后回填到表格 column 0

3c. 对话框"确定"按钮直写 column 0 文本到表格，并同步更新 `port_behaviors_[idx]` 中对应 `fieldValues[row].nodePath`，不经过 `onFrFieldChanged` 信号链。

3d. column 0 修改后触发 `updateHexPreview` 刷新。

### 4. UUT 节点配置概览页

**现状：** 选中 UUT 节点时右边空白。

**改动：**

选中 UUT 节点时 `edit_area_` 切换到一个只读概览页：

```
┌─────────────────────────────────────┐
│ 端口             状态    配置        │
├─────────────────────────────────────┤
│ 串口控制口       ✅    1 条响应     │
│ CAN控制口        ⚠️   未配置        │
│ A429接收口       ✅    1 条响应     │
│ AD采集口         ✅    固定值 0V    │
│ DA输出口         ⚠️   未配置        │
│                                    │
│     [刷新]                         │
└─────────────────────────────────────┘
```

### 5. 边界与兼容性

- **新建配置需原位插入节点**：在父 UUT 节点下 addChild，不调用 clear() 全量重建，自然保留选中/折叠状态。
- **删除配置/响应需确认**：QMessageBox::question 确认后才执行。
- **旧格式兼容**（`frameId` + `responseHex` 无 `fieldValues`）：保持当前处理方式，不作修改。
- **空 emock 文件**：若文件不存在或 `portBehaviors` 为空数组，所有端口均显示为"未配置"。

## 实施计划

### 阶段 1：端口级配置创建/删除

**涉及文件：** `src/app/editors/MockConfigEditor.h`、`src/app/editors/MockConfigEditor.cpp`

#### 任务 1.1：端口"未配置"标记

- **改动：** `buildNavTree()` 中，在查找 `port_behaviors_` 匹配之后、设置端口文本之前，增加 `hasBehavior` 判断
- AD/DA 分支：无 behavior 时端口文本显示 `端口名 (ad, 未配置)` 而非 `端口名 (ad, 固定值: 0)`
- serial/can/a429 分支：无 behavior 时端口文本显示 `端口名 (can, 未配置)`，跳过 response 子节点创建
- 插入位置：在 `portItem` 创建后、`deviceType` 分支之前

#### 任务 1.2：创建提示页

- **新增：** `edit_area_` 中增加一个 `QWidget`（`unconfig_hint_page_`），包含一个 QLabel（提示文字）和一个"创建配置"按钮
- `onCurrentItemChanged()` 的 port 分支中：deviceType 不限，若 `findCurrentBehaviorIndex()` 返回 -1，切换到提示页

#### 任务 1.3：创建操作实现

- 点击"创建配置"按钮 → slot 函数：
  1. 在 `port_behaviors_` 中 append 新 JSON 条目（含默认值）
  2. 在导航树对应 UUT 节点下原位 `addChild` 新端口节点（此时树已构建完毕，不清除/重建）
  3. 选中新创建的端口节点
  4. `markModified()`

#### 任务 1.4：serial/can/a429 端口编辑页

- **新增：** `edit_area_` 中增加 `frame_port_page_`
- 布局包含：
  - 端口信息 QLabel（名称、设备类型）
  - 响应摘要列表：从 `behavior["responses"]` 动态生成，每行显示 frameName + "→" + replyFrameName + 字段数 + "[编辑]"按钮
  - "新建响应"按钮 → slot 在阶段 2 实现，阶段 1 中先不暴露或连接空占位 slot
  - "删除此端口配置"按钮 → QMessageBox 确认后删除
- `onCurrentItemChanged()` 的 port 分支新增 serial/can/a429 处理，切换到该页
- 每次切换到该页时重新从 `behavior["responses"]` 生成摘要列表

#### 任务 1.5：端口配置删除

- DA 编辑页底部加"删除此配置"按钮
- AD 编辑页底部加"删除此配置"按钮
- serial/can/a429 端口页已有"删除此端口配置"按钮（任务 1.4）
- 删除流程（通用 slot）：
  1. `QMessageBox::question` 确认
  2. 从 `port_behaviors_` 移除对应索引
  3. 从导航树移除对应 `portItem`（`parent->removeChild` + `delete`）
  4. 重置当前编辑标识（`current_product_name_` 等清空）
  5. 切换到空页或提示页
  6. `markModified()`

---

### 阶段 2：Response 新增/删除

**涉及文件：** `src/app/editors/MockConfigEditor.h`、`src/app/editors/MockConfigEditor.cpp`

#### 任务 2.1：端口页"新建响应"按钮

- 复用阶段 1 已添加的"新建响应"按钮
- slot 实现：
  1. 在 `port_behaviors_[idx]["responses"]` 中 append 新 response
  2. 默认值：`frameName` = boundFrames[0]（若存在），`replyFrameName` = boundFrames[1]（若存在），`fieldValues` = []
  3. 在导航树对应端口节点下 `addChild` 新 response 子节点
  4. 选中新节点触发 `onCurrentItemChanged` 切换到帧响应编辑页
  5. `markModified()`

#### 任务 2.2：帧响应编辑页"删除此响应"按钮

- 在 `frame_response_page_` 的 `fr_info_label_` 旁增加"删除此响应"按钮
- slot 实现：
  1. `QMessageBox::question` 确认
  2. 从 `port_behaviors_[idx]["responses"]` 移除对应项
  3. 从导航树移除对应 `respItem`
  4. 切换到父端口节点
  5. `markModified()`

---

### 阶段 3：ICD 信号选择对话框

**涉及文件：** `src/app/editors/MockConfigEditor.h`、`src/app/editors/MockConfigEditor.cpp`

#### 任务 3.1：对话框实现

- 新增 `showIcdSignalPicker(const QString& currentPath) → QString` 方法（返回选择的路径，空表示取消）
- 对话框使用 `QDialog`，布局：
  - 左侧 `QTreeWidget`：从 `icd_repo_->frames()` 加载，递归展开 frame → nodes（不限层级深度）
  - 右侧 `QLabel`：选中节点后显示完整节点路径
  - "确定"/"取消"按钮
- 对话框只读展示 ICD 结构，不修改数据

#### 任务 3.2：column 0 交互修改

- `fr_field_table_` column 0 所有 item 的 flags 移除 `Qt::ItemIsEditable`
- column 0 的 `itemDoubleClicked` 信号连接到 `onFrNodePathDoubleClicked` slot
- slot 实现：
  1. 调用 `showIcdSignalPicker(item->text())`
  2. 若返回非空路径：
     - `item->setText(path)` (更新 UI)
     - 直写 `port_behaviors_[idx]["responses"][i]["fieldValues"][row]["nodePath"]` (更新数据)
     - `markModified()`
     - `updateHexPreview(current_reply_frame_name_)`

#### 任务 3.3：column 0 触发 hex 刷新

- `onFrNodePathDoubleClicked` 中，完成路径写入后调用 `updateHexPreview`，确保 hex 预览随字段路径变化更新

---

### 阶段 4：UUT 概览页

**涉及文件：** `src/app/editors/MockConfigEditor.h`、`src/app/editors/MockConfigEditor.cpp`

#### 任务 4.1：概览页实现

- **新增：** `edit_area_` 中增加 `uut_overview_page_`
- 使用 `QTableWidget` 或 `QFormLayout` 展示端口列表：
  - 遍历 `topology_doc_["products"][0]["ports"]`，对每个端口：
    - 获取端口名 `portName`
    - 通过 `connections[]` 找到匹配的连接条目（`product` == 产品名 && `port` == portName），从中取出 `device` 字段
    - 通过 `devices[]` 找到该设备的 `id`（deviceId）
    - 以 `(productName, deviceId, portName)` 三字段在 `port_behaviors_` 中查找匹配条目
    - 有匹配 → 显示 ✅ + 配置摘要
    - 无匹配 → 显示 ⚠️ + "未配置"
  - 此匹配逻辑与 `buildNavTree()` 第 332-349 行一致，可提取公共辅助函数复用
- 可选：点击某行端口跳转到对应配置页

#### 任务 4.2：UUT 节点选中切换

- `onCurrentItemChanged()` 的 UUT type 分支：切换到 `uut_overview_page_`

## 审查记录

### 审查轮次 1

#### 🔴 1. serial/can/a429 端口选中时缺少端口级编辑页 → 已解决（选项 A）

**决议：** 新增 serial/can/a429 端口编辑页，布局包含端口信息、已有响应摘要列表、"新建响应"和"删除此端口配置"按钮。

**详情：** 见"设计方案 1e"。

#### 🔴 2. 导航树重建后的状态恢复方案不完整 → 已解决（选项 A）

**决议：** 采用原位插入/删除节点（addChild / removeChild），不调用 clear() + 全量重建，自然保留选中和折叠状态。仅 `openFile()` 的初始加载走全量重建（一次，无恢复问题）。

**详情：** 见"设计方案 1c"（原位添加节点 + 自然保留状态）。

#### 🔴 3. column 0 的回写路径矛盾 → 已解决（选项 A）

**决议：** ICD 对话框的"确定"按钮直接执行：
1. `fr_field_table_->item(row, 0)->setText(path)`（更新 UI）
2. 直写 `port_behaviors_[idx]` 中对应 `fieldValues[row].nodePath`（更新数据）
3. `markModified()` + `updateHexPreview()`

不经过 `onFrFieldChanged` 信号链。`onFrFieldChanged` 保持只处理 column 1（工程值）不变。

#### 🔴 4. AD/DA 空 behavior 显示假数据 → 已解决（选项 A）

**决议：** 端口级统一处理：若 `port_behaviors_` 中无匹配条目，导航树显示 `(未配置)` 标记，不生成假数值摘要。所有端口类型共享同一语义。
