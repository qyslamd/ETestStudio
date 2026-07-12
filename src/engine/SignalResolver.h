#ifndef ETEST_ENGINE_SIGNAL_RESOLVER_H_
#define ETEST_ENGINE_SIGNAL_RESOLVER_H_

#include <QString>
#include <cstdint>

namespace etest::core {
class SignalRegistry;
}  // namespace etest::core

namespace icd {
class Repository;
class Frame;
class Node;
}  // namespace icd

namespace etest::engine {

// ── 信号类型（硬件接口类型） ──
enum class SignalType {
    AD,
    DA,
    CAN,
    SERIAL,
    A429
};

// ── 字节序 ──
enum class ByteOrder {
    BigEndian,
    LittleEndian
};

// ── 解析结果：含设备身份 + ICD 编码属性 ──
struct ResolvedSignal {
    // ── 设备身份 ──
    QString deviceId;
    QString deviceType;
    QString portName;

    // ── 信号类型 ──
    SignalType signalType = SignalType::AD;

    // ── 工程值映射（线性：eng = raw * coeff + offset） ──
    double coeff = 1.0;
    double offset = 0.0;
    QString unit;
    double engMin = 0.0;
    double engMax = 0.0;
    double rawMin = 0.0;
    double rawMax = 0.0;

    // ── 通道型（AD/DA） ──
    int channel = 0;

    // ── 帧型（CAN/Serial/A429） ──
    uint32_t frameId = 0;
    int byteOffset = 0;
    int bitOffset = 0;
    int bitWidth = 0;
    ByteOrder byteOrder = ByteOrder::BigEndian;

    // ── resolve 结果标志 ──
    bool valid = false;
};

class SignalResolver {
 public:
  explicit SignalResolver(etest::core::SignalRegistry* registry,
                           icd::Repository* icdRepo);

  ResolvedSignal resolve(const QString& uuid) const;

 private:
  // 从 ICD Repository 中查找节点并填充编码属性
  void fillFromIcd(ResolvedSignal& signal, const QString& frameName,
                   const QString& nodePath) const;

  // 在帧的节点树中按 "/" 分隔路径查找节点
  static const icd::Node* findNodeByPath(const icd::Frame* frame,
                                          const QString& nodePath);

  etest::core::SignalRegistry* registry_;
  icd::Repository* icd_repo_;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_SIGNAL_RESOLVER_H_
