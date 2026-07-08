#include "SignalCodec.h"
#include "SignalResolver.h"

#include <QDebug>

#include <cmath>

#include <spdlog/spdlog.h>

namespace etest::engine {

// ── 线性映射：raw → eng ──
double SignalCodec::linearMap(double raw, const ResolvedSignal& info) const {
    return raw * info.coeff + info.offset;
}

// ── 逆线性映射：eng → raw ──
double SignalCodec::inverseLinearMap(double eng,
                                     const ResolvedSignal& info) const {
    if (std::fabs(info.coeff) < 1e-15) {
        spdlog::warn("[SignalCodec] coeff is near-zero ({}), division by zero "
                     "guarded, returning 0.0",
                     info.coeff);
        return 0.0;
    }
    return (eng - info.offset) / info.coeff;
}

// ── 写路径：工程值 → 原始值（AD/DA 通道型） ──
QVariant SignalCodec::encode(double engValue, const ResolvedSignal& info) {
    if (info.signalType == SignalType::AD ||
        info.signalType == SignalType::DA) {
        double raw = inverseLinearMap(engValue, info);
        return QVariant(raw);
    }
    // 帧型信号应使用 encodeToFrame
    spdlog::warn(
        "[SignalCodec] encode() called for frame-type signal (type={}), "
        "use encodeToFrame() instead",
        static_cast<int>(info.signalType));
    return QVariant();
}

// ── 写路径：工程值 → 帧字节（CAN/Serial/A429） ──
QByteArray SignalCodec::encodeToFrame(double engValue,
                                      const ResolvedSignal& info) {
    double raw = inverseLinearMap(engValue, info);
    return packBits(raw, info);
}

// ── 读路径：原始值（AD/DA） → 工程值 ──
double SignalCodec::decode(const QVariant& rawValue,
                           const ResolvedSignal& info) {
    bool ok = false;
    double raw = rawValue.toDouble(&ok);
    if (!ok) {
        spdlog::warn("[SignalCodec] decode() received invalid QVariant, "
                     "returning 0.0");
        return 0.0;
    }
    return linearMap(raw, info);
}

// ── 读路径：帧字节（CAN/Serial/A429） → 工程值 ──
double SignalCodec::decodeFromFrame(const QByteArray& frameData,
                                    const ResolvedSignal& info) {
    double raw = unpackBits(frameData, info);
    return linearMap(raw, info);
}

// ── 打包：将 raw value 的 bitWidth 个 bit 写入 byte array ──
//   bit 寻址采用 LSB0 约定（bitOffset=0 为 byte0 的 bit0）
//   BigEndian 时反转信号所在字节区域
QByteArray SignalCodec::packBits(double value,
                                 const ResolvedSignal& info) const {
    int totalBits = info.bitOffset + info.bitWidth;
    int totalBytes = std::max(1, (totalBits + 7) / 8);
    QByteArray data(totalBytes, '\0');

    uint64_t raw = static_cast<uint64_t>(static_cast<int64_t>(std::llround(value)));

    // 以 LSB0 方式逐 bit 填入
    for (int bit = 0; bit < info.bitWidth; ++bit) {
        int dstPos = info.bitOffset + bit;
        int byteIdx = dstPos / 8;
        int bitIdx = dstPos % 8;  // 0=LSB, 7=MSB
        if (raw & (1ULL << bit)) {
            data[byteIdx] = static_cast<char>(
                static_cast<unsigned char>(data[byteIdx]) |
                (1 << bitIdx));
        }
    }

    // BigEndian：反转信号所在字节区域
    if (info.byteOrder == ByteOrder::BigEndian) {
        int startByte = info.bitOffset / 8;
        int endByte = (info.bitOffset + info.bitWidth - 1) / 8;
        int len = endByte - startByte + 1;
        for (int i = 0; i < len / 2; ++i) {
            char tmp = data[startByte + i];
            data[startByte + i] = data[endByte - i];
            data[endByte - i] = tmp;
        }
    }

    return data;
}

// ── 解包：从 byte array 的 bitOffset 处读取 bitWidth 个 bit ──
//   bit 寻址采用 LSB0 约定
//   BigEndian 时先反转字节再提取
double SignalCodec::unpackBits(const QByteArray& data,
                               const ResolvedSignal& info) const {
    int requiredBits = info.bitOffset + info.bitWidth;
    int requiredBytes = (requiredBits + 7) / 8;

    if (data.size() < requiredBytes) {
        spdlog::warn("[SignalCodec] Frame too short: need {} bytes but got {}",
                     requiredBytes, data.size());
        return 0.0;
    }

    // 若 BigEndian，在提取前先复制并反转信号所在字节
    QByteArray workingData = data;
    if (info.byteOrder == ByteOrder::BigEndian) {
        int startByte = info.bitOffset / 8;
        int endByte = (info.bitOffset + info.bitWidth - 1) / 8;
        int len = endByte - startByte + 1;
        for (int i = 0; i < len / 2; ++i) {
            char tmp = workingData[startByte + i];
            workingData[startByte + i] = workingData[endByte - i];
            workingData[endByte - i] = tmp;
        }
    }

    // 以 LSB0 方式逐 bit 读取
    uint64_t result = 0;
    for (int bit = 0; bit < info.bitWidth; ++bit) {
        int srcPos = info.bitOffset + bit;
        int byteIdx = srcPos / 8;
        int bitIdx = srcPos % 8;
        if (workingData[byteIdx] & (1 << bitIdx)) {
            result |= (1ULL << bit);
        }
    }

    return static_cast<double>(result);
}

}  // namespace etest::engine
