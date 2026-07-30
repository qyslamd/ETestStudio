#pragma once

#include <QVector>

#include <cstdint>

namespace etest::core {

// WaveformGenerator -- AD 通道值生成器，供 ADChannelSimulator 和
// MockADPlugin 共用，确保两层行为一致。
//
// 三种模式：
//   Fixed    -- 返回固定值
//   Waveform -- 正弦/方波/三角波，参数化（幅值/频率/偏置）
//   Series   -- 预设值序列循环回放
class WaveformGenerator {
 public:
  enum class Mode { Fixed, Waveform, Series };
  enum class WaveformType { Sine, Square, Triangle };

  WaveformGenerator() = default;
  ~WaveformGenerator() = default;

  // 按 sampleIndex 和 sampleRate 生成当前采样值
  double generate(qint64 sampleIndex, double sampleRate) const;

  void setFixed(double value);
  void setWaveform(WaveformType type, double amplitude,
                   double frequency, double offset);
  void setSeries(const QVector<double>& data);

  Mode mode() const { return mode_; }

 private:
  Mode mode_ = Mode::Fixed;
  double fixed_value_ = 0.0;
  WaveformType waveform_type_ = WaveformType::Sine;
  double amplitude_ = 1.0;
  double frequency_ = 1.0;
  double offset_ = 0.0;
  QVector<double> series_data_;
};

}  // namespace etest::core
