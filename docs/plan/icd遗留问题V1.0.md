# icd_utility 与 IcdSchema.xsd 兼容性评估

## Context

用户要求只读评估 `src/icd_utility` 当前能力是否兼容 `temp/projects/t1/protocol/IcdSchema.xsd` 的约束，不需要实现改造计划。本次分析聚焦：XSD 约束、`icd_utility` 对外 API、legacy XML 解析/序列化实现、`.eprotox` XML 实现与 XSD 的差异。

## 评估结论

当前 `icd_utility` **部分兼容读取 XSD 所描述的 legacy `<ICDData>` 帧 XML，但不完全兼容 XSD 约束；写出的 legacy XML 也不是严格 XSD 有效文档**。`.eprotox` 的 `<ICDProtocol>` 格式与该 XSD 不是同一套 XML 结构，不能视为兼容。

## 主要证据

- XSD 根结构：`ICDData -> Name, Data -> Item*`，节点类型为 `ICDWord`。
- `parse_xml_frame()` 支持读取 `<ICDData>`、`<Data>`、`<Item>`，并递归读取 `<Childs>`。
- `serialize_xml_frame_file()` 写出 `<ICDData>`，但节点字段顺序和部分字段值不符合 XSD。
- `.eprotox` 使用 `<ICDProtocol>`、`<Frame>`、`<Nodes>`、`<ValueType>`、`<Children>`、`<Attrs>`，不在该 XSD 中定义。

## 关键不兼容点

1. **节点字段顺序不符合 XSD**
   - XSD 要求 `Offset, StartBit, BitWidth, Type, Name, Description, GroupName?, IsScaled, ...` 的严格 sequence。
   - legacy serializer 当前写出 `Name, Description, Offset, StartBit, BitWidth, Type, Tag, IsScaled, ...`。
   - 结果：`serialize_xml_frame_file()` 生成的 XML 不能通过该 XSD 的严格顺序校验。

2. **`IsScaled` 写出类型不符合 XSD**
   - XSD 定义 `IsScaled` 为 `xs:int`，注释为 `0/1`。
   - legacy serializer 当前写出 `true` / `false`。
   - 结果：写出的 legacy XML 不满足 `xs:int`。

3. **`ScaleConveror` legacy 写出名称大小写不符合 XSD**
   - XSD 定义 `ScaleConveror`。
   - legacy parser 能读 `ScaleConveror`。
   - legacy serializer 使用共享常量 `kScaleConverorKey`，当前是 `scaleConveror`，大小写不匹配。

4. **`DefauleValue` 完全未建模**
   - XSD 定义可选 `DefauleValue`。
   - `NodeAttrs` 和 `SchemaNodeDef` 没有对应字段，parser 不读，serializer 不写。
   - 结果：读取后再保存会丢失 XSD 允许的数据。

5. **`xs:double` 精度被降级为 float**
   - XSD 中 `ScaleA/ScaleB/Min/Max` 是 `xs:double`。
   - `NodeAttrs` 使用 `std::optional<float>`，解析也用 `std::stof`。
   - 结果：存在精度损失，不能完整表达 XSD 的 double 能力。

6. **未执行真正的 XSD 校验**
   - parser 只按元素名查找，不验证严格 sequence。
   - 不校验 optional 元素的相对顺序。
   - 不验证 unknown extra elements。
   - 不校验 `ICDWords` 至少一个 `Item` 的递归约束之外的完整 schema 规则。

7. **部分 internal ValueType 无法通过 legacy XML 无损表达**
   - `smallint` 写成 `int`，读回变 `integer`。
   - `boolean` 写成 `byte`，读回变 `byte_`。
   - `ulong_` 写成 `dword`，读回变 `longword`。
   - 结果：legacy XML round-trip 不是完全无损。

8. **`.eprotox` 与该 XSD 不兼容**
   - `.eprotox` 根是 `ICDProtocol`，XSD 根是 `ICDData`。
   - `.eprotox` 节点使用 `ValueType`、`Attrs`、`Children`，XSD 使用 `Type`、扁平属性、`Childs`。
   - 结果：`.eprotox` 不能用该 `IcdSchema.xsd` 校验。

## 可认为兼容的能力

- 能读取基本 `<ICDData>` / `<Data>` / `<Item>` 结构。
- 能读取 XSD 中前 7 个核心必填字段：`Offset`、`StartBit`、`BitWidth`、`Type`、`Name`、`Description`、`IsScaled`。
- 能读取递归 `<Childs>`。
- 能读取 `GroupName`、`SystemName`、`Unit`、`ValueTextList`、`ScaleFormula`、`ScaleConveror`、`LinkTo`、`ScaleA`、`ScaleB`、`Min`、`Max`、`Tag` 的大部分语义。
- 能处理 CJK 路径下的 XML 文件读写。

## 最终判断

如果“兼容”定义为：**能读取部分按该 XSD 编写的 legacy ICDData XML 并构建内部 Repository**，则当前是“基本可用但有数据损失风险”。

如果“兼容”定义为：**读写均能生成/接受严格满足 `IcdSchema.xsd` 的 XML，且 round-trip 不丢字段、不降精度、不改变语义**，则当前 **不兼容**。

## 验证建议

- 使用 XML Schema 校验工具对 `serialize_xml_frame_file()` 输出结果校验 `IcdSchema.xsd`，应能复现字段顺序和 `IsScaled` 类型问题。
- 构造包含 `DefauleValue`、double 高精度值、`ScaleConveror`、递归 `Childs` 的样例做 parse -> serialize round-trip，对比字段丢失和精度变化。
