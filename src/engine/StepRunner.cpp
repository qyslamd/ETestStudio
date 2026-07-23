#include "StepRunner.h"

#include <QElapsedTimer>
#include <QThread>
#include <QtGlobal>

#include <spdlog/spdlog.h>
#include "logger/Logger.h"

#include "HardwareManager.h"
#include "SignalCodec.h"
#include "SignalResolver.h"

#include <cmath>

namespace etest::engine {

// ==========================================================================
// Construction
// ==========================================================================

StepRunner::StepRunner(HardwareManager* hw, SignalCodec* codec,
                       SignalResolver* resolver, QObject* parent)
    : QObject(parent), hw_(hw), codec_(codec), resolver_(resolver) {}

// ==========================================================================
// executeProgram — iterate cases / steps, collect pass/fail counts
// ==========================================================================

void StepRunner::executeProgram(const ProgramData& program) {
    LOG_INFO("ENGINE", "开始执行测试套件 [name={}]", program.suiteName.toStdString());
    cancel_flag_.store(0, std::memory_order_release);

    emit suiteStarted(program.suiteName);

    int totalSteps = 0;
    for (const auto& tc : program.cases) {
        totalSteps += tc.steps.size();
    }
    int completedSteps = 0;
    int passCount = 0;
    int failCount = 0;

    for (int i = 0; i < program.cases.size(); ++i) {
        if (cancel_flag_.load(std::memory_order_acquire) == 2) {
            break;
        }

        const auto& tc = program.cases[i];
        emit caseStarted(i, tc.caseName);

        for (int j = 0; j < tc.steps.size(); ++j) {
            if (cancel_flag_.load(std::memory_order_acquire) == 2) {
                break;
            }

            QString stepPath = QString::number(j + 1);
            StepResult result =
                executeSingleStep(tc.steps[j], i, stepPath);

            if (result.status == PASS) {
                ++passCount;
            } else if (result.status == FAIL || result.status == TIMEOUT ||
                       result.status == ERROR) {
                ++failCount;
            }

            ++completedSteps;
            emit progressUpdated(completedSteps, totalSteps);
        }

        emit caseFinished(i, tc.caseName, 0);
    }

    LOG_INFO("ENGINE", "测试套件执行完毕 [pass={} fail={}]", passCount, failCount);
    emit suiteFinished(program.suiteName, passCount, failCount);
}

// ==========================================================================
// Cancel / reset
// ==========================================================================

void StepRunner::cancel() {
    cancel_flag_.store(2, std::memory_order_release);
}

void StepRunner::pause() {
    cancel_flag_.store(1, std::memory_order_release);
}

void StepRunner::resume() {
    cancel_flag_.store(0, std::memory_order_release);
}

void StepRunner::resetCancel() {
    cancel_flag_.store(0, std::memory_order_release);
}

bool StepRunner::isCancelling() const {
    return cancel_flag_.load(std::memory_order_acquire) == 2;
}

bool StepRunner::isPaused() const {
    return cancel_flag_.load(std::memory_order_acquire) == 1;
}

// ==========================================================================
// executeSingleStep — resolve signal, time, dispatch, emit
// ==========================================================================

StepResult StepRunner::executeSingleStep(const TestStepData& step,
                                          int caseIndex,
                                          const QString& stepPath) {
    // ── Cancel check ──
    if (cancel_flag_.load(std::memory_order_acquire) == 2) {
        StepResult result;
        result.stepPath = stepPath;
        result.command = step.command;
        result.target = step.target;
        result.setStatus(SKIPPED).setMessage(QStringLiteral("Cancelled"));
        emit stepStarted(caseIndex, stepPath, step.command, step.target);
        emit stepFinished(caseIndex, stepPath, result);
        return result;
    }

    // ── Pause check — busy-wait until resumed or cancelled ──
    while (cancel_flag_.load(std::memory_order_acquire) == 1) {
        QThread::msleep(50);
        if (cancel_flag_.load(std::memory_order_acquire) == 2) {
            StepResult result;
            result.stepPath = stepPath;
            result.command = step.command;
            result.target = step.target;
            result.setStatus(SKIPPED).setMessage(QStringLiteral("Cancelled while paused"));
            emit stepStarted(caseIndex, stepPath, step.command, step.target);
            emit stepFinished(caseIndex, stepPath, result);
            return result;
        }
    }

    emit stepStarted(caseIndex, stepPath, step.command, step.target);

    QElapsedTimer timer;
    timer.start();

    StepResult result;
    result.stepPath = stepPath;
    result.command = step.command;
    result.target = step.target;
    result.timestamp = QDateTime::currentDateTime();

    // ── Control-flow commands do not need signal resolution ──
    // (DELAY is pure timing; LOOP/WHILE/IF resolve internally)
    {
      const QString upperCmd = step.command.trimmed().toUpper();
      if (upperCmd == QStringLiteral("DELAY") ||
          upperCmd == QStringLiteral("LOOP") ||
          upperCmd == QStringLiteral("WHILE") ||
          upperCmd == QStringLiteral("IF")) {
        CommandType ct = commandType(step.command);
        switch (ct) {
          case CommandType::DELAY:
            result = execDelay(step);
            break;
          case CommandType::LOOP:
            result = execLoop(step, caseIndex, stepPath);
            break;
          case CommandType::WHILE:
            result = execWhile(step, caseIndex, stepPath);
            break;
          case CommandType::IF:
            result = execIf(step, caseIndex, stepPath);
            break;
          default:
            break;
        }
        result.stepPath = stepPath;
        result.command = step.command;
        result.target = step.target;
        result.timestamp = QDateTime::currentDateTime();
        result.elapsedMs = static_cast<int>(timer.elapsed());
        emit stepFinished(caseIndex, stepPath, result);
        return result;
      }
    }

    // ── Resolve signal by UUID ──
    ResolvedSignal signal;
    if (resolver_ != nullptr) {
        signal = resolver_->resolve(step.target);
    }

    if (!signal.valid) {
        result.setStatus(ERROR)
            .setMessage(QStringLiteral("Signal not found: ") + step.target);
        result.elapsedMs = static_cast<int>(timer.elapsed());
        emit stepFinished(caseIndex, stepPath, result);
        return result;
    }

    LOG_INFO("ENGINE", "执行步骤 [cmd={} target={}]", step.command.toStdString(), step.target.toStdString());

    // ── Dispatch by command type ──
    CommandType ct = commandType(step.command);
    switch (ct) {
        case CommandType::SET:
            result = execSet(step, signal);
            break;
        case CommandType::CHECK:
            result = execCheck(step, signal);
            break;
        case CommandType::VERIFY:
            result = execVerify(step, signal);
            break;
        case CommandType::WAIT:
            result = execWait(step, signal);
            break;
        case CommandType::LOOP:
            result = execLoop(step, caseIndex, stepPath);
            break;
        case CommandType::WHILE:
            result = execWhile(step, caseIndex, stepPath);
            break;
        case CommandType::IF:
            result = execIf(step, caseIndex, stepPath);
            break;
        default:
            result.setStatus(ERROR)
                .setMessage(QStringLiteral("Unknown command: ") + step.command);
            break;
    }

    result.stepPath = stepPath;
    result.command = step.command;
    result.target = step.target;
    result.timestamp = QDateTime::currentDateTime();
    result.elapsedMs = static_cast<int>(timer.elapsed());

    emit stepFinished(caseIndex, stepPath, result);
    return result;
}

// ==========================================================================
// execSet — encode engineering value, write to hardware
// ==========================================================================

StepResult StepRunner::execSet(const TestStepData& step,
                                const ResolvedSignal& signal) {
    LOG_INFO("ENGINE", "SET [target={} value={}]", signal.deviceId.toStdString(), step.value);
    StepResult result;
    result.expectedValue = step.value;

    double rawValue = 0.0;
    QByteArray rawFrame;
    bool success = false;
    if (signal.signalType == SignalType::AD ||
        signal.signalType == SignalType::DA) {
        // Channel-type (AD/DA): encode → write
        QVariant raw = codec_->encode(step.value, signal);
        if (raw.isValid()) {
            rawValue = raw.toDouble();
            success = hw_->write(signal, rawValue);
        }
    } else {
        // Frame-type (CAN/Serial/A429): encodeToFrame → writeFrame
        rawFrame = codec_->encodeToFrame(step.value, signal);
        success = hw_->writeFrame(signal, rawFrame);
    }

    if (success) {
        LOG_INFO("ENGINE", "  -> PASS [value={}]", step.value);
        result.setStatus(PASS).setMessage(QStringLiteral("OK"));
        emit hardwareOperationFinished(signal.deviceId, signal.portName,
                                        rawFrame, rawValue, step.value);
    } else {
        result.setStatus(ERROR)
            .setMessage(QStringLiteral("Hardware write failed"));
    }
    return result;
}

// ==========================================================================
// execCheck — read value once, decode, compare with tolerance
// ==========================================================================

StepResult StepRunner::execCheck(const TestStepData& step,
                                  const ResolvedSignal& signal) {
    LOG_INFO("ENGINE", "CHECK [target={}]", signal.deviceId.toStdString());
    StepResult result;
    result.expectedValue = step.value;

    try {
        double actual = 0.0;
        double rawValue = 0.0;
        QByteArray rawFrame;
        if (signal.signalType == SignalType::AD ||
            signal.signalType == SignalType::DA) {
            // Channel-type: read → decode
            QVariant raw = hw_->read(signal);
            rawValue = raw.toDouble();
            actual = codec_->decode(raw, signal);
        } else {
            // Frame-type: read → decodeFromFrame
            QVariant raw = hw_->read(signal);
            if (raw.canConvert<QByteArray>()) {
                rawFrame = raw.toByteArray();
            }
            actual = codec_->decodeFromFrame(rawFrame, signal);
        }

        result.actualValue = actual;
        emit hardwareOperationFinished(signal.deviceId, signal.portName,
                                        rawFrame, rawValue, actual);

        double diff = std::fabs(actual - step.value);
        if (diff <= step.tolerance) {
            LOG_INFO("ENGINE", "  -> PASS [value={}]", actual);
            result.setStatus(PASS)
                .setMessage(QStringLiteral("OK (actual=%1, tolerance=%2)")
                                .arg(actual)
                                .arg(step.tolerance));
        } else {
            result.setStatus(FAIL)
                .setMessage(QStringLiteral("Mismatch: expected=%1, actual=%2, "
                                           "diff=%3, tolerance=%4")
                                .arg(step.value)
                                .arg(actual)
                                .arg(diff)
                                .arg(step.tolerance));
        }
    } catch (const DeviceException& e) {
        result.setStatus(ERROR)
            .setMessage(QStringLiteral("Device error: ") +
                        QString::fromStdString(e.what()));
    } catch (const TimeoutException& e) {
        result.setStatus(TIMEOUT)
            .setMessage(QStringLiteral("Timeout: ") +
                        QString::fromStdString(e.what()));
    }

    return result;
}

// ==========================================================================
// execVerify — read with timeout, decode, compare with tolerance
// ==========================================================================

StepResult StepRunner::execVerify(const TestStepData& step,
                                   const ResolvedSignal& signal) {
    LOG_INFO("ENGINE", "VERIFY [device={} port={} value={}]",
             signal.deviceId.toStdString(), signal.portName.toStdString(),
             step.value);
    StepResult result;
    result.expectedValue = step.value;

    int timeout = (step.timeoutMs > 0) ? step.timeoutMs : kDefaultTimeoutMs;

    try {
        double actual = 0.0;
        double rawValue = 0.0;
        QByteArray rawFrame;
        if (signal.signalType == SignalType::AD ||
            signal.signalType == SignalType::DA) {
            QVariant raw = hw_->readAndWait(signal, timeout);
            rawValue = raw.toDouble();
            actual = codec_->decode(raw, signal);
        } else {
            QVariant raw = hw_->readAndWait(signal, timeout);
            if (raw.canConvert<QByteArray>()) {
                rawFrame = raw.toByteArray();
            }
            actual = codec_->decodeFromFrame(rawFrame, signal);
        }

        result.actualValue = actual;
        emit hardwareOperationFinished(signal.deviceId, signal.portName,
                                        rawFrame, rawValue, actual);

        double diff = std::fabs(actual - step.value);
        if (diff <= step.tolerance) {
            LOG_INFO("ENGINE", "  -> PASS [actual={}]", actual);
            result.setStatus(PASS)
                .setMessage(QStringLiteral("OK (actual=%1, tolerance=%2)")
                                .arg(actual)
                                .arg(step.tolerance));
        } else {
            LOG_INFO("ENGINE", "  -> FAIL [actual={}]", actual);
            result.setStatus(FAIL)
                .setMessage(QStringLiteral("Mismatch: expected=%1, actual=%2")
                                .arg(step.value)
                                .arg(actual));
        }
    } catch (const DeviceException& e) {
        result.setStatus(ERROR)
            .setMessage(QStringLiteral("Device error: ") +
                        QString::fromStdString(e.what()));
    } catch (const TimeoutException& e) {
        result.setStatus(TIMEOUT)
            .setMessage(QStringLiteral("Timeout: ") +
                        QString::fromStdString(e.what()));
    }

    return result;
}

// ==========================================================================
// execWait — poll read until condition met or timed out
// ==========================================================================

StepResult StepRunner::execWait(const TestStepData& step,
                                 const ResolvedSignal& signal) {
    StepResult result;

    int timeout = (step.timeoutMs > 0) ? step.timeoutMs : kDefaultTimeoutMs;
    QElapsedTimer waitTimer;
    waitTimer.start();

    // Condition is in extra field for WAIT steps
    const QString condition =
        step.extra.isEmpty() ? step.condition : step.extra;

    while (!waitTimer.hasExpired(timeout)) {
        if (cancel_flag_.load(std::memory_order_acquire) == 2) {
            result.setStatus(SKIPPED)
                .setMessage(QStringLiteral("Cancelled during WAIT"));
            return result;
        }

        try {
            double actual = 0.0;
            if (signal.signalType == SignalType::AD ||
                signal.signalType == SignalType::DA) {
                QVariant raw = hw_->read(signal);
                actual = codec_->decode(raw, signal);
            } else {
                QVariant raw = hw_->read(signal);
                QByteArray frameData;
                if (raw.canConvert<QByteArray>()) {
                    frameData = raw.toByteArray();
                }
                actual = codec_->decodeFromFrame(frameData, signal);
            }

            result.actualValue = actual;

            if (evaluateCondition(condition, signal)) {
                result.setStatus(PASS)
                    .setMessage(QStringLiteral("Condition met: %1 (actual=%2)")
                                    .arg(condition)
                                    .arg(actual));
                return result;
            }
        } catch (const DeviceException& e) {
            result.setStatus(ERROR)
                .setMessage(
                    QStringLiteral("Device error during WAIT: ") +
                    QString::fromStdString(e.what()));
            return result;
        }

        QThread::msleep(50);
    }

    result.setStatus(TIMEOUT)
        .setMessage(
            QStringLiteral("Wait condition not met within %1 ms: %2")
                .arg(timeout)
                .arg(condition));
    return result;
}

// ==========================================================================
// execDelay — sleep for the specified duration
// ==========================================================================

StepResult StepRunner::execDelay(const TestStepData& step) {
    StepResult result;

    int delayMs = step.extra.toInt();
    if (delayMs <= 0) {
        // Fallback: use step.value if positive, else default
        delayMs = (step.value > 0) ? static_cast<int>(step.value) : 1000;
    }

    // Chunked sleep so cancel/pause is responsive
    int remaining = delayMs;
    while (remaining > 0) {
        if (cancel_flag_.load(std::memory_order_acquire) == 2) {
            result.setStatus(SKIPPED)
                .setMessage(QStringLiteral("Cancelled during DELAY"));
            return result;
        }
        int chunk = qMin(remaining, 100);
        QThread::msleep(static_cast<unsigned long>(chunk));
        remaining -= chunk;
    }

    result.setStatus(PASS)
        .setMessage(QStringLiteral("Delayed %1 ms").arg(delayMs));
    return result;
}

// ==========================================================================
// execLoop — iterate loopCount times, collect sub-step results
// ==========================================================================

StepResult StepRunner::execLoop(const TestStepData& step, int caseIndex,
                                 const QString& stepPath) {
    StepResult result;
    result.expectedValue = step.loopCount;

    int maxIterations = (step.loopCount > 0) ? step.loopCount : 1;
    int failCount = 0;

    for (int i = 0; i < maxIterations; ++i) {
        if (cancel_flag_.load(std::memory_order_acquire) == 2) {
            result.setStatus(SKIPPED)
                .setMessage(QStringLiteral("Cancelled during LOOP"));
            return result;
        }

        IterationResult iterResult;
        iterResult.iteration = i;

        for (int j = 0; j < step.subSteps.size(); ++j) {
            QString subPath = QStringLiteral("%1/L%2/%3")
                                  .arg(stepPath)
                                  .arg(i + 1)
                                  .arg(j + 1);
            StepResult subResult =
                executeSingleStep(step.subSteps[j], caseIndex, subPath);
            iterResult.subSteps.append(subResult);

            if (subResult.status == FAIL || subResult.status == TIMEOUT ||
                subResult.status == ERROR) {
                ++failCount;
            }
        }

        result.iterations.append(iterResult);
    }

    if (failCount > 0) {
        result.setStatus(FAIL)
            .setMessage(QStringLiteral("Loop completed with %1 failures")
                            .arg(failCount));
    } else {
        result.setStatus(PASS)
            .setMessage(QStringLiteral("Loop completed %1 iterations")
                            .arg(maxIterations));
    }

    return result;
}

// ==========================================================================
// execWhile — evaluate condition, execute sub-steps while true
// ==========================================================================

StepResult StepRunner::execWhile(const TestStepData& step, int caseIndex,
                                  const QString& stepPath) {
    StepResult result;

    int maxIterations = (step.loopCount > 0) ? step.loopCount : 100;
    int iterationCount = 0;
    int failCount = 0;

    while (iterationCount < maxIterations) {
        if (cancel_flag_.load(std::memory_order_acquire) == 2) {
            result.setStatus(SKIPPED)
                .setMessage(QStringLiteral("Cancelled during WHILE"));
            return result;
        }

        // Evaluate condition against the resolved signal
        ResolvedSignal signal;
        if (resolver_ != nullptr && !step.target.isEmpty()) {
            signal = resolver_->resolve(step.target);
        }

        if (!signal.valid ||
            !evaluateCondition(step.condition, signal)) {
            break;
        }

        IterationResult iterResult;
        iterResult.iteration = iterationCount;

        for (int j = 0; j < step.subSteps.size(); ++j) {
            QString subPath = QStringLiteral("%1/W%2/%3")
                                  .arg(stepPath)
                                  .arg(iterationCount + 1)
                                  .arg(j + 1);
            StepResult subResult =
                executeSingleStep(step.subSteps[j], caseIndex, subPath);
            iterResult.subSteps.append(subResult);

            if (subResult.status == FAIL || subResult.status == TIMEOUT ||
                subResult.status == ERROR) {
                ++failCount;
            }
        }

        result.iterations.append(iterResult);
        ++iterationCount;
    }

    result.setStatus(PASS)
        .setMessage(QStringLiteral("While loop completed after %1 iterations")
                        .arg(iterationCount));
    return result;
}

// ==========================================================================
// execIf — evaluate condition, execute then or else branch
// ==========================================================================

StepResult StepRunner::execIf(const TestStepData& step, int caseIndex,
                               const QString& stepPath) {
    StepResult result;

    // Resolve signal for condition evaluation
    ResolvedSignal signal;
    if (resolver_ != nullptr && !step.target.isEmpty()) {
        signal = resolver_->resolve(step.target);
    }

    bool conditionMet = false;
    if (signal.valid) {
        conditionMet = evaluateCondition(step.condition, signal);
    }

    result.branches.conditionMet = conditionMet;

    if (conditionMet) {
        for (int i = 0; i < step.thenSteps.size(); ++i) {
            QString subPath =
                QStringLiteral("%1/THEN/%2").arg(stepPath).arg(i + 1);
            StepResult subResult =
                executeSingleStep(step.thenSteps[i], caseIndex, subPath);
            result.branches.thenSteps.append(subResult);
        }
        result.setStatus(PASS)
            .setMessage(QStringLiteral("Condition true, executed THEN branch"));
    } else {
        for (int i = 0; i < step.elseSteps.size(); ++i) {
            QString subPath =
                QStringLiteral("%1/ELSE/%2").arg(stepPath).arg(i + 1);
            StepResult subResult =
                executeSingleStep(step.elseSteps[i], caseIndex, subPath);
            result.branches.elseSteps.append(subResult);
        }
        result.setStatus(PASS)
            .setMessage(
                QStringLiteral("Condition false, executed ELSE branch"));
    }

    return result;
}

// ==========================================================================
// commandType — map string to enum
// ==========================================================================

CommandType StepRunner::commandType(const QString& cmd) const {
    QString upper = cmd.trimmed().toUpper();
    if (upper == QStringLiteral("SET")) {
        return CommandType::SET;
    }
    if (upper == QStringLiteral("CHECK")) {
        return CommandType::CHECK;
    }
    if (upper == QStringLiteral("VERIFY")) {
        return CommandType::VERIFY;
    }
    if (upper == QStringLiteral("WAIT")) {
        return CommandType::WAIT;
    }
    if (upper == QStringLiteral("DELAY")) {
        return CommandType::DELAY;
    }
    if (upper == QStringLiteral("LOOP")) {
        return CommandType::LOOP;
    }
    if (upper == QStringLiteral("WHILE")) {
        return CommandType::WHILE;
    }
    if (upper == QStringLiteral("IF")) {
        return CommandType::IF;
    }
    return CommandType::UNKNOWN;  // default fallback
}

// ==========================================================================
// evaluateCondition — parse operator + threshold, read hardware, compare
// ==========================================================================

bool StepRunner::evaluateCondition(const QString& condition,
                                    const ResolvedSignal& signal) const {
    QString trimmed = condition.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }

    bool greaterThan = false;
    bool lessThan = false;
    bool equalTo = false;
    bool orEqual = false;
    int valueStart = 1;

    if (trimmed.startsWith(QStringLiteral(">="))) {
        greaterThan = true;
        orEqual = true;
        valueStart = 2;
    } else if (trimmed.startsWith(QStringLiteral("<="))) {
        lessThan = true;
        orEqual = true;
        valueStart = 2;
    } else if (trimmed.startsWith(QStringLiteral("=="))) {
        equalTo = true;
        valueStart = 2;
    } else if (trimmed.startsWith(QLatin1Char('>'))) {
        greaterThan = true;
        valueStart = 1;
    } else if (trimmed.startsWith(QLatin1Char('<'))) {
        lessThan = true;
        valueStart = 1;
    } else {
        // No operator found; treat as "== threshold" (implicit equality)
        equalTo = true;
        valueStart = 0;
    }

    double threshold = 0.0;
    if (valueStart > 0 && valueStart < trimmed.size()) {
        QString numStr = trimmed.mid(valueStart).trimmed();
        bool ok = false;
        threshold = numStr.toDouble(&ok);
        if (!ok) {
            threshold = 0.0;
        }
    } else if (valueStart == 0) {
        // No operator prefix, the whole string is the value
        bool ok = false;
        threshold = trimmed.toDouble(&ok);
        if (!ok) {
            threshold = 0.0;
        }
    }

    // Read current value from hardware
    double actualValue = 0.0;
    try {
        if (signal.signalType == SignalType::AD ||
            signal.signalType == SignalType::DA) {
            QVariant raw = hw_->read(signal);
            actualValue = codec_->decode(raw, signal);
        } else {
            QVariant raw = hw_->read(signal);
            QByteArray frameData;
            if (raw.canConvert<QByteArray>()) {
                frameData = raw.toByteArray();
            }
            actualValue = codec_->decodeFromFrame(frameData, signal);
        }
    } catch (...) {
        return false;
    }

    if (greaterThan) {
        if (orEqual) {
            return actualValue >= threshold;
        }
        return actualValue > threshold;
    }
    if (lessThan) {
        if (orEqual) {
            return actualValue <= threshold;
        }
        return actualValue < threshold;
    }
    if (equalTo) {
        return std::fabs(actualValue - threshold) < 0.001;
    }

    return true;
}

}  // namespace etest::engine
