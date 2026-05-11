#include "LuaDebugger.h"
#include <sol/sol.hpp>
#include <QDebug>
#include <cmath>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

void luaDebugHook(lua_State *L, lua_Debug *ar) {
    sol::state_view lua(L);
    sol::object obj = lua.registry()["__debugger_ptr"];
    if (!obj.is<LuaDebugger*>()) return;
    auto *self = obj.as<LuaDebugger*>();

    switch (ar->event) {
        case LUA_HOOKLINE:
            self->handleLine(L, ar);
            break;
        case LUA_HOOKCALL:
            ++self->currentDepth_;
            break;
        case LUA_HOOKRET:
            if (self->currentDepth_ > 0) --self->currentDepth_;
            break;
    }
}

LuaDebugger::LuaDebugger(QObject *parent) : QObject(parent) {}

LuaDebugger::~LuaDebugger() {
    stop();
    if (workerThread_.joinable())
        workerThread_.join();
}

void LuaDebugger::loadScript(const QString &script) {
    script_ = script;
}

void LuaDebugger::setBreakpoints(const QSet<int> &bps) {
    breakpoints_ = bps;
}

void LuaDebugger::executeScript() {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);

    sol::state_view luaView(lua);
    luaView.registry()["__debugger_ptr"] = this;

    luaView.registry()["__debugger_stop"] = false;
    luaView.registry()["__debugger_pause"] = false;

    lua["SetDevice"] = [this](const std::string &target, double value) {
        QString msg = QString("SetDevice(\"%1\", %2)").arg(target.c_str()).arg(value);
        emit output(msg);
    };

    lua["GetDevice"] = [this](const std::string &target) -> double {
        return 37.5;
    };

    lua["VerifyDevice"] = [this](const std::string &target, double expected,
                                  sol::optional<sol::table> tol) -> bool {
        QString msg = QString("VerifyDevice(\"%1\", %2)").arg(target.c_str()).arg(expected);
        if (tol) {
            double minVal = tol->get_or("min", -0.1);
            double maxVal = tol->get_or("max", 0.1);
            msg += QString(" [%1, %2]").arg(minVal).arg(maxVal);
        }
        emit output(msg + " → PASS");
        return true;
    };

    sol::function getDeviceFn = lua["GetDevice"];
    lua["WaitFor"] = [this, getDeviceFn](const std::string &target, const std::string &op,
                             double value, sol::optional<int> timeout) {
        int maxWait = timeout.value_or(5000);
        int elapsed = 0;
        bool satisfied = false;
        while (elapsed < maxWait && !stopRequested_) {
            double current = getDeviceFn(target);
            if (op == ">=") satisfied = current >= value;
            else if (op == "<=") satisfied = current <= value;
            else if (op == ">") satisfied = current > value;
            else if (op == "<") satisfied = current < value;
            else if (op == "==") satisfied = std::abs(current - value) < 0.001;
            else if (op == "!=") satisfied = std::abs(current - value) >= 0.001;

            if (satisfied) {
                emit output(QString("WaitFor(%1, %2, %3) satisfied after %4ms")
                            .arg(target.c_str()).arg(op.c_str()).arg(value).arg(elapsed));
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            elapsed += 50;
        }
        if (!satisfied) {
            emit output(QString("WaitFor(%1) TIMEOUT after %2ms")
                        .arg(target.c_str()).arg(maxWait));
        }
    };

    lua["Delay"] = [this](int ms) {
        emit output(QString("Delay(%1)").arg(ms));
        int slept = 0;
        while (slept < ms && !stopRequested_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            slept += 10;
            if (pauseRequested_) {
                waitMutex_.lock();
                while (pauseRequested_ && !stopRequested_)
                    resumeCond_.wait(&waitMutex_);
                waitMutex_.unlock();
            }
        }
    };

    lua["UserAction"] = [this](const std::string &desc) {
        emit output(QString("UserAction: %1").arg(desc.c_str()));
    };

    lua["TakePhoto"] = [this]() {
        emit output("TakePhoto()");
    };

    lua["SetRecord"] = [this](bool enable) {
        emit output(QString("SetRecord(%1)").arg(enable ? "true" : "false"));
    };

    lua["InjectFault"] = [this](const std::string &target, sol::optional<sol::table> config) {
        QString msg = QString("InjectFault(\"%1\")").arg(target.c_str());
        if (config) {
            auto type = config->get<std::string>("type");
            msg += QString(" type=%1").arg(type.c_str());
        }
        emit output(msg);
    };

    lua["ClearFault"] = [this](const std::string &target) {
        emit output(QString("ClearFault(\"%1\")").arg(target.c_str()));
    };

    lua["Log"] = [this](const std::string &text) {
        emit output(QString("[LOG] %1").arg(text.c_str()));
    };

    lua_sethook(lua.lua_state(), luaDebugHook,
                LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET, 0);

    state_ = Running;
    auto result = lua.safe_script(script_.toStdString(), sol::script_pass_on_error);

    if (!result.valid()) {
        sol::error err = result;
        QString errMsg = QString::fromStdString(err.what());
        if (errMsg.contains("Execution terminated") || errMsg.contains("Execution stopped")) {
            emit finished();
        } else {
            emit error(errMsg);
        }
    } else {
        emit finished();
    }
    state_ = Finished;
}

void LuaDebugger::handleLine(lua_State *L, lua_Debug *ar) {
    lua_getinfo(L, "nS", ar);
    int line = ar->currentline;
    QString source = QString::fromUtf8(ar->source ? ar->source : "");

    emit lineChanged(line);

    if (breakpoints_.contains(line))
        pauseRequested_ = true;

    if (stepMode_ == StepInto) {
        pauseRequested_ = true;
        stepMode_ = None;
    } else if (stepMode_ == StepOver) {
        if (currentDepth_ <= depthAtPause_) {
            pauseRequested_ = true;
            stepMode_ = None;
        }
    } else if (stepMode_ == StepOut) {
        if (currentDepth_ < depthAtPause_) {
            pauseRequested_ = true;
            stepMode_ = None;
        }
    }

    if (stopRequested_)
        luaL_error(L, "Execution terminated");

    if (pauseRequested_) {
        DebugSnapshot snapshot = captureSnapshot(L, ar);
        state_ = Paused;
        emit paused(snapshot);

        waitMutex_.lock();
        while (pauseRequested_ && !stopRequested_)
            resumeCond_.wait(&waitMutex_);
        waitMutex_.unlock();

        if (stopRequested_)
            luaL_error(L, "Execution terminated");

        state_ = Running;
    }
}

DebugSnapshot LuaDebugger::captureSnapshot(lua_State *L, lua_Debug *ar) {
    DebugSnapshot snap;
    snap.currentLine = ar->currentline;
    snap.sourceName = QString::fromUtf8(ar->source ? ar->source : "");

    for (int level = 0; level < 10; ++level) {
        lua_Debug frameAr;
        if (!lua_getstack(L, level, &frameAr))
            break;
        lua_getinfo(L, "nSl", &frameAr);

        StackFrame frame;
        frame.level = level;
        frame.funcName = frameAr.name ? QString::fromUtf8(frameAr.name) : "(global)";
        frame.source = QString::fromUtf8(frameAr.source ? frameAr.source : "");
        frame.line = frameAr.currentline;
        frame.locals = captureLocals(L, level);
        snap.frames.append(frame);
    }

    lua_pushglobaltable(L);
    lua_pushnil(L);
    int count = 0;
    while (lua_next(L, -2) && count < 50) {
        if (lua_type(L, -2) == LUA_TSTRING) {
            const char *key = lua_tostring(L, -2);
            if (key && key[0] != '_' && strcmp(key, "sol") != 0
                && strcmp(key, "__debugger_ptr") != 0) {
                int t = lua_type(L, -1);
                if (t != LUA_TFUNCTION && t != LUA_TTHREAD
                    && t != LUA_TUSERDATA && t != LUA_TLIGHTUSERDATA
                    && t != LUA_TTABLE) {
                    snap.globals[QString::fromUtf8(key)] = luaToQVariant(L, -1, 0);
                    ++count;
                }
            }
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return snap;
}

QMap<QString, QVariant> LuaDebugger::captureLocals(lua_State *L, int level) {
    QMap<QString, QVariant> locals;
    lua_Debug ar;
    if (!lua_getstack(L, level, &ar))
        return locals;

    int i = 1;
    const char *name;
    while ((name = lua_getlocal(L, &ar, i)) != nullptr) {
        locals[QString::fromUtf8(name)] = luaToQVariant(L, -1, 0);
        lua_pop(L, 1);
        ++i;
    }
    return locals;
}

QVariant LuaDebugger::luaToQVariant(lua_State *L, int idx, int depth) {
    if (depth > 3) return QVariant("[...]");
    idx = lua_absindex(L, idx);

    switch (lua_type(L, idx)) {
        case LUA_TNIL:
            return QVariant();
        case LUA_TBOOLEAN:
            return QVariant(static_cast<bool>(lua_toboolean(L, idx)));
        case LUA_TNUMBER:
            if (lua_isinteger(L, idx))
                return QVariant(static_cast<qlonglong>(lua_tointeger(L, idx)));
            else
                return QVariant(lua_tonumber(L, idx));
        case LUA_TSTRING:
            return QVariant(QString::fromUtf8(lua_tostring(L, idx)));
        case LUA_TTABLE: {
            QVariantMap map;
            lua_pushnil(L);
            while (lua_next(L, idx)) {
                QString key;
                if (lua_isstring(L, -2))
                    key = QString::fromUtf8(lua_tostring(L, -2));
                else if (lua_isinteger(L, -2))
                    key = QString::number(static_cast<qlonglong>(lua_tointeger(L, -2)));
                else
                    key = "?";
                QVariant val = luaToQVariant(L, -1, depth + 1);
                map[key] = val;
                lua_pop(L, 1);
            }
            return map;
        }
        default:
            return QVariant(QString("[%1]").arg(lua_typename(L, lua_type(L, idx))));
    }
}

void LuaDebugger::run() {
    if (state_ != Idle && state_ != Finished) return;
    if (workerThread_.joinable())
        workerThread_.join();
    stopRequested_ = false;
    pauseRequested_ = false;
    stepMode_ = None;
    currentDepth_ = 0;
    state_ = Running;
    emit started();

    workerThread_ = std::thread([this]() {
        executeScript();
    });
}

void LuaDebugger::pause() {
    pauseRequested_ = true;
}

void LuaDebugger::resume() {
    if (state_ != Paused) return;
    pauseRequested_ = false;
    stepMode_ = None;
    resumeCond_.wakeAll();
}

void LuaDebugger::stop() {
    stopRequested_ = true;
    pauseRequested_ = false;
    resumeCond_.wakeAll();
}

void LuaDebugger::stepInto() {
    if (state_ != Paused) return;
    stepMode_ = StepInto;
    depthAtPause_ = currentDepth_;
    pauseRequested_ = false;
    resumeCond_.wakeAll();
}

void LuaDebugger::stepOver() {
    if (state_ != Paused) return;
    stepMode_ = StepOver;
    depthAtPause_ = currentDepth_;
    pauseRequested_ = false;
    resumeCond_.wakeAll();
}

void LuaDebugger::stepOut() {
    if (state_ != Paused) return;
    stepMode_ = StepOut;
    depthAtPause_ = currentDepth_;
    pauseRequested_ = false;
    resumeCond_.wakeAll();
}
