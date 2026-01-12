# CodingGuideLines
## 头文件
### 防护符
为了保证符号的唯一性，头文件前加入保护
```
#pragma once
```
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

## 类
### 构造函数的内部操作
- **构造函数中不得调用虚函数**. 不要在没有错误处理机制的情况下进行可能失败的初始化.

### 隐式类型转换
- 不要定义隐式类型转换. 定义类型转换运算符和单个参数的构造函数时, 请使用 `explicit` 关键字.
```C++
class Foo {
    explicit Foo(int x, double y);
    ...
};

void Func(Foo f);


Func({42, 3.14});  // 报错
```

### 可拷贝类型和可移动类型
 - 类的公有接口必须明确指明该类是可拷贝的、仅可移动的、还是既不可拷贝也不可移动的. 如果该类型的复制和移动操作有明确的语义并且有用，则应该支持这些操作.
```C++
class Copyable {
 public:
  Copyable(const Copyable& other) = default;
  Copyable& operator=(const Copyable& other) = default;
  // 上面的声明阻止了隐式移动运算符.
  // 您可以显式声明移动操作以支持更高效的移动.
};

class MoveOnly {
 public:
  MoveOnly(MoveOnly&& other) = default;
  MoveOnly& operator=(MoveOnly&& other) = default;

  // 复制操作被隐式删除了, 但您也可以显式删除:
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;
};

class NotCopyableOrMovable {
 public:
  // 既不可复制也不可移动.
  NotCopyableOrMovable(const NotCopyableOrMovable&) = delete;
  NotCopyableOrMovable& operator=(NotCopyableOrMovable&) = delete;

  // 移动操作被隐式删除了, 但您也可以显式声明:
  NotCopyableOrMovable(NotCopyableOrMovable&&) = delete;
  NotCopyableOrMovable& operator=(NotCopyableOrMovable&&) = delete;
};
```
- 如果普通用户容易误解某个类的拷贝或移动操作的含义, 或者此操作会产生意想不到的开销, 那么这个类应该设计为不可拷贝或不可移动的. 可拷贝类型的移动操作只是一种性能优化, 容易增加复杂性并引发错误. 除非移动操作明显比拷贝操作更高效, 不要定义移动操作. 如果您的类可拷贝, 那么最好确保自动生成的默认 (default) 实现是正确的. 请像检查您自己的代码一样检查默认实现的正确性.

### 结构体和类
- 只能用`struct`定义那些用于**储存数据的被动对象**. 其他情况应该使用`class`.

### 继承
- 通常情况下, 组合比继承更合适. 请使用`public`继承.
- 所有继承都应该使用`public`的访问权限. 如果要实现私有继承, 可以将基类对象作为**成员变量**保存. 当您不希望您的类被继承时, 可以使用`final`关键字.

### 访问控制
- 类的 所有 数据成员应该声明为私有 (private), 除非是常量. 这样做可以简化类的不变式 (invariant) 逻辑, 代价是需要增加一些冗余的访问器 (accessor) 代码 (通常是 const 方法).

### 声明次序
- 将相似的声明放在一起. 公有 (public) 部分放在最前面.
- 类的定义通常以 `public:`开头, 其次是 `protected:`, 最后以 `private:`结尾
- 按照如下顺序声明
```
1. 类型别名与嵌套类型
2. 构造 / 析构函数
3. 公共接口
4. 常量与只读访问接口
5. 受保护接口(如果有protected API的话)
6. 私有辅助函数
7. 成员变量
```
- 示例
```C++
#pragma once

#include <string>

namespace NexAur {

class AssetManager {
public:
    // 1. Types
    using AssetID = uint64_t;

    enum class State {
        Uninitialized,
        Ready,
        Error
    };

public:
    // 2. Lifecycle
    AssetManager();
    ~AssetManager();

public:
    // 3. Public API
    void Initialize(const std::string& assetRoot);
    void Shutdown();

    bool LoadAsset(AssetID id);
    void UnloadAsset(AssetID id);

    // 4. Const / Query
    State GetState() const { return m_State; }

protected:
    // 5. Protected API (for extension)
    virtual void OnAssetLoaded(AssetID id);

private:
    // 6. Private helper methods
    void ValidateAssetRoot();
    void ClearCache();

private:
    // 7. Member variables
    std::string m_AssetRoot;
    State m_State = State::Uninitialized;
    bool m_Initialized = false;
};

} // namespace NexAur
```

### 构造函数
- 不要在构造函数中写太多逻辑相关的初始化，可以将初始化逻辑抽离到一个辅助函数中

### 类成员变量
- 类成员变量以`m_`前缀开头
- 初始化顺序应与声明顺序一致

### 存取成员函数
- 存取函数一般内联在头文件中

### 接口类
接口类类名以`Interface`为后缀, 除提供带实现的虚析构函数, 静态成员函数外, 其他均为纯虚函数, 不定义非静态数据成员, 不提供构造函数, 提供的话, 声明为`protected`


## 函数
### 输入和输出
- 优先按值返回，其次引用返回。避免返回指针，除非指针可以为空。
- 尽量不使用传出参数，传出参数容易造成全局混乱的情况。

### 编写简短的函数
- 如果一个函数的长度超过50行，考虑是否可以在不影响程序结构的前提下进行分割。
- 在处理代码时, 可能会发现复杂的长函数. 不要害怕修改现有代码: 如果证实这些代码使用 / 调试起来很困难, 或者只需要使用其中的一小段代码, 考虑将其分割为更加简短并易于管理的若干函数.

### 函数重载
- 若要使用函数重载, 则必须能让读者一看调用点就胸有成竹, 而不用花心思猜测调用的重载函数到底是哪一种. 这一规则也适用于构造函数.
- 例子
```C++
class MyClass {
    public:
    void Analyze(const string &text);
    void Analyze(const char *text, size_t textlen);
};
```
- 缺点

如果函数单靠不同的参数类型而重载 (acgtyrant 注：这意味着参数数量不变), 读者就得十分熟悉 C++ 五花八门的匹配规则, 以了解匹配过程具体到底如何. 另外, 如果派生类只重载了某个函数的部分变体, 继承语义就容易令人困惑.

### 缺省参数
- 只允许在**非虚函数**中使用**缺省参数**, 且必须保证缺省参数的值始终一致. 缺省参数与函数重载遵循同样的规则. **一般情况下建议使用函数重载**, 尤其是在缺省函数带来的可读性提升不能弥补下文中所提到的缺点的情况下.
- 对于虚函数, 不允许使用缺省参数, 因为在虚函数中缺省参数不一定能正常工作. 如果在每个调用点缺省参数的值都有可能不同, 在这种情况下缺省函数也不允许使用. (例如, 不要写像 `void f(int n = counter++);` 这样的代码.)

### 函数返回类型后置语法
- 只有在常规写法 (返回类型前置) 不便于书写或不便于阅读时使用返回类型后置语法.
```C++
// 正常写法
int foo(int x);

// 后置返回类型写法
auto foo(int x) -> int;

template <class T, class U> auto add(T t, U u) -> decltype(t + u);

template <class T, class U> decltype(declval<T&>() + declval<U&>()) add(T t, U u);
```
- 在大部分情况下, 应当继续使用以往的函数声明写法, 即将返回类型置于函数名前. 只有在必需的时候 (如 Lambda 表达式) 或者使用后置语法能够简化书写并且提高易读性的时候才使用新的返回类型后置语法. 但是后一种情况一般来说是很少见的, 大部分时候都出现在相当复杂的模板代码中, 而多数情况下不鼓励写这样 复杂的模板代码.