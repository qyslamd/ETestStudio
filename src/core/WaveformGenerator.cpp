#include "WaveformGenerator.h"

#include <cmath>

#include "logger/Logger.h"

namespace etest::core {

namespace {
constexpr double kPi = 3.14159265358979323846;
}  // namespace

double WaveformGenerator::generate(qint64 sampleIndex,
                                    double sampleRate) const {
  switch (mode_) {
    case Mode::Fixed: {
      return fixed_value_;
    }
    case Mode::Waveform: {
      if (sampleRate <= 0.0) {
        LOG_WARN("WAVEFORM", "WaveformGenerator sampleRate={} <= 0, returning offset",
                 sampleRate);
        return offset_;
      }
      double t = static_cast<double>(sampleIndex) / sampleRate;
      double phase = 2.0 * kPi * frequency_ * t;
      double normalized = 0.0;  // 归一化波形值 [-1, 1]
      switch (waveform_type_) {
        case WaveformType::Sine: {
          normalized = std::sin(phase);
          break;
        }
        case WaveformType::Square: {
          normalized = (std::sin(phase) >= 0.0) ? 1.0 : -1.0;
          break;
        }
        case WaveformType::Triangle: {
          normalized = (2.0 / kPi) * std::asin(std::sin(phase));
          break;
        }
      }
      return amplitude_ * normalized + offset_;
    }
    case Mode::Series: {
      if (series_data_.isEmpty()) {
        return 0.0;
      }
      qint64 size = static_cast<qint64>(series_data_.size());
      qint64 idx = ((sampleIndex % size) + size) % size;
      return series_data_[static_cast<int>(idx)];
    }
  }
  return 0.0;
}

void WaveformGenerator::setFixed(double value) {
  mode_ = Mode::Fixed;
  fixed_value_ = value;
}

void WaveformGenerator::setWaveform(WaveformType type, double amplitude,
                                     double frequency, double offset) {
  mode_ = Mode::Waveform;
  waveform_type_ = type;
  amplitude_ = amplitude;
  frequency_ = frequency;
  offset_ = offset;
}

void WaveformGenerator::setSeries(const QVector<double>& data) {
  mode_ = Mode::Series;
  series_data_ = data;
}

}  // namespace etest::core
