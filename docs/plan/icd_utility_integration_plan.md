# icd_utility 彻底融入 etest-demo — 改造计划

## 目标

将 `src/icd_utility/` 从独立子项目彻底融入主项目 `etest_demo`，实现：

1. **CMake 统一管理** — icd_utility 作为 `src/` 下的子目录，继承顶层 CMake 配置
2. **三方库集中到 `3rdparty/`** — pugixml、nlohmann/json、tl/expected 移到顶层
3. **测试纳入 `add_etest` 体系** — 全部 8 个测试迁移到 `tests/icd_utility/`，使用 gtest

---

## 最终目录结构变化

```
etest-demo/
├── CMakeLists.txt                          ← [EDIT] 新增 3 个 3rdparty 子目录
│
├── 3rdparty/
│   ├── pugixml-1.15/                       ← [MOVE] from src/icd_utility/src/pugixml/
│   │   ├── pugixml.cpp
│   │   ├── pugixml.hpp
│   │   ├── pugiconfig.hpp                  ← [MOVE] from src/icd_utility/third_party/pugixml/
│   │   └── CMakeLists.txt                  ← [NEW]
│   ├── nlohmann/                           ← [MOVE] from src/icd_utility/src/nlohmann/
│   │   ├── json.hpp
│   │   └── detail/
│   └── tl/                                 ← [MOVE] from src/icd_utility/src/tl/
│       └── expected.hpp
│
├── src/
│   ├── CMakeLists.txt                      ← [EDIT] + add_subdirectory(icd_utility)
│   ├── core/                               ← 已有，不变
│   ├── app/                                ← 已有，不变
│   └── icd_utility/                        ← [REWRITE CMakeLists.txt]
│       ├── CMakeLists.txt                  ← [REWRITE] 移除 standalone 特征
│       ├── include/icd/                    ← 保留不动（public API）
│       │   ├── export.hpp
│       │   ├── error.hpp
│       │   ├── types.hpp
│       │   ├── node.hpp
│       │   ├── frame.hpp
│       │   ├── repository.hpp
│       │   └── loader.hpp
│       └── src/
│           ├── compat/
│           │   ├── span.hpp
│           │   └── bit.hpp
│           ├── core/                       ← 保留
│           │   ├── node.cpp
│           │   ├── frame.cpp
│           │   └── repository.cpp
│           ├── schema/                     ← 保留
│           │   ├── schema.hpp
│           │   ├── schema.cpp
│           │   ├── builder.hpp
│           │   └── builder.cpp
│           ├── format/                     ← 保留
│           │   ├── xml_parser.hpp
│           │   ├── xml_parser.cpp
│           │   ├── json_parser.hpp
│           │   └── json_parser.cpp
│           └── loader/                     ← 保留
│               └── loader.cpp
│
├── tests/
│   ├── CMakeLists.txt                      ← [EDIT] + add_subdirectory(icd_utility)
│   ├── icd_utility/                        ← [NEW]
│   │   ├── CMakeLists.txt                  ← [NEW] 使用 add_etest 宏
│   │   ├── data/
│   │   │   ├── xml/                        ← [MOVE] from src/icd_utility/tests/data/xml/
│   │   │   │   ├── config-valid.xml
│   │   │   │   ├── frame-valid.xml
│   │   │   │   ├── frame-malformed.xml
│   │   │   │   ├── frame-missing-field.xml
│   │   │   │   └── compat/                 ← 真实世界 ICD 样本
│   │   │   │       ├── ICDConfig.xml
│   │   │   │       ├── fdr0.xml
│   │   │   │       ├── fdr0_config.xml
│   │   │   │       ├── fdr0_s.xml
│   │   │   │       ├── fdr1.xml
│   │   │   │       ├── io.xml
│   │   │   │       ├── io2.xml
│   │   │   │       └── 10AIO2.xml
│   │   │   └── json/                       ← [MOVE] from src/icd_utility/tests/data/json/
│   │   │       ├── config-valid.json
│   │   │       ├── frame-valid.json
│   │   │       ├── frame-malformed.json
│   │   │       └── frame-missing-field.json
│   │   ├── test_builder.cpp                ← [MOVE + REWRITE gtest]
│   │   ├── test_repository.cpp             ← [MOVE + REWRITE gtest]
│   │   ├── test_xml_parser.cpp             ← [MOVE + REWRITE gtest]
│   │   ├── test_json_parser.cpp            ← [MOVE + REWRITE gtest]
│   │   ├── test_loader.cpp                 ← [MOVE + REWRITE gtest]
│   │   ├── test_node_value.cpp             ← [MOVE + REWRITE gtest]
│   │   ├── test_export_macros.cpp          ← [MOVE + REWRITE gtest]
│   │   └── test_compat_snapshot.cpp        ← [MOVE + REWRITE gtest]
│   ├── core/
│   ├── sol2/
│   ├── lua/
│   └── ...                                 ← 其他已有测试不变
```

---

## Phase 1 — 三方库迁移到 3rdparty/

### 1.1 pugixml-1.15（需要编译）

**操作：** 将 `src/icd_utility/src/pugixml/`（含 pugixml.cpp、pugixml.hpp）移到 `3rdparty/pugixml-1.15/`，同时将 `src/icd_utility/third_party/pugixml/pugiconfig.hpp` 也复制到同一目录。

**新增文件：** `3rdparty/pugixml-1.15/CMakeLists.txt`

```cmake
add_library(pugixml_local STATIC pugixml.cpp)
target_include_directories(pugixml_local PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
```

### 1.2 nlohmann/json（header-only）

**操作：** 将 `src/icd_utility/src/nlohmann/` 整体移到 `3rdparty/nlohmann/`。

**不需要新增 CMakeLists.txt**，在顶层 CMakeLists.txt 中直接定义 INTERFACE 目标。

### 1.3 tl/expected（header-only）

**操作：** 将 `src/icd_utility/src/tl/` 整体移到 `3rdparty/tl/`。

**不需要新增 CMakeLists.txt**，在顶层 CMakeLists.txt 中直接定义 INTERFACE 目标。

---

## Phase 2 — 顶层 CMakeLists.txt 修改

在 `D:\trae_workspace\etest-demo\CMakeLists.txt` 中，在现有 `3rdparty` 区块（约第 100 行，sol2 定义之前）追加以下内容：

```cmake
# =======================================
# icd_utility 三方依赖
# =======================================

# pugixml XML 解析库
add_subdirectory(3rdparty/pugixml-1.15)

# nlohmann/json (header-only)
add_library(nlohmann INTERFACE)
target_include_directories(nlohmann INTERFACE ${CMAKE_SOURCE_DIR}/3rdparty/nlohmann)

# tl/expected (header-only)
add_library(tl INTERFACE)
target_include_directories(tl INTERFACE ${CMAKE_SOURCE_DIR}/3rdparty/tl)
```

---

## Phase 3 — src/CMakeLists.txt 修改

在 `D:\trae_workspace\etest-demo\src\CMakeLists.txt` 中追加一行（在 `add_subdirectory(app)` 之后）：

```cmake
add_subdirectory(icd_utility)
```

---

## Phase 4 — src/icd_utility/CMakeLists.txt 重写

**删除原有全部内容**，替换为：

```cmake
add_library(icd_utility
    src/core/node.cpp
    src/core/frame.cpp
    src/core/repository.cpp
    src/schema/schema.cpp
    src/schema/builder.cpp
    src/format/xml_parser.cpp
    src/format/json_parser.cpp
    src/loader/loader.cpp
)

target_compile_features(icd_utility PUBLIC cxx_std_17)

target_include_directories(icd_utility PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/src"
)

target_link_libraries(icd_utility PUBLIC
    pugixml_local
    nlohmann
    tl
)
```

**关键说明：**

- 移除了 `project(icd_utility LANGUAGES CXX)` — 不再独立项目
- 移除了 `option(BUILD_SHARED_LIBS)` — 跟随顶层配置
- 移除了 `enable_testing()` + `add_subdirectory(tests)` — 测试移入 tests/
- 移除了 `ICD_UTILITY_BUILDING_DLL / ICD_UTILITY_SHARED / ICD_UTILITY_STATIC` 导出宏定义 — 静态编译即可
- 将 `"${CMAKE_CURRENT_LIST_DIR}/src"` 设为 PUBLIC include — 确保 `loader.cpp` 中的 `#include "../format/json_parser.hpp"` 等相对路径能正确解析

---

## Phase 5 — 测试迁移

### 5.1 tests/CMakeLists.txt 修改

在 `D:\trae_workspace\etest-demo\tests\CMakeLists.txt` 中，在 `add_subdirectory(core)` 之后新增：

```cmake
add_subdirectory(icd_utility)
```

### 5.2 tests/icd_utility/CMakeLists.txt（新增）

```cmake
set(ICD_TEST_DATA_DIR "${CMAKE_CURRENT_LIST_DIR}/data")

# ====== 需要内部头文件访问的测试（链接 icd_utility 即可，其 PUBLIC include 暴露了 src/） ======

add_etest(
    NAME test_builder
    SOURCES test_builder.cpp
    LIBS icd_utility
    LABELS ICD
)

add_etest(
    NAME test_xml_parser
    SOURCES test_xml_parser.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_xml_parser PRIVATE
    ICD_TEST_XML_DIR="${ICD_TEST_DATA_DIR}/xml"
    ICD_TEST_JSON_DIR="${ICD_TEST_DATA_DIR}/json"
)

add_etest(
    NAME test_json_parser
    SOURCES test_json_parser.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_json_parser PRIVATE
    ICD_TEST_JSON_DIR="${ICD_TEST_DATA_DIR}/json"
    ICD_TEST_XML_DIR="${ICD_TEST_DATA_DIR}/xml"
)

add_etest(
    NAME test_loader
    SOURCES test_loader.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_loader PRIVATE
    ICD_TEST_XML_DIR="${ICD_TEST_DATA_DIR}/xml"
    ICD_TEST_JSON_DIR="${ICD_TEST_DATA_DIR}/json"
)

add_etest(
    NAME test_compat_snapshot
    SOURCES test_compat_snapshot.cpp
    LIBS icd_utility
    LABELS ICD
)
target_compile_definitions(test_compat_snapshot PRIVATE
    ICD_TEST_XML_COMPAT_DIR="${ICD_TEST_DATA_DIR}/xml/compat"
)

# ====== 仅需 public API 的测试 ======

add_etest(
    NAME test_repository
    SOURCES test_repository.cpp
    LIBS icd_utility
    LABELS ICD
)

add_etest(
    NAME test_node_value
    SOURCES test_node_value.cpp
    LIBS icd_utility
    LABELS ICD
)

add_etest(
    NAME test_export_macros
    SOURCES test_export_macros.cpp
    LIBS icd_utility
    LABELS ICD
)
```

### 5.3 测试源文件改造（main → gtest）

全部 8 个测试文件需要从传统的 `int main()` + `return N` 模式改造为 gtest 的 `TEST()` 宏模式。

#### 通用改造规则

| 原写法 | gtest 写法 |
|--------|-----------|
| `int main() { ... return 0; }` | `int main(argc, argv) { InitGoogleTest; return RUN_ALL_TESTS(); }` |
| `if (cond) return N;` | `EXPECT_TRUE(cond)` 或 `ASSERT_TRUE(cond)` |
| `if (a != b) return N;` | `EXPECT_EQ(a, b)` |
| `if (!result.has_value()) return N;` | `ASSERT_TRUE(result.has_value())` |
| `if (result.error().code != E) return N;` | `EXPECT_EQ(result.error().code, E)` |

#### 各文件具体改造方案

##### test_builder.cpp（4 个独立场景 → 4 个 TEST）

```cpp
#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/frame.hpp>
#include <icd/repository.hpp>
#include <icd/types.hpp>
#include "../src/schema/builder.hpp"    // 路径不变，链接 icd_utility 后可从 src/ 根目录解析
#include "../src/schema/schema.hpp"

namespace {

icd::schema::SchemaNodeDef make_node(std::string name, int offset) { /* 不变 */ }
icd::schema::SchemaFrameDef make_frame(int id, std::string name) { /* 不变 */ }

} // namespace

TEST(BuilderTest, DuplicateFrameId) {
    icd::schema::SchemaConfig config;
    config.frames.push_back(make_frame(1, "frame-a"));
    config.frames.push_back(make_frame(1, "frame-b"));
    auto result = icd::schema::build_repository(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::duplicate_frame_id);
}

TEST(BuilderTest, DuplicateFrameName) {
    icd::schema::SchemaConfig config;
    config.frames.push_back(make_frame(1, "frame-a"));
    config.frames.push_back(make_frame(2, "frame-a"));
    auto result = icd::schema::build_repository(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::duplicate_frame_name);
}

TEST(BuilderTest, NormalBuildWithAttrs) {
    icd::schema::SchemaConfig config;
    auto frame = make_frame(10, "frame-ok");
    auto child = make_node("child", 1);
    child.attrs.system_name = "SystemA";
    child.attrs.group_name = "GroupA";
    child.attrs.unit = "V";
    child.attrs.min = 1.5f;
    child.attrs.max = 9.5f;
    child.attrs.scale_a = 2.0f;
    child.attrs.scale_b = 3.0f;
    frame.roots.front().children.push_back(std::move(child));
    config.frames.push_back(std::move(frame));
    auto result = icd::schema::build_repository(config);
    ASSERT_TRUE(result.has_value());
    const auto& repo = result.value();
    EXPECT_EQ(repo.frames().size(), 1u);
    EXPECT_NE(repo.find(10), nullptr);
    EXPECT_NE(repo.find("frame-ok"), nullptr);
    const auto* child_node = repo.find("frame-ok", "child");
    ASSERT_NE(child_node, nullptr);
    EXPECT_EQ(child_node->attrs().system_name, "SystemA");
    EXPECT_EQ(child_node->attrs().group_name, "GroupA");
    EXPECT_EQ(child_node->attrs().unit, "V");
    ASSERT_TRUE(child_node->attrs().min.has_value());
    EXPECT_EQ(*child_node->attrs().min, 1.5f);
    ASSERT_TRUE(child_node->attrs().max.has_value());
    EXPECT_EQ(*child_node->attrs().max, 9.5f);
    ASSERT_TRUE(child_node->attrs().scale_a.has_value());
    EXPECT_EQ(*child_node->attrs().scale_a, 2.0f);
    ASSERT_TRUE(child_node->attrs().scale_b.has_value());
    EXPECT_EQ(*child_node->attrs().scale_b, 3.0f);
}

TEST(BuilderTest, EmptyNodeNameRejected) {
    icd::schema::SchemaConfig config;
    auto frame = make_frame(20, "frame-empty-node");
    frame.roots.front().name.clear();
    config.frames.push_back(std::move(frame));
    auto result = icd::schema::build_repository(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_node);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

##### test_repository.cpp（多场景 → 多个 TEST）

```cpp
#include <gtest/gtest.h>
#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>
#include <memory>

TEST(RepositoryTest, EmptyRepoReturnsNull) {
    icd::Repository repo;
    EXPECT_TRUE(repo.frames().empty());
    EXPECT_EQ(repo.find(42), nullptr);
    EXPECT_EQ(repo.find("missing-frame"), nullptr);
}

TEST(RepositoryTest, NodeTreeFind) {
    auto root = std::make_unique<icd::Node>("root", "root desc", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    auto target = std::make_unique<icd::Node>("target", "target desc", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    auto nested = std::make_unique<icd::Node>("nested", "nested desc", 2, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    nested->add_child(std::make_unique<icd::Node>("deep-target", "deep desc", 3, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    target->add_child(std::move(nested));
    auto sibling = std::make_unique<icd::Node>("sibling", "sibling desc", 4, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root->add_child(std::move(target));
    root->add_child(std::move(sibling));
    EXPECT_NE(root->find("target"), nullptr);
    EXPECT_NE(root->find("deep-target"), nullptr);
}

TEST(RepositoryTest, FrameFind) {
    auto root = std::make_unique<icd::Node>("root", "root desc", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root->add_child(std::make_unique<icd::Node>("target", "target desc", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    root->add_child(std::make_unique<icd::Node>("deep-target", "deep desc", 2, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    auto frame = std::make_unique<icd::Frame>(1, "frame-a", "frame desc", icd::FrameType::data, icd::ByteOrder::little_endian);
    frame->add_root(std::move(root));
    EXPECT_EQ(frame->find("missing-node"), nullptr);
    EXPECT_NE(frame->find("target"), nullptr);
    EXPECT_NE(frame->find("deep-target"), nullptr);
}

TEST(RepositoryTest, MultiFrameFind) {
    icd::Repository repo;
    auto frame_a = std::make_unique<icd::Frame>(1, "frame-a", "frame desc", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto root_a = std::make_unique<icd::Node>("root-a", "root a", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root_a->add_child(std::make_unique<icd::Node>("target", "target in a", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    frame_a->add_root(std::move(root_a));
    auto frame_b = std::make_unique<icd::Frame>(2, "frame-b", "frame desc", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto root_b = std::make_unique<icd::Node>("root-b", "root b", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{});
    root_b->add_child(std::make_unique<icd::Node>("target", "duplicate name in other frame", 1, 0, 8, icd::ValueType::byte_, icd::Tag::none, icd::NodeAttrs{}));
    frame_b->add_root(std::move(root_b));
    repo.add_frame(std::move(frame_a));
    repo.add_frame(std::move(frame_b));
    EXPECT_NE(repo.find(1), nullptr);
    EXPECT_NE(repo.find("frame-a"), nullptr);
    EXPECT_NE(repo.find("frame-a", "target"), nullptr);
    EXPECT_NE(repo.find("frame-b", "target"), nullptr);
    EXPECT_NE(repo.find("frame-a", "target"), repo.find("frame-b", "target"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

##### test_xml_parser.cpp（6 场景 → 6 个 TEST）

```cpp
#include <gtest/gtest.h>
#include <icd/error.hpp>
#include "../src/format/xml_parser.hpp"
#include "../src/format/json_parser.hpp"
#include <filesystem>

#ifndef ICD_TEST_XML_DIR
#error ICD_TEST_XML_DIR is not defined
#endif
#ifndef ICD_TEST_JSON_DIR
#error ICD_TEST_JSON_DIR is not defined
#endif

namespace fs = std::filesystem;

struct XmlParserTest : ::testing::Test {
    fs::path xmlBase = fs::path(ICD_TEST_XML_DIR);
    fs::path jsonBase = fs::path(ICD_TEST_JSON_DIR);
};

TEST_F(XmlParserTest, CrossFormatConsistency) {
    auto xml = icd::format::parse_xml_frame(xmlBase / "frame-valid.xml");
    auto json = icd::format::parse_json_frame(jsonBase / "frame-valid.json");
    ASSERT_TRUE(xml.has_value());
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ(xml->name, json->name);
    EXPECT_EQ(xml->roots.size(), json->roots.size());
    EXPECT_EQ(xml->roots.front().name, json->roots.front().name);
    EXPECT_EQ(xml->roots.front().offset, json->roots.front().offset);
    EXPECT_EQ(xml->roots.front().children.size(), json->roots.front().children.size());
    EXPECT_EQ(xml->roots.front().children.front().name, json->roots.front().children.front().name);
    EXPECT_EQ(xml->roots.front().attrs.system_name, json->roots.front().attrs.system_name);
    EXPECT_EQ(xml->roots.front().attrs.unit, json->roots.front().attrs.unit);
}

TEST_F(XmlParserTest, MalformedXml) {
    auto result = icd::format::parse_xml_frame(xmlBase / "frame-malformed.xml");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::parse_error);
    EXPECT_EQ(result.error().file.filename(), "frame-malformed.xml");
}

TEST_F(XmlParserTest, MissingFieldXml) {
    auto result = icd::format::parse_xml_frame(xmlBase / "frame-missing-field.xml");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::schema_error);
    EXPECT_EQ(result.error().file.filename(), "frame-missing-field.xml");
    EXPECT_FALSE(result.error().path_hint.empty());
}

TEST_F(XmlParserTest, MalformedJson) {
    auto result = icd::format::parse_json_frame(jsonBase / "frame-malformed.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::parse_error);
    EXPECT_EQ(result.error().file.filename(), "frame-malformed.json");
}

TEST_F(XmlParserTest, MissingFieldJson) {
    auto result = icd::format::parse_json_frame(jsonBase / "frame-missing-field.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::schema_error);
    EXPECT_EQ(result.error().file.filename(), "frame-missing-field.json");
    EXPECT_FALSE(result.error().path_hint.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

##### test_json_parser.cpp（4 场景 → 4 个 TEST）

```cpp
#include <gtest/gtest.h>
#include <icd/error.hpp>
#include "../src/format/json_parser.hpp"
#include "../src/format/xml_parser.hpp"
#include <filesystem>

#ifndef ICD_TEST_JSON_DIR
#error ICD_TEST_JSON_DIR is not defined
#endif
#ifndef ICD_TEST_XML_DIR
#error ICD_TEST_XML_DIR is not defined
#endif

namespace fs = std::filesystem;

struct JsonParserTest : ::testing::Test {
    fs::path jsonBase = fs::path(ICD_TEST_JSON_DIR);
    fs::path xmlBase = fs::path(ICD_TEST_XML_DIR);
};

TEST_F(JsonParserTest, ConfigParse) {
    auto config = icd::format::parse_json_config(jsonBase / "config-valid.json");
    ASSERT_TRUE(config.has_value()) << "error code=" << static_cast<int>(config.error().code);
    ASSERT_EQ(config->files.size(), 1u);
    EXPECT_EQ(config->files.front().logical_name, "FrameA");
}

TEST_F(JsonParserTest, FrameParseCrossCheck) {
    auto frame = icd::format::parse_json_frame(jsonBase / "frame-valid.json");
    auto xml = icd::format::parse_xml_frame(xmlBase / "frame-valid.xml");
    ASSERT_TRUE(frame.has_value());
    ASSERT_TRUE(xml.has_value());
    EXPECT_EQ(frame->name, "FrameA");
    EXPECT_EQ(frame->roots.size(), 1u);
    EXPECT_EQ(frame->roots.front().children.size(), 1u);
    EXPECT_EQ(frame->roots.front().name, xml->roots.front().name);
}

TEST_F(JsonParserTest, MalformedJson) {
    auto result = icd::format::parse_json_frame(jsonBase / "frame-malformed.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::parse_error);
    EXPECT_EQ(result.error().file.filename(), "frame-malformed.json");
}

TEST_F(JsonParserTest, MissingFieldJson) {
    auto result = icd::format::parse_json_frame(jsonBase / "frame-missing-field.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::schema_error);
    EXPECT_EQ(result.error().file.filename(), "frame-missing-field.json");
    EXPECT_FALSE(result.error().path_hint.empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

##### test_loader.cpp（4 场景 → 4 个 TEST）

注意修改 `#include`：原来有 `#include "../include/icd/loader.hpp"`，改为 `<icd/loader.hpp>`。

```cpp
#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/loader.hpp>
#include <icd/types.hpp>
#include <filesystem>

#ifndef ICD_TEST_XML_DIR
#error ICD_TEST_XML_DIR is not defined
#endif
#ifndef ICD_TEST_JSON_DIR
#error ICD_TEST_JSON_DIR is not defined
#endif

namespace fs = std::filesystem;

struct LoaderTest : ::testing::Test {
    fs::path xmlBase = fs::path(ICD_TEST_XML_DIR);
    fs::path jsonBase = fs::path(ICD_TEST_JSON_DIR);
};

TEST_F(LoaderTest, XmlAutoDetect) {
    auto repo = icd::Loader::init(xmlBase / "config-valid.xml");
    ASSERT_TRUE(repo.has_value()) << repo.error().message;
    EXPECT_NE(repo->find("FrameA"), nullptr);
}

TEST_F(LoaderTest, JsonAutoDetect) {
    auto repo = icd::Loader::init(jsonBase / "config-valid.json");
    ASSERT_TRUE(repo.has_value()) << repo.error().message;
    EXPECT_NE(repo->find("FrameA"), nullptr);
}

TEST_F(LoaderTest, ExplicitXmlFormat) {
    auto repo = icd::Loader::init(xmlBase / "config-valid.xml", icd::Format::xml);
    ASSERT_TRUE(repo.has_value());
}

TEST_F(LoaderTest, UnsupportedFormat) {
    auto repo = icd::Loader::init(jsonBase / "config-valid.unsupported", icd::Format::auto_detect);
    ASSERT_FALSE(repo.has_value());
    EXPECT_EQ(repo.error().code, icd::ErrorCode::unsupported_format);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

##### test_node_value.cpp（43 子测试 → 分组 TEST）

原文件 362 行、43 个 return N，每个有独立注释。建议按功能分组，拆分并保留原有测试逻辑。

```cpp
#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace {

template <typename T>
bool expect_value(const tl::expected<icd::NodeValue, icd::Error>& value, const T& expected) {
    return value.has_value() && std::holds_alternative<T>(*value) && std::get<T>(*value) == expected;
}

std::unique_ptr<icd::Node> make_node(std::string name, int offset, int bit_offset, int bit_width, icd::ValueType vt) {
    return std::make_unique<icd::Node>(std::move(name), "", offset, bit_offset, bit_width, vt, icd::Tag::none, icd::NodeAttrs{});
}

} // namespace

TEST(NodeValueTest, EagerDecodeWord) {
    auto root = make_node("root", 0, 0, 16, icd::ValueType::word);
    auto child = make_node("child", 2, 0, 8, icd::ValueType::byte_);
    auto* child_ptr = child.get();
    root->add_child(std::move(child));
    icd::Frame frame(1, "frame", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto* root_ptr = root.get();
    frame.add_root(std::move(root));
    const std::array<std::byte, 3> payload{std::byte{0x34}, std::byte{0x12}, std::byte{0xAB}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    auto root_value = root_ptr->get_value();
    ASSERT_TRUE(root_value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint16_t>(**root_value));
    EXPECT_EQ(std::get<std::uint16_t>(**root_value), static_cast<std::uint16_t>(0x1234));
    auto child_value = child_ptr->get_value();
    ASSERT_TRUE(child_value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint64_t>(**child_value));
    EXPECT_EQ(std::get<std::uint64_t>(**child_value), static_cast<std::uint64_t>(0xAB));
}

TEST(NodeValueTest, LazyDecodeWord) {
    auto root = make_node("root", 0, 0, 16, icd::ValueType::word);
    auto* root_ptr = root.get();
    icd::Frame frame(2, "frame-lazy", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    frame.add_root(std::move(root));
    const std::array<std::byte, 2> payload{std::byte{0x78}, std::byte{0x56}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::lazy).has_value());
    EXPECT_TRUE(root_ptr->modified());
    auto value = root_ptr->get_value();
    ASSERT_TRUE(expect_value<std::uint16_t>(value, static_cast<std::uint16_t>(0x5678)));
    EXPECT_FALSE(root_ptr->modified());
}

TEST(NodeValueTest, SetValueNoFrame) {
    icd::Node node("manual", "", 0, 0, 16, icd::ValueType::word, icd::Tag::none, {});
    auto result = node.set_value(icd::NodeValue{static_cast<std::uint16_t>(0x2222)});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, SetValueAndPropagate) {
    auto root = make_node("parent", 0, 0, 16, icd::ValueType::word);
    auto child = make_node("child", 0, 0, 8, icd::ValueType::byte_);
    auto* child_ptr = child.get();
    root->add_child(std::move(child));
    icd::Frame frame(3, "frame-set", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto* root_ptr = root.get();
    frame.add_root(std::move(root));
    const std::array<std::byte, 2> payload{std::byte{0x11}, std::byte{0x22}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    EXPECT_FALSE(child_ptr->modified());
    ASSERT_TRUE(root_ptr->set_value(icd::NodeValue{static_cast<std::uint16_t>(0x3344)}).has_value());
    auto root_value = root_ptr->get_value();
    ASSERT_TRUE(expect_value<std::uint16_t>(root_value, static_cast<std::uint16_t>(0x3344)));
    EXPECT_TRUE(child_ptr->modified());
    auto child_value = child_ptr->get_value();
    ASSERT_TRUE(expect_value<std::uint64_t>(child_value, static_cast<std::uint64_t>(0x44)));
    auto root_decoded = root_ptr->decode(icd::span<const std::byte>(payload), icd::ByteOrder::little_endian);
    // decode should return the original buffer value, not the modified one
    ASSERT_TRUE(root_decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<std::uint16_t>(*root_decoded));
    EXPECT_EQ(std::get<std::uint16_t>(*root_decoded), static_cast<std::uint16_t>(0x3344));
}

TEST(NodeValueTest, BytesType) {
    icd::Frame frame(4, "frame-bytes", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("bytes", 1, 0, 16, icd::ValueType::bytes);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 4> payload{std::byte{0x00}, std::byte{0x11}, std::byte{0x22}, std::byte{0x00}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    ASSERT_TRUE(node_ptr->set_value(icd::NodeValue{std::vector<std::byte>{std::byte{0xAA}, std::byte{0xBB}}}).has_value());
    auto value = node_ptr->get_value();
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<std::byte>>(**value));
    const auto& bytes = std::get<std::vector<std::byte>>(**value);
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], std::byte{0xAA});
    EXPECT_EQ(bytes[1], std::byte{0xBB});
    auto decoded = node_ptr->decode(icd::span<const std::byte>(payload), icd::ByteOrder::little_endian);
    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<std::byte>>(*decoded));
    const auto& stale = std::get<std::vector<std::byte>>(*decoded);
    // decode reads raw buffer, but buffer was modified by set_value
    ASSERT_EQ(stale.size(), 2u);
    EXPECT_EQ(stale[0], std::byte{0xAA});
    EXPECT_EQ(stale[1], std::byte{0xBB});
}

TEST(NodeValueTest, StringType) {
    icd::Frame frame(5, "frame-string", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("str", 0, 0, 24, icd::ValueType::string_);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 3> payload{std::byte{'X'}, std::byte{'Y'}, std::byte{'Z'}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    ASSERT_TRUE(node_ptr->set_value(icd::NodeValue{std::string{"ABC"}}).has_value());
    auto value = node_ptr->get_value();
    ASSERT_TRUE(expect_value<std::string>(value, std::string{"ABC"}));
}

TEST(NodeValueTest, LengthMismatchBytes) {
    icd::Frame frame(6, "frame-bytes-mismatch", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("bytes", 0, 0, 16, icd::ValueType::bytes);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 2> payload{std::byte{0x11}, std::byte{0x22}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    auto result = node_ptr->set_value(icd::NodeValue{std::vector<std::byte>{std::byte{0xAA}}});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, LengthMismatchString) {
    icd::Frame frame(7, "frame-string-mismatch", "", icd::FrameType::data, icd::ByteOrder::little_endian);
    auto node = make_node("str", 0, 0, 24, icd::ValueType::string_);
    auto* node_ptr = node.get();
    frame.add_root(std::move(node));
    const std::array<std::byte, 3> payload{std::byte{'X'}, std::byte{'Y'}, std::byte{'Z'}};
    ASSERT_TRUE(frame.decode(payload, icd::DecodeMode::eager).has_value());
    auto result = node_ptr->set_value(icd::NodeValue{std::string{"AB"}});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, icd::ErrorCode::invalid_argument);
}

// ====== 独立 decode 测试（不依赖 Frame） ======

TEST(NodeValueTest, DecodeWord) {
    icd::Node node("word", "", 0, 0, 16, icd::ValueType::word, icd::Tag::none, {});
    const std::array<std::byte, 2> frame{std::byte{0x34}, std::byte{0x12}};
    EXPECT_TRUE(expect_value<std::uint16_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::uint16_t>(0x1234)));
}

TEST(NodeValueTest, DecodeSmallint) {
    icd::Node node("smallint", "", 0, 0, 16, icd::ValueType::smallint, icd::Tag::none, {});
    const std::array<std::byte, 2> frame{std::byte{0xFE}, std::byte{0xFF}};
    EXPECT_TRUE(expect_value<std::int16_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::int16_t>(-2)));
}

TEST(NodeValueTest, DecodeLongword) {
    icd::Node node("longword", "", 0, 0, 32, icd::ValueType::longword, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0x12}, std::byte{0x34}, std::byte{0x56}, std::byte{0x78}};
    EXPECT_TRUE(expect_value<std::uint32_t>(node.decode(frame, icd::ByteOrder::big_endian), static_cast<std::uint32_t>(0x12345678)));
}

TEST(NodeValueTest, DecodeInteger) {
    icd::Node node("integer", "", 0, 0, 32, icd::ValueType::integer, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x80}};
    EXPECT_TRUE(expect_value<std::int32_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::int32_t>(-2130706433)));
}

TEST(NodeValueTest, DecodeByte) {
    icd::Node node("byte", "", 0, 0, 8, icd::ValueType::byte_, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0xAB}};
    EXPECT_TRUE(expect_value<std::uint64_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::uint64_t>(0xAB)));
}

TEST(NodeValueTest, DecodeShortint) {
    icd::Node node("shortint", "", 0, 0, 8, icd::ValueType::shortint, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0xFE}};
    EXPECT_TRUE(expect_value<std::int64_t>(node.decode(frame, icd::ByteOrder::little_endian), static_cast<std::int64_t>(-2)));
}

TEST(NodeValueTest, DecodeBoolean) {
    icd::Node node("flag", "", 0, 2, 1, icd::ValueType::boolean, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0b00000100}};
    EXPECT_TRUE(expect_value<bool>(node.decode(frame, icd::ByteOrder::little_endian), true));
}

TEST(NodeValueTest, DecodeSingle) {
    icd::Node node("single", "", 0, 0, 32, icd::ValueType::single, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0x00}, std::byte{0x00}, std::byte{0x80}, std::byte{0x3F}};
    EXPECT_TRUE(expect_value<double>(node.decode(frame, icd::ByteOrder::little_endian), 1.0));
}

TEST(NodeValueTest, DecodeDouble) {
    icd::Node node("double", "", 0, 0, 64, icd::ValueType::double_, icd::Tag::none, {});
    const std::array<std::byte, 8> frame{std::byte{0x3F}, std::byte{0xF0}, std::byte{0x00}, std::byte{0x00},
                                         std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
    EXPECT_TRUE(expect_value<double>(node.decode(frame, icd::ByteOrder::big_endian), 1.0));
}

TEST(NodeValueTest, DecodeBytes) {
    icd::Node node("bytes", "", 1, 0, 16, icd::ValueType::bytes, icd::Tag::none, {});
    const std::array<std::byte, 4> frame{std::byte{0x00}, std::byte{0xAA}, std::byte{0xBB}, std::byte{0x00}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_TRUE(value.has_value());
    ASSERT_TRUE(std::holds_alternative<std::vector<std::byte>>(*value));
    const auto& bytes = std::get<std::vector<std::byte>>(*value);
    ASSERT_EQ(bytes.size(), 2u);
    EXPECT_EQ(bytes[0], std::byte{0xAA});
    EXPECT_EQ(bytes[1], std::byte{0xBB});
}

TEST(NodeValueTest, DecodeString) {
    icd::Node node("string", "", 0, 0, 24, icd::ValueType::string_, icd::Tag::none, {});
    const std::array<std::byte, 3> frame{std::byte{'A'}, std::byte{'B'}, std::byte{'C'}};
    EXPECT_TRUE(expect_value<std::string>(node.decode(frame, icd::ByteOrder::little_endian), std::string{"ABC"}));
}

// ====== 错误场景测试 ======

TEST(NodeValueTest, DecodeOutOfRange) {
    icd::Node node("oor", "", 1, 0, 16, icd::ValueType::word, icd::Tag::none, {});
    const std::array<std::byte, 2> frame{std::byte{0x00}, std::byte{0x01}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, DecodeNonAlignedBytes) {
    icd::Node node("bad-bytes", "", 0, 1, 16, icd::ValueType::bytes, icd::Tag::none, {});
    const std::array<std::byte, 3> frame{std::byte{0x00}, std::byte{0x01}, std::byte{0x02}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, DecodeUnknownType) {
    icd::Node node("unknown", "", 0, 0, 8, icd::ValueType::unknown, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0x00}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

TEST(NodeValueTest, DecodeWordWrongWidth) {
    icd::Node node("bad-word", "", 0, 0, 8, icd::ValueType::word, icd::Tag::none, {});
    const std::array<std::byte, 1> frame{std::byte{0x01}};
    auto value = node.decode(frame, icd::ByteOrder::little_endian);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().code, icd::ErrorCode::invalid_argument);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

##### test_export_macros.cpp（编译期校验 → 1 个 TEST）

```cpp
#include <gtest/gtest.h>
#include <icd/export.hpp>
#include <icd/loader.hpp>
#include <type_traits>

TEST(ExportMacrosTest, CompileTimeChecks) {
    static_assert(!std::is_copy_constructible_v<icd::Node>);
    static_assert(!std::is_copy_assignable_v<icd::Node>);
    static_assert(!std::is_move_constructible_v<icd::Node>);
    static_assert(!std::is_move_assignable_v<icd::Node>);
    static_assert(!std::is_copy_constructible_v<icd::Frame>);
    static_assert(!std::is_copy_assignable_v<icd::Frame>);
    static_assert(std::is_move_constructible_v<icd::Frame>);
    static_assert(std::is_move_assignable_v<icd::Frame>);
    static_assert(!std::is_copy_constructible_v<icd::Repository>);
    static_assert(!std::is_copy_assignable_v<icd::Repository>);
    static_assert(std::is_move_constructible_v<icd::Repository>);
    static_assert(std::is_move_assignable_v<icd::Repository>);
    [[maybe_unused]] auto init_fn = &icd::Loader::init;
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

##### test_compat_snapshot.cpp（真实 ICD 样本 → 6 个 TEST）

```cpp
#include <gtest/gtest.h>
#include <icd/error.hpp>
#include <icd/loader.hpp>
#include "../src/format/xml_parser.hpp"
#include "../src/schema/builder.hpp"
#include <filesystem>

#ifndef ICD_TEST_XML_COMPAT_DIR
#error ICD_TEST_XML_COMPAT_DIR is not defined
#endif

namespace fs = std::filesystem;

struct CompatSnapshotTest : ::testing::Test {
    fs::path base = fs::path(ICD_TEST_XML_COMPAT_DIR);
};

TEST_F(CompatSnapshotTest, LoadFullConfig) {
    auto repo = icd::Loader::init(base / "ICDConfig.xml", icd::Format::xml);
    ASSERT_TRUE(repo.has_value()) << repo.error().message;
    EXPECT_EQ(repo->frames().size(), 7u);
    EXPECT_NE(repo->find("FDR0"), nullptr);
    EXPECT_NE(repo->find("DA0_1"), nullptr);
    EXPECT_NE(repo->find("AD0"), nullptr);
}

TEST_F(CompatSnapshotTest, Fdr0FrameStructure) {
    auto frame = icd::format::parse_xml_frame(base / "fdr0.xml");
    ASSERT_TRUE(frame.has_value()) << frame.error().message;
    EXPECT_EQ(frame->roots.size(), 3u);
    EXPECT_EQ(frame->roots[0].name, "\xE5\xB8\xA7\xE5\xA4\xB4");  // 帧头 (UTF-8)
    EXPECT_EQ(frame->roots[1].name, "\xE7\x87\x83\xE6\xB2\xB9\xE9\x98\x80\xE9\x97\xA8" "1");  // 燃油阀门1 (UTF-8)
    EXPECT_EQ(frame->roots[2].attrs.unit, "\xE5\xBA\xA6");  // 度 (UTF-8)
}

TEST_F(CompatSnapshotTest, IoFrameStructure) {
    auto frame = icd::format::parse_xml_frame(base / "io.xml");
    ASSERT_TRUE(frame.has_value()) << frame.error().message;
    EXPECT_EQ(frame->roots.size(), 7u);
    EXPECT_EQ(frame->roots[4].name, "100mv1");
    EXPECT_EQ(frame->roots[6].children.size(), 2u);
    EXPECT_EQ(frame->roots[6].children[0].name, "\xE7\xA8\x8B\xE6\x8E\xA7\xE7\x94\xB5\xE6\xBA\x90" "4");  // 程控电源4 (UTF-8)
}

TEST_F(CompatSnapshotTest, Io2FrameStructure) {
    auto frame = icd::format::parse_xml_frame(base / "io2.xml");
    ASSERT_TRUE(frame.has_value()) << frame.error().message;
    EXPECT_EQ(frame->roots.size(), 3u);
    EXPECT_EQ(frame->roots[0].name, "K1");
    EXPECT_EQ(frame->roots[1].attrs.system_name, "\xE9\xA3\x9E\xE5\x8F\x82");  // 飞参 (UTF-8)
    EXPECT_EQ(frame->roots[2].description, "\xE7\x94\xB5\xE5\x8E\x8B" "1");  // 电压1 (UTF-8)
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

---

## 执行顺序

1. 移动 pugixml、nlohmann、tl 到 `3rdparty/`
2. 新增 `3rdparty/pugixml-1.15/CMakeLists.txt`
3. 编辑顶层 `CMakeLists.txt` — 新增 3 个 3rdparty 子目录/目标
4. 重写 `src/icd_utility/CMakeLists.txt`
5. 编辑 `src/CMakeLists.txt` — 加 `add_subdirectory(icd_utility)`
6. 移动测试数据：`src/icd_utility/tests/data/` → `tests/icd_utility/data/`
7. 移动并重写 8 个测试源文件到 `tests/icd_utility/`
8. 新增 `tests/icd_utility/CMakeLists.txt`
9. 编辑 `tests/CMakeLists.txt` — 加 `add_subdirectory(icd_utility)`
10. 删除 `src/icd_utility/tests/` 空目录
11. 删除 `src/icd_utility/src/pugixml/`、`src/icd_utility/src/nlohmann/`、`src/icd_utility/src/tl/`
12. 构建 + 运行全部 icd_utility 测试验证

---

## 验证命令

```bash
# 在 VS2019 Developer Command Prompt 中：
cd D:\trae_workspace\etest-demo

# 构建全部 icd_utility 测试
ninja -C build/ninja-debug test_builder test_repository test_xml_parser test_json_parser test_loader test_node_value test_export_macros test_compat_snapshot

# 运行测试
ctest -R "test_builder|test_repository|test_xml_parser|test_json_parser|test_loader|test_node_value|test_export_macros|test_compat_snapshot" --output-on-failure
```

预期结果：全部 8 个测试通过。
