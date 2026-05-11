-- Lua Debugger Demo Script
-- 在编辑器中点击行号右侧的边栏设置断点
-- 然后点击 "运行" 开始调试
--
-- 调试功能:
--   Step Into  - 进入函数内部
--   Step Over  - 跳过函数调用
--   Step Out   - 跳出当前函数

SetDevice("温度", 25.0)
Delay(100)

local target_temp = 37.5
SetDevice("温度", target_temp)

local ok = VerifyDevice("温度", target_temp, {min = -0.1, max = 0.1})
if ok then
    Log("温度验证通过")
else
    Log("温度验证失败")
end

for i = 1, 3 do
    SetDevice("加热器", i)
    Delay(50)
    local status = "running"
    Log("加热器档位: " .. status .. " " .. i)
end

InjectFault("温度", {type = "stuck_at", value = 999})
Log("故障已注入")

ClearFault("温度")
Log("故障已清除")

SetRecord(true)
UserAction("请观察指示灯状态")
TakePhoto()
Log("测试完成")
