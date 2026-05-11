#ifndef LUADEBUGGER_H
#define LUADEBUGGER_H

#include <QObject>
#include <QString>

struct lua_State;
struct lua_Debug;
#include <QMap>
#include <QVector>
#include <QVariant>
#include <QSet>
#include <QMutex>
#include <QWaitCondition>
#include <thread>
#include <atomic>

struct StackFrame {
    int level;
    QString funcName;
    QString source;
    int line;
    QMap<QString, QVariant> locals;
};

struct DebugSnapshot {
    QString sourceName;
    int currentLine;
    QVector<StackFrame> frames;
    QMap<QString, QVariant> globals;
};

Q_DECLARE_METATYPE(StackFrame)
Q_DECLARE_METATYPE(DebugSnapshot)

class LuaDebugger : public QObject {
    Q_OBJECT
public:
    enum StepMode { None, StepInto, StepOver, StepOut };
    enum State { Idle, Running, Paused, Finished };

    explicit LuaDebugger(QObject *parent = nullptr);
    ~LuaDebugger() override;

    void loadScript(const QString &script);
    void setBreakpoints(const QSet<int> &bps);
    const QSet<int> &breakpoints() const { return breakpoints_; }
    State state() const { return state_; }

signals:
    void started();
    void lineChanged(int line);
    void paused(const DebugSnapshot &snapshot);
    void finished();
    void error(const QString &message);
    void output(const QString &text);
    void evalResultReady(const QString &result);

public slots:
    void run();
    void pause();
    void resume();
    void stop();
    void stepInto();
    void stepOver();
    void stepOut();
    void requestEval(const QString &expr);

private:
    void executeScript();
    void handleLine(lua_State *L, lua_Debug *ar);
    DebugSnapshot captureSnapshot(lua_State *L, lua_Debug *ar);
    QMap<QString, QVariant> captureLocals(lua_State *L, int level);
    QVariant luaToQVariant(lua_State *L, int idx, int depth = 0);
    void evalInPausedContext(lua_State *L, const QString &expr);

    std::thread workerThread_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> pauseRequested_{false};
    StepMode stepMode_{None};
    int currentDepth_{0};
    int depthAtPause_{0};

    QSet<int> breakpoints_;
    QString script_;
    State state_{Idle};

    QMutex waitMutex_;
    QWaitCondition resumeCond_;

    QString pendingExpr_;
    QMutex evalMutex_;

    friend void luaDebugHook(lua_State *L, lua_Debug *ar);
};

#endif
