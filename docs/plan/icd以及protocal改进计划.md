# ICD 与协议模块改进计划

> **状态**：待实施
> **创建日期**：2026-06-24
> **关联文档**：`docs/规划/帧协议编辑器设计.md`、`docs/ICD可视化核心设计思路.md`、`docs/plan/帧协议解析交互问题.md`

---

## 1. 背景与问题

### 1.1 架构矛盾

`icd_utility` 原始设计围绕**配置驱动多文件模型**：一个 `ICDConfig.xml` 引用 N 个独立帧文件，Loader 解析配置、解析各帧文件、叠加元数据覆盖、构建 Repository。

当前 `.eproto` 实现采用**单文件 JSON 模型**：`deserialize_repository()` 直接解析 JSON 为 Repository，完全绕过 Loader/SchemaConfig 层。

两者并行导致：

| 问题 | 说明 |
|------|------|
| 两套 JSON 解析管线共存 | `json_parser.cpp` 中 legacy JSON（`parse_json_config`/`parse_json_frame`/`parse_node`）与 .eproto JSON（`deserialize_repository`/`parse_eproto_node`）Schema 不兼容 |
| 三套 ValueType 字符串映射 | XML parser（byte/word/dword...）、JSON config parser（byte/word/int16/smallint...）、.eproto parser（uint8/uint16/int32...）各一套，不完全对齐 |
| Loader + schema 层对 .eproto 是死代码 | 仅 XML 导入时使用 |
| ICDConfig 无回写能力 | 只能读不能写，导入后丢失文件溯源和元数据覆盖 |
| 导入是单向有损的 | XML → .eproto 转换丢失文件路径、元数据覆盖能力，无反向路径 |

### 1.2 三套 Tag 值体系不统一

| enum Tag | enum 值 | legacy XML/JSON 整数 |
|----------|---------|---------------------|
| none | 0 | 0 |
| head | 1 | 1 |
| sum | 4 | **40** |
| sum2 | 5 | **40**（不区分） |
| xor_ / xor1 / xor2 | 6/7/8 | **40**（不区分） |
| signal_in_value | 10 | **41** |
| big_endian_value | 11 | **60** |

当前 `xml_parser.cpp` 和 `json_parser.cpp` 的 legacy `parse_node()` 均直接 `static_cast<Tag>(40)`，产生无效枚举值。

### 1.3 IcdProtocolUtils 有损映射

- `tagName()` 将 `sum2`→"sum"、`xor1`/`xor2`→"xor"，`tagFromName()` 无法逆向恢复
- `shortint`/`smallint` 都映射到 "int16"

### 1.4 关联文档中的遗留问题

`docs/plan/帧协议解析交互问题.md` 指出：
- `ProjectStructureWidget` 与 `ProtocolManagerWidget` 功能重叠
- `ProtocolRef` 领域盲，不携带 Frame/Node 信息
- 无项目级 Repository 概念
- `ProtocolManagerWidget::parseEprotoFrames()` 手动 QJsonDocument 解析，重复 icd_utility 能力
- 导入无追踪

本计划解决其中"ICDConfig 无回写"和"格式不统一"两个核心问题，为后续 ProtocolService 统一数据源奠定基础。

---

## 2. 设计目标

### 2.1 三种协议模型并存

```
protocol/
├── ICDConfig.xml                    ← 配置驱动多文件（新项目默认）
│   ├── frame_001_发送.xml            ← 被 ICDConfig 引用
│   ├── frame_002_接收.xml
│   └── ...
├── standalone.eproto                 ← 单文件 JSON（已有）
└── standalone.eprotox                ← 单文件 XML（新增）
```

| 入口 | 格式 | 加载方式 | 保存方式 |
|------|------|---------|---------|
| `.eproto` | 单文件 JSON | `deserialize_repository()` (已有) | `serialize_repository()` (已有) |
| `.eprotox` | 单文件 XML | `deserialize_xml_repository()` (新增) | `serialize_xml_repository()` (新增) |
| `ICDConfig.xml` | 配置驱动多文件 XML | `Loader::init_with_metadata()` (新增) | `serialize_xml_config()` + `serialize_xml_frame_file()` (新增) |
| `ICDConfig.json` | 配置驱动多文件 JSON | 同上 | `serialize_json_config()` + `serialize_xml_frame_file()` (新增) |

**设计假设**：每个项目一个 ICDConfig。多 ICDConfig 共存场景作为后续演进方向（见 §11）。

### 2.2 设计原则

1. `.eproto` 与 `.eprotox` 使用完全相同的类型系统（uint8/uint16/int32...）、Tag 整数、FrameType/ByteOrder 字符串，仅语法不同，确保完美互转
2. ICDConfig 回写使用 legacy 格式约定（byte/word/dword 类型名、整数 FrameType/ByteOrder/Tag），与现有 XML 文件兼容
3. Tag 值映射在 legacy XML/JSON 序列化/反序列化层处理，不修改枚举定义
4. icd_utility 保持纯 C++17，不引入 Qt 依赖
5. ICDConfig 帧文件统一使用 `.xml` 扩展名，即使 ICDConfig 本身是 JSON 格式。此约束适用于 ICDConfig 保存路径——ICDConfig 保存时**始终写入 XML 帧文件**（无论 config 格式是 XML 还是 JSON）。`serialize_json_frame_file()` 仅用于独立帧文件导出或读取现有 `.json` 帧文件，不用于 ICDConfig 保存。如果加载的 ICDConfig.json 引用了 `.json` 帧文件，保存时自动转为 XML（`file_entries_` 中 `path` 同步更新 `.json` → `.xml`）。
6. `init_with_metadata()` 中帧文件格式检测**按扩展名独立判断**（对每个帧文件调用 `detect_format(frame_path)`），不再继承 config 格式。现有 `init()` 如要复用 `init_with_metadata()`，需同步此行为变更。
7. ICDConfig 识别采用**内容检测**（读取文件前 4096 字节检测根元素 `<ICDConfig>`），而非文件名匹配。内容检测仅在 `EditorManager::openFile()` 处理 `.xml`/`.json` 扩展名文件时触发。检测到 `<ICDConfig>` 后覆盖编辑器类型为 `"protocol"`，不影响 `.eproto`/`.eprotox` 的快速路径。

---

## 3. .eprotox XML 格式规范

### 3.1 文件结构

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ICDProtocol version="1.0">
  <Frame>
    <ID>90</ID>
    <Name>A429_00_ISI_01_发送_Label110</Name>
    <Description>UTC 时间</Description>
    <Type>cmd</Type>
    <ByteOrder>littleEndian</ByteOrder>
    <Length>4</Length>  <!-- 仅信息性，解析器忽略 -->
    <Nodes>
      <Item>
        <Name>LABEL</Name>
        <Description>Label 110</Description>
        <Offset>0</Offset>
        <StartBit>0</StartBit>
        <BitWidth>8</BitWidth>
        <ValueType>uint8</ValueType>
        <Tag>1</Tag>
        <Attrs>
          <IsScaled>false</IsScaled>
          <Unit>A</Unit>
          <ScaleA>0.01</ScaleA>
        </Attrs>
        <Children>
          <Item>...</Item>
        </Children>
      </Item>
    </Nodes>
  </Frame>
</ICDProtocol>
```

### 3.2 与 legacy XML 格式的区别

| 方面 | .eprotox (新) | ICDData (legacy) |
|------|--------------|------------------|
| 根元素 | `<ICDProtocol>` | `<ICDData>` |
| 值类型元素 | `<ValueType>uint8</ValueType>` | `<Type>byte</Type>` |
| 子节点容器 | `<Children>` | `<Childs>` |
| 属性包装 | `<Attrs>` 包装 | 扁平平铺 |
| 类型名 | uint8/uint16/int32/float/double/string... | byte/word/dword/short/float/double/string... |
| FrameType | 字符串 "cmd"/"data"/"dataCfg" | 整数 1/2/4 |
| ByteOrder | 字符串 "littleEndian"/"bigEndian" | 整数 0/1 |
| 多帧支持 | 是（多个 `<Frame>`） | 否（单帧文件） |

### 3.3 类型映射表（.eproto / .eprotox 共用）

**ValueType**：

| enum | 字符串 |
|------|--------|
| boolean | `boolean` |
| byte_ | `uint8` |
| bytes | `bytes` |
| word | `uint16` |
| shortint | `int16` |
| smallint | `smallint` |
| longword | `uint32` |
| integer | `int32` |
| ulong_ | `uint64` |
| single | `float` |
| double_ | `double` |
| string_ | `string` |
| unknown | 保留 `"unknown"`（.eproto/.eprotox 序列化时直接输出 `"unknown"`，反序列化时 `"unknown"` 映射回 `ValueType::unknown`，保持往返一致） |

> **注意**：`unknown` 仅在 legacy 格式（XML/JSON）降级时映射为 `"byte"`。`.eproto`/`.eprotox` 保留 `"unknown"` 字符串，因为现有序列化器（`json_serializer.cpp:33`）已输出 `"unknown"`，反序列化器（`json_parser.cpp:137`）已将 `"unknown"` 正确映射回 `ValueType::unknown`。如果改为写 `"uint8"`，读回后会变成 `ValueType::byte_`，丢失 `unknown` 语义。

**FrameType**：`data` / `cmd` / `dataCfg`

> `"dataCfg"` 字符串对应的枚举值是 `FrameType::data_cmd`。映射正确，但枚举名 `data_cmd` 与字符串名 `dataCfg` 含义不同（"Cfg" vs "cmd"），在 `type_mapping.hpp` 中需加注释说明原因。

**ByteOrder**：`littleEndian` / `bigEndian`

**Tag**：整数（与 enum 序数一致：none=0, head=1, length=2, count=3, sum=4, sum2=5, xor_=6, xor1=7, xor2=8, init_value=9, signal_in_value=10, big_endian_value=11）

### 3.4 Boolean 值解析

`<IsScaled>false</IsScaled>` — 解析时同时支持 `"true"`/`"false"` 和 `"0"`/`"1"`，提高兼容性。

---

## 4. Legacy XML/JSON 格式映射

### 4.1 Legacy 类型名映射

| ValueType enum | legacy XML 字符串 | legacy JSON 字符串 |
|----------------|------------------|-------------------|
| byte_ | `byte` | `byte` |
| bytes | `bytes` | `bytes` |
| word | `word` | `word` |
| longword | `dword` | `int`（降级，有损） |
| shortint | `short` | `int16` |
| smallint | `int`（降级，类型展宽 16→32 位） | `smallint` |
| single | `float` | `float` |
| double_ | `double` | `double` |
| string_ | `string` | `string` |
| integer | `int` | `int` |
| boolean | `byte`（降级） | `byte`（降级） |
| ulong_ | `dword`（降级，有损） | `int`（降级，有损） |
| unknown | `byte`（兜底） | `byte`（兜底） |

**有损降级说明**：
- `longword`（uint32）→ legacy JSON `"int"`：读取端解析为 `integer`（int32），**丢失无符号语义**
- `ulong_`（uint64）→ legacy XML `"dword"` / legacy JSON `"int"`：**丢失 64 位精度**
- `smallint`（int16）→ legacy XML `"int"`：类型展宽 16→32 位，读回变为 `integer`
- `boolean` → legacy XML/JSON `"byte"`：**丢失布尔语义**，读回变为 `byte_`
- 从 .eproto/.eprotox 导出为 legacy 格式时，如有 `longword`/`ulong_`/`smallint`/`boolean` 类型节点，应弹出警告提示有损转换

### 4.2 Tag 值映射（legacy XML/JSON ↔ enum）

**正向映射（enum → legacy 整数，写入时）**：

| enum Tag | enum 值 | legacy 整数 |
|----------|---------|------------|
| none | 0 | 0 |
| head | 1 | 1 |
| length | 2 | 2 |
| count | 3 | 3 |
| sum | 4 | **40** |
| sum2 | 5 | **40**（降级） |
| xor_ | 6 | **40**（降级） |
| xor1 | 7 | **40**（降级） |
| xor2 | 8 | **40**（降级） |
| init_value | 9 | 9 |
| signal_in_value | 10 | **41** |
| big_endian_value | 11 | **60** |

**反向映射（legacy 整数 → enum，读取时）**：

| legacy 整数 | enum Tag |
|------------|----------|
| 0 | none |
| 1 | head |
| 2 | length |
| 3 | count |
| 9 | init_value |
| 40 | sum（无法区分 sum2/xor_，统一映射为 sum） |
| 41 | signal_in_value |
| 60 | big_endian_value |
| 其他 | `Tag::none`（无法识别的值统一为 none） |

**实现位置**：`xml_parser.cpp` 和 `json_parser.cpp` 的 Tag 解析逻辑需更新（两者均有 `static_cast<Tag>()` bug），`xml_serializer.cpp` 和 `json_serializer.cpp` 的 Tag 写入逻辑需新增映射。

### 4.3 Legacy FrameType / ByteOrder 映射

| enum | legacy 整数 |
|------|------------|
| FrameType::data | 1 |
| FrameType::cmd | 2 |
| FrameType::data_cmd | 4 |
| ByteOrder::little_endian | 0 |
| ByteOrder::big_endian | 1 |

### 4.4 Legacy 元素名/键名（含已知拼写错误）

legacy XML/JSON 格式中存在已知的拼写错误，序列化器写入时必须匹配，否则无法被现有 parser 读回：

| 字段 | legacy XML 元素名 | legacy JSON 键名 | .eproto/.eprotox 键名 |
|------|-------------------|-----------------|----------------------|
| 缩放转换器 | `<ScaleConveror>`（非 ScaleConverter） | `"scaleConveror"` | `"scaleConveror"`（三种格式一致，均无 `t`） |

> **注意**：C++ 字段名是 `scale_convertor`（有 `t`），但所有序列化格式的键名均为 `"scaleConveror"`（无 `t`）。这个差异是故意的——"修复"拼写会破坏与现有文件的兼容性。在 `type_mapping.hpp` 中定义为命名常量：
> ```cpp
> // 所有格式统一使用 "scaleConveror"（无 't'），必须与遗留格式匹配
> inline constexpr auto kScaleConverorKey = "scaleConveror";
> ```
> 加注释说明 `scale_convertor`（C++ 字段名，有 `t`）与 `"scaleConveror"`（序列化键名，无 `t`）之间的差异不可"修复"。

### 4.5 帧元数据在 config 和帧文件间的分布

ICDData（legacy 帧文件）格式只有 `<Name>` 和 `<Data>`，不包含帧级元数据：

| 字段 | 帧文件(ICDData) | 配置文件(ICDConfig) |
|------|-----------------|-------------------|
| Name | `<Name>` | `<Name>`（可覆盖） |
| ID | ❌ | `<ID>` |
| Description | ❌ | `<Description>` |
| Type | ❌ | `<Type>` |
| ByteOrder | ❌ | `<ByteOrder>` |

因此：
- `serialize_xml_frame_file()` 只写入 `<Name>` 和 `<Data>`（节点树），不写入帧级元数据
- `serialize_xml_config()` 写入所有帧级元数据（ID/Name/Description/Type/ByteOrder/Path）
- 用户在属性面板修改帧的 Description/Type/ByteOrder 时，只更新 `file_entries_` 中对应条目，不触发帧文件重写
- 直接打开帧文件（不通过 ICDConfig）会看不到 ID/Description/Type/ByteOrder — 这是 legacy 格式的固有限制

---

## 5. 实施计划

### Phase 1: icd_utility — .eprotox XML 单文件格式 + 共享类型映射

**目标**：实现 .eprotox 的读取和写入，提取共享类型映射模块。

#### 涉及文件

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/icd_utility/src/format/type_mapping.hpp` | 新增 | 共享类型映射函数声明 |
| `src/icd_utility/src/format/type_mapping.cpp` | 新增 | 共享类型映射函数实现 |
| `src/icd_utility/src/format/xml_serializer.hpp` | 新增 | 声明 `deserialize_xml_repository()`、`serialize_xml_repository()` |
| `src/icd_utility/src/format/xml_serializer.cpp` | 新增 | pugixml 实现 |
| `src/icd_utility/CMakeLists.txt` | 修改 | 添加 `format/xml_serializer.cpp`、`format/type_mapping.cpp` |
| `tests/icd_utility/CMakeLists.txt` | 修改 | 添加 `test_eprotox` 的 `add_etest()` 条目 |
| `tests/icd_utility/test_eprotox.cpp` | 新增 | 往返测试 |

#### 1.1 共享类型映射模块

提取到 `src/format/type_mapping.hpp` / `type_mapping.cpp`（内部共享头文件），供所有 parser/serializer 引用：

- `value_type_to_string(ValueType)` / `value_type_from_string(string_view)` — .eproto/.eprotox 共用
- `frame_type_to_string(FrameType)` / `frame_type_from_string()`
- `byte_order_to_string(ByteOrder)` / `byte_order_from_string()`
- `tag_to_legacy_int(Tag)` / `tag_from_legacy_int(int)` — legacy XML/JSON 共用
- `value_type_to_legacy_xml_string(ValueType)` / `value_type_from_legacy_xml_string()`
- `value_type_to_legacy_json_string(ValueType)` / `value_type_from_legacy_json_string()`
- `unknown` 处理：`.eproto`/`.eprotox` 函数保持 `"unknown"` 字符串往返一致；legacy 函数降级为 `"byte"` 并发出警告（序列化器和解析器各发一次，标注来源位置）
- `"dataCfg"` → `FrameType::data_cmd` 映射加注释说明命名差异

#### 1.2 .eprotox 实现要点

- `deserialize_xml_repository(path)`：返回 `tl::expected<Repository, Error>`，解析 `<ICDProtocol>` 根元素，遍历 `<Frame>` 子元素，对每个 `<Nodes><Item>` 调用 `parse_eprotox_node()` 递归解析。错误处理与 `Loader::init()` 一致（返回 `Error` 描述失败原因）。
- `parse_eprotox_node()`：使用 .eproto 类型系统（uint8/uint16/int32...），不复用 legacy `parse_node()`
- `serialize_xml_repository(path, repo)`：返回 `tl::expected<void, Error>`（或 `bool` 表示成功），遍历 Repository 的 frames，每个 Frame 写为 `<Frame>` 元素，Node 递归写为 `<Item>`
- ValueType 映射引用 `type_mapping.hpp`
- `Length` 字段写入时自动计算（与 JSON 序列化器一致），读取时忽略
- **文件读取**复用 `load_xml_document()` 的 CJK 路径 workaround（`std::ifstream` 代替 `fopen`）。注意该函数当前在 `xml_parser.cpp` 的匿名命名空间中（`namespace { ... }`），`xml_serializer.cpp` 是另一个翻译单元无法直接调用，需提取到共享工具如 `format/xml_util.hpp`
- **文件写入**使用 `std::ofstream` + `doc.save(stream)` 方式，避免 pugixml `save_file()` 的 CJK 路径问题
- `unknown` 处理：写 `"unknown"` 字符串（非降级），保持 .eprotox 往返一致
- `update_max_bits()`（用于 `Length` 自动计算）在各序列化器中各自实现（简单递归函数，避免跨模块依赖），或统一放到 `format/utility.hpp`

> **`xml_serializer.cpp` 分区要求**：Phase 1.2 添加 .eprotox 函数（新类型系统 uint8/uint16/int32...），Phase 2.5 在同一文件中追加 legacy XML 序列化函数（旧类型系统 byte/word/dword...）。两套类型映射完全不同，必须用明显的注释区块分隔：
> ```cpp
> // ============================================================================
> // .eprotox format (ICDProtocol root, uint8/uint16/int32 type system)
> // ============================================================================
> // ...
> // ============================================================================
> // Legacy XML format (ICDData root, byte/word/dword type system)
> // ============================================================================
> ```

#### 1.3 测试

- 构造 Repository → 写 .eprotox → 读回 → 写 .eproto JSON → 对比两者 Repository 一致
- 测试空帧、单帧、多帧、深层嵌套节点
- 测试所有 ValueType 字符串映射
- 测试 CJK 文件路径（读写双向）
- 测试 `<IsScaled>` 的 `"true"`/`"false"`/`"0"`/`"1"` 多种写法

---

### Phase 2: icd_utility — ICDConfig 多文件回写（XML + JSON）

**目标**：Loader 加载后保留文件元数据，支持写回 ICDConfig + 各帧文件。

#### 2.1 公共类型定义

**涉及文件**：`src/icd_utility/include/icd/file_entry.hpp`（新增）

```cpp
struct ICD_UTILITY_API FrameFileInfo {
    int id {0};
    std::string name;
    std::string description;
    std::string path;               // 相对于 config 文件的路径
    FrameType type {FrameType::data};
    ByteOrder order {ByteOrder::little_endian};
    Format format {Format::xml};
};
```

> 需要 `#include <icd/types.hpp>` 以引入 `FrameType`/`ByteOrder`/`Format` 类型。

与私有 `SchemaFileEntry` 的区别：
- `id` 改为非 optional（空 ICDConfig 时使用默认值 0）
- `type` 和 `order` 改为非 optional（使用默认值而非 optional）
- `logical_name` 改名为 `name`（与 `Frame::name()` 一致）
- `format` 默认为 `Format::xml`（最常见情况）

`SchemaFileEntry` 保持私有，仅在 `loader.cpp` 内部用于 `build_repository()` 前的中间表示。

#### 2.2 扩展 Loader 返回元数据

**涉及文件**：`src/icd_utility/include/icd/loader.hpp` + `src/icd_utility/src/loader/loader.cpp`

```cpp
struct LoadResult {
    Repository repository;
    std::filesystem::path config_path;
    Format format;                                  // 始终为 xml 或 json，不会是 auto_detect
    std::vector<FrameFileInfo> file_entries;        // 每帧的文件路径和元数据
};

class Loader {
    static tl::expected<Repository, Error> init(path, format);                  // 已有
    static tl::expected<LoadResult, Error> init_with_metadata(path, format);    // 新增
};
```

**实现要点**：
- `init()` 内部调用 `init_with_metadata()`，丢弃元数据后返回 Repository，避免代码重复
- `init_with_metadata()` 在现有逻辑基础上，将 `SchemaFileEntry` 转换为 `FrameFileInfo` 返回
- `file_entries` 中的 `path` 字段始终为相对于 `config_path.parent_path()` 的相对路径
- **行为变更**：`init_with_metadata()` 对每帧独立调用 `detect_format(frame_path)` 判断格式，不再继承 config 格式。当前 `init()` 的行为是"非 auto_detect 时帧格式继承 config 格式"（见 `loader.cpp:69`），`init_with_metadata()` 将此改为"始终按扩展名独立判断"。如需保持一致，`init()` 也应复用此逻辑。

#### 2.3 允许空 ICDConfig

**涉及文件**：`src/icd_utility/src/format/xml_parser.cpp`、`src/icd_utility/src/format/json_parser.cpp`

修改 `parse_xml_config()` 和 `parse_json_config()`，允许空文件列表（`config.files.empty()` 时返回空 `SchemaConfig`，而非报错）。

**同时验证** `src/icd_utility/src/schema/builder.cpp` 的 `build_repository()` 对空 `SchemaConfig` 的行为，如不支持则修改为返回空 Repository。

#### 2.4 修复 Tag 解析 bug

**涉及文件**：`src/icd_utility/src/format/xml_parser.cpp`、`src/icd_utility/src/format/json_parser.cpp`

两个 parser 的 legacy `parse_node()` 中 Tag 解析均为 `static_cast<Tag>(值)`，对 legacy 整数 40/41/60 产生无效枚举值。改为使用 `tag_from_legacy_int()` 反向映射查表，未识别的值统一为 `Tag::none`。

> **依赖注意**：`tag_from_legacy_int()` 在 Phase 1 的 `type_mapping.hpp` 中定义，Phase 2.4 固然后于 Phase 1。如果因为计划排期需要在 Phase 1 完成前先行修复 Tag bug，可先在 `xml_parser.cpp`/`json_parser.cpp` 中内联映射逻辑（if/else 或 switch），待 Phase 1 完成后替换为 `tag_from_legacy_int()` 调用。

#### 2.5 Legacy XML 序列化器

**涉及文件**：`src/icd_utility/src/format/xml_serializer.hpp` + `.cpp`（追加函数）

| 函数 | 说明 |
|------|------|
| `serialize_xml_config(path, file_entries)` | 写 `<ICDConfig><Files><FileInfo>...</FileInfo></Files></ICDConfig>` |
| `serialize_xml_frame_file(path, frame)` | 写 `<ICDData><Name/><Data><Item>...</Item></Data></ICDData>`（仅 Name + 节点树） |

**实现要点**：
- 类型名使用 §4.1 中的 legacy XML 映射（含降级规则：smallint→int, boolean→byte, ulong_→dword 等）
- FrameType 写整数（1/2/4），ByteOrder 写整数（0/1）
- Tag 写 legacy 整数（经 `tag_to_legacy_int()` 映射）
- `<Childs>` 而非 `<Children>`
- 属性扁平平铺（`<Unit>`, `<ScaleA>` 等直接作为 `<Item>` 子元素）
- 元素名匹配拼写错误：`<ScaleConveror>`（非 ScaleConverter）
- 文件写入使用 `std::ofstream` + `doc.save(stream)` 避免 CJK 路径问题
- ICDConfig 写入使用临时文件 + 原子替换（写入 `.tmp` 后 rename）

#### 2.6 Legacy JSON 序列化器

**涉及文件**：`src/icd_utility/src/format/json_serializer.hpp` + `.cpp`（追加函数）

| 函数 | 说明 |
|------|------|
| `serialize_json_config(path, file_entries)` | 写 `{"files":[{"name","path","id","type"(int),"byteOrder"(int)}]}` |
| `serialize_json_frame_file(path, frame)` | 写 `{"name":"...","data":[...]}`（legacy 格式，扁平 attrs，`"childs"` 数组）。**注意**：此函数仅用于独立帧文件导出，不用于 ICDConfig 保存。ICDConfig 保存时无论 config 格式为 JSON 还是 XML，帧文件始终使用 `serialize_xml_frame_file()` |

**实现要点**：
- 类型名使用 §4.1 中的 legacy JSON 映射（含降级规则：longword→int, boolean→byte, ulong_→int 等）
- Tag/FrameType/ByteOrder 写整数（与 XML 相同的映射）
- `"isScaled"` 写整数 0/1（非 bool）
- `"childs"` 而非 `"children"`
- 键名匹配拼写错误：`"scaleConveror"`

#### 2.7 测试

**涉及文件**：`tests/icd_utility/test_config_writeback.cpp`（新增）；`tests/icd_utility/CMakeLists.txt`（修改，添加 `add_etest()` 条目）

- 空 ICDConfig.xml → 加载成功 → 返回空 Repository + 空 file_entries
- 加载 ICDConfig.xml → `init_with_metadata()` → 修改 Repository → 写回 → 重新加载 → 对比
- 同样测试 ICDConfig.json 路径
- 测试 Tag 值映射正确性（40 ↔ sum, 41 ↔ signal_in_value, 60 ↔ big_endian_value）
- 测试文件路径元数据保留
- 测试有损降级场景（longword/ulong_/smallint/boolean 写入 legacy 格式）

---

### Phase 3: ProtocolEditorWidget — 格式路由

**目标**：编辑器根据文件类型自动选择加载/保存策略。

#### 涉及文件

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/protocol/ProtocolEditorWidget.h` | 修改 | 新增 `ProtocolFormat` 枚举、`config_format_`、`config_path_`、`file_entries_` 成员 |
| `src/protocol/ProtocolEditorWidget.cpp` | 修改 | 加载/保存路由、ConfigDriven 帧管理、undo/redo 磁盘重建 |

#### 3.1 新增类型和成员

```cpp
enum class ProtocolFormat {
    Json,           // .eproto
    Xml,            // .eprotox
    ConfigDriven    // ICDConfig.xml/.json
};
```

新增成员：
- `ProtocolFormat format_ = ProtocolFormat::Json`
- `icd::Format config_format_ = icd::Format::xml`（ConfigDriven 模式下区分 XML/JSON 配置）
- `std::filesystem::path config_path_`（ConfigDriven 模式下的配置文件路径）
- `std::vector<icd::FrameFileInfo> file_entries_`（每帧的文件路径元数据，`path` 字段为相对于 `config_path_.parent_path()` 的相对路径）
- `QHash<int /*frame_id*/, QString /*relative file path*/> frame_file_map_`（frame_id → 文件路径映射，用于快速查找。查找时使用 `value(key)` 而非 `operator[]`，避免键不存在时静默插入默认值）

#### 3.2 加载路由 — `setEditorId()`

ICDConfig 识别采用**内容检测**（读取文件前 4096 字节检测根元素 `<ICDConfig>`），而非文件名匹配。注意内容检测发生在 `EditorManager::openFile()` 层（见 Phase 4.1），`setEditorId()` 可信任路由已正确分配，只需根据格式区分加载策略：

```
路径以 .eproto 结尾          → deserialize_repository() (JSON)
                                ├─ 成功 → 继续
                                └─ 失败 → QMessageBox::warning("无法加载协议文件: <错误信息>") → 关闭编辑器
路径以 .eprotox 结尾         → deserialize_xml_repository() (XML)
                                ├─ 成功 → 继续
                                └─ 失败 → 同上
文件内容根元素为 <ICDConfig>  → Loader::init_with_metadata()
                                ├─ 成功 → 保存 format_, config_format_, config_path_, file_entries_
                                │          → 构建 frame_file_map_
                                └─ 失败 → 同上
```

**异步加载**：三种格式均沿用 `QtConcurrent::run` 异步加载模式（与当前 .eproto 实现一致）。ICDConfig 涉及多文件读取，耗时更长，异步加载尤为必要。如在后台线程中失败，通过 `QFutureWatcher` 在主线程弹错误对话框。

#### 3.3 保存路由

```
format_ == Json                     → serialize_repository(path, repo_)
format_ == Xml                      → serialize_xml_repository(path, repo_)
format_ == ConfigDriven
  config_format_ == Format::xml     → serialize_xml_config(config_path_, file_entries_)
                                      + 对每个 frame: serialize_xml_frame_file(base_dir / entry.path, frame)
  config_format_ == Format::json    → serialize_json_config(config_path_, file_entries_)
                                      + 对每个 frame: serialize_xml_frame_file(base_dir / entry.path, frame)
                                      + file_entries_ 中 path 同步更新 (.json → .xml)
```

> **注意**：ICDConfig 保存时无论 config 格式是 XML 还是 JSON，帧文件**始终使用 XML 格式**（`serialize_xml_frame_file()`）。规则源自 §2.2 第 5 条："ICDConfig 帧文件统一使用 `.xml` 扩展名"。如果加载的 ICDConfig.json 引用了 `.json` 帧文件，保存时自动转为 XML，`file_entries_` 中的 `path` 字段同步更新（`.json` → `.xml`）。`serialize_json_frame_file()` 仅用于独立帧文件导出场景。
>
> **实现要点**：`ProtocolEditorWidget::saveAs()` 当前仅调用 `saveEproto()`（写入 .eproto），需改为根据 `format_` 分发到 §3.7 定义的保存路由。`saveAs()` 和 `saveFile()` 共享相同的格式分发逻辑。

帧文件与 `file_entries_` 的匹配通过 `frame_file_map_`（frame_id → path）实现。查找时使用 `value(key)` 而非 `operator[]`，不存在时发出警告日志并跳过该帧的写入。

`file_entries_` 的 `id` 字段始终与 `Frame::id()` 同步，`name` 字段始终与 `Frame::name()` 同步。

#### 3.4 ConfigDriven 模式 — 新建帧

1. 分配帧 ID：找出当前最大 ID + 1（`max_id + 1`，与现有实现 `ProtocolEditorWidget.cpp:578-583` 一致），无需冲突检测
2. 创建 Frame（默认值：Name=`"NewFrame"`，Description=空，Type=`data`，ByteOrder=`little_endian`，空节点树）
3. 生成帧文件名：`frame_{id:03d}_{name}.xml`（非法字符替换为 `_`，帧名超过 50 字符时截断并附加 hash 后缀）。**Windows 非法文件名字符集**：`\ / : * ? " < > |` 及控制字符 0x00-0x1F。可使用 `QDir::toNativeSeparators()` 配合正则替换
4. 检查文件名冲突：同时检查**磁盘存在性**和当前 `file_entries_` 中的路径集合，双重验证。如冲突，追加数字后缀 `frame_001_发送_2.xml`
5. 在 Repository 中添加帧
6. 创建 `FrameFileInfo` 并添加到 `file_entries_`
7. 在 `frame_file_map_` 中建立 frame_id → relative path 映射
8. 写入帧文件到磁盘
9. 更新 ICDConfig 文件
10. 保存快照（undo）

**帧文件名在创建后不可变**。文件名仅用于磁盘标识，不反映当前帧名。用户重命名帧时只更新 `Frame::name()` 和 `file_entries_` 中对应条目的 `name`，不重命名磁盘文件。

#### 3.5 ConfigDriven 模式 — 删除帧

1. 通过 `frame_file_map_` 找到对应帧的文件路径
2. 删除磁盘上的帧文件（先检查文件存在性）
3. 从 `file_entries_` 移除对应条目
4. 从 `frame_file_map_` 移除映射
5. 从 Repository 中移除帧
6. 更新 ICDConfig 文件
7. 保存快照（undo）

#### 3.6 ConfigDriven 模式 — undo/redo

ConfigDriven 模式下 undo/redo 需要处理文件系统副作用，采用**快照 + 磁盘重建**策略：

快照内容扩展为：
- Repository 的 JSON 序列化（已有）
- `file_entries_` 的 JSON 序列化（新增，每个条目包含 `id`、`name`、`path` 三个字段供磁盘重建使用，`description`/`type`/`order` 从 Repository 的 Frame 元数据恢复）
- `config_path_`（新增）
- 快照格式版本号（`"snapshot_version": 1`）

快照 JSON 格式示例：
```json
{
  "snapshot_version": 1,
  "repository": { ... },
  "file_entries": [
    {"id": 1, "name": "Frame_001", "path": "frame_001_frame.xml"},
    {"id": 2, "name": "Frame_002", "path": "frame_002_frame.xml"}
  ],
  "config_path": "protocol/ICDConfig.xml"
}
```

`restoreSnapshot()` 在 ConfigDriven 模式下额外执行：
1. 删除当前 `file_entries_` 中记录的所有帧文件（按 `path` 字段定位，非删除目录下全部文件）。**文件不存在时静默跳过**（`QFile::remove()` 对不存在的文件返回 false，不阻塞流程）
2. 恢复 `file_entries_` 到快照状态
3. 按恢复后的 `file_entries_` 和 Repository 重写所有帧文件
4. 重写 ICDConfig
5. **重建 `frame_file_map_`**：遍历恢复后的 `file_entries_`，建立 `id → path` 映射

> **已知限制**：ConfigDriven 目录下未被 `file_entries_` 记录的孤儿帧文件不受 undo/redo 管理，会永久残留在磁盘上。undo/redo 不会创建或删除这些文件。

非 ConfigDriven 模式下 `file_entries_` 和 `config_path_` 为空，不影响现有逻辑。

#### 3.7 SaveAs 行为

SaveAs 模式判断采用**扩展名优先 + 文件名兜底**（SaveAs 是写入新文件，目标文件可能不存在，无法做内容检测）：

1. 扩展名为 `.eproto` → Json 模式，单文件保存
2. 扩展名为 `.eprotox` → Xml 模式，单文件保存
3. 文件名为 `ICDConfig.xml` → ConfigDriven (XML) 模式
4. 文件名为 `ICDConfig.json` → ConfigDriven (JSON) 模式
5. 其他 `.xml` 或 `.json` → 弹出确认对话框，询问用户意图：
   - "保存为 ICDConfig (XML 配置驱动)" → 重命名为 `ICDConfig.xml`
   - "保存为 .eprotox (XML 单文件)" → 扩展名改为 `.eprotox`
   - "取消"

SaveAs 到 ConfigDriven 模式时，`path` 参数指向目标 ICDConfig 文件路径：
1. 从 `path` 推导目标目录（`path.parent_path()`）
2. 创建目标目录（如不存在）
3. 生成新的 ICDConfig 文件
4. 为 Repository 中的每个 Frame 生成帧文件名，创建 `FrameFileInfo` 条目。字段来源：
   - `id` → `Frame::id()`
   - `name` → `Frame::name()`
   - `description` → `Frame::description()`
   - `path` → 生成的文件名（相对路径）
   - `type` → `Frame::type()`
   - `order` → `Frame::order()`
5. 写入所有帧文件到目标目录（从 Repository 内容生成，非复制源文件）
6. 更新 `file_entries_`、`config_path_` 和 `frame_file_map_`
7. 切换 `format_` 为 `ConfigDriven`，设置 `config_format_`

SaveAs 文件对话框过滤器分选项展示：
- `ICDConfig (XML) (ICDConfig.xml)`
- `ICDConfig (JSON) (ICDConfig.json)`
- `协议文件 (JSON) (*.eproto)`
- `协议文件 (XML) (*.eprotox)`

#### 3.8 属性编辑同步

用户在属性面板修改帧的 ID/Name/Description/Type/ByteOrder 时：
- 更新 `Frame` 对象（内存）
- 同步更新 `file_entries_` 中对应条目的 `id`/`name`/`description`/`type`/`order`
- 如修改了帧 ID，同步更新 `frame_file_map_` 的键
- 标记 modified，下次保存时写入 ICDConfig
- 不触发帧文件重写（帧文件不包含这些元数据）

---

### Phase 4: 项目集成

#### 涉及文件

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/app/EditorManager.cpp` | 修改 | 注册 `.eprotox` → protocol 编辑器；ICDConfig 内容检测识别 |
| `src/app/ProjectStructureWidget.cpp` | 修改 | 协议目录多扩展名、ICDConfig 树形展开、图标映射 |
| `src/app/ProtocolManagerWidget.cpp` | 修改 | 多扩展名扫描、新建格式选择对话框 |
| `src/app/CMakeLists.txt` | 修改 | 添加 `icd_utility` 到 `target_link_libraries`，供 `ProjectStructureWidget` 调用 |
| `src/core/project/ProjectInfo.cpp` | 修改 | `scanDirectory` 支持多扩展名（重载，不修改现有签名） |
| `src/core/project/ProjectManager.cpp` | 修改 | `createProjectStructure()` 中 protocol 目录创建后写入空 `ICDConfig.xml` |
| 图标资源 | 新增 | `file_eprotox`、`file_icdconfig` |

#### 4.1 EditorManager 扩展名注册

```cpp
EditorFactoryRegistry::registerExtension("eprotox", "protocol");
```

**ICDConfig 特殊处理**：`.xml` 和 `.json` 扩展名太通用，不能直接注册为 protocol 类型。在 `EditorManager::openFile()` 的扩展名查找后增加内容检测步骤：

1. 对 `.xml`/`.json` 扩展名的文件，在 `EditorFactoryRegistry::typeForExtension(suffix)` 查找后追加内容检测
2. 读取文件前 4096 字节检测根元素 `<ICDConfig>`（使用 `contains()` 而非 `starts_with()` 以兼容 UTF-8 BOM）
3. 匹配则覆盖编辑器类型为 `"protocol"`，创建 `ProtocolEditorWidget`
4. 不匹配则保持原 `"text"` 类型

内容检测**仅对 `.xml`/`.json` 扩展名触发**，不影响 `.eproto`/`.eprotox` 的快速路径。

#### 4.2 ProjectStructureWidget

**协议目录注册**：
- `newFileExt` 改为支持多扩展名或弹出选择对话框
- 图标映射：`.eprotox` → `file_eprotox`，ICDConfig 文件 → `file_icdconfig`

**ICDConfig 树形展开**：
1. 扫描 `protocol/` 目录时检测 ICDConfig 文件（内容检测）
2. 解析配置文件获取帧文件列表（通过 `Loader::list_config_files()` 新增公共 API，或 `Loader::init_with_metadata()` 从 `LoadResult::file_entries` 获取。**不推荐直接调用** `parse_xml_config()`/`parse_json_config()`——这些函数返回 `schema::SchemaConfig`（私有类型，定义在 `src/icd_utility/src/schema/schema.hpp`），虽然通过 PUBLIC 包含路径可在 `src/app/` 层编译通过（`ProtocolManagerWidget.cpp` 已如此），但这依赖于 icd_utility 的内部实现细节，未来重构时易被破坏。）
3. 将引用的帧文件作为 ICDConfig 的子节点显示
4. 未被任何 ICDConfig 引用的帧文件作为独立文件显示（孤儿帧文件）

**帧文件点击行为**：
- 正常情况（作为 ICDConfig 子节点）：打开 ICDConfig 编辑器 + 选中对应帧
- 孤儿帧文件（无 ICDConfig 引用）：ProjectStructureWidget 调用 `EditorManager::openFile(path, "protocol")` 强制指定编辑器类型（绕过内容检测）。编辑器用 `parse_xml_frame()` 单独加载，显示警告"此帧文件未关联 ICDConfig，帧级元数据不可用"

#### 4.3 ProtocolManagerWidget

**多扩展名扫描**：
- `refreshList()` 扫描 `.eproto` + `.eprotox` + ICDConfig 文件（内容检测）

**新建文件对话框**：

```
┌─ 新建协议文件 ──────────────────┐
│  格式:                          │
│  ○ ICDConfig (XML) — 配置驱动   │
│  ○ ICDConfig (JSON) — 配置驱动  │
│  ○ .eproto (JSON 单文件)        │
│  ○ .eprotox (XML 单文件)        │
│                                 │
│  文件名: [___________]          │
│              [取消] [确定]       │
└─────────────────────────────────┘
```

**"导入XML"按钮**：保留现有行为（XML → .eproto 转换），不改动。导入按钮的改进列入技术债务（§10）。

#### 4.4 新项目默认 ICDConfig.xml

在项目创建逻辑中：
1. 创建 `protocol/` 目录
2. 写入空 `ICDConfig.xml`：

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ICDConfig>
  <Files>
  </Files>
</ICDConfig>
```

#### 4.5 ProjectInfo 多扩展名扫描

`scanDirectory()` 用**重载**而非修改现有签名：

```cpp
// 已有 — 保持不变
QStringList scanDirectory(const QString& category, const QString& ext) const;
// 新增 — 多扩展名
QStringList scanDirectory(const QString& category, const QStringList& exts) const;
```

#### 4.6 图标资源

| 图标名 | 用途 |
|--------|------|
| `file_eprotox` | .eprotox 文件 |
| `file_icdconfig` | ICDConfig.xml / ICDConfig.json |

---

### Phase 5: 清理与整合

| 任务 | 说明 |
|------|------|
| 替换 `ProtocolManagerWidget::parseEprotoFrames()` | 改为调用 `icd::format::deserialize_repository()`，移除手动 QJsonDocument 解析。Phase 3 开始时即在原函数上加 `[[deprecated]]` 属性标记弃用，防止 Phase 3/4 产生新的调用方 |
| 简化 `ProtocolManagerWidget::onImportXml()` | 评估是否可直接打开 XML 而非强制转换 |
| 创建 `test_cross_format.cpp` | 跨格式往返测试，同时修改 `tests/icd_utility/CMakeLists.txt` 添加 `add_etest()` 条目 |
| 清理 `json_parser.cpp` 中的注释和死代码 | 确保 legacy 和 .eproto 两套管线的边界清晰 |
| 统一类型映射函数引用 | 所有 parser/serializer 统一引用 `type_mapping.hpp` |

---

## 6. 数据流总览

### 6.1 加载流程

```
用户打开协议文件
  │
  ├─ .eproto ──→ deserialize_repository() ──→ Repository
  │
  ├─ .eprotox ─→ deserialize_xml_repository() ──→ Repository
  │
  └─ ICDConfig ─→ Loader::init_with_metadata()
                   ├─ parse_config() ──→ SchemaFileEntry[] (私有)
                   ├─ for each entry: parse_frame() ──→ SchemaFrameDef
                   ├─ overlay metadata
                   ├─ build_repository() ──→ Repository
                   └─ 返回 LoadResult { repository, config_path, format, file_entries }
                        └─ file_entries 类型为 FrameFileInfo[] (公共类型)
  │
  └─→ ProtocolEditorWidget 持有 Repository + format_ + config_format_
       + config_path_ + file_entries_ + frame_file_map_
       ├─→ 填充信号树 + 位视图 + 属性面板
       └─→ 保存快照（undo）
```

### 6.2 保存流程

```
用户保存 (Ctrl+S)
  │
  ├─ format_ == Json ──→ serialize_repository(path, repo_)
  │
  ├─ format_ == Xml ───→ serialize_xml_repository(path, repo_)
  │
  └─ format_ == ConfigDriven
       ├─ serialize_xml_config(config_path_, file_entries_)
       └─ for each frame in repo_:
            ├─ 通过 frame_file_map_[frame.id()] 获取 relative path
            └─ serialize_xml_frame_file(base_dir / path, frame)
```

### 6.3 编辑器内部数据流

```
编辑操作 (属性修改/增删节点)
  → 直接修改 icd::Frame / icd::Node
  → 保存快照 (snapshot_version + serialize_repository_to_json + file_entries JSON + qCompress)
  → setModified(true)
  → 刷新 affected 视图
```

快照格式统一使用 JSON（`serialize_repository_to_json()`），与文件格式无关。ConfigDriven 模式下快照额外包含 `file_entries_` 序列化和 `config_path_`，且 `restoreSnapshot()` 时执行磁盘重建（删除旧帧文件 → 重写所有帧文件 → 重写 ICDConfig → 重建 `frame_file_map_`）。非 ConfigDriven 模式下快照仅含 Repository，行为不变。

---

## 7. 模块依赖关系

```
ProtocolEditorWidget
├── icd::format::deserialize_repository()        (已有)
├── icd::format::serialize_repository()           (已有)
├── icd::format::deserialize_xml_repository()     (Phase 1 新增)
├── icd::format::serialize_xml_repository()       (Phase 1 新增)
├── icd::Loader::init_with_metadata()             (Phase 2 新增)
├── icd::FrameFileInfo                             (Phase 2 新增, 公共类型)
├── icd::format::serialize_xml_config()           (Phase 2 新增)
├── icd::format::serialize_xml_frame_file()       (Phase 2 新增)
├── icd::format::serialize_json_config()          (Phase 2 新增)
└── icd::format::serialize_json_frame_file()      (Phase 2 新增)

icd_utility 内部共享
└── type_mapping.hpp / type_mapping.cpp           (Phase 1 新增)

ProjectStructureWidget (src/app)
└── icd_utility                                    (Phase 4 新增依赖)
```

---

## 8. 测试策略

| 层级 | 测试文件 | 测试内容 |
|------|---------|---------|
| icd_utility 单元测试 | `test_eprotox.cpp` | .eprotox 往返、类型映射、嵌套节点、CJK 路径、boolean 多种写法 |
| icd_utility 单元测试 | `test_config_writeback.cpp` | 空 ICDConfig 加载、ICDConfig XML/JSON 回写、Tag 值映射、文件路径元数据保留、有损降级 |
| icd_utility 单元测试 | `test_cross_format.cpp`（Phase 5 创建） | .eproto ↔ .eprotox 往返、ICDConfig → .eproto → ICDConfig 往返、Tag 跨格式映射、类型降级验证 |
| ProtocolEditorWidget | 手动测试 | 三种格式加载/保存、ConfigDriven 新建/删除帧、undo/redo 磁盘重建、SaveAs 跨格式转换、孤儿帧文件打开 |
| 项目集成 | 手动测试 | 项目树 ICDConfig 展开、新建对话框、多扩展名扫描 |

---

## 9. 实施顺序与预估

```
Phase 1 (.eprotox + type_mapping.hpp)                ──→  3-4 天
Phase 2 (ICDConfig 回写 + FrameFileInfo + 空config)    ──→  5-6 天
Phase 3 (编辑器路由 + undo/redo磁盘重建 + 孤儿帧)       ──→  4-5 天
Phase 4 (项目集成 + CMake变更)                          ──→  3-4 天
Phase 5 (清理与整合)                                    ──→  2-3 天
```

**依赖关系**：
- Phase 2 依赖 Phase 1（type_mapping 模块）
- Phase 3 依赖 Phase 1 + Phase 2
- Phase 4 依赖 Phase 3
- Phase 5 依赖 Phase 4
- Phase 2.3（允许空 config）可独立先行
- Phase 2.4（修复 Tag bug）可在 Phase 1 前临时先行——`tag_from_legacy_int()` 在 Phase 1 的 `type_mapping.hpp` 中定义，如需提前修复，先在 `xml_parser.cpp`/`json_parser.cpp` 中内联 Tag 映射逻辑（if/else 或 switch），待 Phase 1 完成后替换为 `tag_from_legacy_int()` 调用

---

## 10. 已知技术债务（不在本次范围）

| 问题 | 影响 | 建议修复时机 |
|------|------|-------------|
| `IcdProtocolUtils::tagName()` 有损映射 | UI 显示 sum2→"sum", xor1→"xor" | 下次 UI 迭代 |
| `IcdProtocolUtils` 中 shortint/smallint 都映射 "int16" | UI 类型显示有损 | 同上 |
| 快照式 undo 替代 QUndoStack | 内存开销大（32 条全状态压缩 JSON） | 性能问题出现时 |
| `ProtocolManagerWidget::parseEprotoFrames()` 手动解析 | 重复 icd_utility 能力 | Phase 5 |
| ProtocolRef 领域盲 | 项目层不携带 Frame/Node 信息 | 引入 ProtocolService 时 |
| 位域重叠检测未实现 | 设计文档规划但未实现 | 校验体系迭代 |
| 帧完整性校验未实现 | 设计文档规划但未实现 | 同上 |
| 文件名编码解析/生成器 | 设计文档规划但未实现 | 编辑效率迭代 |
| ConfigDriven 模式无文件监视 | 外部修改帧文件后内存过期 | 引入 QFileSystemWatcher |
| "导入XML"按钮强制转换为 .eproto | 不支持保持原格式导入 | 后续迭代 |

---

## 11. 后续演进方向

本计划解决格式统一和 ICDConfig 回写问题后，为以下演进奠定基础：

1. **ProtocolService**：在 core 层引入项目级 Repository 缓存，统一查询"项目有哪些帧/信号可用"，消除 `parseEprotoFrames()` 重复解析
2. **UI 合并**：`ProtocolManagerWidget` 能力合并到 `ProjectStructureWidget`，项目树对协议文件有领域感知（展开显示帧/节点）
3. **多 ICDConfig 支持**：当前设计假设每项目一个 ICDConfig，后续可支持按总线类型分组的多 ICDConfig
4. **文件监视**：ConfigDriven 模式下引入 `QFileSystemWatcher` 监视帧文件变化，检测外部修改时提示重新加载
5. **校验体系**：位域重叠检测、帧完整性校验、Tag 唯一性
6. **位视图增强**：位图换行算法、Hover 联动高亮、右键快捷操作、分组着色
7. **外部集成**：CSV 映射预览、设备资源选择器、XML ↔ eproto 格式转换工具

---

## 12. 审查修正记录

> 本计划经过四轮审查（2026-06-25），共 46 条修正已全部合并到 §1-§11 正文中。以下为审查轮次和修正编号索引：
>
> - **首轮审查**（§12 原内容）：S1-S10, M1-M16, L1-L12（已在初次合并时并入正文）
> - **二次审查**（原 §13）：P1-P3, M1-M3, L1-L5
> - **三次审查**（原 §14）：P4-P5, M4-M7, L6-L8
> - **四次审查**（原 §15）：P6-P9, M8-M13, L9-L13