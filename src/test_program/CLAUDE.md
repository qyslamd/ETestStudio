# test_program 模块规则

- test_program 作为测试程序编辑器模块。它主要负责测试用例的可视化编辑，包括测试套件信息、Setup/Teardown 前后置步骤、以及多个测试用例的步骤编辑。
- 数据模型（TestProgramData）定义了测试步骤、测试用例、测试套件的三层结构，并提供 JSON 序列化与反序列化功能，对应 `.etprog` 文件格式。
- 编辑器（TestProgramEditorWidget）实现 IEditor 接口，支持文件加载/保存、快照式撤销/重做、用例的增删和步骤编辑。
- test_program 作为项目的一个子模块，采用静态库编译。
- test_program 同样也需要可以完全独立到 `examples\testprogram-demo\CMakeLists.txt` 中，作为一个独立的示例程序使用。
  - 提供完整的可视化界面
  - 独立作为可执行程序后，由于它是 QMainWindow 的主窗口，需要提供菜单栏以支撑文件的操作。
