## 使用CAN通道
该通道支持以下操作：read_buff、read_buff_sync、read_msg、read_msg_sync、write_buff、write_buff_sync、write_msg、write_msg_sync、on_buff_recv、iocmd
### 简介
CAN（Controller Area Network, 控制器局域网）是一种高可靠性、高实时性的串行通信总线, 主要用于汽车、工业控制、机器人等领域。CAN通道是指CAN总线上的一个通信通道, 用于连接CAN节点之间的数据交换。

CAN通道的特点：

1. 高可靠性：CAN通道采用差分信号传输, 能够抵抗电磁干扰和噪声干扰, 保证数据传输的可靠性
2. 高实时性：CAN通道采用非阻塞式传输方式, 能够实现实时数据传输, 满足高实时性的要求
3. 多节点连接：CAN通道支持多节点连接, 最多可连接127个节点, 能够满足大规模系统的数据交换需求
4. 灵活性：CAN通道支持多种数据传输方式, 如广播、单播、多播等, 能够满足不同应用场景的需求

### CAN通道的属性配置
- baudrate：波特率, 值类型为字符串, 默认值：500Kbps
- acc_code：过滤验收码, 值类型为number, 默认值：0x00000000
- acc_mask：过滤屏蔽码, 值类型为number, 默认值：0xFFFFFFFF
- format：帧格式, 值类型为字符串, 标准帧为 standard, 扩展帧为 extend

### CAN通道的数据读写

**1.异步写原始数据**
> local w_res = write_buff(channel, buff, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - buff：EBuff类型, 数据缓存
    - option：dict类型, 可选参数选项
        - option.id：number类型, CAN ID
        - option.remote: boolean类型，是否为远程帧（默认为true）
- 返回值：
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
        - option.id：number类型, CAN ID
        - option.remote: boolean类型，是否为远程帧（默认为true）
- 返回值：
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
        - option.id：number类型, CAN ID
        - option.remote: boolean类型，是否为远程帧（默认为true）
- 返回值：
    - w_res：dict类型
        - w_res.timestamp：number类型, 执行的时间（ns）
        - w_res.size：number类型, 输出的字节长度
        - w_res.value：EBuff类型, 输出值

**4.同步写消息数据**
>local w_res = write_msg_sync(channel, prot, msg, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - prot：EProtocol类型, 协议对象
    - msg：dict类型, 报文数据
    - option：dict类型, 可选参数选项
        - option.id：number类型, CAN ID
        - option.remote: boolean类型，是否为远程帧（默认为true）
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
        - res.value：EBuff类型, 读取到的数据缓存结果

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

### CAN通道IO命令
**1.设置通道参数**
> iocmd(channel, "SetConfig", option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - option：dict类型, 可选参数选项
        - option.baudrate：string类型, 波特率
        - option.acc_code：number类型, 过滤验收码
        - option.acc_mask：number类型, 过滤屏蔽码
        - option.format：string类型, 帧格式类型

**2.获取通道参数**
> local res = iocmd(channel, "GetConfig", {})
- 输入参数：
	- channel：EChannel类型, 通信通道对象
- 返回值：
	- res：dict类型, 可选参数选项
        - res.baudrate：string类型, 波特率
        - res.acc_code：number类型, 过滤验收码
        - res.acc_mask：number类型, 过滤屏蔽码
        - res.format：string类型, 帧格式类型
