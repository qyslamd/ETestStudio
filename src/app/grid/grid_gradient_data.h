#ifndef ETEST_APP_GRID_GRADIENT_DATA_H_
#define ETEST_APP_GRID_GRADIENT_DATA_H_

#include <QString>
#include <QVector>

namespace etest::app::grid {

struct GradientEntry {
  QString css_code;  // CSS linear-gradient string
  QString name;      // Chinese name for tooltip/debug
};

// 32 curated designer gradients from pxlab.cn
// Covers warm, cool, nature, dark, vibrant, and pastel series.
inline const QVector<GradientEntry> kCuratedGradients = {
    // === Warm series (8) ===
    {"background-image: linear-gradient(45deg, #ff9a9e 0%, #fad0c4 99%, #fad0c4 100%)",
     "温暖的火焰"},
    {"background-image: linear-gradient(to right, #ffecd2 0%, #fcb69f 100%)",
     "美味的水蜜桃"},
    {"background-image: linear-gradient(120deg, #f6d365 0%, #fda085 100%)",
     "阳光明媚的早晨"},
    {"background-image: linear-gradient(to right, #fa709a 0%, #fee140 100%)",
     "已是近黄昏"},
    {"background-image: linear-gradient(to right, #f83600 0%, #f9d423 100%)",
     "浴火凤凰"},
    {"background-image: linear-gradient(-60deg, #ff5858 0%, #f09819 100%)",
     "快乐的记忆"},
    {"background-image: linear-gradient(-20deg, #fc6076 0%, #ff9a44 100%)",
     "橙汁么么哒"},
    {"background-image: linear-gradient(to top, #e14fad 0%, #f9d423 100%)",
     "美味的蛋糕"},

    // === Cool / Blue series (7) ===
    {"background-image: linear-gradient(120deg, #a1c4fd 0%, #c2e9fb 100%)",
     "冬季的哈尔滨湖"},
    {"background-image: linear-gradient(to right, #4facfe 0%, #00f2fe 100%)",
     "三亚的海滩"},
    {"background-image: linear-gradient(135deg, #667eea 0%, #764ba2 100%)",
     "紫色的盘子"},
    {"background-image: linear-gradient(120deg, #89f7fe 0%, #66a6ff 100%)",
     "快乐的渔夫"},
    {"background-image: linear-gradient(15deg, #13547a 0%, #80d0c7 100%)",
     "水珠飞溅"},
    {"background-image: linear-gradient(to top, #4481eb 0%, #04befe 100%)",
     "幸福的晚会"},
    {"background-image: linear-gradient(-225deg, #5D9FFF 0%, #B8DCFF 48%, #6BBBFF 100%)",
     "飞机下降"},

    // === Nature / Green series (6) ===
    {"background-image: linear-gradient(120deg, #d4fc79 0%, #96e6a1 100%)",
     "尘埃的草"},
    {"background-image: linear-gradient(to right, #43e97b 0%, #38f9d7 100%)",
     "新的生活"},
    {"background-image: linear-gradient(to top, #0ba360 0%, #3cba92 100%)",
     "浓郁的树叶"},
    {"background-image: linear-gradient(60deg, #abecd6 0%, #fbed96 100%)",
     "丛林刚升起的太阳"},
    {"background-image: linear-gradient(60deg, #64b3f4 0%, #c2e59c 100%)",
     "天与花的相接"},
    {"background-image: linear-gradient(to top, #9be15d 0%, #00e3ae 100%)",
     "老牛吃嫩草"},

    // === Dark / Deep series (4) ===
    {"background-image: linear-gradient(to top, #09203f 0%, #537895 100%)",
     "永恒的宇宙"},
    {"background-image: linear-gradient(60deg, #29323c 0%, #485563 100%)",
     "邪恶的立场"},
    {"background-image: linear-gradient(-20deg, #2b5876 0%, #4e4376 100%)",
     "黑夜的深圳"},
    {"background-image: linear-gradient(to right, #243949 0%, #517fa4 100%)",
     "海底的深洞"},

    // === Vibrant / Multi-color series (4) ===
    {"background-image: linear-gradient(to right, #92fe9d 0%, #00c9ff 100%)",
     "活力的夏季运动"},
    {"background-image: linear-gradient(to right, #00dbde 0%, #fc00ff 100%)",
     "科技精彩生活"},
    {"background-image: linear-gradient(to right, #0acffe 0%, #495aff 100%)",
     "活力的好心情"},
    {"background-image: linear-gradient(60deg, #3d3393 0%, #2b76b9 37%, #2cacd1 65%, #35eb93 100%)",
     "空间移位"},

    // === Pastel / Soft series (3) ===
    {"background-image: linear-gradient(to top, #fbc2eb 0%, #a6c1ee 100%)",
     "夕阳下的雨"},
    {"background-image: linear-gradient(to top, #d299c2 0%, #fef9d7 100%)",
     "野外的苹果"},
    {"background-image: linear-gradient(-20deg, #ddd6f3 0%, #faaca8 100%, #faaca8 100%)",
     "姑娘腮红"},
};

inline constexpr int kGradientCount = 32;

}  // namespace etest::app::grid

#endif  // ETEST_APP_GRID_GRADIENT_DATA_H_
