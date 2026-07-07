#ifndef ETEST_PROGRAM_CONTROL_FLOW_CONFIG_H_
#define ETEST_PROGRAM_CONTROL_FLOW_CONFIG_H_

namespace etest::app {

// 控制流功能（LOOP/WHILE/IF）开关。
// true  ：控制流命令出现在命令列表、详情页、校验中。
// false ：控制流功能从 UI/校验中消失（命令列表无这三项、控制流命令切到空页、
//         校验跳过）；已有数据仍保留（字段/序列化/ext data 不变），重新打开
//         开关可继续编辑。
// constexpr 编译期常量，false 时编译器优化掉相关分支，等效剔除二进制。
constexpr bool kControlFlowEnabled = true;

}  // namespace etest::app

#endif  // ETEST_PROGRAM_CONTROL_FLOW_CONFIG_H_
