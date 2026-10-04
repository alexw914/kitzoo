# 代码编写规则

本规则适用于仓库自有代码和 CMake 生成头文件的模板；第三方依赖及
`build/` 下的生成文件不直接修改。既有公开接口的命名调整需要明确授权，
不要仅为符合新规则而破坏兼容性。

## 文件头与头文件保护

- 自有头文件和实现文件使用下列文件头，`File` 填写从仓库根目录开始的
  实际路径，`Description` 简洁说明职责；续行与描述正文对齐。
- 使用英文描述，不加入作者、日期或修改历史；这些信息由 Git 记录。
- 头文件使用 include guard，禁止使用 `#pragma once`。
- 宏名由公开头文件路径生成：从 `kitzoo/` 开始，将路径分隔符、点替换为
  下划线并全部大写。例如 `include/kitzoo/os/osadaptor.hpp` 对应
  `KITZOO_OS_OSADAPTOR_HPP`。不得重复或使用保留的双下划线。
- `.hpp.in` 模板按生成的 `.hpp` 路径命名，不将 `_IN` 加入宏名。
- 模板中的 `@变量@` 占位符必须保持完整；必要时对该段使用
  `// clang-format off` 和 `// clang-format on` 保护。
- 末尾写 `#endif  // 宏名`。

```cpp
// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/os/osadaptor.hpp
// Description: Declares the singleton OS adaptor for system and thread operations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_OS_OSADAPTOR_HPP
#define KITZOO_OS_OSADAPTOR_HPP

// Declarations.

#endif  // KITZOO_OS_OSADAPTOR_HPP
```

## 格式与函数

- 使用仓库根目录 `.clang-format`，通过 `clang-format -i <修改的文件>` 格式化。
  格式检查使用 `clang-format --dry-run --Werror <修改的文件>`。
- 使用四空格缩进，禁止制表符，行宽上限 100；大括号和 include 排序交给
  clang-format。文本使用 LF，Windows `.bat`、`.cmd` 使用 CRLF。
- 指针 `*`、引用 `&` 和右值引用 `&&` 均紧靠类型名，例如 `Type* pointer`、
  `std::vector<std::uint32_t>& values`、`Type&& value`。禁止从已有代码推断
  对齐风格；`.clang-format` 使用 `DerivePointerAlignment: false` 和 Left 对齐。
- 相邻函数实现、类型定义之间保留一个空行，不连续堆叠，也不使用多个空行。
  函数声明之间保留一个空行；紧密相关的重载可成组排列。
- 新增和修改的函数声明、定义使用尾置返回类型：
  `auto read_file(...) -> std::string`、`auto write_file(...) -> void`。
  构造函数、析构函数、类型转换运算符按 C++ 所要求的语法书写。
- 有显式返回值的 lambda 优先使用 `[](...) -> ReturnType { ... }`；泛型
  lambda 可按需要使用返回类型推导。
- 头文件只声明需要编译的函数，实现在对应模块的 `.cpp` 中；模板、constexpr
  和有意采用头文件实现的代码例外。

## 命名与枚举

- 类、结构体和枚举类型使用大驼峰命名，例如 `ThreadPool`、`OSAdaptor`、
  `OSAdaptorShedPolicy`；常见缩写可保留已有的 `OS` 写法。
- 函数和变量使用小写下划线命名，例如 `set_thread_name`、`cpu_count`。
  标准库要求的名称和第三方接口遵循其原有约定。
- 枚举使用 `enum class`，枚举项使用大驼峰命名，禁止全大写加模块前缀的
  枚举项，例如 `OSAdaptorShedPolicy::Other`、`Rr`、`Fifo`。
- 底层枚举类型仅在序列化、ABI 或明确的存储需求下指定；数值只有在接口
  语义需要时显式给出。枚举项逐行排列，最后一项保留逗号。
- 常量遵循已有的 `kNamePrefix` 风格；宏使用大写下划线形式。
- 单例优先继承 `kitzoo::util::Singleton<T>`，派生类构造函数设为私有，并
  声明模板基类为 friend；不重复实现 `instance()` 或拷贝、移动禁用逻辑。

## 修改验证

- 同步更新受影响的 CMake 依赖、调用方、示例和文档。
- 运行与修改相关的构建和测试，平台实现应明确支持范围；报告未验证的平台。
- 提交前执行 `git diff --check`；有暂存修改时同时执行
  `git diff --cached --check`。不要直接改写第三方代码或生成文件。
