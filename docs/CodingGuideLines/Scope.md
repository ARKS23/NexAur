# 作用域
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