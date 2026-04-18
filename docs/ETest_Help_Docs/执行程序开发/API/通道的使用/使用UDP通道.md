## 使用UDP通道

该通道支持以下操作：read_buff、read_buff_sync、read_msg、read_msg_sync、write_buff、write_buff_sync、write_msg、write_msg_sync、on_buff_recv、iocmd

### 简介
UDP（User Datagram Protocol, 用户数据报协议）是轻量的、不可靠的、面向数据报（datagram）、无连接的协议, 它可以用于对可靠性要求不高的场合。与TCP通信不同, 两个程序之间进行UDP通信不需要预先建立持久的socket连接, UDP每次发送数据报都需要指定目的地址和端口

UDP消息传送的模式为三种单播、组播、广播
 
- 单播(unicast)模式：一个UDP客户端发出的数据报只发送到另一个指定地址和端口的UDP客户端,是一对一的数据传输
- 广播 (broadcast)模式：一个UDP客户端发出的数据报,在同一网络范围内其他所有的UDP客户端都可以收到。广播经常用于实现网络发现的协议。一般的广播址是255.255.255.255
- 组播( multicast)模式：也称为多播。UDP客户端加入到另一个组播IP地址指定的多播组, 成员向组播地址发送的数据报组内成员都可接收到, 类似于QQ群的功能。加入多播组后, UDP的收发与正常的UDP数据收发方法一样

### 通道的属性配置
- ip：值类型为string, ip地址
- port：值类型为number, 端口号
- ttl：值类型为number, 存活周期
- reuseaddr：值类型为boolean, 复用地址端口
- bind：绑定组播地址(Linux), 只在Linux设置

### UDP通道数据读写

**1.异步写原始数据**
> local w_res = write_buff(channel, buff, option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - buff：EBuff类型, 数据缓存
    - option：dict类型, 可选参数选项
        - option.to_ip：string类型, 目标ip地址
        - option.to_port：number类型, 目标端口号
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
        - option.to_ip：string类型, 目标ip地址
        - option.to_port：number类型, 目标端口号
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
        - option.to_ip：string类型, 目标ip地址
        - option.to_port：number类型, 目标端口号
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
        - option.to_ip：string类型, 目标ip地址
        - option.to_port：number类型, 目标端口号
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

### UDP通道IO命令
**1.发送广播**
> iocmd(channel, "BroadCast", option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
    - option：dict类型, 可选参数选项
        - option.to_port：number类型, 目标端口号
		- option.buff：EBuff类型, 数据缓存
- **注意：广播是指将报文发送到网络中的所有可能的接收者, 需要指定发送的端口号，Linux下接收广播指令IP地址要设置为0.0.0.0**

**2.加入组播**
> iocmd(channel, "JoinGroup", option)
- 输入参数：
	- channel：EChannel类型, 通信通道对象
    - option：dict类型, 可选参数选项
		- option.multicast_ip：string类型, 组播ip地址
- **注意：组播的IP地址IANA(互联网数字分配机构)把D类地址空间分配给多播使用, 范围从224.0.0.0到239.255.255.255组播传输作为IP数据传输的三种方式之一 , 是指接收者的数量和位置在源端主机不知道的情况下, 仅由源发出一份组播报文, 向目标组播IP地址与端口发送数据的过程**

**3.退出组播**
> iocmd(channel, "LeaveGroup", option)
- 输入参数：
    - channel：EChannel类型, 通信通道对象
	- option：dict类型, 可选参数选项
		- option.multicast_ip：string类型, 组播ip地址

**4.获取通道配置**
> local res = iocmd(channel, "GetConfig")
- 输入参数：
	- channel：EChannel类型, 通信通道对象
- 返回值：
	- res：返回值dict类型
		- res.ip：string类型, ip地址
		- res.port：number类型, 端口号

