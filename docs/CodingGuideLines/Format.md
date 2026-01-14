# 格式
### 1. 行长度
- 每一行的字符数不超过90
- 头文件`include`可以忽视这条规则

### 2. 非ASCILL字符
- 尽量不要使用非ASCILL字符，使用`utf-8`编码

### 3. 空格和缩进
- 每次缩进4个空格

### 4. 函数声明与定义
- 返回类型和函数名在同一行, 参数也尽量放在同一行, 如果放不下就对形参分行, 分行方式与 函数调用 一致.
``` C++
ReturnType ClassName::FunctionName(Type par_name1, Type par_name2) {
  DoSomething();
  ...
}

ReturnType ClassName::ReallyLongFunctionName(Type par_name1, Type par_name2,
                                             Type par_name3) {
  DoSomething();
  ...
}

ReturnType LongClassName::ReallyReallyReallyLongFunctionName(
    Type par_name1,  // 4 space indent
    Type par_name2,
    Type par_name3) {
  DoSomething();  // 2 space indent
  ...
}
```
- 做法
```
使用好的参数名.

只有在参数未被使用或者其用途非常明显时, 才能省略参数名.

如果返回类型和函数名在一行放不下, 分行.

如果返回类型与函数声明或定义分行了, 不要缩进.

左圆括号总是和函数名在同一行.

函数名和左圆括号间永远没有空格.

圆括号与参数间没有空格.

左大括号总在最后一个参数同一行的末尾处, 不另起新行.

右大括号总是单独位于函数最后一行, 或者与左大括号同一行.

右圆括号和左大括号间总是有一个空格.

所有形参应尽可能对齐.
```

### 5. Lambda表达式
- Lambda 表达式对形参和函数体的格式化和其他函数一致; 捕获列表同理, 表项用逗号隔开.

### 6. 函数调用
- 要么一行写完函数调用, 要么在圆括号里对参数分行, 要么参数另起一行且缩进四格. 如果没有其它顾虑的话, 尽可能精简行数, 比如把多个参数适当地放在同一行里.

应该遵循如下形式
```C++
bool retval = DoSomething(argument1, argument2, argument3);
```
如果同一行放不下, 可断为多行, 后面每一行都和第一个实参对齐, 左圆括号后和右圆括号前不要留空格：
```C++
bool retval = DoSomething(averyveryveryverylongargument1,
                          argument2, argument3);
```

参数也可以放在次行, 缩进四格
```c++
if (...) {
  ...
  ...
  if (...) {
    DoSomething(
        argument1, argument2,  // 4 空格缩进
        argument3, argument4);
  }
```

如果一些参数本身就是略复杂的表达式, 且降低了可读性, 那么可以直接创建临时变量描述该表达式, 并传递给函数：
```C++
int my_heuristic = scores[x] * y + bases[x];
bool retval = DoSomething(my_heuristic, x, y, z);
```

或者放着不管, 补充上注释：
```C++
bool retval = DoSomething(scores[x] * y + bases[x],  // Score heuristic.
                          x, y, z);
```

如果一系列参数本身就有一定的结构, 可以酌情地按其结构来决定参数格式：
```C++
my_widget.Transform(x1, x2, x3,
                    y1, y2, y3,
                    z1, z2, z3);
```

### 7. 条件语句
- 保持风格一致性即可
```C++
if (condition) {  // 圆括号里没有空格.
  ...  // 2 空格缩进.
} 
else if (...) { 
  ...
} 
else {
  ...
}
```
- 例子
```C++
if(condition)     // 差 - IF 后面没空格.
if (condition){   // 差 - { 前面没空格.
if(condition){    // 变本加厉地差.

if (condition) {  // 好 - IF 和 { 都与空格紧邻.
```
- 如果能增强可读性, 简短的条件语句允许写在同一行. 只有当语句简单并且没有使用`else`子句时使用:
```C++
if (x == kFoo) return new Foo();
if (x == kBar) return new Bar();
```
- 如果有`else`分支则不允许！！
```C++
// 不允许 - 当有 ELSE 分支时 IF 块却写在同一行
if (x) DoThis();
else DoThat();
```
- 语句中如果某个`if-else`分支使用了大括号，那么其他分支也必须使用
```C++
// 不可以这样子 - IF 有大括号 ELSE 却没有.
if (condition) {
  foo;
} else
  bar;

// 不可以这样子 - ELSE 有大括号 IF 却没有.
if (condition)
  foo;
else {
  bar;
}
```
```C++
// 只要其中一个分支用了大括号, 两个分支都要用上大括号.
if (condition) {
  foo;
} else {
  bar;
}
```
### 8.循环和开关选择语句
- 在单语句循环里, 括号可用可不用. 空循环体应使用 `{}` 或 `continue`.
- `switch`中的`case`块也可以不用大括号，但是需要保持一致性，为了让代码看起来简洁，这里选择不使用大括号。另外`switch`中应该总包含一个`default`,如果永远执行不到`default`,简单在内部加个`assert`即可。
```C++
switch (var) {
  case 0: 
    ...  
    break;
  case 1: 
    ...
    break;
  default: 
    assert(false);
}
```
- 单语句循环里，大括号可以不用
```C++
for (int i = 0; i < kSomeNumber; ++i)
  printf("I love you\n");

for (int i = 0; i < kSomeNumber; ++i) {
  printf("I take it back\n");
}
```
- 空循环体应使用`{}`或`continue`, 而不是一个简单的分号.
```C++
while (condition) {
  // 反复循环直到条件失效.
}
for (int i = 0; i < kSomeNumber; ++i) {}  // 可 - 空循环体.
while (condition) continue;  // 可 - contunue 表明没有逻辑.
```
```C++
while (condition);  // 差 - 看起来仅仅只是 while/loop 的部分之一.
```

### 9.指针和引用表达式
- 句点或箭头前后不要有空格. 指针/地址操作符 (`*`, `&`) 之后不能有空格.
- 正确使用规范
```C++
x = *p;
p = &x;
x = r.y;
x = r->y;
```
- 在声明指针变量或参数时, 星号与类型或变量名紧挨都可以:
```C++
// 好, 空格前置.
char *c;
const string &str;

// 好, 空格后置.
char* c;
const string& str;
```
```C++
int x, *y;  // 不允许 - 在多重声明中不能使用 & 或 *
char * c;  // 差 - * 两边都有空格
const string & str;  // 差 - & 两边都有空格.
```

### 10. 布尔表达式
- 如果一个布尔表达式超过 标准行宽, 断行方式要统一一下.
- 下例中, 逻辑与 `&&` 操作符总位于行尾:
```C++
if (this_one_thing > this_other_thing &&
    a_third_thing == a_fourth_thing &&
    yet_another && last_one) {
  ...
}
```

### 11. 函数返回值
- 不要在`return`表达式里加上非必须的圆括号.
```C++
return result;                  // 返回值很简单, 没有圆括号.
// 可以用圆括号把复杂表达式圈起来, 改善可读性.
return (some_long_condition &&
        another_condition);
```
```C++
return (value);                // 毕竟您从来不会写 var = (value);
return(result);                // return 可不是函数！
```

### 12. 变量和数组初始化
- 用 `=`, `()` 和 `{}` 均可.
```C++
int x = 3;
int x(3);
int x{3};
string name("Some Name");
string name = "Some Name";
string name{"Some Name"};
```
- 小心歧义构造！
```C++
vector<int> v(100, 1);  // 内容为 100 个 1 的向量.
vector<int> v{100, 1};  // 内容为 100 和 1 的向量.
```
- 列表初始化不允许整型类型的四舍五入, 这可以用来避免一些类型上的编程失误.
```C++
int pi(3.14);  // 好 - pi == 3.
int pi{3.14};  // 编译错误: 缩窄转换.
```

### 13. 预处理指令
- 预处理指令不要缩进, 从行首开始.
```C++
// 好 - 指令从行首开始
  if (lopsided_score) {
#if DISASTER_PENDING      // 正确 - 从行首开始
    DropEverything();
# if NOTIFY               // 非必要 - # 后跟空格
    NotifyClient();
# endif
#endif
    BackToNormal();
  }
```
```C++
// 差 - 指令缩进
  if (lopsided_score) {
    #if DISASTER_PENDING  // 差 - "#if" 应该放在行开头
    DropEverything();
    #endif                // 差 - "#endif" 不要缩进
    BackToNormal();
  }
```

### 14. 构造函数初始值列表
- 下面两种初始值列表方式都可以接受:
```c++
// 如果所有变量能放在同一行:
MyClass::MyClass(int var) : some_var_(var) {
  DoSomething();
}

// 如果不能放在同一行,
// 必须置于冒号后, 并缩进 4 个空格
MyClass::MyClass(int var)
    : some_var_(var), some_other_var_(var + 1) {
  DoSomething();
}

// 如果初始化列表需要置于多行, 将每一个成员放在单独的一行
// 并逐行对齐
MyClass::MyClass(int var)
    : some_var_(var),             // 4 space indent
      some_other_var_(var + 1) {  // lined up
  DoSomething();
}

// 右大括号 } 可以和左大括号 { 放在同一行
// 如果这样做合适的话
MyClass::MyClass(int var)
    : some_var_(var) {}
```

### 15. 命名空间格式化
- 命名空间的内容缩进
```C++
namespace {
  void foo() {
    ...
  }

}  // namespace
```
- 声明嵌套命名空间时, 每个命名空间都独立成行.
```C++
namespace foo {
namespace bar {
```

### 16. 水平留白
- 水平留白的使用根据在代码中的位置决定. 永远不要在行尾添加没意义的留白.
- 通用
```C++
void f(bool b) {  // 左大括号前总是有空格.
  ...
int i = 0;  // 分号前不加空格.
// 列表初始化中大括号内的空格是可选的.
// 如果加了空格, 那么两边都要加上.
int x[] = { 0 };
int x[] = {0};

// 继承与初始化列表中的冒号前后恒有空格.
class Foo : public Bar {
 public:
  // 对于单行函数的实现, 在大括号内加上空格
  // 然后是函数实现
  Foo(int b) : Bar(), baz_(b) {}  // 大括号里面是空的话, 不加空格.
  void Reset() { baz_ = 0; }  // 用空格把大括号与实现分开.
  ...
```
- 循环和条件语句
```C++
if (b) {          // if 条件语句和循环语句关键字后均有空格.
} else {          // else 前后有空格.
}
while (test) {}   // 圆括号内部不紧邻空格.
switch (i) {
for (int i = 0; i < 5; ++i) {
switch ( i ) {    // 循环和条件语句的圆括号里可以与空格紧邻.
if ( test ) {     // 圆括号, 但这很少见. 总之要一致.
for ( int i = 0; i < 5; ++i ) {
for ( ; i < 5 ; ++i) {  // 循环里内 ; 后恒有空格, ;  前可以加个空格.
switch (i) {
  case 1:         // switch case 的冒号前无空格.
    ...
  case 2: break;  // 如果冒号有代码, 加个空格.
```
- 操作符
```C++
// 赋值运算符前后总是有空格.
x = 0;

// 其它二元操作符也前后恒有空格, 不过对于表达式的子式可以不加空格.
// 圆括号内部没有紧邻空格.
v = w * x + y / z;
v = w*x + y/z;
v = w * (x + z);

// 在参数和一元操作符之间不加空格.
x = -5;
++x;
if (x && !y)
  ...
```
- 模板和转换
```C++
// 尖括号(< and >) 不与空格紧邻, < 前没有空格, > 和 ( 之间也没有.
vector<string> x;
y = static_cast<char*>(x);

// 在类型与指针操作符之间留空格也可以, 但要保持一致.
vector<char *> x;
```