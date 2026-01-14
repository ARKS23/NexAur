# 类
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