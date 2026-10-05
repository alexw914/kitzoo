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
- 末尾的 `#endif` 注释标明 include guard 宏名。

```cpp
// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/os/osadaptor.hpp
// Description: Declares the singleton OS adaptor for system and thread operations.
// -----------------------------------------------------------------------------

#ifndef KITZOO_OS_OSADAPTOR_HPP
#define KITZOO_OS_OSADAPTOR_HPP

// Declarations.

#endif // KITZOO_OS_OSADAPTOR_HPP
```

## 格式与函数

- 使用仓库根目录 `.clang-format`，通过 `clang-format -i <修改的文件>` 格式化。
  格式检查使用 `clang-format --dry-run --Werror <修改的文件>`。
- 格式选项以 `.clang-format` 为准，不在本文件重复规定。
- `const` 等限定符统一写在类型左侧（`const T&`、`const T*`），由 `.clang-format`
  的 `QualifierAlignment: Left` 保证；生成头文件模板需手动保持一致。
- 非 C++ 文本使用 LF，Windows `.bat`、`.cmd` 使用 CRLF。
- 函数声明之间保留一个空行；紧密相关的重载可成组排列。
- 新增和修改的函数声明、定义使用尾置返回类型：
  `auto read_file(...) -> std::string`、`auto write_file(...) -> void`。
  构造函数、析构函数、类型转换运算符按 C++ 所要求的语法书写。
- 有显式返回值的 lambda 优先使用 `[](...) -> ReturnType { ... }`；泛型
  lambda 可按需要使用返回类型推导。
- 函数参数按使用语义优先采用引用：只读访问较大对象使用 `const T&`，修改
  调用方对象使用 `T&`；转移所有权时按需要使用值传递或 `T&&`。数值、指针、
  `std::string_view` 等轻量类型按值传递，不为使用引用而增加间接访问。
  既有公开接口的签名调整需要明确授权。
- 头文件只声明需要编译的函数，实现在对应模块的 `.cpp` 中；模板、constexpr
  和有意采用头文件实现的代码例外。

## 命名与枚举

- 同一模块的公开代码统一使用该模块的命名空间，不按文件或功能另建公开
  命名空间；utilities 统一使用 `kitzoo::util`，其余模块使用
  `kitzoo::<module>`。内部辅助实现可使用 `detail` 或匿名命名空间。
- 类、结构体和枚举类型使用大驼峰命名，例如 `ThreadPool`、`OSAdaptor`、
  `OSAdaptorShedPolicy`；常见缩写可保留已有的 `OS` 写法。
- 函数和变量使用小写下划线命名，例如 `set_thread_name`、`cpu_count`。
  标准库要求的名称和第三方接口遵循其原有约定。
- 枚举使用 `enum class`，枚举项使用大驼峰命名，禁止全大写加模块前缀的
  枚举项，例如 `OSAdaptorShedPolicy::Other`、`Rr`、`Fifo`。
- 底层枚举类型仅在序列化、ABI 或明确的存储需求下指定；数值只有在接口
  语义需要时显式给出。最后一项保留逗号。
- 常量遵循已有的 `kNamePrefix` 风格；宏使用大写下划线形式。
- 单例优先继承 `kitzoo::core::Singleton<T>`，派生类构造函数设为私有，并
  声明模板基类为 friend；不重复实现 `instance()` 或拷贝、移动禁用逻辑。

## README 编写规则

- README 使用英文，保持简略，只保留项目介绍、`Modules`、`Build and test`、
  `Use in another CMake project` 和 `Reference`。
- 项目介绍简要说明项目定位和支持平台；模块表只列现有模块，每个模块用一句
  简短描述概括功能。模块合并或删除时同步更新，不保留过时模块说明。
- 不在 README 展开实现细节、内部架构、接口语义、生命周期或迁移历史；
  使用示例放在 `examples/`，不为内部实现说明重建仓库文档目录。
- `Build and test` 只保留必要的环境要求、构建和测试命令，以及相关可选功能开关。
- `Use in another CMake project` 保留 FetchContent 和安装包两种接入方式，示例简短。
- `Reference` 简要列出参考项目或依赖的名称、链接和用途。

## 示例编写规则

- `examples/` 按模块组织示例，除 core 外，每个模块只保留一个示例文件和一个
  可执行目标；core 不编写独立示例，其功能由测试验证。
- 示例文件名和目标名必须与模块名完全对应：模块 `<module>` 使用
  `examples/<module>.cpp` 和 `kitzoo_example_<module>`。例如 utilities、queue、log
  模块分别使用 `utilities.cpp`、`queue.cpp`、`log.cpp`，不使用复数、功能别名或
  子组件名作为示例名称。
- 同一模块的基本功能、子组件和不同使用场景统一放入该模块的示例文件，
  不按类、函数或子组件另建示例文件和目标。
- 同一个文件可以包含多个案例，使用简洁的英文注释明确分隔；必要时将案例
  封装为文件内的辅助函数，由一个 `main()` 调用。
- 示例展示模块的典型用法；边界条件、异常和并发正确性验证放在 `tests/`。
- 合并或删除示例时，同步更新 `examples/CMakeLists.txt` 和相关文档。

## 测试编写规则

- `tests/` 只按现有模块建立子目录，测试放入 `tests/<module>/`；根目录仅
  保留测试构建入口，不保留已删除模块、独立功能目录或第三方库自身的测试。
- 测试文件名使用现有源文件的原名加 `_test.cpp`，不重命名源文件来适配
  测试；头文件实现和生成头文件模板按对应头文件的基本名称命名。例如
  `str.hpp` 对应 `str_test.cpp`，`basicmemory.cpp` 对应 `basicmemory_test.cpp`。
- 同一源文件中的多个类型、函数和场景合并到对应测试文件，不按子组件
  另建测试文件。单例初始化等需要进程隔离的场景使用独立子进程验证。
- 测试案例组和 fixture 以源文件基本名称的大驼峰写法开头，可追加子组件
  或场景名称，并以 `Test` 结尾，例如 `StrTrimTest`、`ObjectPoolLocalTest`。
  单个案例名称简洁描述被验证的行为。
- 测试目标使用 `kitzoo_test_<源文件基本名称>`；新增、合并或删除测试时
  同步更新对应 CMake 配置，清理失效目标、空目录和仅含注释的无效案例。
- 已有 `docs/` 内容从本地和 Git 跟踪中删除；保留 `.gitignore` 中的忽略
  规则，后续不将该目录纳入提交。

## 提交规范

- 沿用远程提交记录的 `<type>: <summary>` 格式，标题使用简洁英文，以
  动词说明实际改动，尽量不超过 72 个字符，不添加句末句号。
- 每次提交选择一个主要类型：`feat` 新增功能、`fix` 修复问题、`chore`
  维护或结构整理、`doc` 文档、`test` 测试、`ci` 持续集成。不使用
  `feat&chore` 等混合前缀，不沿用历史记录中的拼写错误。
- 提交应围绕完整的改动目的组织，同时包含必要的调用方、构建配置、测试、
  示例和规则更新，避免将无法独立构建的中间状态提交。
- 较大提交在标题后空一行，正文概括主要改动及原因，明确已授权的公开
  头文件、命名空间和接口变化，并记录构建、测试及未验证的平台。
- 提交前检查工作区和暂存差异，不包含生成文件、临时文件、第三方代码或
  `docs/` 本地文档；执行下列修改验证要求。
- 推送前获取远程记录并检查分支差异，使用普通推送，不强制覆盖远程历史。
  只有用户明确要求提交或推送时才执行对应操作。

## 修改验证

- 同步更新受影响的 CMake 依赖、调用方、示例和文档。
- 运行与修改相关的构建和测试，平台实现应明确支持范围；报告未验证的平台。
- 提交前执行 `git diff --check`；有暂存修改时同时执行
  `git diff --cached --check`。不要直接改写第三方代码或生成文件。
