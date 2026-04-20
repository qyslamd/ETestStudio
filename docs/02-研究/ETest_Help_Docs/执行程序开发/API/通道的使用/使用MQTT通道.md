## 使用MQTT通道

该通道支持以下操作：read_buff、read_buff_sync、read_msg、read_msg_sync、write_buff、write_buff_sync、write_msg、write_msg_sync、on_buff_recv、iocmd

### 简介
MQTT（Message Queuing Telemetry Transport）是一种轻量级的、开放的、基于发布/订阅模式的消息传输协议。它专门设计用于低带宽、高延迟或不可靠的网络环境中传输小型数据包,MQTT通道广泛应用于物联网、传感器网络、远程监控等领域

MQTT通道是基于MQTT协议建立的通信通道，它具有以下特点：
1. 轻量级：MQTT协议非常轻量，协议头部只有2个字节，非常适合在低带宽、高延迟的网络环境中使用
2. 发布/订阅模式：MQTT采用发布/订阅模式，消息发布者（Publisher）将消息发布到一个特定的主题（Topic）上，消息订阅者（Subscriber）可以通过订阅该主题来接收消息
3. 异步通信：MQTT通道是异步的，消息发布者和消息订阅者之间没有直接的连接，消息通过MQTT代理服务器进行转发。发布者发布消息后，不需要等待订阅者接收消息的确认，可以继续进行其他操作
4. 可靠性：MQTT提供三种消息质量等级：最多一次（At most once）、最少一次（At least once）和只有一次（Exactly once）。发布者可以根据需求选择适当的消息质量等级，保证消息的可靠性
5. 低功耗：MQTT协议非常适合在资源有限的设备上使用，它可以在低功耗的设备上运行，并且能够有效地利用网络带宽
6. 安全性：MQTT支持TLS/SSL加密，可以保证消息在传输过程中的安全性。同时，MQTT也支持基于用户名和密码的身份验证


### MQTT通道的属性配置
- server：值类型为string, 服务器的IP地址与端口号
- clientId: 值类型为字符串，客户端ID
- keepalive: 值类型为number, 活动保持时长单位为秒
- timeout：值类型为number，超时时长单位毫秒
- username：值类型为string，服务器用户名
- password：值类型为string，服务器密码

### MQTT通道的数据读写
**1.异步写原始数据**
> local w_res = write_buff(channel, buff, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - buff：EBuff类型, 数据缓存
    - option：dict类型
        - option.topic：string类型, 指定的主题名称
        - option.qos：number类型, 类别，0表示不保证设备收到，1表示保证设备至少收到一次，2表示每个设备只收到一次
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
    - option：dict类型
        - option.topic：string类型, 指定的主题名称
        - option.qos：number类型, 类别，0表示不保证设备收到，1表示保证设备至少收到一次，2表示每个设备只收到一次
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
    - option：dict类型
        - option.topic：string类型, 指定的主题名称
        - option.qos：number类型, 类别，0表示不保证设备收到，1表示保证设备至少收到一次，2表示每个设备只收到一次
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
    - option：dict类型
        - option.topic：string类型, 指定的主题名称
        - option.qos：number类型, 类别，0表示不保证设备收到，1表示保证设备至少收到一次，2表示每个设备只收到一次
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

### MQTT通道IO命令

**1.订阅MQTT主题**
> iocmd(channel, "subscribe", option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - option：dict类型
        - option.topic: string类型，订阅的主题
        - option.qos: number类型，0表示不保证设备收到，1表示保证设备至少收到一次，2表示每个设备只收到一次
- **注意：只有先订阅主题，才可以Read**

**2.取消订阅MQTT主题**
> iocmd(channel, "unsubscribe", option)
- 输入参数：
	- channel：EChannel类型, 通信通道对象
    - option：dict类型
        - option.topic: string类型，取消订阅的主题
    
