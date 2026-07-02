#ifndef ETEST_PROGRAM_STEP_VALIDATION_H_
#define ETEST_PROGRAM_STEP_VALIDATION_H_

#include <QStringList>

#include "TestProgramData.h"

namespace etest::app {
namespace StepValidation {

// 校验单步，返回错误/警告信息列表（空表示无问题）
QStringList validateStep(const TestStepData& step);

// 校验单个用例（含所有步骤）
QStringList validateCase(const TestCaseData& tc);

// 校验完整套件（用例名去重检测）
QStringList validateSuite(const TestProgramData& suite);

}  // namespace StepValidation
}  // namespace etest::app

#endif  // ETEST_PROGRAM_STEP_VALIDATION_H_
