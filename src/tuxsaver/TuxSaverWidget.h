#pragma once

#include <QElapsedTimer>
#include <QFont>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QPainter;

/// MobaXterm Tux 风格的桌面宠物企鹅控件
///
/// 内置空闲检测和随机状态机（站、走、看、挠头、哈欠、睡、吓、逃）。
/// 子类可覆盖 drawBackground() 绘制自定义背景。
class TuxSaverWidget : public QWidget {
  Q_OBJECT
  struct PenguinData;  // forward declaration for protected method signatures
 public:
  explicit TuxSaverWidget(QWidget* parent = nullptr);
  ~TuxSaverWidget() override;

  /// 设置空闲多少秒后企鹅现身（默认 5）
  void setIdleThreshold(int sec);

  /// 设置企鹅数量（默认 3）
  void setPenguinCount(int n);

  /// 设置说的话列表（空列表 = 不说话）
  void setPhrases(const QStringList& phrases);

  /// 获取当前说的话列表
  QStringList phrases() const;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  bool eventFilter(QObject* obj, QEvent* event) override;

  /// 子类 hook：在企鹅绘制前绘制背景
  virtual void drawBackground(QPainter& p) const;

  /// 在 (px, py) 处绘制企鹅，mirror 为 true 时水平翻转
  void drawPenguin(QPainter& p, qreal px, qreal py, bool mirror,
                   const PenguinData& data) const;

  /// 第一只企鹅的状态中文名称
  QString stateName() const;

  // ── 位置（子类绘制背景时需要）──
  qreal floorY() const { return height() - 90; }
  qreal penguinCenterY() const { return floorY() - 30; }

 private:
  // ── 主循环 ──
  void tick();
  void startIdleDetect();

  // ── 状态机 ──
  enum class State {
    HIDDEN,     // 未出现
    IDLE,       // 站立，偶尔眨眼
    WALKING,    // 左右行走
    LOOKING,    // 左右张望
    SCRATCHING, // 挠头
    YAWNING,    // 打哈欠
    SLEEPING,   // 打盹
    SURPRISED,  // 被点击吓到
    FLEEING,    // 逃跑
  };

  struct PenguinData {
    State state = State::HIDDEN;
    qreal px = 200;              // 企鹅 x 位置（中心）
    qreal py = 200;              // 企鹅 y 位置（中心）
    bool facing_left_ = false;
    double anim_phase = 0;       // 行走/动画相位
    int state_elapsed = 0;       // 当前状态持续帧数
    double head_tilt = 0;        // 头部倾斜（张望）
    double beak_open = 0;        // 嘴巴张开（哈欠）
    double body_squash = 0;      // 身体挤压（惊喜）
    double blink = 0;            // 眨眼 0~1
    bool blink_closing = true;
    qreal flee_target_x = 0;     // 逃跑/行走目标 x
    QString speech_text;         // 当前说的话（空 = 不显示气泡）
    int speech_elapsed = 0;      // 气泡持续帧数
  };

  void setState(PenguinData& d, State s);
  void pickRandomState(PenguinData& d);
  void clampX(PenguinData& d);

  QVector<PenguinData> penguins_;

  // ── 定时器 ──
  QTimer* anim_timer_ = nullptr;  // ~33ms (30fps)
  QTimer* blink_sched_ = nullptr; // 随机眨眼触发（轮流给企鹅发眨眼）

  // ── 空闲检测 ──
  QElapsedTimer idle_timer_;
  int idle_threshold_ = 5;
  bool user_active_ = false;

  // ── 对话配置 ──
  QStringList phrases_;

  // ── 首次显示标记 ──
  bool positioned_ = false;
};
