# 屏保·哲思模块 Code Review 修复计划

> 评审对象：tuxsaver → app/widgets 模块重组 + WisdomWidget 诗词屏保 + WisdomDatabase
> 评审日期：2026-06-19
> 评分：7.5 / 10

---

## P1 — setStyleSheet 硬编码违反 CLAUDE.md 规则 #10

### 问题位置

| 文件 | 行号 | 内容 |
|------|------|------|
| `src/app/widgets/TuxSaverOverlay.cpp` | 21-33 | `close_btn_->setStyleSheet(...)` — 关闭按钮样式 |
| `src/app/widgets/TuxSaverOverlay.cpp` | 41-55 | `prev_btn_` / `next_btn_` 共用的 `kBtnStyle` 常量 |
| `src/app/widgets/WisdomWidget.cpp` | 247-249 | `divider_->setStyleSheet(...)` — 动态颜色（可豁免，但建议统一） |

### 修复方案

1. **TuxSaverOverlay 的三个按钮**：
   - 给 close_btn_ 设置 `setObjectName("saverCloseBtn")`
   - 给 prev_btn_ 设置 `setObjectName("saverPrevBtn")`
   - 给 next_btn_ 设置 `setObjectName("saverNextBtn")`
   - 删除 C++ 中所有 `setStyleSheet` 调用
   - 在 `src/app/resources/styles/default.qss` 末尾追加：

```css
/* ==================== TuxSaverOverlay ==================== */
QPushButton#saverCloseBtn {
    color: rgba(255,255,255,160);
    background: rgba(255,255,255,25);
    border: 1px solid rgba(255,255,255,40);
    font-size: 16px;
    border-radius: 16px;
}
QPushButton#saverCloseBtn:hover {
    background: rgba(255,80,80,130);
    color: white;
    border-color: rgba(255,80,80,200);
}
QPushButton#saverPrevBtn, QPushButton#saverNextBtn {
    color: rgba(255,255,255,140);
    background: rgba(255,255,255,20);
    border: 1px solid rgba(255,255,255,30);
    font-size: 14px;
    border-radius: 14px;
    min-width: 28px;
    min-height: 28px;
}
QPushButton#saverPrevBtn:hover, QPushButton#saverNextBtn:hover {
    background: rgba(255,255,255,50);
    color: white;
    border-color: rgba(255,255,255,80);
}
```

2. **WisdomWidget 的 divider 动态颜色**：
   - divider 颜色随主题切换（朱砂红），属于运行时动态值
   - **保留 setStyleSheet**，但添加注释说明豁免原因
   - 或者：改用 QPalette 设置 QFrame 背景，避免 setStyleSheet

### 验证

- QSS 已通过 `qApp->setStyleSheet()` 全局加载（确认加载机制）
- 按钮 objectName 唯一，不会误匹配其他控件

---

## P2 — initDatabase() 去掉 tableExists() 守卫导致潜在重复插入

### 问题位置

- `src/app/widgets/WisdomDatabase.cpp` — 未暂存版本（工作区当前版本）

### 当前（有 bug）的逻辑

```cpp
// 未暂存版本
QVector<PoemData> builtin = loadBuiltinFromJson();
impl_ = new Impl();
if (impl_->openDb(dbPath)) {
    impl_->exec("CREATE TABLE poems (...)");  // ← 无 IF NOT EXISTS，无 tableExists 检查
    impl_->insertBatch(builtin);               // ← 表已存在则插入重复数据
    return builtin;
}
```

### 问题场景

1. 首次运行 → 创建 DB → 建表 → 插入 400 条 → 正常
2. 第二次运行 → DB 文件存在 → `loadAll()` 成功返回 → 正常（不走下面的分支）
3. **DB 文件存在但表存在且 loadAll() 因 SQL 错误返回空** → 走到 `CREATE TABLE`（报错或忽略）→ `insertBatch` → **插入 400 条重复数据**

### 修复方案

恢复 `tableExists()` 守卫：

```cpp
QVector<PoemData> builtin = loadBuiltinFromJson();
impl_ = new Impl();
if (impl_->openDb(dbPath)) {
    if (!impl_->tableExists(QStringLiteral("poems"))) {
        impl_->exec(QStringLiteral(
            "CREATE TABLE poems ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "sentence TEXT NOT NULL,"
            "source TEXT NOT NULL,"
            "commentary TEXT,"
            "tag TEXT,"
            "dynasty TEXT"
            ")"));
        impl_->insertBatch(builtin);
    }
    QVector<PoemData> poems = impl_->loadAll();
    if (!poems.isEmpty()) {
        return poems;
    }
}
```

注意：同时恢复了 `loadAll()` 二次读取，确保从 DB 返回的数据优先。

---

## P3 — 死代码清理

### 问题位置

| 文件 | 行号 | 内容 |
|------|------|------|
| `src/app/widgets/WisdomDatabase.h` | 38 | `QVector<PoemData> loadFromSqlite(const QString& dbPath);` — 声明无实现 |
| `src/app/widgets/WisdomDatabase.h` | 39 | `void createDatabase(const QString& dbPath);` — 声明无实现 |
| `src/app/widgets/WisdomDatabase.cpp` | 41-47 | `Impl::tableExists()` — P2 修复后会被重新使用，**保留** |

### 修复方案

删除 `WisdomDatabase.h` 中第 38-39 行的两个私有方法声明。

---

## P4 — resource.qrc 末尾缺换行符

### 问题位置

- `src/app/resource.qrc` 最后一行 `</RCC>` 后无换行

### 修复方案

在 `</RCC>` 后添加一个换行符。

---

## P5 — 清理未跟踪的 Python 临时脚本

### 问题位置

```
docs/plan/check_dyn2.py
docs/plan/check_dynasty.py
docs/plan/check_mismatch.py
docs/plan/check_weijin.py
```

### 修复方案

直接删除这 4 个文件（它们是诗词数据验证的临时脚本，已完成使命）。

---

## 修复顺序

1. P1 — QSS 迁移（TuxSaverOverlay 三按钮）
2. P2 — 恢复 tableExists() 守卫
3. P3 — 删除死代码声明
4. P4 — resource.qrc 换行
5. P5 — 删除 Python 脚本
6. 编译验证：`scripts/build_ninja.bat debug`
