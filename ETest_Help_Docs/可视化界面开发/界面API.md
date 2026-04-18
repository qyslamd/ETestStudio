## 界面开发API
### 说明
以下所有API需要在rui界面的回调函数里应用。回调函数输入框不允许有回车换行。
- 回调函数格式：api => {}，API需要写到大括号内，如果有多条语句，需要用“;”隔开。
- 例：api => {let res = api.get_value("$.myNum");api.cmd("Getv", [res]);}

**1.向测试程序发送命令**
> api.cmd(cmd，params, dev_name)
- 输入参数：
    - cmd：string类型, 测试程序中全局函数名称
    - params：array类型，数组元素依次为函数的输入参数
    - dev_name: string类型，可选参数仿真设备名称字符串，只有一个仿真设备时可以缺省
- 示例
    ```lua
        api.cmd("send", ["$.a"，"b"], "dev_name")
        -- 向全局函数send中发送命令，参数为网络变量a的值与字符串b，对应的仿真设备名称dev_name
    ```

**2.重置网络变量的值**
> api.reset_value(key)
- 输入参数：
    - key：string类型, 绑定的网络变量的key值，若key缺省，重置所有网络变量
- 示例
    ```lua
        api.reset_value()
        --重置所有网络变量
        api.reset_value ("$.msg")
        --重置 key 值为msg的网络变量(适用于绑定的最底层节点)
    ```

**3.修改网络变量的值**
>api.set_value(key，value)
- 输入参数：
    - key：string类型, 绑定的网络变量的key值
    - value: any类型，修改的值
- 示例
    ```lua
        api.set_value("$.number", 20) 
        --把key为number的网络变量值设置为20
    ```
**4.获取网络变量的值**
> res = api.get_value(key)
- 输入参数：
    - key：string类型, 绑定的网络变量的key值
- 返回值：
    - res：any类型，网络变量key对应的值
- 示例
    ```lua
        --回调函数：
        api => {let res = api.get_value("$.myString");api.cmd("Getv", [res]);}
        --获取网络变量msg对应的值

        --lua文件：
        function Getv(res)
          print("getvalue=",res)
        end
    ```

**5.弹出对话框**
> api.show(page，option)
- 输入参数：
    - page：string类型, 项目路径下.rui文件的路径
    - option：dict类型
        - option.title：string类型，对话框标题
        - option.width：number类型，弹框宽度
        - onOk(){}: function类型，点击确定按钮要执行的操作
        - onCancel(){}: function类型，点击取消按钮要执行的操作
- 示例
    ```lua
        (api)=>{api.show("UI/show.rui", {title: "界面设置",width: 400,onOk(){ api.cmd("Show", ["number"])}, onCancel() {api.cmd("", [])} })}
        --需要事先有UI/show.rui文件
        --执行弹出对话框为UI/show.rui文件
    ```

**6.界面跳转**
> api.goto(page)
- 输入参数：
    - page：string类型, 项目路径下.rui文件的路径
    - run：string类型, 项目路径下.run文件的路径
- 注意：
    - 不相同执行配置（.run）文件实现跳转会退出当前，启动跳转界面对应的执行配置文件
- 示例
    ```lua
        api.goto("UI/page1.rui", "run/run1.run") 
        --跳转到名称为 page1的界面
    ```


**7.打开文本文件**
> api.open_txt().then((f) => {})
- 注意：
    - f为当前文本文件对象
    - f.value为文本文件内容
    - {}内可使用本章API
- 示例
    ```lua
        api.open_txt().then((re) => {console.log(re.value)})
        --点击组件弹出对话框，打开文本文件，在控制台打印内容
    ```
**8.打开文件对话框**
> api.open_dialog().then((f)=>{})
- 注意：
    - f为当前对象
    - f.value为文件路径
    - {}内可使用本章API
- 示例
    ```lua
        api.open_dialog().then((f)=>{console.log(f.value)})
        --点击组件弹出对话框，选择选择文件，并在控制台打印文件路径
    ```
**9.打开新窗口**
> api.open_win(page)
- 输入参数：
    - page：string类型, 新窗口要显示的UI界面。
    - run：string类型, 项目路径下.run文件的路径
- 注意：
  - 确保当前窗口( *.rui )和要打开窗口( *.rui )绑定同一个配置文件( *.run )。
  - 调试模式不支持，必须打包输出，然后开两个et_player.exe。
- 示例
    ```lua
         api.open_win("ui/page.rui", "run/run1.run")
         --打开ui下的page.rui文件
    ```




