#ifndef ETEST_ENGINE_MOCK_TYPES_H_
#define ETEST_ENGINE_MOCK_TYPES_H_

#include <QByteArray>
#include <QString>
#include <QVector>
#include <optional>

namespace etest::engine {

// ── Mock 端口模拟器的回复描述（onFrameReceived 的返回值） ──
struct MockResponse {
    int targetFrameId = 0;   // CAN ID / A429 Label / Serial 忽略
    QByteArray data;         // 回复的原始字节
};

// ── Mock 响应配置（从 MockResponses.emock 读取） ──
struct MockResponseConfig {
    QString frameName;       // 收到帧名（触发帧）
    QString replyFrameName;  // 回复帧名
    struct FieldValue {
        QString nodePath;
        double engValue = 0.0;
    };
    QVector<FieldValue> fieldValues;
};

}  // namespace etest::engine

#endif  // ETEST_ENGINE_MOCK_TYPES_H_
