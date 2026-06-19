# 哲思屏保 Demo — 设计文档

## 概述

一个独立可运行的诗词哲思桌面应用，以"电子宣纸"形态漂浮于桌面。点击或按空格刷新古诗词金句，配合淡入淡出动画与意境配色，营造沉浸式阅读体验。

## 目录结构

放置在 `examples/wisdom/` 下：

```
wisdom/
├── CMakeLists.txt           # 构建脚本
├── data/
│   ├── poems.qrc            # Qt 资源配置
│   └── poems.db             # 预置 SQLite 数据库（精选哲理金句）
├── include/
│   └── WisdomWidget.h       # 对外仅暴露一个窗口类
└── src/
    ├── WisdomWidget.cpp     # UI + 动画 + 逻辑全封装
    ├── WisdomDatabase.h     # SQLite 操作封装
    ├── WisdomDatabase.cpp
    └── main.cpp             # 独立 App 入口（含 QApplication）
```

集成到主项目时只需一行：

```cmake
add_subdirectory(examples/wisdom)
```

## CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.16)
project(WisdomViewer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql)

# 将 SQLite 数据库打包进资源
qt_add_resources(WISDOM_RESOURCES data/poems.qrc)

# 生成静态库（方便其他人集成）
add_library(wisdom_lib STATIC
    src/WisdomWidget.cpp
    src/WisdomDatabase.cpp
    ${WISDOM_RESOURCES}
)
target_include_directories(wisdom_lib PUBLIC include)
target_link_libraries(wisdom_lib PUBLIC Qt5::Widgets Qt5::Sql)

# 生成独立可执行文件（双击即运行）
add_executable(wisdom_viewer src/main.cpp)
target_link_libraries(wisdom_viewer PRIVATE wisdom_lib)
```

## WisdomWidget 类设计

头文件暴露两个公共方法：

```cpp
class WisdomWidget : public QWidget {
    Q_OBJECT
public:
    explicit WisdomWidget(QWidget *parent = nullptr);

    void refresh();                        // 随机切换下一句（带淡入淡出）
    void setDatabasePath(const QString &path); // 支持外部指定数据库

protected:
    void mousePressEvent(QMouseEvent *event) override; // 点击刷新
    void keyPressEvent(QKeyEvent *event) override;     // 空格刷新

private slots:
    void onFadeOutFinished(); // 动画链：淡出 -> 换文本 -> 淡入

private:
    void initUI();
    void loadData();          // 启动时复制只读数据库到可写路径

    QLabel *sentenceLabel;
    QLabel *sourceLabel;
    QLabel *commentLabel;
    QGraphicsOpacityEffect *effect;
    QPropertyAnimation *anim;

    QSqlDatabase db;
    QVector<int> ids;         // 加载所有 id，随机打乱，轮播取数
    int currentIndex;
};
```

## 数据库初始化策略

将资源中的只读数据库复制到 AppData 目录，确保零外部文件依赖：

```cpp
void WisdomWidget::loadData() {
    QString writablePath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(writablePath);
    QString dbPath = writablePath + "/poems.db";

    if (!QFile::exists(dbPath)) {
        QFile::copy(":/data/poems.db", dbPath);
        QFile::setPermissions(dbPath, QFile::ReadOwner | QFile::WriteOwner);
    }

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    db.open();

    // 加载所有 ID 并打乱（轮播不重复）
    QSqlQuery query("SELECT id FROM poems");
    while(query.next()) ids << query.value(0).toInt();
    std::shuffle(ids.begin(), ids.end(), std::random_device{}());
    currentIndex = 0;
    refresh();
}
```

## 切换动画

### 基础淡入淡出

```cpp
void WisdomWidget::refresh() {
    if (anim->state() == QAbstractAnimation::Running) return;
    anim->setTargetObject(effect);
    anim->setPropertyName("opacity");
    anim->setDuration(300);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, &WisdomWidget::onFadeOutFinished);
    anim->start();
}

void WisdomWidget::onFadeOutFinished() {
    int id = ids[currentIndex % ids.size()];
    currentIndex++;

    QSqlQuery query;
    query.prepare("SELECT sentence, source, commentary FROM poems WHERE id=?");
    query.addBindValue(id);
    query.exec();
    if(query.next()) {
        sentenceLabel->setText(query.value(0).toString());
        sourceLabel->setText(query.value(1).toString());
        commentLabel->setText(query.value(2).toString());
    }

    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setDuration(600);
    anim->start();
}
```

### 风吹散效果（进阶）

针对冷色调诗词（如"托遗响于悲风"），切换动画可升级为"风吹散"质感：

- **旧句消失**：向右上方平移（`QPropertyAnimation` 移动 `pos`）同时淡出，模拟被风吹走
- **新句出现**：从下方缓慢上浮（`pos` 从 `y+20` 到 `y=0`），透明度从 0 到 1，耗时 600ms

### 赏析延迟上浮

金句淡入完成后延迟 1.5s，赏析文字从底部"上浮"出现（800ms）。这种"延迟反转"能形成强烈的认知冲击。

## UI 设计

### 布局（留白是最高级的审美）

```
┌──────────────────────────────────┐
│                                  │  ← 顶部留白 20%
│                                  │
│      知不可乎骤得，托遗响于悲风。      │  ← 金句（垂直居中，44pt 楷体）
│         ─── ── ───               │  ← 极细水平线（宽 40px，#B0B8C0）
│         元丰五年 秋 · 苏轼·《前赤壁赋》  │  ← 出处（右对齐，小字）
│                                  │
│   直指人生"求不得"之常态。明知万事     │  ← 赏析（右对齐，延迟上浮）
│   不可急躁强求，却仍要将悲慨寄予秋风。  │
│                                  │  ← 底部留白 20%
│                              [悟] │  ← 朱砂红印章（30x30，右下角）
│                              舟   │  ← 极淡"舟"字彩蛋（opacity 0.15）
└──────────────────────────────────┘
```

- 使用 `QVBoxLayout`，上下留白占窗口高度的 40%
- 金句 `setWordWrap(true)` + 居中对齐，设置 `setMinimumHeight` 和 `setMaximumWidth` 营造悬浮感
- 底部右对齐：出处 + 赏析
- 底部中间：极细灰线分隔

### 字体与排版

| 元素 | 字体 | 字号 | 字重 | 说明 |
|------|------|------|------|------|
| 金句 | KaiTi / STKaiti | 36-48pt | Light / ExtraLight | 轻盈雅致，可加 italic |
| 出处 | SimSun / 宋体 | 10-12pt | Normal | 缩小，制造层次 |
| 赏析 | Microsoft YaHei | 10-12pt | Normal | 中灰色，字间距拉大 |
| 时间标注 | SimSun | 8pt | Normal | 如"元丰五年 秋"，叙事感 |

### 配色系统

不用纯黑纯白，追求"纸墨感"：

| 主题 | 背景色 | 文字色 | 赏析色 | 点缀色 |
|------|--------|--------|--------|--------|
| 宣纸（暖） | `#F7F4EB` | `#2C2C2C` | `#8A8A8A` | `#C43D3D` |
| 月白（冷） | `#E2E6EB` | `#2C3542` | `#6A7380` | `#C43D3D` |
| 深夜 | `#1A1A1A` | `#D4C5A0` | `#999999` | `#C43D3D` |

朱砂红 `#C43D3D` 仅用于分隔线或极小印章，切忌大面积使用。

### 点睛装饰

1. **朱砂印章**：右下角 30x30 红框 + 白底 + "悟"或"禅"字
2. **扁舟彩蛋**：右下角极淡（opacity 0.15）的"舟"字或弧形，用户不仔细看不会发现

### QSS 示例（月白冷色调）

```css
QWidget#centralWidget {
    background-color: #E2E6EB;  /* 月白风清 */
}
QLabel#sentenceLabel {
    color: #2C3542;
    font-family: "STKaiti", "KaiTi", serif;
    font-size: 44px;
    font-weight: 100;
    font-style: italic;
    background: transparent;
}
QLabel#commentLabel {
    color: #6A7380;
    font-family: "Microsoft YaHei", sans-serif;
    font-size: 12px;
    letter-spacing: 2px;  /* 字间距拉大，显得更清冷 */
}
```

### 动态主题切换

根据诗词 `tag` 字段（哲理/人生/自然/情感/励志）调用 `setThemeByTag()` 动态切换冷暖主题：

- **冷色调**（自然/哲理）：月白 `#E2E6EB`，鸦青文字
- **暖色调**（人生/励志）：宣纸 `#F7F4EB`，墨色文字
- **深色调**（情感）：深夜 `#1A1A1A`，淡金文字

## main.cpp

无边框 + 置顶 + 透明背景，让应用像一张漂浮的"电子宣纸"：

```cpp
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    WisdomWidget w;
    w.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    w.setAttribute(Qt::WA_TranslucentBackground);
    w.setGeometry(100, 100, 600, 400);
    w.show();
    return app.exec();
}
```

---

## 实现计划

> 以下为基于上述设计文档的工程实现方案。

### 技术决策

| 项目 | 决策 | 原因 |
|------|------|------|
| 代码复用 | 独立重写 | 符合"一行集成"理念，不依赖 src/app |
| 数据源 | 生成 poems.db | 按设计文档用 SQLite 嵌入资源 |
| 美学细节 | 完整版 | 冷色主题 + 风吹散动效 + 印章 + 扁舟彩蛋 |
| Qt 版本 | Qt5.15.2 | 与项目一致 |
| 动画方案 | QGraphicsOpacityEffect | 独立 top-level 窗口无 painter 冲突 |

### 数据准备

1. **`gen_db.py`** — 读取 `src/app/resources/data/poems.json`（400首），生成 `poems.db`：
   - 表结构：`id INTEGER PRIMARY KEY, sentence TEXT, source TEXT, commentary TEXT, tag TEXT, dynasty TEXT`
   - 输出到 `examples/wisdom/data/poems.db`
2. **`poems.qrc`** — 将 `poems.db` 嵌入资源（`:data/poems.db`）

### Widget 层级

```
WisdomWidget (top-level, frameless, WA_TranslucentBackground)
├── containerWidget          // 动画目标 — 风吹散效果作用于此
│   ├── sentenceLabel        // 金句，44px KaiTi italic
│   ├── divider              // 极细水平线（#B0B8C0，宽 40px）
│   ├── sourceLabel          // 出处 + 时间标注（如"元丰五年 秋"）
│   └── commentaryLabel      // 赏析，延迟上浮（1.5s 后，800ms）
├── sealLabel                // 右下角朱砂红印章 "悟"（30x30）
└── boatLabel                // 右下角极淡 "舟" 彩蛋（opacity 0.15）
```

### 动画时间线

```
点击/空格
  │
  ├─ [0ms]     旧句：向右上方平移 + 淡出（300ms）
  │
  ├─ [300ms]   换文本（sentenceLabel/sourceLabel/commentaryLabel）
  │
  ├─ [300ms]   新句：从下方上浮（y+20→y+0）+ 淡入（600ms）
  │            （commentaryLabel 保持 opacity=0）
  │
  ├─ [900ms]   金句淡入完成
  │
  ├─ [2400ms]  赏析从底部上浮 + 淡入（800ms）
  │
  └─ [3200ms]  完成，可再次刷新
```

### 主题映射

```cpp
enum class ThemeMood { Warm, Cool, Dark };

ThemeMood moodByTag(const QString& tag) {
    if (tag == "自然" || tag == "哲理") return ThemeMood::Cool;
    if (tag == "情感")                  return ThemeMood::Dark;
    return ThemeMood::Warm;  // 人生、励志
}
```

| 主题 | 背景 | 金句文字 | 赏析文字 | 分隔线 | 印章 |
|------|------|---------|---------|--------|------|
| Warm | `#F7F4EB` | `#2C2C2C` | `#8A8A8A` | `#B0B8C0` | `#C43D3D` |
| Cool | `#E2E6EB` | `#2C3542` | `#6A7380` | `#9AA4AE` | `#C43D3D` |
| Dark | `#1A1A1A` | `#D4C5A0` | `#999999` | `#444444` | `#C43D3D` |

### 文件清单

| 文件 | 职责 |
|------|------|
| `examples/wisdom/CMakeLists.txt` | 构建脚本，输出 wisdom_viewer.exe |
| `examples/wisdom/data/poems.qrc` | Qt 资源配置 |
| `examples/wisdom/data/poems.db` | SQLite 数据库（gen_db.py 生成） |
| `examples/wisdom/data/gen_db.py` | 从 poems.json 生成 poems.db |
| `examples/wisdom/include/WisdomWidget.h` | 对外头文件 |
| `examples/wisdom/src/WisdomWidget.cpp` | UI + 动画 + 逻辑 |
| `examples/wisdom/src/WisdomDatabase.h` | SQLite 操作封装 |
| `examples/wisdom/src/WisdomDatabase.cpp` | 数据库实现 |
| `examples/wisdom/src/main.cpp` | 独立 App 入口 |

### 执行顺序

1. 生成 `poems.db`（Python 脚本）
2. 创建所有源文件（WisdomWidget / WisdomDatabase / main.cpp）
3. 创建 `CMakeLists.txt` 和 `poems.qrc`
4. 在顶层 `CMakeLists.txt` 添加 `add_subdirectory(examples/wisdom)`
5. 编译验证
6. 提交
