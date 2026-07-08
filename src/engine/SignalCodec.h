#ifndef ETEST_ENGINE_SIGNAL_CODEC_H_
#define ETEST_ENGINE_SIGNAL_CODEC_H_

#include <QByteArray>
#include <QVariant>

namespace etest::engine {

struct ResolvedSignal;

class SignalCodec {
public:
    SignalCodec() = default;
    ~SignalCodec() = default;

    // 写路径：工程值 → 原始值（AD/DA）或帧字节（CAN/Serial/A429）
    QVariant encode(double engValue, const ResolvedSignal& info);
    QByteArray encodeToFrame(double engValue, const ResolvedSignal& info);

    // 读路径：原始值（AD/DA）或帧字节（CAN/Serial/A429） → 工程值
    double decode(const QVariant& rawValue, const ResolvedSignal& info);
    double decodeFromFrame(const QByteArray& frameData,
                           const ResolvedSignal& info);

private:
    double linearMap(double raw, const ResolvedSignal& info) const;
    double inverseLinearMap(double eng, const ResolvedSignal& info) const;
    QByteArray packBits(double value, const ResolvedSignal& info) const;
    double unpackBits(const QByteArray& data, const ResolvedSignal& info) const;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_SIGNAL_CODEC_H_
