# 头文件
### 防护符
为了保证符号的唯一性，头文件前加入保护
```
#pragma once
```
### 预编译头文件`pch`编写规范
- 标准A: 体积巨大且编译耗时
- 标准B: 几乎不被修改
- 标准C: 项目中高频引用

### 依赖导入
- 若代码文件或头文件引用了其他地方定义的符号 (symbol), 该文件应该直接导入 提供该符号的声明 或者定义的头文件. 不应该为了其他原因而导入头文件.
- 前置声明是为了降低编译依赖，防止修改一个头文件引发多米诺效应;

### 内联函数
- 把10行内的小函数定义为内联函数`inline`
- 类内部的函数一般会自动内联。所以某函数一旦不需要内联，**其定义就不要再放在头文件里**，而是放到对应的 .cpp 文件里。这样可以保持头文件的类相当精炼，也很好地贯彻了声明与定义分离的原则。

### #include路径和顺序
- 按照以下顺序导入头文件: 配套的头文件, C 语言系统库头文件, C++ 标准库头文件, 其他库的头文件, 本项目的头文件.
- 头文件的路径应相对于项目源码目录, 不能出现 UNIX 目录别名 (alias) . (当前目录) 或 .. (上级目录).
- 比如文件 (dir2/foo2.cpp)
```
1. dir2/foo2.h
2. 空行
3. C语言系统文件
4. 空行
5. C++标准库头文件
6. 空行
7. 其他库的.h文件
8. 空行
9. 本项目的.h文件
```
- 示例
```C++
#include "foo/server/fooserver.h"

#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "foo/server/bar.h"
#include "third_party/absl/flags/flag.h"
```
- 引擎的`.cpp`文件中最先引入pch

## 作用域
### 命名空间
- 命名空间后用注释给命名空间收尾
```C++
namespace outer {
    inline namespace inner {
        void foo();
    }  // namespace inner
}  // namespace outer
```
- 不允许使用using指令注入命名空间
```C++
// 禁止: 这会污染命名空间.
using namespace foo;
```
- 可以使用单行嵌套命名空间声明
```C++
namespace foo::bar {
...
}  // namespace foo::bar
```

### 内部链接
- 若其他文件不需要使用 .cc 文件中的定义, 这些定义可以放入匿名命名空间 (unnamed namespace) 或声明为 static, 以实现内部链接 (internal linkage). 但是不要在 .h 文件中使用这些手段.
```C++
namespace {
...
}  // namespace
```

### 非成员函数、静态成员函数和全局函数
- 有时我们需要定义一个和类的实例无关的函数. 这样的函数可以定义为**静态成员函数**或**非成员函数**. 非成员函数不应该依赖外部变量, 且大部分情况下应该**位于命名空间**中. 不要仅仅为了给静态成员分组而创建一个新类; 这相当于给所有名称添加一个公共前缀, 而这样的分组通常是不必要的.

### 局部变量
- 应该尽可能缩小函数变量的作用域 (scope), 并在声明的同时初始化.
```C++
int i;
i = f();     // 不好: 初始化和声明分离.

int i = f(); // 良好: 声明时初始化​。

// ----------------

int jobs = NumJobs();
// 更多代码...
f(jobs);      // 不好: 初始化和使用位置分离.
int jobs = NumJobs();
f(jobs);      // 良好: 初始化以后立即 (或很快) 使用.

// ----------------

vector<int> v;
v.push_back(1);  // 用花括号初始化更好.
v.push_back(2);

vector<int> v = {1, 2}; // 良好: 立即初始化 v.
```
- 通常应该在语句内声明用于 if、while 和 for 语句的变量, 这样会把作用域限制在语句内. 例如:
```C++
while (const char* p = strchr(str, '/')) str = p + 1;
```
- 对象在循环内部时考虑构造和析构成本
```C++
// 低效
for (int i = 0; i < 1000000; ++i) {
    Foo f;  // 调用 1000000 次构造函数和析构函数.
    f.DoSomething(i);
}

// 高效
Foo f;  // 调用 1 次构造函数和析构函数.
for (int i = 0; i < 1000000; ++i) {
    f.DoSomething(i);
}
```