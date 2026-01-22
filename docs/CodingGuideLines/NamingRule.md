## 命名规则
### 1. 通用命名规则
- 文件命名需要有描述性，少用缩写
```C++
int price_count_reader;    // 无缩写
int num_errors;            // "num" 是一个常见的写法
int num_dns_connections;   // 人人都知道 "DNS" 是什么

/* -------------------------- 反面例子 -------------------------- */
int n;                     // 毫无意义.
int nerr;                  // 含糊不清的缩写.
int n_comp_conns;          // 含糊不清的缩写.
int wgc_connections;       // 只有贵团队知道是什么意思.
int pc_reader;             // "pc" 有太多可能的解释了.
int cstmr_id;              // 删减了若干字母.
```

### 2. 文件命名
- 做法: 文件命名全部小写,可以包含下划线`_`
- 示例
```C++
my_useful_class.h
my_useful_class.cpp
```

### 3. 类型命名
- 做法: 类型名称的每个单词首字母均大写，不包含下划线，比如:`MyExcitingClass`, `MyExcitingEnum`.
- 所有类型命名 —— 类, 结构体, 类型定义 (`typedef`), 枚举, 类型模板参数 —— 均使用相同约定, 即以大写字母开始, 每个单词首字母均大写, 不包含下划线. 例如:
```C++
// 类和结构体
class UrlTable { ...
class UrlTableTester { ...
struct UrlTableProperties { ...

// 类型定义
typedef hash_map<UrlTableProperties *, string> PropertiesMap;

// using 别名
using PropertiesMap = hash_map<UrlTableProperties *, string>;

// 枚举
enum UrlTableErrors { ...
```

### 4. 变量命名
- 变量 (包括函数参数) 和数据成员名一律小写, 单词之间用下划线连接. 类的成员变量以下划线结尾, 但结构体的就不用, 如:`a_local_variable`, `a_struct_data_member`, `a_class_data_member_`.

#### 普通变量命名
```C++
string table_name;  // 好 - 用下划线.
string tablename;   // 好 - 全小写.

string tableName;  // 差 - 混合大小写
```

#### 全局变量命名
- 全局变量加前缀`g_`
```C++
RuntimeGlobalContext g_runtime_global_context;
```

#### 类数据成员
不管是静态的还是非静态的, 类数据成员都可以和普通变量一样, 但**成员变量**接`m_`前缀,**静态成员变量**接`s_`前缀.
```C++
class TableInfo {
  ...
 private:
  string m_table_name;  // 好 - 后加下划线.
  string m_tablename_;   // 好.
  static Pool<TableInfo>* s_pool;  // 好.
};
```

#### 结构体变量
不管是静态的还是非静态的, 结构体数据成员都可以和普通变量一样, 不用像类那样接下划线:
```C++
struct UrlTableProperties {
  string name;
  int num_entries;
  static Pool<UrlTableProperties>* pool;
};
```

### 常量命名
声明为`constexpr`或`const`的变量, 或在程序运行期间其值始终保持不变的, 命名时以 “k” 开头, 大小写混合. 例如:
```C++
const int kDaysInAWeek = 7;
```

### 用作全局常量的用`下环线 + 大写`的形式
```C++
constexpr int MAX_NUM = 100;
```

### 函数命名
- 常规函数使用大小写混合(驼峰式命名), 取值和设值函数则要求与变量名匹配: `MyExcitingFunction()`, `MyExcitingMethod()`, `my_exciting_member_variable()`, `set_my_exciting_member_variable()`.
- 本项目要求函数采用小**写开头的驼峰式命名**
```C++
setViewMatrix();
setProjectionMatrix();
```

### 枚举命名
- 枚举命名大写开头的驼峰式命名，枚举成员按照驼峰式命名
```C++
enum class KeyType {
    mouseButton,
    keyButton
};
```

### 宏命名
- 一般情况下不使用宏，如一定要使用则进行`下划线 + 大写`命名风格如`MY_MACRO_THAT_SCARES_SMALL_CHILDREN`.