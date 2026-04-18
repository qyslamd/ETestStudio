## 使用FlexRay通道

该通道支持以下操作：read_buff、read_buff_sync、read_msg、read_msg_sync、write_buff、write_buff_sync、write_msg、write_msg_sync、on_buff_recv、iocmd

### 简介
FlexRay是一种用于高速数据通信的串行总线协议，主要用于汽车电子系统中的实时通信。它由一对双绞线组成，支持高达10 Mbps的数据传输速率，并提供了高可靠性和实时性。

FlexRay通道具有以下特点：
1. 高速数据传输：FlexRay支持高达10 Mbps的数据传输速率，可以满足实时通信的需求，如车辆控制系统中的传感器数据和控制命令的传输。
2. 高可靠性：FlexRay使用差分信号传输和冗余通道，可以减少信号的干扰和噪声，提高通信的可靠性。同时，数据链路层提供了错误检测和纠正机制，可以及时发现和修复传输错误。
3. 实时性：FlexRay使用TDMA技术，将时间划分为若干个时间槽，每个时间槽都用于传输一个数据帧。这种时间划分方式可以确保数据的实时传输，避免数据冲突和延迟。
4. 灵活性：FlexRay支持静态分配和动态分配两种时间槽分配方式，可以根据系统需求进行灵活配置。同时，FlexRay还支持多个节点之间的同步通信，可以实现复杂的分布式控制系统


### FlexRay通道的属性配置
- baudrate：值类型为string, 波特率
- is_start: 值类型为boolean，启动节点，负责初始化和协调整个网络的启动过程，发送启动帧来启动网络，并负责分配和管理网络中的时间槽。还负责发送同步帧，用于同步整个网络的时钟
- is_sync: 值类型为boolean, 同步节点，在启动节点的指导下进行同步。会接收启动节点发送的同步帧，并根据同步帧中的时钟信息来同步自己的时钟。同步节点还负责发送数据帧来进行实时通信

### FlexRay通道的数据读写
**1.异步写原始数据**
> local w_res = write_buff(channel, buff, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - buff：EBuff类型, 数据缓存
    - option：dict类型, 可选参数选项
        - option.slot：number类型, 时隙号
- 返回值
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**2.同步写原始数据**
> local w_res = write_buff_sync(channel, buff, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - buff：EBuff类型, 数据缓存
    - option：dict类型, 可选参数选项
        - option.slot：number类型, 时隙号
- 返回值
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**3.异步写消息数据**        
> local w_res = write_msg(channel, prot, msg, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - prot：EProtocol类型, 协议对象
    - msg：dict类型, 报文数据
    - option：dict类型, 可选参数选项
        - option.slot：number类型, 时隙号
- 返回值：
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**4.同步写消息数据**
> local w_res = write_msg_sync(channel, prot, msg, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - prot：EProtocol类型, 协议对象
    - msg：dict类型, 报文数据
    - option：dict类型, 可选参数选项
        - option.slot：number类型, 时隙号
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

### FlexRay通道IO命令

**1.加入总线通信**
> iocmd(channel, "Start", option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - option：dict类型, FlexRay通道配置

**2.退出总线通信**
> iocmd(channel, "Stop", {})
- 输入参数：
	- channel：EChannel类型, 通信通道对象
    
