# protocal 模块规则

- protocal 作为ICD模块的UI部分。它主要负责可视化用户协议展示和编辑。
- 通过 icd_utility提供的协议的解析、序列化和反序列化功能来支撑。
- protocal 作为项目的一个子模块，采用静态库编译。
- protocal 同样也需要可以完全独立到 `tools\protocol-editor\CMakeLists.txt` 中，作为一个独立的工具程序使用。
  - 提供完整的可视化界面
  - 独立作为可执行程序后，由于它是QMainWindow的主窗口，需要提供菜单栏以支撑文件的操作。
