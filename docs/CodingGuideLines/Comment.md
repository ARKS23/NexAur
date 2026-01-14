# 注释
### 1. 注释风格
- 单行用`//`，多行用`/* */`, 尽量保持风格统一。

### 2. 类注释
- 每个类的定义都要附带一份注释, 描述类的功能和用法, 除非它的功能相当明显.
```C++
// Iterates over the contents of a GargantuanTable.
// Example:
//    GargantuanTableIterator* iter = table->NewIterator();
//    for (iter->Seek("foo"); !iter->done(); iter->Next()) {
//      process(iter->key(), iter->value());
//    }
//    delete iter;
class GargantuanTableIterator {
  ...
};
```
- 如果类的声明和定义分开了(例如分别放在了`.h` 和 `.cpp` 文件中), 此时, **描述类用法的注释应当和接口定义放在一起, 描述类的操作和实现的注释应当和实现放在一起**.


### 3.函数注释
- 函数声明处的注释描述函数功能; 定义处的注释描述函数实现.
- 函数声明注释的内容
```
函数的输入输出.

对类成员函数而言: 函数调用期间对象是否需要保持引用参数, 是否会释放这些参数.

函数是否分配了必须由调用者释放的空间.

参数是否可以为空指针.

是否存在函数使用上的性能隐患.

如果函数是可重入的, 其同步前提是什么?
```
- 避免啰嗦：如果函数名称和返回值是显而易见的则不用进行注释了

### 4. 变量注释
- 如果变量名称不足以说明变量的用途，则对变量加注释进行说明，比如一些全局变量

### 5.实现注释
- 对于代码中巧妙的, 晦涩的, 有趣的, 重要的地方加以注释.
- 不要用自然语言直接翻译代码！要提供的注释应该解释代码为什么要这么做以及代码要达成的目的。

### 6. TODO注释
- 对那些临时的, 短期的解决方案, 或已经够好但仍不完美的代码使用`TODO`注释.
```C++
// TODO(kl@gmail.com): Use a "*" here for concatenation operator.
// TODO(Zeke) change this to use relations.
// TODO(bug 12345): remove the "Last visitors" feature
```