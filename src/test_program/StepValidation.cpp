#include "StepValidation.h"

#include "control_flow_config.h"

namespace etest::app {
namespace StepValidation {

QStringList validateStep(const TestStepData& step) {
  QStringList issues;
  const QString cmd = step.cmd.trimmed().toUpper();

  if (cmd.isEmpty()) {
    issues << QStringLiteral("命令不能为空");
    return issues;
  }

  // 通用：所有命令共用
  static const QStringList validCommands = [] {
    QStringList list = {
        QStringLiteral("SET"),         QStringLiteral("VERIFY"),
        QStringLiteral("WAIT"),        QStringLiteral("DELAY"),
        QStringLiteral("ACTION"),      QStringLiteral("LOG"),
        QStringLiteral("INJECT_FAULT"), QStringLiteral("CLEAR_FAULT"),
        QStringLiteral("PHOTO"),       QStringLiteral("RECORD")};
    if (kControlFlowEnabled) {
      list << QStringLiteral("LOOP") << QStringLiteral("WHILE")
           << QStringLiteral("IF");
    }
    return list;
  }();

  if (!validCommands.contains(cmd)) {
    if (!kControlFlowEnabled && (cmd == QStringLiteral("LOOP") ||
                                 cmd == QStringLiteral("WHILE") ||
                                 cmd == QStringLiteral("IF"))) {
      issues << QStringLiteral("控制流功能已禁用: %1").arg(cmd);
    } else {
      issues << QStringLiteral("未知命令类型: %1").arg(cmd);
    }
    return issues;
  }

  // 按命令类型校验
  if (cmd == QStringLiteral("SET")) {
    if (step.target.trimmed().isEmpty()) {
      issues << QStringLiteral("SET: 目标不能为空");
    }
  } else if (cmd == QStringLiteral("VERIFY")) {
    if (step.target.trimmed().isEmpty()) {
      issues << QStringLiteral("VERIFY: 目标不能为空");
    }
    if (!step.value.isValid() || step.value.toString().trimmed().isEmpty()) {
      issues << QStringLiteral("VERIFY: 期望值不能为空");
    }
    if (step.tolerance.enabled && step.tolerance.min > step.tolerance.max) {
      issues << QStringLiteral("VERIFY: 容差下限不能大于上限");
    }
  } else if (cmd == QStringLiteral("WAIT")) {
    if (step.condition.target.trimmed().isEmpty()) {
      issues << QStringLiteral("WAIT: 条件目标不能为空");
    }
    if (step.timeoutMs <= 0) {
      issues << QStringLiteral("WAIT: 超时时间必须大于0");
    }
  } else if (cmd == QStringLiteral("DELAY")) {
    if (step.delayMs <= 0) {
      issues << QStringLiteral("DELAY: 延迟值必须大于0");
    }
  } else if (kControlFlowEnabled && cmd == QStringLiteral("LOOP")) {
    if (step.loopCount < 1) {
      issues << QStringLiteral("LOOP: 循环次数必须 >= 1");
    }
    if (step.subSteps.isEmpty()) {
      issues << QStringLiteral("LOOP: 至少需要一个循环体步骤");
    }
  } else if (kControlFlowEnabled && cmd == QStringLiteral("WHILE")) {
    if (step.condition.target.trimmed().isEmpty()) {
      issues << QStringLiteral("WHILE: 条件目标不能为空");
    }
    if (step.timeoutMs <= 0) {
      issues << QStringLiteral("WHILE: 超时时间必须大于0");
    }
    if (step.subSteps.isEmpty()) {
      issues << QStringLiteral("WHILE: 至少需要一个循环体步骤");
    }
  } else if (kControlFlowEnabled && cmd == QStringLiteral("IF")) {
    if (step.condition.target.trimmed().isEmpty()) {
      issues << QStringLiteral("IF: 条件目标不能为空");
    }
    if (step.subSteps.isEmpty()) {
      issues << QStringLiteral("IF: Then 分支至少需要一个步骤");
    }
  } else if (cmd == QStringLiteral("INJECT_FAULT")) {
    if (step.target.trimmed().isEmpty()) {
      issues << QStringLiteral("INJECT_FAULT: 目标不能为空");
    }
    if (step.fault.type.trimmed().isEmpty()) {
      issues << QStringLiteral("INJECT_FAULT: 故障类型不能为空");
    }
  } else if (cmd == QStringLiteral("CLEAR_FAULT")) {
    if (step.target.trimmed().isEmpty()) {
      issues << QStringLiteral("CLEAR_FAULT: 目标不能为空");
    }
  } else if (cmd == QStringLiteral("ACTION")) {
    if (step.description.trimmed().isEmpty()) {
      issues << QStringLiteral("ACTION: 提示信息不能为空");
    }
  } else if (cmd == QStringLiteral("LOG")) {
    if (step.description.trimmed().isEmpty()) {
      issues << QStringLiteral("LOG: 日志内容不能为空");
    }
  }

  // 校验子步骤（递归）
  for (const auto& sub : step.subSteps) {
    issues << validateStep(sub);
  }
  for (const auto& sub : step.elseSubSteps) {
    issues << validateStep(sub);
  }

  return issues;
}

QStringList validateCase(const TestCaseData& tc) {
  QStringList issues;
  if (tc.name.trimmed().isEmpty()) {
    issues << QStringLiteral("用例名称不能为空");
  }
  for (const auto& step : tc.steps) {
    issues << validateStep(step);
  }
  return issues;
}

QStringList validateSuite(const TestProgramData& suite) {
  QStringList issues;

  // Setup/Teardown 步骤校验
  for (const auto& step : suite.setup) {
    issues << validateStep(step);
  }
  for (const auto& step : suite.teardown) {
    issues << validateStep(step);
  }

  // 用例名去重检测
  QStringList caseNames;
  for (const auto& tc : suite.cases) {
    issues << validateCase(tc);
    if (caseNames.contains(tc.name)) {
      issues << QStringLiteral("重复的用例名称: %1").arg(tc.name);
    }
    caseNames.append(tc.name);
  }

  return issues;
}

}  // namespace StepValidation
}  // namespace etest::app
