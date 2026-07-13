# Plugin 目录结构调整计划

> 目标：将 mock 设备插件从 `examples/plugins/` 迁移到 `src/plugins/mock/`，
> 使其成为与 `MockUUTBuilder` 同级的生产代码，而非"示例"。

---

## 现状

```
examples/plugins/
├── hello_plugin/               ← 纯示例，演示 Qt 插件机制
├── mock_serial_device/          ← 实现 ISerialDevicePlugin
├── mock_can_device/             ← 实现 ICANPlugin
├── mock_a429_device/            ← 实现 IArinc429Plugin
├── mock_ad_device/              ← 实现 IADevicePlugin
└── mock_da_device/              ← 实现 IDADevicePlugin

src/engine/
├── MockUUTBuilder.h/.cpp        ← Mock UUT 模拟逻辑（静态库）
├── HardwareManager.cpp          ← 在 mock 模式下加载上述插件
└── ...
```

### 问题

1. **归属错位** — mock 设备插件不是"示例"，`HardwareManager` 在 mock 模式下依赖它们完成 IO。`BUILD_ETEST_EXAMPLES=OFF` 会打断 mock 流程。
2. **命名空间不匹配** — 插件类在 `etest::examples` 下，但它们服务于 `etest::engine` 的 mock 能力。
3. **没有预留真实硬件插件的目录** — 将来加入 PXI/PCI 设备插件时没有明确的位置约定。

### 架构定位

当前 mock 是两层结构：

| 层 | 角色 | 位置 |
|------|------|------|
| **Device Plugin**（动态库） | 实现 ISerialDevicePlugin 等接口，模拟硬件 IO | 本计划迁移 |
| **UUT Simulator**（静态库） | MockUUTBuilder 读拓扑+ICD 生成模拟回复 | `src/engine/` 不动 |

两层解耦：MockUUTBuilder 通过 `HardwareManager` 和 `devicesByMockType()` 按 device_type 查找插件，
不依赖具体路径。插件搬家不影响 engine 层。

---

## 目标结构

```
src/plugins/
├── mock/
│   ├── serial/          ← 原 examples/plugins/mock_serial_device/
│   ├── can/             ← 原 examples/plugins/mock_can_device/
│   ├── a429/            ← 原 examples/plugins/mock_a429_device/
│   ├── ad/              ← 原 examples/plugins/mock_ad_device/
│   └── da/              ← 原 examples/plugins/mock_da_device/
└── CMakeLists.txt

examples/
├── plugins/
│   └── hello_plugin/    ← 保留，纯示例
├── eye-tracking-demo/
├── guidance-demo/
├── lua-debugger-demo/
└── wisdom_demo/
```

> `src/plugins/real/` 暂不创建，等有真实硬件插件时再说。

---

## 具体改动

### 1. 目录迁移（保留 git 历史）

使用 `git mv` 直接搬移，保留文件历史：

```bash
git mv examples/plugins/mock_serial_device src/plugins/mock/serial
git mv examples/plugins/mock_can_device   src/plugins/mock/can
git mv examples/plugins/mock_a429_device  src/plugins/mock/a429
git mv examples/plugins/mock_ad_device    src/plugins/mock/ad
git mv examples/plugins/mock_da_device    src/plugins/mock/da
```

迁移映射：

| 原路径 | 目标路径 |
|--------|---------|
| `examples/plugins/mock_serial_device/` | `src/plugins/mock/serial/` |
| `examples/plugins/mock_can_device/` | `src/plugins/mock/can/` |
| `examples/plugins/mock_a429_device/` | `src/plugins/mock/a429/` |
| `examples/plugins/mock_a429_device/a429_protocal.md` | `src/plugins/mock/a429/a429_protocal.md` |
| `examples/plugins/mock_ad_device/` | `src/plugins/mock/ad/` |
| `examples/plugins/mock_da_device/` | `src/plugins/mock/da/` |

### 2. 命名空间 + include guard

| 当前 | 改为 |
|------|------|
| `namespace etest::examples` | `namespace etest::plugins::mock` |

同时更新每个 `.h` 文件的 include guard：

| 当前 guard | 改为 |
|-----------|------|
| `ETEST_EXAMPLES_MOCK_SERIAL_PLUGIN_H_` | `ETEST_PLUGINS_MOCK_SERIAL_PLUGIN_H_` |
| `ETEST_EXAMPLES_MOCK_CAN_PLUGIN_H_` | `ETEST_PLUGINS_MOCK_CAN_PLUGIN_H_` |
| `ETEST_EXAMPLES_MOCK_A429_PLUGIN_H_` | `ETEST_PLUGINS_MOCK_A429_PLUGIN_H_` |
| `ETEST_EXAMPLES_MOCK_AD_PLUGIN_H_` | `ETEST_PLUGINS_MOCK_AD_PLUGIN_H_` |
| `ETEST_EXAMPLES_MOCK_DA_PLUGIN_H_` | `ETEST_PLUGINS_MOCK_DA_PLUGIN_H_` |

### 3. JSON 元数据补充 is_mock

每个 mock 插件的 `.json` 文件追加 `"is_mock": true` 字段。
当前仅在 C++ 构造器中设置 `meta_.is_mock = true`，JSON 中缺失会导致仅读 DLL
元数据（不加载插件）的工具误判插件类型。

```json
{
  "id": "etest.plugin.device.mock_serial",
  "is_mock": true,
  ...
}
```

涉及文件：
- `src/plugins/mock/serial/mock_serial_device.json`
- `src/plugins/mock/can/mock_can_device.json`
- `src/plugins/mock/a429/mock_a429_device.json`
- `src/plugins/mock/ad/mock_ad_device.json`
- `src/plugins/mock/da/mock_da_device.json`

### 4. CMake 调整

**新增 `src/plugins/CMakeLists.txt`：**
```cmake
option(BUILD_MOCK_PLUGINS "Build mock device plugins" ON)

if(BUILD_MOCK_PLUGINS)
    add_subdirectory(mock/serial)
    add_subdirectory(mock/can)
    add_subdirectory(mock/a429)
    add_subdirectory(mock/ad)
    add_subdirectory(mock/da)
endif()
```

**修改 `src/CMakeLists.txt`**（在现有的 `add_subdirectory` 块末尾追加）：
```cmake
add_subdirectory(plugins)
```

> 不直接从根 CMakeLists.txt 加，保持 `src/` 下模块统一由 `src/CMakeLists.txt` 管理。

**修改根 `CMakeLists.txt`：**
```cmake
# 从 BUILD_ETEST_EXAMPLES 条件块中删除 5 行：
#   add_subdirectory(examples/plugins/mock_ad_device)
#   add_subdirectory(examples/plugins/mock_da_device)
#   add_subdirectory(examples/plugins/mock_serial_device)
#   add_subdirectory(examples/plugins/mock_a429_device)
#   add_subdirectory(examples/plugins/mock_can_device)

# BUILD_ETEST_EXAMPLES 块中保留：
#   hello_plugin / eye-tracking-demo / guidance-demo / ...
```

**兼容性迁移处理：**
在根 CMakeLists.txt 中增加一段提示，当 `BUILD_ETEST_EXAMPLES` 被显式设置但
`BUILD_MOCK_PLUGINS` 未设置时，打印 deprecation 指引：

```cmake
if(DEFINED BUILD_ETEST_EXAMPLES AND NOT DEFINED BUILD_MOCK_PLUGINS)
    message(DEPRECATION
        "BUILD_ETEST_EXAMPLES no longer controls mock plugins. "
        "Use BUILD_MOCK_PLUGINS instead (default ON).")
endif()
```

### 5. 清理

`git mv` 后旧目录自动消失，无需额外删除。

---

## 影响范围

| 依赖方 | 影响 | 处理 |
|--------|------|------|
| `HardwareManager::instantiateDevice()` | 通过 `devicesByMockType()` 按 device_type 查找 | 无影响 |
| 拓扑文件（`.etopo`） | 引用 `pluginId` 字符串 | 无影响 |
| `PluginManager` 扫描路径 | 运行时扫描 `bin/plugins/`，DLL 输出路径不变 | 无影响 |
| 根 `CMakeLists.txt` | 移除 5 个 `add_subdirectory`，新增兼容性判断 | 需要改 |
| `src/CMakeLists.txt` | 新增 `add_subdirectory(plugins)` | 需要改 |
| `tests/core/ad_device_plugin_test.cpp` | 通过 pluginId 查找 mock_ad 插件 | 源码无影响，但需确保测试在 `BUILD_MOCK_PLUGINS=OFF` 时不编译 |
| `examples/` 结构 | 减少 5 个目录 | 无影响 |
| 现有 `#include` 引用 | 无外部文件 include mock 插件头文件 | 无影响 |

**无外部影响** — mock 插件是独立动态库，不暴露头文件给其他模块，纯运行时加载。

---

## 测试依赖

`tests/core/ad_device_plugin_test.cpp` 在运行时会加载 `mock_ad_device` 动态库。
修改 `tests/core/CMakeLists.txt` 将其置于条件块中：

```cmake
if(BUILD_MOCK_PLUGINS)
    add_etest(
        NAME test_core_ad_device_plugin
        SOURCES ad_device_plugin_test.cpp
        LIBS etest_core
        QT_MODULES Core Test
        LABELS CORE
    )
endif()
```

---

## 执行顺序

```
1. git mv 迁移 5 个 mock 插件目录
2. 更新命名空间 + include guard（每个 .h/.cpp）
3. 追加 JSON 元数据 is_mock: true（每个 .json）
4. 修改 src/CMakeLists.txt → add_subdirectory(plugins)
5. 新建 src/plugins/CMakeLists.txt
6. 修改根 CMakeLists.txt → 移除旧 add_subdirectory + 兼容性处理
7. 修改 tests/core/CMakeLists.txt → 条件编译 ad_device_plugin_test
8. 构建验证
   - cmake configure + ninja（全量 + 分别测试 BUILD_MOCK_PLUGINS=ON/OFF）
   - 确认 bin/plugins/ 下 mock plugin DLL 仍生成
   - 运行 CLI 测试验证 mock 流程正常
9. 提交
```

---

## 后续需更新的文档（不阻塞代码变更）

- `docs/plan/Qt剥离计划Phase1.md` — 提及 examples/plugins 路径
- `docs/plan/MockUUTSimulator设计.md` — 提及 mock 插件路径
- `docs/03-开发/1.3-通用插件框架开发内容.md` — 路径参考
- `docs/thinking/2026-07-08-硬件接入与测试执行引擎设计.md` — 架构图路径
