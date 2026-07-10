#ifndef ETEST_ENGINE_MOCK_TYPES_H_
#define ETEST_ENGINE_MOCK_TYPES_H_

#include <QByteArray>
#include <optional>

namespace etest::engine {

// ── Mock 端口模拟器的回复描述（onFrameReceived 的返回值） ──
struct MockResponse {
    int targetFrameId = 0;   // CAN ID / A429 Label / Serial 忽略
    QByteArray data;         // 回复的原始字节
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_MOCK_TYPES_H_
