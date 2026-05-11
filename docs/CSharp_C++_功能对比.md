# C# (EphAutoTest.ICD) ↔ C++ (icd_utility) 核心功能逐项对比

> 生成日期：2026-05-12
> 目标：精确定位 C++ 实现的功能缺口，评估优先级

---

## 一、Schema 数据模型

| # | 功能项 | C# (EphAutoTest.ICD) | C++ (icd_utility) | 状态 |
|---|--------|---------------------|-------------------|------|
| 1.1 | `Offset`/`StartBit`/`BitWidth` 读取 | `ICDWord.Offset/StartBit/BitWidth` | `xml_parser:77-87`, `json_parser:64-74` | ✅ |
| 1.2 | `Type` 字符串 → `ValueType` 映射 | `IcdNode.ToValueType()` 支持 12 种类型名 | `xml_parser:63-72`, `json_parser:50-59` — 缺少 `short`/`char`/`dword`/`long`/`ulong`/`bool` | ⚠️ 不全 |
| 1.3 | `Name`/`Description` 读取 | `ICDWord.Name/Description` | `xml_parser:98-99`, `json_parser:84-85` | ✅ |
| 1.4 | `IsScaled`/`ScaleA`/`ScaleB` 读取 | `ICDWord.IsScaled/ScaleA/ScaleB` | `xml_parser:101-122`, `json_parser:86,97-98` | ✅ |
| 1.5 | `GroupName`/`SystemName` 读取 | `ICDWord.GroupName/SystemName` | `xml_parser:105-106`, `json_parser:88-89` | ✅ |
| 1.6 | `Unit`/`Min`/`Max` 读取 | `ICDWord.Unit/Min/Max` | `xml_parser:107,123-132`, `json_parser:90,99-100` | ✅ |
| 1.7 | `Tag` 读取（int → enum） | `ICDWord.Tag` → `IcdTag` | `xml_parser:133-137`, `json_parser:95` | ⚠️ C++ `Tag` enum 缺少 `offset_code`(11), `sum3`(22), `hex_mode`(60), `diff`(80) |
| 1.8 | `ValueTextList` 读取 | `ICDWord.ValueTextList` | `xml_parser:108`, `json_parser:91` | ✅ |
| 1.9 | `ScaleFormula`/`ScaleConveror`/`LinkTo` 读取 | `ICDWord.ScaleFormula/ScaleConveror/LinkTo` | `xml_parser:109-111`, `json_parser:92-94` | ✅ |
| 1.10 | `DefauleValue` 读取 | `ICDWord.DefauleValue` | ❌ | ❌ |
| 1.11 | `Childs` 递归读取 | `ICDWord.Childs.Item[]` | `xml_parser:139-145`, `json_parser:102-111` | ✅ |
| 1.12 | `IcdConfig.xml` 配置文件 `FileInfo[]` 读取 | `SchemaFileInfo(Name/ID/Type/ByteOrder/Path)` | `xml_parser:178-201`, `json_parser:129-161` | ✅ |
| 1.13 | `FileInfo.Enable`/`WordType` 读取 | `SchemaFileInfo.Enable/WordType` | ❌ | ❌ |
| 1.14 | `FrameType` 映射 | 0=none, 1=data, 2=cmd, 3=data_cmd, 4=config | `xml_parser:23-33`, `json_parser:145-155` — 只映射 1/2/3，缺少 0 和 4 | ⚠️ 缺 config |
| 1.15 | `ByteOrder` 映射 | 0=LittleEndian, 1=BigEndian | `xml_parser:35-37`, `json_parser:158` | ✅ |

---

## 二、配置加载管道

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 2.1 | 两级配置：先读 IcdConfig.xml → 遍历加载各帧 XML | `IcdManager.Init()` → `new IcdFrame(..., curData)` | `loader:52-95` → `parse_config` → `parse_frame` → `build_repository` | ✅ |
| 2.2 | 配置文件相对路径拼接 baseDir | `Path.GetDirectoryName() + @"\" + item.Path` | `loader:68: base_dir / file_entry.path` | ✅ |
| 2.3 | 支持名称过滤（仅加载匹配的帧） | `IcdManager(filter)` → `IsInclude(name)` | ❌ | ❌ |
| 2.4 | 按 `ID` 索引帧 | `_framesWithId` dictionary | `repository:34-35 frames_by_id_` | ✅ |
| 2.5 | 保存帧回 XML | `IcdManager.SaveToXMLFile()` | ❌ | ❌ |

---

## 三、核心运行时模型

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 3.1 | `Frame` 包含 root 节点列表 | `IcdFrame._childNodes` | `Frame::roots_` | ✅ |
| 3.2 | `Frame` 包含所有节点扁平索引 | `IcdFrame._allChildNodes` | `Frame::nodes_` | ✅ |
| 3.3 | `Node` 树形结构（parent + children） | `IcdNode._parentNode, _childNodes` | `Node::parent_, children_` | ✅ |
| 3.4 | `Node` 按 name 查找（递归树） | `IcdFrame.Find(name)` | `Node::find(name)` | ✅ |
| 3.5 | `Repository` 按 name/id 查找 Frame | `IcdManager.FindFrame()`, `Find(id)` | `Repository::find(name/id)` | ✅ |
| 3.6 | `Repository` 按 frame+node name 交叉查找 | `IcdManager.Find(systemName, signalName, frameType)` | `Repository::find(frame_name, node_name)` | ✅ |
| 3.7 | `Frame` 类型区分 | 4 种: none/data/cmd/data_cmd/config | 3 种: data/cmd/data_cmd | ⚠️ 缺 config |
| 3.8 | `IcdCmdNode` 标记类（空继承） | `IcdCmdNode : IcdNode` | ❌（统一用 `Node`） | 低 |
| 3.9 | `IcdFilterFrame` 跨帧按 SystemName 分组 | `IcdFilterFrame.Init(systemName)` | ❌ | ❌ |
| 3.10 | 帧长度计算（末节点 Offset+BitWidth） | `IcdFrame.CalLength()` | ❌ | ⚠️ |
| 3.11 | 帧内部数据缓冲区 | `IcdFrame._dataCache = new byte[length]` | `Frame::decode_buffer_` | ✅ |
| 3.12 | `Node.ID`（扁平列表序号） | `IcdNode.ID` | ❌ | 低 |
| 3.13 | `Node.Modified` 脏标记 | `IcdNode.Modified` | `Node::modified_` | ✅ |
| 3.14 | `Frame.HeadNode/SumNode/CountNode` 标注 | `IcdFrame._headNode/_sumNode/_countNode` | ❌ | ❌ |

---

## 四、值解码管道

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 4.1 | `Extract`：从字节缓冲区提取原始值 | `IcdValueBase.Extract(pData)` → 子类覆盖 | `Node::decode()` → `extract_bits`/`read_integer_bits`/`read_bytes`/`read_floating` | ✅ |
| 4.2 | `Convert`：原始值 → 物理值（含缩放 ScaleA×raw+ScaleB） | `IcdValueBase.Convert()` | `Node::decode()` 不应用缩放，返回原始值 | ⚠️ 缺失 |
| 4.3 | 无符号整型解码 | `IcdUShortValue.Convert()`, `IcdUIntValue.Convert()` | `Node::decode()` word/longword/ulong_/byte_ | ✅ |
| 4.4 | 有符号整型解码（符号扩展） | `IcdShortValue.Convert()`, `IcdIntValue.Convert()` | `Node::decode()` smallint/integer/shortint → `sign_extend()` | ✅ |
| 4.5 | Bool 解码（位测试） | `IcdBoolValue.Convert()` | `Node::decode()` boolean → `extract_bits` | ✅ |
| 4.6 | Float/Double 解码（bit_cast） | `IcdFloatValue`, `IcdDoubleValue` | `Node::decode()` single/double → `read_floating` | ✅ |
| 4.7 | String 解码（bytes → string） | `IcdStringValue.Convert()` → `Encoding.Default.GetString` | `Node::decode()` string_ → 逐 byte → string | ✅ |
| 4.8 | Bytes 解码 | `IcdBytesValue.Extract()` | `Node::decode()` bytes → `read_bytes` | ✅ |
| 4.9 | 端序处理（LE/BE 字节交换） | 各子类 `ntohs()` | `read_integer_bits` 中 LE/BE 分支 `:63-75` | ✅ |
| 4.10 | 非对齐位域提取（start_bit != 0） | `BitSet64.Extract()` 按宽度分 8 个分支，掩码法 | `extract_bits()` 逐位循环 `:38-48` + 对齐路径 `:56-75` | ✅（法不同，效果等价） |
| 4.11 | 非对齐位域写回 | `BitSet64.UnExtract()` 掩码法 | `write_integer_bits()` 逐位循环 `:162-176` + 对齐路径 `:133-150` | ✅ |
| 4.12 | 缩放反向解码（物理值 → 原始值逆运算） | C# `UnConvert()` 中 `(value-ScaleB)/ScaleA` | ❌ | ❌ |
| 4.13 | Tag 特殊解码行为（Head/Length/RawCode/OffsetCode） | `IcdUShortValue.Convert()` 检查 `Tag.RawCode`, `IcdNode.ExtractInitValue()` 处理 Head/Length 初始值 | ❌ C++ `Node::decode()` 不检查 `tag_` | ❌ |

---

## 五、值编码管道

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 5.1 | `set_value`：设值并写回缓冲区 | `IcdValueBase.DoSetValue()` → `UnConvert()` → `UnExtract()` | `Node::set_value()` `:308-441` | ✅ |
| 5.2 | `UnConvert`：物理值 → 原始值 | 各子类 `UnConvert()` 含缩放逆算 `(v-ScaleB)/ScaleA` | `Node::set_value()` 无缩放逆算 | ⚠️ 无缩放 |
| 5.3 | `UnExtract`：原始值写回缓冲区 | `IcdBitValue.UnExtract()` → `BitSet64.UnExtract()` | `write_integer_bits()` / `write_bytes()` | ✅ |
| 5.4 | 缩放正向编码（物理值×ScaleA+ScaleB→原始值） | C# `UnConvert()` 中 `(value-ScaleB)/ScaleA` | ❌ | ❌ |
| 5.5 | Tag 特殊编码行为（RawCode/OffsetCode 路径切换） | `IcdShortValue.UnConvert()` 检查 `Tag.RawCode` | ❌ | ❌ |
| 5.6 | 大端编码（set_value 时 BE 写回） | 各子类中 `ntohs()` | `write_integer_bits()` LE/BE `:139-148` | ✅ |
| 5.7 | 设值后递归标记子节点 modified | `IcdNode.OnHandleValueChanged()` | `Node::mark_children_modified()` | ✅ |

---

## 六、帧级编解码编排

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 6.1 | `Frame.decode()` 复制外部数据到内部缓冲区 | `IcdFrame.Decode(uint, int)` → `Marshal.Copy` + `DoDecode` | `Frame::decode(frame_bytes, mode)` → `decode_buffer_.assign(...)` | ✅ |
| 6.2 | 惰性解码（只标记不解析） | `IcdFrame.DoDecode(..., lazy=true)` → `item.Modified = true` | `Frame::decode(bytes, DecodeMode::lazy)` | ✅ |
| 6.3 | 惰性读取时自动触发 `resolve_value`（重新 decode） | `IcdValueBase.Value.get` → 若 `_modified` 则重新 Decode | `Node::get_value()` → 若 `modified_` 则 `resolve_value()` | ✅ |
| 6.4 | `Frame.Encode()` 整体编排：自增 Count | `IcdFrame.Encode()` → `_countNode.Value.Value++` | ❌ | ❌ |
| 6.5 | `Frame.Encode()` 整体编排：重算校验和 | `IcdFrame.Encode()` → `_sumNode.Value.Encode()` | ❌ | ❌ |
| 6.6 | Sum 校验和（累加所有字节） | `IcdSumValue.DoUnExtract()` for 循环累加 | ❌ | ❌ |
| 6.7 | Sum2 校验和（累加后取补: `0-sum`） | `IcdSumValue.DoUnExtract()` → `(byte)(0 - _rawValue)` | ❌ | ❌ |
| 6.8 | Sum3 校验和（跳过帧头/长度区域） | `IcdSumValue.DoUnExtract()` 中 `Tag.Sum3 → tmpOffset = _offset_follow_head` | ❌ | ❌ |
| 6.9 | XOR 校验（所有字节 XOR） | `IcdXorValue.UnExtract()` for 循环 XOR | ❌ | ❌ |
| 6.10 | XOR1 校验（跳过帧头/长度区域 XOR） | `IcdXorValue.UnExtract()` → tmpOffset | ❌ | ❌ |
| 6.11 | XOR2 校验（跳过帧头/长度 XOR 后取反） | `IcdXorValue.UnExtract()` → `(byte)~_rawValue` | ❌ | ❌ |
| 6.12 | `Frame.Encode()` 输出到外部缓冲区 | `IcdFrame.Encode(byte[] data)` → `_dataCache.CopyTo` | ❌ | ❌ |
| 6.13 | 帧头 Head 初始值提取（从 ValueTextList 解析） | `IcdNode.ExtractInitValue()` 中 `Tag.Head` 分支 | ❌ | ❌ |
| 6.14 | Length/Count 初始值提取 | `IcdNode.ExtractInitValue()` 中 `Tag.Length`/`Tag.InitValue` 分支 | ❌ | ❌ |

---

## 七、查询与导航

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 7.1 | Frame 内按节点 name 查找 | `IcdFrame.Find(string name)` | `Frame::find(string_view name)` | ✅ |
| 7.2 | Repository 内 Frame 名模糊匹配（Contains） | `FindFrame()` → `item.Name.Contains(systemName)` | `Repository::find(name)` 精确匹配 | ⚠️ |
| 7.3 | URL 式查找 `"systemName/signalName"` | `IcdManager.Find(url, frameType)` 按 `'/'` 分割 | ❌ | ❌ |
| 7.4 | 精确匹配查找 | `IcdManager.Pos(frameName, signalName, frameType)` | `Repository::find(frame_name, node_name)` | ✅ |
| 7.5 | 全局查找信号（所有帧中搜节点名） | `IcdManager.FindAll(signalName, frameType)` | ❌ | ❌ |
| 7.6 | 按信号名模板收集（LookUp） | `IcdManager.LookUp(signalTemplate, frame, frameType)` | ❌ | ❌ |

---

## 八、事件与通知

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 8.1 | `IIcdValue.OnChanged` 事件 | `IcdValueBase.OnChanged` | ❌ | ❌ |
| 8.2 | `IcdNode.OnValueChanged` 传播 | `IcdNode.OnHandleValueChanged()` 递归标记父子 | `Node::mark_children_modified()` | ⚠️ C++ 有标记但无事件广播 |
| 8.3 | `IcdFrame.ListChanged` (IBindingList WPF 绑定) | `IcdFrame._listChanged` | ❌ | 低 |

---

## 九、扩展与工具

| # | 功能项 | C# | C++ | 状态 |
|---|--------|----|-----|------|
| 9.1 | ScaleConvertor 插件式缩放转换 | `ConvertorFactory` / `IConvert` 接口 | `NodeAttrs::scale_convertor` 未连接 | ❌ |
| 9.2 | `BitSet64` 独立位操作 | `BitSet64.Extract/UnExtract` | `extract_bits()`/`write_integer_bits()`（集成在 node.cpp） | ✅ 功能等价 |
| 9.3 | `IValueConvertor` 值转换接口 | `IValueConvertor` interface | ❌ | 低 |
| 9.4 | `object Clone()` 深拷贝 | `IcdNode.Clone()` | ❌ | 低 |

---

## 十、类型系统覆盖

| # | C# ValueType | C++ 对应 | decode | set_value | 状态 |
|---|-------------|----------|--------|-----------|------|
| 10.1 | `ftBoolean` | `boolean` | ✅ | ✅ | ✅ |
| 10.2 | `ftByte` (uint8) | `byte_` | ✅ | ✅（存 uint64） | ✅ |
| 10.3 | `ftBytes` | `bytes` | ✅ | ✅ | ✅ |
| 10.4 | `ftWord` (uint16) | `word` | ✅ | ✅ | ✅ |
| 10.5 | `ftShortint` (int16) | `smallint` | ✅ | ✅ | ✅ |
| 10.6 | `ftSmallint` (int8 / SByte) | `shortint` | ✅ | ✅ | ✅ |
| 10.7 | `ftLongWord` (uint32) | `longword` | ✅ | ✅ | ✅ |
| 10.8 | `ftInteger` (int32) | `integer` | ✅ | ✅ | ✅ |
| 10.9 | `ftLong` (int64) | — | ❌ 无 decode case | ❌ 无 set_value case | ❌ |
| 10.10 | `ftUlong` (uint64) | `ulong_` | ✅ | ✅ | ✅ |
| 10.11 | `ftSingle` (float) | `single` | ✅ | ✅ | ✅ |
| 10.12 | `ftFloat` (double) | `double_` | ✅ | ✅ | ✅ |
| 10.13 | `ftString` | `string_` | ✅ | ✅ | ✅ |
| 10.14 | `ftUnknown` | `unknown` | ✅（返回 error） | ✅（返回 error） | ✅ |

---

## 十一、测试覆盖

| # | 功能项 | C++ 测试文件 | 状态 |
|---|--------|-------------|------|
| 11.1 | word decode / set_value | `test_node_value.cpp:24-48,217-224` | ✅ |
| 11.2 | smallint decode (int16) | `test_node_value.cpp:226-233` | ✅ |
| 11.3 | longword decode (BE) | `test_node_value.cpp:235-243` | ✅ |
| 11.4 | integer decode (int32) | `test_node_value.cpp:245-253` | ✅ |
| 11.5 | byte decode | `test_node_value.cpp:255-262` | ✅ |
| 11.6 | shortint decode (int8) | `test_node_value.cpp:264-271` | ✅ |
| 11.7 | boolean decode (非对齐 bit) | `test_node_value.cpp:273-280` | ✅ |
| 11.8 | single / double decode | `test_node_value.cpp:282-300` | ✅ |
| 11.9 | bytes decode / set_value | `test_node_value.cpp:125-156,302-314` | ✅ |
| 11.10 | string decode / set_value | `test_node_value.cpp:158-179,317-323` | ✅ |
| 11.11 | lazy decode mode | `test_node_value.cpp:50-72` | ✅ |
| 11.12 | set_value 传播 modified | `test_node_value.cpp:82-123` | ✅ |
| 11.13 | 越界/类型不匹配错误路径 | `test_node_value.cpp:74-80,182-215,326-359` | ✅ |
| 11.14 | 兼容性加载真实 XML | `test_compat_snapshot.cpp` | ✅ |
| 11.15 | 缩放解码测试 | ❌ | ❌ |
| 11.16 | 校验和（sum/xor）测试 | ❌ | ❌ |
| 11.17 | Tag 特殊行为测试 | ❌ | ❌ |
| 11.18 | Frame::encode 测试 | ❌ | ❌ |
| 11.19 | 帧长度计算测试 | ❌ | ❌ |

---

## 优先级建议

### 🔴 影响正确性（建议立即修复）

| 编号 | 问题 | 影响 |
|------|------|------|
| 1.2 | `Type` 映射缺少 `short`/`char`/`dword`/`long`/`ulong`/`bool` | 真实 XML 中这些类型会 fallback 到 `unknown`，导致 decode 失败或得到错误结果 |
| 1.7 | `Tag` enum 缺 `offset_code`/`sum3`/`hex_mode`/`diff` | 真实 XML 中对应 Tag 值解析为错误 enum 值或失败 |
| 1.14 | `FrameType` 缺 `config`(4) | 真实 XML 中 type=4 的帧会被映射为 data |
| 10.9 | `long`(int64) 无 decode/set_value | 对应类型值为 int64 的节点无法解码 |
| 4.2 | decode 时不应用 `ScaleA`/`ScaleB` 缩放 | 缩放过的节点 decode 得到的是原始值而非物理值 |
| 5.4 | set_value 时无缩放逆运算 `(v-ScaleB)/ScaleA` | 设物理值写回缓冲区时不会正确反算原始值 |

### 🟡 功能欠缺（可做可不做）

| 编号 | 问题 | 原因 |
|------|------|------|
| 六 | 帧级 Encode 编排 | 目前业务不需要写回协议报文（已验证） |
| 4.13 | Tag 特殊解码行为 | 真实 XML 触发频率低，无实际数据样本表明必要 |
| 7.2 | Frame 名模糊匹配（Contains） | C++ 精确匹配已可满足框架查找需求 |
| 7.3 | URL 式查找 | 非核心功能，调用方可以自行分割 |
| 7.5 | 全局查找信号 | 非核心功能 |
| 3.14 | Frame 标注 Head/Sum/Count 节点 | 只有 Frame::encode 需要此项 |

### 🟢 低优先级 / 可忽略

| 编号 | 问题 |
|------|------|
| 1.10 | `DefauleValue` 读取 |
| 1.13 | `FileInfo.Enable/WordType` 读取 |
| 3.8 | `IcdCmdNode` 标记类 |
| 3.9 | `IcdFilterFrame` |
| 8.x | 事件系统 |
| 9.1 | ScaleConvertor 插件 |
| 9.2-9.4 | 其他工具类 |

---

## 关键 C# 实现参考

各值类型的 decode/encode 管道可参考以下 C# 源文件：

- `IcdValue.cs` — 基类 `IcdValueBase` + `IcdBitValue` + `IcdBoolValue` + `IcdLByteValue` + `IcdHeadValue`
- `IcdUShortValue.cs` — `IcdUShortValue` + `IcdShortValue`（附 RawCode 原码处理）
- `IcdUIntValue.cs` — `IcdUIntValue`（无符号 32 位 + 缩放）
- `IcdIntValue.cs` — `IcdIntValue`（有符号 32 位 + 缩放 + 符号扩展）
- `IcdLongValue.cs` / `IcdULongValue.cs` — 64 位值类型
- `IcdByteValue.cs` — `IcdByteValue` + `IcdInt8Value`（sbyte + RawCode）
- `IcdBytesValue.cs` — `IcdBytesValue`（原始字节数组）
- `IcdStringValue.cs` — `IcdStringValue`（字符串编码转换）
- `IcdFloatValue.cs` / `IcdDoubleValue.cs` — 浮点数（直接指针读取）
- `IcdSumValue.cs` — `IcdSumValue` + `IcdXorValue`（校验和 + XOR 校验）
- `BitSet.cs` — `BitSet64` + `BitsSet32`（位域提取/写回工具）
