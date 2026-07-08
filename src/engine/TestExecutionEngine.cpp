#include "TestExecutionEngine.h"

namespace etest::engine {

TestExecutionEngine::TestExecutionEngine(etest::core::SignalRegistry* registry,
                                          icd::Repository* icdRepo,
                                          QObject* parent)
    : QObject(parent)
    , signal_registry_(registry)
    , icd_repository_(icdRepo) {
    qRegisterMetaType<EngineState>("etest::engine::EngineState");
}

TestExecutionEngine::~TestExecutionEngine() {
    stop();
}

bool TestExecutionEngine::start() {
    if (state_.load() != EngineState::Idle) {
        return false;
    }
    state_ = EngineState::Running;
    emit engineStarted();
    emit engineStateChanged(state_);
    return true;
}

void TestExecutionEngine::stop() {
    if (state_.load() == EngineState::Idle) {
        return;
    }
    state_ = EngineState::Idle;
    emit engineStateChanged(state_);
    emit engineFinished();
}

void TestExecutionEngine::pause() {
    if (state_.load() != EngineState::Running) {
        return;
    }
    state_ = EngineState::Paused;
    emit engineStateChanged(state_);
}

void TestExecutionEngine::resume() {
    if (state_.load() != EngineState::Paused) {
        return;
    }
    state_ = EngineState::Running;
    emit engineStateChanged(state_);
}

}  // namespace etest::engine
