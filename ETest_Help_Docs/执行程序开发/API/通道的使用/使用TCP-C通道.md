## 使用TCP-C通道

该通道支持以下操作：read_buff、read_buff_sync、read_msg、read_msg_sync、write_buff、write_buff_sync、write_msg、write_msg_sync、on_buff_recv、iocmd

### 简介
TCP（传输控制协议）是一种可靠的、面向连接的协议, 它可以在网络上可靠地传输数据。TCP客户端是指使用TCP协议连接到服务器的应用程序或设备

TCP客户端通常由两部分组成：套接字和应用程序。套接字是一个网络连接的端点, 它包含了连接的IP地址和端口号。应用程序则是使用TCP协议进行通信的程序, 它可以向服务器发送数据并接收来自服务器的响应

TCP客户端的工作流程如下：

1. 创建一个套接字并指定连接的服务器的IP地址和端口号。
2. 连接到服务器并发送请求数据。
3. 等待服务器响应并接收来自服务器的数据。
4. 处理服务器响应并关闭连接。

### 通道的配置及属性
- ip：值类型为string, ip地址
- port：值类型为number, 端口号, 默认0, TCP-S端自动分配
- autoconnect：值类型为boolean, 自动启动连接服务器
- keepalive：值类型为boolean, 长连接
- nodelay：值类型为boolean, 禁用Nagle算法

### TCP-C通道数据读写

**1.异步写原始数据**
> local w_res = write_buff(channel, buff)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - buff：EBuff类型, 数据缓存
- 返回值
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**2.同步写原始数据**
> local w_res = write_buff_sync(channel, buff)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - buff：EBuff类型, 数据缓存
    - option：dict类型, 可选参数选项
- 返回值
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**3.异步写消息数据**        
> local w_res = write_msg(channel, prot, msg)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - prot：EProtocol类型, 协议对象
    - msg：dict类型, 报文数据
- 返回值：
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**4.同步写消息数据**
> local w_res = write_msg_sync(channel, prot, msg)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - prot：EProtocol类型, 协议对象
    - msg：dict类型, 报文数据
- 返回值：
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**5.异步读原始数据**
> local r_res = read_buff(channel, size, tout_ms)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - size：number类型, 需要读取的字节长度, 0表示全部读取
    - tout_ms：number类型, 超时时长（单位ms）, 超时返回nil
- 返回值：
    - r_res：dict类型
        - r_res.timestamp：number类型, 读取发生时的时间（ns）
        - r_res.size：number类型, 读取到的数据缓存字节长度
        - r_res.value：EBuff类型, 读取到的数据缓存结果

**6.同步读原始数据**
> local r_res = read_buff_sync(channel, size)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - size：number类型, 需要读取的字节长度, 0表示全部读取
- 返回值：
    - r_res：dict类型
        - r_res.timestamp：number类型, 读取发生时的时间（ns）
        - r_res.size：number类型, 读取到的数据缓存字节长度
        - r_res.value：EBuff类型, 读取到的数据缓存结果

**7.异步读消息数据**    
> local r_res = read_msg(channel, prot, tout_ms)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - prot：EProtocol类型, 协议对象
    - tout_ms：number类型, 超时时长（单位ms）, 超时返回nil
- 返回值：
    - r_res：dict类型
        - r_res.timestamp：number类型, 读取发生时的时间（ns）
        - r_res.size：number类型, 读取到的数据缓存字节长度
        - r_res.value：EBuff类型, 读取到的数据缓存结果

**8.同步读消息数据**
> local r_res = read_msg_sync(channel, prot)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - prot：EProtocol类型, 协议对象
- 返回值：
    - r_res：dict类型
        - r_res.timestamp：number类型, 读取发生时的时间（ns）
        - r_res.size：number类型, 读取到的数据缓存字节长度
        - r_res.value：EBuff类型, 读取到的数据缓存结果

**9.事件订阅**
> on_buff_recv(channel, fn)
- 输入参数：
	- channel：EChannel类型, 通信通道对象
	- fn：回调函数, 函数原型为fn(ch, res)
        - ch: EChannel类型, 通信通道对象
        - res: dict类型, 事件触发的消息
### TCP-C通道IO命令

**1.连接服务端**
> iocmd(channel, "Connect", option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - option：dict类型, 可选参数选项
        - option.to_ip：string类型, 目标IP地址
        - option.to_port：number类型, 目标端口

**2.断开连接**
> iocmd(channel, "DisConnect", {})
- 输入参数：
	- channel：EChannel类型, 通信通道对象
    
**3.判断是否连接**
> local b = iocmd(channel, "IsConnected", {})
- 输入参数：
	- channel：EChannel类型, 通信通道对象
- 返回值：
	- b：返回值boolean类型, true表示成功, false表示失败