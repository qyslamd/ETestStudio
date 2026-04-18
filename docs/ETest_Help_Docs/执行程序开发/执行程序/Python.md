
## Python简介

Python 是一个高层次的结合了解释性、编译性、互动性和面向对象的脚本语言。

Python 的设计具有很强的可读性，相比其他语言经常使用英文关键字，其他语言的一些标点符号，它具有比其他语言更有特色语法结构。
- Python版本：3.7.9
- Python 是一种解释型语言： 这意味着开发过程中没有了编译这个环节。类似于PHP和Perl语言。

- Python 是交互式语言： 这意味着，您可以在一个 Python 提示符 >>> 后直接执行代码。

- Python 是面向对象语言: 这意味着Python支持面向对象的风格或代码封装在对象的编程技术。

- Python 是初学者的语言：Python 对初级程序员而言，是一种伟大的语言，它支持广泛的应用程序开发，从简单的文字处理到 WWW 浏览器再到游戏。

### 下载及安装

- 在[Python官网](https://www.python.org/downloads/source/)下载自行安装，这里以3.7.9为例，适配的Python版本为3.7.9
### 基础语法
#### 脚本式编程

- 通过脚本参数调用解释器开始执行脚本，直到脚本执行完毕。当脚本执行完成后，解释器不再有效。

- 让我们写一个简单的 Python 脚本程序。所有 Python 文件将以 .py 为扩展名。将以下的源代码拷贝至 test.py 文件中
    >  &gt;&GT;&GT;   `print("hello, python")`

- 这里假设你已经设置了Python解释器PATH变量，使用以下命令执行程序
    >  &dollar;   `python test.py`

- 让我们尝试在linux下执行 Python 脚本。修改 test.py 文件，如下所示：
    ```python
        #实例
        #!/usr/bin/python
        print("hello, python")
    ```

- 这里假定python解释器在/usr/bin目录中，使用以下命令执行脚本：
    > &dollar; `chmod +x test.py     # 脚本文件添加可执行权限`
    >
    > &dollar; `./test.py`

    - 输出结果：
    >  `hello, python`

#### 标识符
- 在 Python 里，标识符由字母、数字、下划线组成。

- 在 Python 中，所有标识符可以包括英文、数字以及下划线(_)，但不能以数字开头。

- Python 中的标识符是区分大小写的。

- 以下划线开头的标识符是有特殊意义的。以单下划线开头 _foo 的代表不能直接访问的类属性，需通过类提供的接口进行访问，不能用 from xxx import * 而导入。

- 以双下划线开头的 __foo 代表类的私有成员，以双下划线开头和结尾的 __foo__ 代表 Python 里特殊方法专用的标识，如 __init__() 代表类的构造函数。

- Python 可以同一行显示多条语句，方法是用分号 ; 分开，如:
    > &gt;&GT;&GT; `print ('hello');print ('runoob');`
    >
    >`hello` 
    >
    >`runoob`

#### 保留字符
- 下面的列表显示了在Python中的保留字。这些保留字不能用作常量或变量，或任何其他标识符名称。

- 所有 Python 的关键字只包含小写字母


|||||||
|  ----  | ----  | ---- | ---- | ---- | ---- |
| and  | exec | not | assert | finally | or |
| break  | for | pass | class | from | print |
| continue  | global | raise | def | if | return |
|del  | import | try | elif | in | while |
| else  | is | with | except | lambda | yeild |

#### 行和缩进
- Python 的代码块不使用大括号 {} 来控制类，函数以及其他逻辑判断。python 最具特色的就是用缩进来写模块。

- 缩进的空白数量是可变的，但是所有代码块语句必须包含相同的缩进空白数量，这个必须严格执行。

- 以下实例缩进为四个空格:

    ```python
        #实例
        if True:
            print("True")
        else:
            print("False")
    ```

- 以下代码将会执行错误：
    ```python
        #!/usr/bin/python
        # -*- coding: UTF-8 -*-
        # 文件名：test.py
        if True:
            print("Answer")
            print("True")
        else:
            print("Answer")
         #没有严格缩进，在执行时会报错`
          print("False")
    ```

- 执行以上代码，会出现以下错误提醒：
  > <font color=#FF0000>  IndentationError: unindent does not match any outer indentation level</font>  

- 因此，在 Python 的代码块中必须使用相同数目的行首缩进空格数。

- 建议你在每个缩进层次使用 单个制表符 或 两个空格 或 四个空格 , 切记不能混用


#### 多行语句
- Python语句中一般以新行作为语句的结束符。

- 但是我们可以使用斜杠（ \）将一行的语句分为多行显示，如下所示：

    ```python
        total = item_one + \
                item_two + \
                item_three
    ```
- 语句中包含 [], {} 或 () 括号就不需要使用多行连接符。如下实例：
    ```python
        days = ['Monday', 'Tuesday', 'Wednesday',
                'Thursday', 'Friday']
    ```
#### 引号

- Python 可以使用引号( ' )、双引号( " )、三引号( ''' 或 """ ) 来表示字符串，引号的开始与结束必须是相同类型的。
- 其中三引号可以由多行组成，编写多行文本的快捷语法，常用于文档字符串，在文件的特定地点，被当做注释。
```python
    word = 'word'
    sentence = "这是一个句子。"
    paragraph = """这是一个段落。
    包含了多个语句"""
```

#### 注释

- python中单行注释采用 # 开头
- python 中多行注释使用三个单引号(''')或三个双引号(""")。

#### 空行

- 函数之间或类的方法之间用空行分隔，表示一段新的代码的开始。类和函数入口之间也用一行空行分隔，以突出函数入口的开始。

- 空行与代码缩进不同，空行并不是Python语法的一部分。书写时不插入空行，Python解释器运行也不会出错。但是空行的作用在于分隔两段不同功能或含义的代码，便于日后代码的维护或重构。

- 记住：空行也是程序代码的一部分。


### 变量赋值
变量存储在内存中的值，这就意味着在创建变量时会在内存中开辟一个空间。

基于变量的数据类型，解释器会分配指定内存，并决定什么数据可以被存储在内存中。

因此，变量可以指定不同的数据类型，这些变量可以存储整数，小数或字符。

#### 单个变量赋值

- Python 中的变量赋值不需要类型声明
- 每个变量在内存中创建，都包括变量的标识，名称和数据这些信息
- 每个变量在使用前都必须赋值，变量赋值以后该变量才会被创建
- 等号 = 用来给变量赋值。
- 等号 = 运算符左边是一个变量名，等号 = 运算符右边是存储在变量中的值。例如

```python
    counter = 100 # 赋值整型变量
    miles = 1000.0 # 浮点型
    name = "John" # 字符串
    
    print(counter)
    print(miles)
    print(name)

    #以上运行输出为：
    100
    1000.0
    John

```

#### 多个变量赋值

+ Python允许你同时为多个变量赋值。例如

```python
    a = b = c = 1
    # 以上实例，创建一个整型对象，值为1，三个变量被分配到相同的内存空间上

    #您也可以为多个对象指定多个变量,如下：

    a, b, c = 1, 2, "john"
    #以上实例，两个整型对象 1 和 2 分别分配给变量 a 和 b，字符串对象 "john" 分配给变量 c。
```

### 运算符

Python语言主要支持以下运算符：

- 算术运算符
- 比较（关系）运算符
- 赋值运算符
- 逻辑运算符
- 位运算符
- 成员运算符
- 身份运算符
- 运算符优先级

#### 算术运算符

以下假设变量： a = 10， b = 20

|运算符|描述|举例|
|-|-|-|
|+|	加 - 两个对象相加|	a + b 输出结果 30|
|-|	减 - 得到负数或是一个数减去另一个数|	a - b 输出结果 -10|
|*	|乘 - 两个数相乘或是返回一个被重复若干次的字符串|	a * b 输出结果 200|
|/|	除 - x除以y	|b / a 输出结果 2|
|%|	取模 - 返回除法的余数|	b % a 输出结果 0|
|**	|幂 - 返回x的y次幂	|a**b 为10的20次方， 输出结果 100000000000000000000|
|//	|取整除 - 返回商的整数部分（向下取整）	| 9//2 输出结果4，-9//2输出结果 -5|


#### 比较运算符

以下假设变量a为10，变量b为20：

|运算符	|描述	|举例|
|-|-|-|
|==|	等于 - 比较对象是否相等	|(a == b) 返回 False。|
|!=	|不等于 - 比较两个对象是否不相等|	(a != b) 返回 true.|
|>|	大于 - 返回x是否大于y	|(a > b) 返回 False。|
|<|	小于 - 返回x是否小于y。所有比较运算符返回1表示真，返回0表示假。这分别与特殊的变量True和False等价。	|(a < b) 返回 true。|
|>=|	大于等于 - 返回x是否大于等于y。|	(a >= b) 返回 False。|
|<=|	小于等于 - 返回x是否小于等于y。|	(a <= b) 返回 true。|


#### 赋值运算符

以下假设变量a为10，变量b为20：

|运算符|	描述|举例|
|-|-|-|
|=|	简单的赋值运算符|	c = a + b 将 a + b 的运算结果赋值为 c|
|+=	|加法赋值运算符|	c += a 等效于 c = c + a|
|-=|	减法赋值运算符|	c -= a 等效于 c = c - a|
|*=|	乘法赋值运算符|	c *= a 等效于 c = c * a|
|/=	|除法赋值运算符|	c /= a 等效于 c = c / a|
|%=	|取模赋值运算符|	c %= a 等效于 c = c % a|
|**=|	幂赋值运算符|	c **= a 等效于 c = c ** a|
|//=|	取整除赋值运算符|	c //= a 等效于 c = c // a|

#### 位运算符

假设变量 a 为 60(二进制：0011 1100)，b 为 13(二进制：0000 1101)

|运算符|	描述|举例|
|-|-|-|
|&|	按位与运算符：参与运算的两个值,如果两个相应位都为1,则该位的结果为1,否则为0|	(a & b) 输出结果 12 ，二进制解释： 0000 1100|
| &vert;|	按位或运算符：只要对应的二个二进位有一个为1时，结果位就为1。	|(a  &vert; b) 输出结果 61 ，二进制解释： 0011 1101|
|^|	按位异或运算符：当两对应的二进位相异时，结果为1	|(a ^ b) 输出结果 49 ，二进制解释： 0011 0001|
|~|	按位取反运算符：对数据的每个二进制位取反,即把1变为0,把0变为1 。~x 类似于 -x-1	|(~a ) 输出结果 -61 ，二进制解释： 1100 0011，在一个有符号二进制数的补码形式。|
|<<|左移动运算符：运算数的各二进位全部左移若干位，由 << 右边的数字指定了移动的位数，高位丢弃，低位补0。	|a << 2 输出结果 240 ，二进制解释： 1111 0000|
|>>|右移动运算符：把">>"左边的运算数的各二进位全部右移若干位，>> 右边的数字指定了移动的位数|	a >> 2 输出结果 15 ，二进制解释： 0000 1111|

#### 逻辑运算符

以下假设变量 a 为 10, b为 20:

|运算符|逻辑表达式|描述|举例|
|-|-|-|-|
|and|	x and y|	布尔"与" - 如果 x 为 False，x and y 返回 False，否则它返回 y 的计算值。	|(a and b) 返回 20。|
|or|	x or y|	布尔"或" - 如果 x 是非 0，它返回 x 的值，否则它返回 y 的计算值。	|(a or b) 返回 10。|
|not|	not x|	布尔"非" - 如果 x 为 True，返回 False 。如果 x 为 False，它返回 True。|	not(a and b) 返回 False|

#### 成员运算符

除了以上的一些运算符之外，Python还支持成员运算符，测试序列中包含了一系列的成员，包括字符串，列表或元组。

|运算符|	描述|举例|
|-|-|-|
|in|	如果在指定的序列中找到值返回 True，否则返回 False。|	x 在 y 序列中 , 如果 x 在 y 序列中返回 True。|
|not in|	如果在指定的序列中没有找到值返回 True，否则返回 False。	|x 不在 y 序列中 , 如果 x 不在 y 序列中返回 True|


#### 身份运算符

身份运算符用于比较两个对象的存储单元

|运算符|	描述|举例|
|-|-|-|
|is	|is 是判断两个标识符是不是引用自一个对象	|x is y, 类似 id(x) == id(y) , 如果引用的是同一个对象则返回 True，否则返回 False|
|is not|	is not 是判断两个标识符是不是引用自不同对象	|x is not y ， 类似 id(a) != id(b)。如果引用的不是同一个对象则返回结果 True，否则返回 False。|


- 注： `is 用于判断两个变量引用对象是否为同一个(同一块内存空间)， == 用于判断引用变量的值是否相等`

#### 运算符优先级

以下表格列出了从最高到最低优先级的所有运算符：

|运算符|举例|
|-|-|
|**	|指数 (最高优先级)|
|~ |	按位翻转|
|* &ensp;&ensp; /	&ensp;&ensp;% &ensp;&ensp;//|	乘，除，取模和取整除|
|+ &ensp;&ensp;-|	加法减法|
|>>&ensp;&ensp; <<|	右移，左移运算符|
|&	|位 'AND'|
|^ &vert;|	位运算符|
|<=&ensp;&ensp; <&ensp;&ensp; > &ensp;&ensp;>=|比较运算符|
|<>&ensp;&ensp; == &ensp;&ensp;!=	|等于运算符
|= &ensp;&ensp;%=&ensp;&ensp; /= &ensp;&ensp;//= &ensp;&ensp;-= &ensp;&ensp;+= &ensp;&ensp;*= &ensp;&ensp;**=	|赋值运算符|
|is &ensp;&ensp;is not	|身份运算符|
|in &ensp;&ensp;not in	|成员运算符|
|not&ensp;&ensp; and&ensp;&ensp; or	|逻辑运算符|


<!-- ## 标准数据类型

- Number（数字）
- String（字符串）
- List（列表）
- Tuple（元组）
- Set（集合）
- Dictionary（字典）
- Boolean （布尔）

    `不可变数据（4 个）：Number（数字）、String（字符串）、Tuple（元组） 、Boolean （布尔）`

    `可变数据（3 个）：List（列表）、Dictionary（字典）、Set（集合）` -->


### Number(数字)

- Python3 支持 int、float、complex（复数）
- 在Python3里，只有一种整数类型 int，表示为长整型，没有 python2 中的 Long。
- 像大多数语言一样，数值类型的赋值和计算都是很直观的。
- 内置的 type() 函数可以用来查询变量所指的对象类型。
- 此外还可以用 isinstance 来判断
- isinstance 和 type 的区别在于：
    - type()不会认为子类是一种父类类型。
    - isinstance()会认为子类是一种父类类型
    ```python
        class A:
            pass
         
        class B(A):
            pass
         
        isinstance(A(), A) # True

        type(A()) == A  #True 

        isinstance(B(), A) #True
        
        type(B()) == A #False
    ```

#### 数字类型转换

- int(x) 将x转换为一个整数。
- float(x) 将x转换到一个浮点数。
- complex(x) 将x转换到一个复数，实数部分为 x，虚数部分为 0。
- complex(x, y) 将 x 和 y 转换到一个复数，实数部分为 x，虚数部分为 y。x 和 y 是数字表达式。

#### 数学函数

|函数|返回值(描述)|
|-|-|
|abs(x)	|返回数字的绝对值，如abs(-10) 返回 10|
|ceil(x)|	返回数字的上入整数，如math.ceil(4.1) 返回 5|
|exp(x)	|返回e的x次幂(ex),如math.exp(1) 返回2.718281828459045|
|fabs(x)|	返回数字的绝对值，如math.fabs(-10) 返回10.0|
|floor(x)|	返回数字的下舍整数，如math.floor(4.9)返回 4|
|log(x)	|如math.log(math.e)返回1.0,math.log(100,10)返回2.0|
|log10(x)|	返回以10为基数的x的对数，如math.log10(100)返回 2.0|
|max(x1, x2,...)|	返回给定参数的最大值，参数可以为序列。|
|min(x1, x2,...)|	返回给定参数的最小值，参数可以为序列。|
|modf(x)|	返回x的整数部分与小数部分，两部分的数值符号与x相同，整数部分以浮点型表示。|
|pow(x, y)	|x**y 运算后的值。|
|round(x [,n])|	返回浮点数x的四舍五入值，如给出n值，则代表舍入到小数点后的位数。|
|sqrt(x)|	返回数字x的平方根。|

#### 随机数函数

Python包含以下常用随机数函数：

|函数|描述|
|-|-|
|choice(seq)	|从序列的元素中随机挑选一个元素，比如random.choice(range(10))，从0到9中随机挑选一个整数。|
|randrange ([start,] stop [,step])|	从指定范围内，按指定基数递增的集合中获取一个随机数，基数默认值为 1|
|random()	|随机生成下一个实数，它在[0,1)范围内。|
|seed([x])	|改变随机数生成器的种子seed。如果你不了解其原理，你不必特别去设定seed，Python会帮你选择seed。|
|shuffle(lst)|	将序列的所有元素随机排序|
|uniform(x, y)|	随机生成下一个实数，它在[x,y]范围内。|

#### 三角函数

|函数|描述|
|-|-|
|math.acos(x)	|返回x的反余弦弧度值。|
|math.asin(x)	|返回x的反正弦弧度值。|
|math.atan(x)	|返回x的反正切弧度值。|
|math.atan2(y, x)	|返回给定的 X 及 Y 坐标值的反正切值。|
|math.cos(x)	|返回x的弧度的余弦值。|
|math.hypot(x, y)	|返回欧几里德范数 sqrt(x*x + y*y)。|
|math.sin(x)	|返回的x弧度的正弦值。|
|math.tan(x)|	返回x弧度的正切值。|
|math.degrees(x)	|将弧度转换为角度,如degrees(math.pi/2) ， 返回90.0|
|math.radians(x)	|将角度转换为弧度|

#### 数字常量

|常量|描述|
|-|-|
|pi	|数学常量 pi（圆周率，一般以π来表示）|
|e	|数学常量 e，e即自然常数（自然常数）|

### Str(字符串)

字符串是由 Unicode 码位构成的不可变序列。 

字符串字面值有多种不同的写法，单引号、双引号、三引号


访问字符串，可以使用方括号 [] 来截取字符串,支持索引切片等操作

#### 字符串的转义


|符号  | 说明|
|-|-|
|&bsol;&bsol;	    |&bsol;	|
|&bsol;'	    |单引号|
|&bsol;"	    |双引号|
|\a	    |发出系统响铃声|
|\b	    |退格符|
|\n     |换行符|
|\t     |横向制表符|
|\v	    |纵向制表符|
|\r     |回车符|
|\f     |换页符|
|\o     |八进制数代表的字符|
|\x     |十六进制数代表的字符|
|\000   |终止符，\000后的字符串全部忽略|

#### 字符串运算符

下表实例变量 a 值为字符串 "Hello"，b 变量值为 "Python"：

|操作符|描述|举例|
|-|-|-|
|+|	字符串连接|	a + b 输出结果： HelloPython|
|* |	重复输出字符串|	a*2 输出结果：HelloHello|
|[]|	通过索引获取字符串中字符|	a[1] 输出结果 e|
|[ : ]|截取字符串中的一部分，遵循左闭右开原则，str[0:2] 是不包含第 3 个字符的|	a[1:4] 输出结果 ell|
|in|	成员运算符 - 如果字符串中包含给定的字符返回 True	|'H' in a 输出结果 True|
|not in	|成员运算符 - 如果字符串中不包含给定的字符返回 True	|'M' not in a 输出结果 True|
|r/R|	原始字符串 - 原始字符串：所有的字符串都是直接按照字面的意思来使用，没有转义特殊或不能打印的字符。 原始字符串除在字符串的第一个引号前加上字母 r（可以大小写）以外，与普通字符串有着几乎完全相同的语法。|print( r'\n') |

#### 字符串格式化


|符号|	描述|
|-|-|
|%c  |格式化字符及其ASCII码|
|%s	 |格式化字符串|
|%d	 |格式化整数|
|%u	 |格式化无符号整型|
|%o	 |格式化无符号八进制数|
|%x	 |格式化无符号十六进制数|
|%X	 |格式化无符号十六进制数（大写）|
|%f	 |格式化浮点数字，可指定小数点后的精度|
|%e	 |用科学计数法格式化浮点数|
|%E	 |作用同%e，用科学计数法格式化浮点数|
|%g	 |%f和%e的简写|
|%G	 |%f 和 %E 的简写|
|%p	 |用十六进制数格式化变量的地址|

#### 字符串内建函数

|方法|描述|
|-|-|
|str.capitalize()|将字符串的第一个字符转换为大写|
|str.center(width[, fillchar])|返回一个指定的宽度 width 居中的字符串，fillchar 为填充的字符，默认为空格|
|str.count(str, beg= 0,end=len(string))|返回 str 在 string 里面出现的次数，如果 beg 或者 end 指定则返回指定范围内 str 出现的次数|
|str.casefold()| 消除大小写的字符串可用于忽略大小写的匹配
|str.encode(encoding="utf-8", errors="strict")|返回原字符串编码为字节串对象的版本。 默认编码为 'utf-8'。 可以给出 errors 来设置不同的错误处理方案。|
|str.endswith(suffix, beg=0, end=len(string))|检查字符串是否以 obj 结束，如果beg 或者 end 指定则检查指定的范围内是否以 obj 结束，如果是，返回 True,否则返回 False|
|str.expandtabs(tabsize=8)|把字符串 string 中的 tab 符号转为空格，tab 符号默认的空格数是 8 |
|str.find(str, beg=0, end=len(string))|检测 str 是否包含在字符串中，如果指定范围 beg 和 end ，则检查是否包含在指定范围内，如果包含返回开始的索引值，否则返回-1|
|str.index(str, beg=0, end=len(string))|跟find()方法一样，只不过如果str不在字符串中会报一个异常|
|str.isalnum()|如果字符串至少有一个字符并且所有字符都是字母或数字则返 回 True，否则返回 False|
|str.isalpha()|如果字符串至少有一个字符并且所有字符都是字母或中文字则返回 True, 否则返回 False|
|str.isdigit()|如果字符串只包含数字则返回 True 否则返回 False|
|str.islower()|如果字符串中包含至少一个区分大小写的字符，并且所有这些(区分大小写的)字符都是小写，则返回 True，否则返回 False|
|str.isnumeric()|如果字符串中只包含数字字符，则返回 True，否则返回 False|
|str.isspace()|如果字符串中只包含空白，则返回 True，否则返回 False|
|str.istitle()|如果字符串是标题化的(见 title())则返回 True，否则返回 False|
|str.supper()|如果字符串中包含至少一个区分大小写的字符，并且所有这些(区分大小写的)字符都是大写，则返回 True，否则返回 False|
|str.join(seq)|以指定字符串作为分隔符，将 seq 中所有的元素(的字符串表示)合并为一个新的字符串|
|len(str)|返回字符串的长度|
|str.ljust(width[, fillchar])|返回一个原字符串左对齐,并使用 fillchar 填充至长度 width 的新字符串，fillchar 默认为空格。|
|str.lower()|转换字符串中所有大写字符为小写|
|str.lstrip()|截掉字符串左边的空格或指定字符|
|str.maketrans()|创建字符映射的转换表，对于接受两个参数的最简单的调用方式，第一个参数是字符串，表示需要转换的字符，第二个参数也是字符串表示转换的目标|
|max(str)|返回字符串 str 中最大的字母|
|min(str)|返回字符串 str 中最小的字母|
|str.replace(old, new[, max])|把将字符串中的 old 替换成 new,如果 max 指定，则替换不超过 max 次|
|str.rfind(str, beg=0,end=len(string))|类似于 find()函数，不过是从右边开始查找|
|str.rindex( str, beg=0, end=len(string))|类似于 index()，不过是从右边开始|
|str.rjust(width,[, fillchar])|返回一个原字符串右对齐,并使用fillchar(默认空格）填充至长度 width 的新字符串|
|str.rstrip()|删除字符串字符串末尾的空格|
|str.split(str="", num=string.count(str))|以 str 为分隔符截取字符串，如果 num 有指定值，则仅截取 num+1 个子字符串|
|str.splitlines([keepends])|按照行('\r', '\r\n', \n')分隔，返回一个包含各行作为元素的列表，如果参数 keepends 为 False，不包含换行符，如果为 True，则保留换行符|
|str.startswith(substr, beg=0,end=len(string))|检查字符串是否是以指定子字符串 substr 开头，是则返回 True，否则返回 False。如果beg 和 end 指定值，则在指定范围内检查。|
|str.strip([chars])|在字符串上执行 lstrip()和 rstrip()|
|str.swapcase()|将字符串中大写转换为小写，小写转换为大写|
|str.title()|返回"标题化"的字符串,就是说所有单词都是以大写开始，其余字母均为小写(见 istitle())|
|str.translate(table, deletechars="")|根据 str 给出的表(包含 256 个字符)转换 string 的字符, 要过滤掉的字符放到 deletechars 参数中|
|str.upper()|转换字符串中的小写字母为大写|
|str.zfill (width)|返回长度为 width 的字符串，原字符串右对齐，前面填充0|
|str.isdecimal()|检查字符串是否只包含十进制字符，如果是返回 true，否则返回 false|


### Bytes(字节, bytearray)

- 操作二进制数据的核心内置类型是 bytes 和 bytearray。 它们由 memoryview 提供支持，该对象使用 缓冲区协议 来访问其他二进制对象所在内存，不需要创建对象的副本。
- array 模块支持高效地存储基本数据类型，例如 32 位整数和 IEEE754 双精度浮点值
- 表示 bytes 字面值的语法与字符串字面值的大致相同，只是添加了一个 b 前缀
- bytes 字面值中只允许 ASCII 字符（无论源代码声明的编码为何）。 任何超出 127 的二进制值必须使用相应的转义序列形式加入 bytes 字面值
- 像字符串字面值一样，bytes 字面值也可以使用 r 前缀来禁用转义序列处理
- 虽然 bytes 字面值和表示法是基于 ASCII 文本的，但 bytes 对象的行为实际上更像是不可变的整数序列，序列中的每个值的大小被限制为 0 <= x < 256 (如果违反此限制将引发 ValueError）



#### Bytes基础

- 定义空字节，空字节前有个b来区分
- 直接定义字节的个数，就是创造几个空字节，但是每个字节里面都是空的
- 创造可迭代对象的元素个数相等的字节，然后把每个元素填充进去(注意，必须是整型int 的可迭代对象)

     ```python
        # 首先，表示 bytes 字面值的语法与字符串字面值的大致相同，只是添加了一个 b 前缀
        b'a'            #单引号: b'同样允许嵌入 "双" 引号'。
        b"a"            #双引号: b"同样允许嵌入 '单' 引号"。
        b"""a"""        #三重引号: b'''三重单引号''', b"""三重双引号"""

        a = bytes()     #定义空字节 a = b''
        a = bytes(3)    #定义3个空字节 a = b'\x00\x00\x00'，这是ASCII 0 ，不是阿拉伯数字0，阿拉伯数字0是十进制48，十六进制30.
        a = bytes(range(20))#通过由整数组成的可迭代对象

    ```

#### Bytes的基本用法

- bytes 和 str 一样，都是不可修改的类型，所以，所谓的修改，都是创造一个新的bytes 和str
- 二者的用法也类似,部分示例如下
    ```python
        b'abc'.replace(b'c',b'a')
        b'aba'
        # 替换字节

        b'abc'.find(b'b')
        1
        # 查找某个字节的索引号
    ```

#### bytes.fromhex('hexstr')

- 一串由十六进制数字组成的字符串，来表示字节
- 举例如下
    ```python
        bytes.fromhex('45 54 65 73 74') #输出 b'ETest'
        #注意，在括号内，空格是无效的，相当于没输入，所以想打出空格，就要打ASCII编码表里空格对应的编码，也就是20。
    ```

#### bytes.hex()

- 返回一个字符串对象，该对象包含实例中每个字节的两个十六进制数字
- 举例
    ```python
        'ETest'.encode().hex() # 输出 '4554657374'
        # 这里的'ETest'.encode() 等同于 b'ETest'
    ```

#### decode与encode

- decode的作用是将二进制数据解码成unicode编码，如str1.decode('utf-8'),表示将utf-8的编码字符串解码成unicode编码。
- encode的作用是将unicode编码的字符串编码成二进制数据，如str2.encode('utf-8'),表示将unicode编码的字符串编码成utf-8


#### bytearray

- 叫做字节数组，是可变的，有序的 
- b前缀，代表的是bytes 类型；而bytearray 类型，表示方法是bytearray(b'')
- bytearray 类型的操作，和bytes 类型的操作基本差不多
- 可以理解成这个bytearray 类型，是好多bytes 组合在一起。

#### bytearray的基本用法

- 基本用法同bytes
- 修改等操作同str


|名称|作用|
|-|-|
|bytearray()|空的字节数组|
|bytearray(int)|创造一个有int个字节的字节数组，所有字节都被十六进制的0x0填充，也就是被null 填充|
|bytearray(interatable_of_int)|把一个由int 组成的可迭代对象里的元素，依次取出，放在字节数组的字节中。返回值为bytearray(b'')|
|bytearray('string',encode)|把字符串string，按encode 指定的编码来解码，然后把解码之后的十六进制数字，放入字节中，组成字节数组|
|bytearray(bytes_or_buffer)|从一个字节序列或者是buffer中，复制一个新的bytearray|

#### 整型和字节序列的互相转化

- 如果无论字符串str 还是整型int ，在内存中都是以字节来储存，那么二者之间可以通过bytes 互相转化
- int.from_bytes(bytes,byteorder)表示把字符串通过解码，以整型int（十进制）表示出来
- int.to_bytes(length,byteorder)表示把一个整型的值，转换成字节, length 是指定有多少个字符，只能多不能少，多了话，多余的字符会被十六进制的0填充，少了的话会报错

```python
    int.from_bytes(b'python','big')             # 123666946355054
    int(123666946355054).to_bytes(6, 'big')     # b'python'
    # 后面的byteorder 是指大小端模式
```

### List(列表)

序列是 Python 中最基本的数据结构。

序列中的每个值都有对应的位置值，称之为索引，第一个索引是 0，第二个索引是 1，依此类推。

Python的序列的内置类型，最常见的是列表和元组。

列表都可以进行的操作包括索引，切片，加，乘，检查成员。

此外，Python 已经内置确定序列的长度以及确定最大和最小的元素的方法。

列表是最常用的 Python 数据类型，它可以作为一个方括号内的逗号分隔值出现。

#### 访问列表中的值

- 与字符串的索引一样，列表索引从 0 开始，第二个索引是 1，依此类推。
- 索引也可以从尾部开始，最后一个元素的索引为 -1，往前一位为 -2，以此类推
- 使用下标索引来访问列表中的值，同样你也可以使用方括号 [] 的形式截取字符

```python
    list1 = ['red', 'green', 'blue', 'yellow', 'white', 'black']

    print(list1[0])      #red
    print(list1[1])      #green
    print(list1[-1])     #black
    print(list1[-2])     #white
    print(list1[0:4])    #['red', 'green', 'blue', 'yellow']
    print(list1[1:-2])   #['green', 'blue', 'yellow']

```

#### 更新列表

- 可以对列表的数据项进行修改或更新，你也可以使用 append() 方法来添加列表项
- 在接下来的章节讨论append()方法的使用

```python
    list1 = ['Google', 'Runoob', 1997, 2000]

    print(list1[2])      # 1997
    list1[2] = 2001
    print(list1[2])      # 2001
    print(list1)         # ['Google', 'Runoob', 2001, 2000]
```

#### 删除列表元素

- 可以使用 del 语句来删除列表的的元素
- 在接下来的章节讨论 remove() 方法的使用

```python
    list1 = ['Google', 'Runoob', 1997, 2000]
    del list1[2]
    print(list1)  #  ['Google', 'Runoob', 2000]

```

#### 列表的函数及方法

假设 list1 = [1, 2, 2, 3]  list2 = [4, 5]

|函数&方法|作用|举例|
|-|-|-|
|len|获取列表元素的个数(长度)|len(list1) 输出4|
|max|获取列表元素的最大值|max(list1) 输出3|
|min|获取列表元素的最小值|min(list1) 输出1|
|sum|对列表元素进行求和|sum(list1) 输出8|
|list.append(obj)|在列表末尾添加新的对象|list2.append(6) list2输出[4, 5, 6]|
|list.count(obj)|统计某个元素在列表中出现的次数|list1.count(2) 输出2|
|list.extend(seq)|在列表末尾一次性追加另一个序列中的多个值（用新列表扩展原来的列表）|list1.extend(list2) list1输出 [1, 2, 2, 3, 4, 5]|
|list.index(obj)|从列表中找出某个值第一个匹配项的索引位置|list1.index(2) 输出1|
|list.insert(index, obj)|将对象插入列表,index表示要插入的索引位置|list2.insert(-1, 6) list2输出[4, 6, 5]|
|list.pop([index=-1])|移除列表中的一个元素（默认最后一个元素），并且返回该元素的值|list2.pop()输出5|
|list.remove(obj)|移除列表中某个值的第一个匹配项|list1.remove(2) list1输出[1, 2, 3]|
|list.reverse()|反向列表中元素|list2.reverse() list2输出 [5, 4]|
|list.sort( key=None, reverse=False)|对原列表进行排序,reverse=False升序，reverse=True降序|list2.sort(reverse=True) list2输出[5, 4]|
|list.clear()|清空列表|list2.clear() list2输出 []|
|list.copy()|复制列表|list2.copy()输出 [4, 5]|

### Tuple(元组)

Python 的元组与列表类似，不同之处在于元组的元素不能修改。

元组使用小括号 ( )，列表使用方括号 [ ]。

元组创建很简单，只需要在括号中添加元素，并使用逗号隔开即可

#### 元组的访问，删除以及索引截取同列表

```python
    tup1 = ('Google', 'Runoob', 1997, 2000)
    tup2 = (1, 2, 3, 4, 5, 6, 7)
    
    print(tup1[0])    # Google
    print(tup2[1:5])  # (2, 3, 4, 5)

    tup3 = tup1 + tup2
    print(tup3) # ('Google', 'Runoob', 1997, 2000, 1, 2, 3, 4, 5, 6, 7)
```

#### 元组的内置函数

|函数|作用|
|-|-|
|len|计算元组元素个数(长度)|
|max|返回元组中元素最大值|
|min|返回元组中元素最小值|
|tuple|将可迭代系列转换成元组|

### Dictionary(dict 字典)

字典是另一种可变容器模型，且可存储任意类型对象。

字典的每个键值 key=>value 对用冒号 : 分割，每个对之间用逗号(,)分割，整个字典包括在花括号 {} 中

键必须是唯一的，但值则不必。

值可以取任何数据类型，但键必须是不可变的，如字符串，数字。

#### 访问字典中的值

- 根据键取值
- 如果用字典里没有的键访问数据会抛出KeyError错误

```python
    dict1 = {'Name': 'Runoob', 'Age': 7, 'Class': 'First'}
    print(dict1['Age'])  # 7  
    # dict1['Age'] 等同于 dict1.get('Age') 
    print(dict1['ALi']) # 会抛出KeyError错误

```

#### 修改及删除

- 向字典添加新内容的方法是增加新的键/值对
- 能删单一的元素也能清空字典，清空只需一项操作
- 删除一个字典用del命令

```python
    dict1 = {'Name': 'Runoob', 'Age': 7, 'Class': 'First'}
    dict1['Age'] = 10
    dict1['count'] = "9"
    print(dict1) # {'Name': 'Runoob', 'Age': 10, 'Class': 'First', 'count': '9'}

    del dict1['Name']  # {'Age': 10, 'Class': 'First', 'count': '9'}
    dict1.clear() # {}
    del dict1
    # 执行del 操作后字典不再存在
```

#### 字典的特性

- 字典值可以是任何的 python 对象，既可以是标准的对象，也可以是用户定义的，但键不行
- 不允许同一个键出现两次。创建时如果同一个键被赋值两次，后一个值会被记住
- 键必须不可变，所以可以用数字，字符串或元组充当，而用列表就不行

```python
    dict1 = {'Name': 'Runoob', 'Age': 7, 'Name': 'tom'}
    dict1['Name']  # tom

    dict2 = {['name']: 'tom'}
    dict2[['name']]  # TypeError: unhashable type: 'list'

    a = dict(one=1, two=2, three=3)
    b = {'one': 1, 'two': 2, 'three': 3}
    c = dict(zip(['one', 'two', 'three'], [1, 2, 3]))
    d = dict([('two', 2), ('one', 1), ('three', 3)])
    e = dict({'three': 3, 'one': 1, 'two': 2})
    a == b == c == d == e # 返回 True

```

#### 字典函数及方法

假设 d 为某一个字典对象

|函数|作用|
|-|-|
|list(d)|返回字典d中使用的所有键的列表|
|len(d)|返回字典d中的项目数|
|d[key]|返回字典d中key所对应的value值，若不存在，引发KeyError|
|d[key] = value|设置字典d中key的值为value|
|key in d|如果d有一个键，则返回True，否则返回False|
|key not in d|等同于not key in d|
|iter(d)|在字典的键上返回一个迭代器|
|d.clear()|从字典中删除所有项目|
|d.copy()|返回字典的浅表副本|
|d.fromkeys(iterable[, value])|是一个返回新字典的类方法。值 默认为None|
|d.get(key[, default]) |如果key在字典中，则返回key的值，否则返回default|
|d.items()|返回字典d的新视图|
|d.keys()|返回字典键的新视图|
|d.values()|返回字典值的新视图|
|d.pop(key[, default)|如果key在字典中，请删除它并返回其值，否则返回 default|
|d.popitem()|从字典中删除并返回一个键值对。按先进后出顺序返回|
|d.setdefault(key[, default])|如果key在字典中，则返回其值。如果不是，请插入具有default值的key并返回default,默认是None|

```python
    # dict 的循环遍历
    dict1 = {"name": "Tom", "age": 14, "class": ["1班", "2班"]}

    #遍历key，value
    for k, v in dict1.items():
        print(k, v)
    # 以上输出
    #name Tom
    # age 14
    # class ['1班', '2班']

    #遍历key
    for k in dict1.keys():
        print(k)
    # 以上输出
    #name
    # age
    # class

    #遍历value
    for v in dict1.values():
        print(v)
    # 以上输出
    # Tom
    # 14
    # ['1班', '2班']
```

### Set(集合, frozenset)

目前有两种内置集合类型，set 和 frozenset

set 类型是可变的 --- 其内容可以使用 add() 和 remove() 这样的方法来改变。 由于是可变类型，它没有哈希值，且不能被用作字典的键或其他集合的元素

frozenset 类型是不可变并且为 hashable --- 其内容在被创建后不能再改变；因此它可以被用作字典的键或其他集合的元素

除了可以使用 set 构造器，非空的 set (不是 frozenset) 还可以通过将以逗号分隔的元素列表包含于花括号之内来创建

#### 集合的基本操作

- 添加元素 
- 移除元素
- 计算集合元素个数
- 清空集合
- 判断元素是否在集合中

```python
    s = set()
    s.add("x")      # 将字符串x添加到集合 s 中，如果元素已存在，则不进行任何操作。
    s.update(["a", "b"])
    s.update(("c", "d"))
    print(s)        # {'c', 'd', 'x', 'a', 'b'}
    s.remove("x")   # 移除元素, 如果元素不存在抛出KerError异常
    s.discard("x")  # 移除元素, 如果元素不存在不会抛出异常
    s.pop()         # 随机删除集合中的一个元素
    len(s)          # 获取集合的元素个数
    s.clear()       # 清空集合

```

#### 集合常用的函数及方法

假设s为某一集合对象

|函数|作用|
|-|-|
|s.add(obj)|为集合添加元素|
|s.clear()	|移除集合中的所有元素|
|s.copy()|拷贝一个集合|
|s.difference(set|返回多个集合的差集|
|s.discard(obj)|删除集合中指定的元素|
|len(s)|返回集合 s 中的元素数量|
|x in s|检测 x 是否为 s 中的成员|
|x not in s|检测 x 是否非 s 中的成员|
|s.isdisjoint(other)|如果集合中没有与 other 共有的元素则返回 True,否则返回False|
|s.issubset(other)|s <= other检测是否集合中的每个元素都在 other 之中,是返回 True,否则返回False|
|s.issuperset(other)|et >= other检测是否 other 中的每个元素都在集合之中,是返回 True,否则返回False|


### Boolean(布尔类型)

- Python中的布尔值使用常量True和False来表示，注意大小写
- 比较运算符<&ensp; >&ensp; == 等返回的类型就是bool类型；布尔类型通常在 if 和 while 语句中应用
- bool是int的子类（继承int），故 True==1  False==0 是会返回True的
- 数字0  ---------- False；
- None ---------- False;   None是真空；
- null （包括空字符串、空列表、空元组....） --------- False；
- 除了以上的，其他的表达式均会被判定为 True，这个需要注意，与其他的语言有比较大的不同

### 条件控制

- 条件语句是通过一条或多条语句的执行结果（True 或者 False）来决定执行的代码块
- 每个条件后面要使用冒号 :，表示接下来是满足条件后要执行的语句块
- 使用缩进来划分语句块，相同缩进数的语句在一起组成一个语句块
- 在嵌套 if 语句中，可以把 if...elif...else 结构放在另外一个 if...elif...else 结构中

```python
    num = int(input("输入一个数字："))
    if 0 < num < 10:
        if num > 5 :
            pass
        elif num > 6:
            print()
        else:
            pass
    else:
        if num % 3 == 0:
            pass
        else:
            pass

```


### 循环语句

循环语句有 for 和 while

同样需要注意冒号和缩进

break 语句可以跳出 for 和 while 的循环体。如果你从 for 或 while 循环中终止，任何对应的循环 else 块将不执行。

continue 语句被用来告诉 Python 跳过当前循环块中的剩余语句，然后继续进行下一轮循环

pass是空语句，是为了保持程序结构的完整性

#### while循环

- 我们可以通过设置条件表达式永远不为 false 来实现无限循环
- while 循环使用 else 语句

```python
    #使用了 while 来计算 1 到 100 的总和
    n = 100
    sum_t = 0
    counter = 1
    while counter <= n:
        sum_t = sum_t + counter
        counter += 1
    print("1 到 %d 之和为: %d" % (n,sum_t))


    #在 while … else 在条件语句为 false 时执行 else 的语句块，循环输出数字，并判断大小
    count = 0
    while count < 5:
        print (count, " 小于 5")
        count = count + 1
    else:
        print (count, " 大于或等于 5")


    #无限循环，你可以使用 CTRL+C 来退出当前的无限循环
    var = 1
    while var == 1 :  # 表达式永远为 true
        num = int(input("输入一个数字  :"))
        print ("你输入的数字是: ", num)
```

#### for循环

- for循环可以遍历任何序列的项目，如一个列表或者一个字符串
- 如果你需要遍历数字序列，可以使用内置range()函数

```python

    for letter in 'Runoob':     
        if letter == 'o':       # 字母为 o 时跳过输出
            continue
        print('当前字母 :', letter)

     
    for letter in 'Runoob':     
        if letter == 'b':       # 字母为 b 时终止输出
            break
        else:
            pass
        print ('当前字母为 :', letter)

```

### 异步(asyncio)

asyncio 是用来编写 并发 代码的库，使用 async/await 语法。

asyncio 被用作多个提供高性能 Python 异步框架的基础，包括网络和网站服务，数据库连接库，分布式任务队列等等。

asyncio 往往是构建 IO 密集型和高层级 结构化 网络代码的最佳选择

以下主要介绍 async await task的用法

#### 并发、并行、同步和异步

- 并发指的是 一个 CPU 同时处理多个程序，但是在同一时间点只会处理其中一个。并发的核心是：程序切换。
- 并行指的是多个 CPU 同时处理多个程序，同一时间点可以处理多个
- 同步：执行 IO 操作时，必须等待执行完成才得到返回结果
- 异步：执行 IO 操作时，不必等待执行就能得到返回结果

#### 协程、线程和进程

- 多进程通常利用的是多核 CPU 的优势，同时执行多个计算任务。每个进程有自己独立的内存管理，所以不同进程之间要进行数据通信比较麻烦
- 多线程是在一个 cpu 上创建多个子任务，当某一个子任务休息的时候其他任务接着执行。多线程的控制是由 python 自己控制的。 子线程之间的内存是共享的，并不需要额外的数据通信机制。但是线程存在数据同步问题，所以要有锁机制
- 协程的实现是在一个线程内实现的，相当于流水线作业。由于线程切换的消耗比较大，所以对于并发编程，可以优先使用协程


### 协程的基础使用

#### async task await

- 在普通的函数前面加 async 关键字,声明为异步函数；
- await 表示在这个地方等待子函数执行完成，再往下执行。（在并发操作中，把程序控制权交给主程序，让它分配其他协程执行。） await 只能在带有 async 关键字的函数中运行。task表示任务
- 如果一个对象可以在 await 语句中使用，那么它就是 可等待 对象
- 可等待 对象有三种主要类型: 协程, 任务 和 Future
- asynico.run()表示启动执行任务
- 举例说明如下

```python
    import asyncio
    import time

    async def visit_url(url, response_time):
        """访问 url"""
        await asyncio.sleep(response_time)
        return f"访问{url}, 已得到返回结果"

    start_time = time.perf_counter()
    task = visit_url('https://www.baidu.com', 2)
    asyncio.run(task)
    print(f"消耗时间：{time.perf_counter() - start_time}")
    #  这个程序消耗时间 2s 左右。
```

#### 增加协程

- 以上示例中添加一个任务举例如下
- 这 2 个程序一共消耗 5s 左右的时间。并没有发挥并发执行的优势
- 如果启动并发，这个程序只需要消耗 3s左右,也就是task2的等待时间

```python
    import asyncio
    import time


    async def visit_url(url, response_time):
        """访问 url"""
        await asyncio.sleep(response_time)
        return f"访问{url}, 已得到返回结果"


    async def run_task():
        """收集子任务"""
        task = visit_url('https://www.baidu.com', 2)
        task_2 = visit_url('https://www.taobao.com', 3)
        await asyncio.gather(task)
        await asyncio.gather(task_2)

    start_time = time.perf_counter()
    asyncio.run(run_task())
    print(f"消耗时间：{time.perf_counter() - start_time}")

    #以上代码消耗的时间为5秒左右

```

#### 并发执行

- 继续修改以上示例,启动并发执行
- 创建子任务还可以使用 asyncio.create_task 进行创建

```python

    import asyncio
    import time

    async def visit_url(url, response_time):
        """访问 url"""
        await asyncio.sleep(response_time)
        return f"访问{url}, 已得到返回结果"


    async def run_task():
        coro = visit_url('https://www.baidu.com', 2)
        coro_2 = visit_url('https://www.taobao.com', 3)

        task1 = asyncio.create_task(coro)
        task2 = asyncio.create_task(coro_2)

        await task1
        await task2

    start_time = time.perf_counter()
    asyncio.run(run_task())
    print(f"消耗时间：{time.perf_counter() - start_time}")

    #以上代码消耗的时间为3秒左右
```


### 函数

函数是组织好的，可重复使用的，用来实现单一，或相关联功能的代码段。

函数能提高应用的模块性，和代码的重复利用率

#### 函数的定义

- 函数代码块以 def 关键词开头，后接函数标识符名称和圆括号 ()。
- 任何传入参数和自变量必须放在圆括号中间，圆括号之间可以用于定义参数。
- 函数的第一行语句可以选择性地使用文档字符串—用于存放函数说明。
- 函数内容以冒号 : 起始，并且缩进。
- return [表达式] 结束函数，选择性地返回一个值给调用方，不带表达式的 return 相当于返回 None 

```python
    # 比较两个数，并返回较大的数
    def max(a, b):
        if a > b:
            return a
        else:
            return b
    
    a = 4
    b = 5
    print(max(a, b))

    #以上输出5
```

#### 函数的调用

- 定义一个函数：给了函数一个名称，指定了函数里包含的参数，和代码块结构。
- 这个函数的基本结构完成以后，你可以通过另一个函数调用执行，也可以直接从 Python 命令提示符执行
- 函数存在多个返回值, 可定义多个变量进行接收

```python
    # 定义函数
    def printme(str_me):
        # 打印任何传入的字符串
        print (str_me)
        return
    
    # 调用函数
    printme("我要调用用户自定义函数!")
    printme("再次调用同一函数")



    #多返回值的函数的调用与接收
    def add_x(a,b):
        x = a + b
        y = a - b
        return x, y
    
    sub1, sub2 = add_x(3, 4)
    print(sub1, sub2) # 7， -1
    

```

#### 参数

- 必需参数
    - 必需参数须以正确的顺序传入函数。调用时的数量必须和声明时的一样
- 关键字参数
    - 关键字参数和函数调用关系紧密，函数调用使用关键字参数来确定传入的参数值
    - 使用关键字参数允许函数调用时参数的顺序与声明时不一致，因为 Python 解释器能够用参数名匹配参数值
- 默认参数
    - 调用函数时，如果没有传递参数，则会使用默认参数
- 不定长参数
    - 你可能需要一个函数能处理比当初声明时更多的参数
    - 加了星号 * 的参数会以元组(tuple)的形式导入，存放所有未命名的变量参数
    - 加了两个星号 ** 的参数会以字典的形式导入

```python
    #必须参数
    def printme(stri):
        #"打印任何传入的字符串"
        print (stri)
        return
    # 调用 printme 函数，不加参数会报错
    printme()


    #关键字参数------
    def printme(stri):
        #"打印任何传入的字符串"
        print (stri)
        return
    #调用printme函数
    printme(stri = "ETest")


    #默认参数
    def printinfo(name, age = 35):
        #"打印任何传入的字符串"
        print ("名字: ", name)
        print ("年龄: ", age)
        return
    #调用printinfo函数
    printinfo( age=50, name="runoob" )
    print ("------------------------")
    printinfo( name="runoob" )
    # 以上代码输出
    # 名字:  runoob
    # 年龄:  50
    # ------------------------
    # 名字:  runoob
    # 年龄:  35



    #不定长参数
    def printinfo( arg1, **vardict ):
        # "打印任何传入的参数"
        print ("输出: ")
        print (arg1)
        print (vardict)
    # 调用printinfo 函数
    printinfo(1, a=2,b=3)
    #以上代码输出
    # 输出:
    # 1
    # {'a': 2, 'b': 3}

```

#### 匿名函数 

- python 使用 lambda 来创建匿名函数
- 所谓匿名，意即不再使用 def 语句这样标准的形式定义一个函数
- lambda 只是一个表达式，函数体比 def 简单很多
- lambda的主体是一个表达式，而不是一个代码块。仅仅能在lambda表达式中封装有限的逻辑进去
- lambda 函数拥有自己的命名空间，且不能访问自己参数列表之外或全局命名空间里的参数
- 虽然lambda函数看起来只能写一行，却不等同于C或C++的内联函数，后者的目的是调用小函数时不占用栈内存从而增加运行效率

```python
    # 可写函数说明
    sum_l = lambda arg1, arg2: arg1 + arg2
    
    print ("相加后的值为 : ", sum_l( 10, 20 ))
    print ("相加后的值为 : ", sum_l( 20, 20 ))
    # 以上输出结果：
    # 相加后的值为 :  30
    # 相加后的值为 :  40

```

#### return语句

- return [表达式] 语句用于退出函数，选择性地向调用方返回一个表达式
- 不带参数值的return语句返回None

```python
    def sum_( arg1, arg2 ):
        # 返回2个参数的和."
        total = arg1 + arg2
        print ("函数内 : ", total)
        return total
        
    # 调用sum函数
    total = sum_( 10, 20 )
    print ("函数外 : ", total)
```

### 模块

模块是一个包含所有你定义的函数和变量的文件，其后缀名是.py

模块可以被别的程序引入，以使用该模块中的函数等功能

这也是使用 python 标准库的方法

#### import语句

- 当解释器遇到 import 语句，如果模块在当前的搜索路径就会被导入。
- 搜索路径是一个解释器会先进行搜索的所有目录的列表。如想要导入模块 time，需要把命令放在脚本的顶端

```python
    
    import time

    def math_sum(s):
        # 延时一段时间
        time.sleep(s)
        print('延时时间 %d s' % s)
    math_sum(3)


```

#### from … import 语句

- from 语句让你从模块中导入一个指定的部分到当前命名空间中
- *表示把一个模块的所有内容全都导入到当前的命名空间

```python
    from time import localtime
    print(localtime())

    #以上输出
    #time.struct_time(tm_year=2021, tm_mon=3, tm_mday=3, tm_hour=17, tm_min=31, tm_sec=39, tm_wday=2, tm_yday=62, tm_isdst=0

```

### 全局函数 
|函数名|作用|
|-|-|
|abs()|求数值的绝对值，如果是复数则返回其模|
|delattr()| 删除对象中一个实例属性|
|hash()|返回对象的hash值 依据主机位宽截取，分32位和64位|
|memoryview()|返回给定参数的内存查看对象(memory view)|
|set()|集合，将iterable对象中元素依次添加到集合中，集合天生无序和去重|
|all()|可迭代对象所有元素为True则返回True，否则返回False|
|dict()|生成一个新的字典对象|
|help()|获取方法名和方法名下面的注释|
|min()|获取可迭代对象最小项元素|
|setattr()|往对象中添加属性和方法|
|any()　|　可迭代对象所有元素为False则返回False，否则返回True|
|dir()|没有参数返回当前作用域变量和方法列表，添加对象则返回对象中变量和方法列表|
|hex()|将整数转换为16进制|
|next()|获取迭代器下一个值，没有则触发StopIteration错误，也可以传递一个默认值，迭代耗尽时返回默认值，|
|slice()|返回切片范围对象， start, end, step，不写默认None|
|ascii()|以ascii码依据转换为字符串，非ascii如UTF-8字符则有\u前缀进行转义|
|divmod()|非复数整数，a//b a%b， 返回整除整数和余数|
|id()|获取对象内存地址，10进制|
|object()|返回一个没有特征的新对象。`object` 是所有类的基类。|
|sorted()|将iterable对象进行排序|
|bin() |整数转换为二进制字符串，遵守Python协议 __index__返回整数|
|enumerate()|返回一个枚举对象，可以指定start指定枚举计数，默认为0|
|input()|获取终端标准输入，自动去除末尾换行符，返回的是字符串|
|oct()|将整数转换为8进制|
|staticmethod()|返回函数的静态方法|
|bool()|对值进行布尔运算，一般 None、空字符串、空列表、空元组、空集合、空字典、0等空元素和空数据结构为False，其他为True|
|eval()|运行字符串代码，不更改源码逻辑，可完成数学运算|
|int()|默认10进制，将其他对象转换为10进制，base指定字符的进制，无对象则为0，|
|open()|用于打开一个文件，并返回文件对象，在对文件进行处理过程都需要使用到这个函数，如果该文件无法被打开，会抛出 OSError|
|str()|把对象转换为字符str类型|
|breakpoint()|来设置断点(前提是未定义 PYTHONBREAKPOINT环境变量，或该环境变量的值等于空字符串)。|
|exec()|运行字符代码，改变源码逻辑|
|isinstance()|判断对象是否是某种或多个类型，判断对象是否继承某个类|
|ord()|返回Unicode字符对应的数字|
|sum()|序列求和，整数序列|
|bytearray()|返回新的二进制数组，和list数据结构类似，拥有序列大多数方法，这个是存放二进制数据，添加数据需要输入整数参数范围为(0, 256)|
|filter()|依次取出iterable中元素交给一个函数，取返回True的元素|
|issubclass()|判断类是否是某个类的子类|
|pow()|返回 x * y 或 返回 x * y % z的值|
|super()|调用父类方法，本质上是寻找.__mro__下一个类中方法|
|bytes()|返回一个二进制不可变对象|
|float()|把字符串转换为float数据|
|iter()|返回迭代器对象|
|print()|打印输出到终端或者到文件|
|tuple()|元组，不可变序列类型|
|callable()|判断对象是否可调用|
|format()|格式化字符串|
|len()|获取对象长度|
|property()| 在新式类中返回属性值|
|type()|返回对象是由什么类型构建的|
|chr()|返回数字对应的Unicode字符|
|frozenset()|返回一个冻结的集合，冻结后集合不能再添加或删除任何元素|
|list()|Python中列表，可以将Iterable转换为列表|
|range()|不可变数据序列，有三个参数 start, stop, step|
|vars()|获取对象(模块、类、实例、字典等)具有__dict__属性的字典，对象 __dict__另外一种实现方式|
|classmethod()|修饰符对应的函数不需要实例化，不需要 self 参数，但第一个参数需要是表示自身类的 cls 参数，可以来调用类的属性，类的方法，实例化对象等|
|getattr() |通过字符串获取对象属性和方法值，一般联合hasattr使用|
|locals()|会以字典类型返回当前位置的全部局部变量|
|reper()|返回包含一个对象的可打印表示形式的字符串。对于大多数的类型，eval(repr(obj)) == obj|
|zip()|依次取出可迭代对象中元素组成新的元组，返回一个迭代器|
|compile()|将一个字符串编译为字节代码|
|globals()|当前模块的全局变量字典|
|map()|将可迭代对象依次传入函数，返回可迭代对象|
|reversed()|返回给定序列值的反向迭代器|
|complex()| 返回复数， 分real和imag两个部分，通过+来连接real和imag，j标识imag部分|
|hasattr()|判断对象中是否有对应字符串的属性和方法|
|max()|获取可迭代对象最大项元素|
|round()|四舍五入保留多少小数位|








