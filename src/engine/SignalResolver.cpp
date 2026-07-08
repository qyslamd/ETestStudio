#include "SignalResolver.h"

#include "SignalRegistry.h"

#include <icd/frame.hpp>
#include <icd/node.hpp>
#include <icd/repository.hpp>

#include <spdlog/spdlog.h>

namespace etest::engine {

SignalResolver::SignalResolver(etest::core::SignalRegistry* registry,
                                icd::Repository* icdRepo)
    : registry_(registry), icd_repo_(icdRepo) {}

ResolvedSignal SignalResolver::resolve(const QString& uuid) const {
    ResolvedSignal result;

    // 检查空指针
    if (!registry_ || !icd_repo_) {
        spdlog::warn("[SignalResolver] registry_ or icd_repo_ is null");
        result.valid = false;
        return result;
    }

    // ① 查 SignalRegistry 获取设备/端口/帧/节点路径
    auto resolved = registry_->resolve(uuid);
    if (!resolved.has_value()) {
        spdlog::warn("[SignalResolver] UUID {} not found in SignalRegistry",
                     uuid.toStdString());
        result.valid = false;
        return result;
    }

    result.deviceId = resolved->deviceId;
    result.portName = resolved->portName;

    // ② 查 ICD Repository 补充编码属性
    fillFromIcd(result, resolved->frameName, resolved->nodePath);

    result.valid = true;
    return result;
}

void SignalResolver::fillFromIcd(ResolvedSignal& signal,
                                  const QString& frameName,
                                  const QString& nodePath) const {
    // 查找 ICD 帧
    const auto* frame = icd_repo_->find(frameName.toStdString());
    if (!frame) {
        spdlog::warn("[SignalResolver] Frame '{}' not found in ICD Repository",
                     frameName.toStdString());
        return;
    }

    // 帧级属性
    signal.frameId = static_cast<uint32_t>(frame->id());
    signal.byteOrder = (frame->order() == icd::ByteOrder::little_endian)
                           ? ByteOrder::LittleEndian
                           : ByteOrder::BigEndian;

    // 根据帧类型推断信号类型
    switch (frame->type()) {
        case icd::FrameType::data:
        case icd::FrameType::data_cmd:
            signal.signalType = SignalType::CAN;
            break;
        case icd::FrameType::cmd:
            signal.signalType = SignalType::SERIAL;
            break;
    }

    // 在帧中按路径查找节点
    const auto* node = findNodeByPath(frame, nodePath);
    if (!node) {
        spdlog::warn("[SignalResolver] Node path '{}' not found in frame '{}'",
                     nodePath.toStdString(), frameName.toStdString());
        return;
    }

    // 节点级编码属性
    signal.byteOffset = node->offset();
    signal.bitOffset = node->bit_offset();
    signal.bitWidth = node->bit_width();

    const auto& attrs = node->attrs();
    signal.unit = QString::fromStdString(
        attrs.unit.empty() ? std::string() : attrs.unit);

    // scale_a = coeff (斜率), scale_b = offset (截距), 公式: eng = raw * a + b
    if (attrs.is_scaled) {
        if (attrs.scale_a.has_value()) {
            signal.coeff = static_cast<double>(attrs.scale_a.value());
        }
        if (attrs.scale_b.has_value()) {
            signal.offset = static_cast<double>(attrs.scale_b.value());
        }
    }

    // 工程值范围
    if (attrs.min.has_value()) {
        signal.engMin = static_cast<double>(attrs.min.value());
    }
    if (attrs.max.has_value()) {
        signal.engMax = static_cast<double>(attrs.max.value());
    }

    // rawMin/rawMax 在 ICD 数据中不直接存储，Phase 1 中保持默认值 0
}

// static
const icd::Node* SignalResolver::findNodeByPath(const icd::Frame* frame,
                                                  const QString& nodePath) {
    if (!frame || nodePath.isEmpty()) {
        return nullptr;
    }

    // 按 "/" 分割路径
    QStringList segments = nodePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return nullptr;
    }

    // 第一段匹配根节点
    const icd::Node* current = nullptr;
    for (const auto& root : frame->roots()) {
        if (root->name() == segments[0].toStdString()) {
            current = root.get();
            break;
        }
    }
    if (!current) {
        return nullptr;
    }

    // 后续段逐层向下查找
    for (int i = 1; i < segments.size(); ++i) {
        current = current->find(segments[i].toStdString());
        if (!current) {
            return nullptr;
        }
    }

    return current;
}

}  // namespace etest::engine
