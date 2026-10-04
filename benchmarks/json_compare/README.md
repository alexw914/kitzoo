# mjson 与 nlohmann/json 性能对比

在 WSL2 Linux x86_64、Ryzen 5 5600X、GCC 13.3.0 下实测，固定 CPU 2。
nlohmann/json 为本地 3.12.0；mjson 使用用户指定目录的 `reconstructed` 源码。
该目录的 `provenance.json` 表明源码由已有完整头文件拆分，而非从二进制恢复；
结果不代表原 ARM64 库的性能。

两者一起使用 C++20、Release（`-O3 -DNDEBUG`），不开 LTO，不替换内存分配器。
每个项目先校准迭代次数，再交替测试两库各 9 次，取中位数；完整重复两轮。
输入生成与正确性检查不计时。所有样本通过解析及序列化后的语义等价检查。
解析计时包含 DOM 销毁；序列化计时包含返回字符串分配和销毁。
字段访问使用已解析 DOM，含创建相同短键字符串和整数读取，不含 Reader 路径解析。

## 实测结果

以下为第一轮中位数，单位 μs/次。最后一列是 mjson 耗时 / nlohmann 耗时，
小于 1 表示 mjson 更快。第二轮结论一致，原始数据保存在两个 CSV 中。

| 输入 | 操作 | mjson | nlohmann | 耗时比 |
| --- | --- | ---: | ---: | ---: |
| 小配置，139 B | 解析 | 1.056 | 1.489 | 0.71 |
| 100 条记录，7.6 KB | 解析 | 131.44 | 103.00 | 1.28 |
| 10000 条记录，859 KB | 解析 | 21346.82 | 10622.34 | 2.01 |
| 100000 个整数，589 KB | 解析 | 17514.86 | 5651.35 | 3.10 |
| 10000 个浮点数，89 KB | 解析 | 822.47 | 1114.82 | 0.74 |
| 1000 条中文字符串，48 KB | 解析 | 149.80 | 233.49 | 0.64 |
| 小配置 | 序列化 | 0.631 | 0.485 | 1.30 |
| 10000 条记录 | 序列化 | 5448.68 | 2367.70 | 2.30 |
| 100000 个整数 | 序列化 | 4109.83 | 1062.44 | 3.87 |
| 10000 个浮点数 | 序列化 | 2483.06 | 340.19 | 7.30 |
| 中文，两库均转义非 ASCII | 序列化 | 371.63 | 468.22 | 0.79 |
| 中文，两库各自默认输出 | 序列化 | 403.11 | 94.70 | 4.26 |
| 10000 条记录 | 数组定位并读取 id | 0.0177 | 0.0113 | 1.57 |

## 如何解释

- mjson 对这些小配置、浮点和中文解析样本更快，大数组和结构化记录更慢。
  整数数组解析两轮耗时比分别为 3.10、3.58，具体倍数受运行环境影响。
- mjson 浮点默认固定精度：浮点数组输出 218891 B，nlohmann 输出 88891 B。
  因而 7.30 倍包含数值格式和输出量的差异，不能视为完全相同输出的吞吐率差。
  样本使用可精确表示的二进制浮点数，未验证任意浮点数精度保持情况。
- 中文公平转义比较时，两者输出均为 91891 B，mjson 更快；nlohmann 默认保留
  UTF-8，输出仅 47891 B，此时默认序列化显著更快。两种结果不可混用。
- `sizeof(mjson::Json) = 256`，`sizeof(nlohmann::json) = 16`。mjson 节点同时
  保存多类值的成员，节点本体较大；这有助于解释大数组表现，但不是实测总内存
  或峰值 RSS 的 16 倍结论，字符串、容器及分配器开销需另行测量。
- 本次只测试有效输入和默认 DOM 接口，未覆盖 SAX、Reader、重复键、错误输入、
  任意 Unicode 和全数值边界，也未测 Windows、macOS 或原目标 ARM64。

就这些样本而言，没有证据表明替换仓库现有 nlohmann/json 能获得整体性能提升。
mjson 的 Reader 等接口可以另行评估是否值得借鉴；性能决策应再加入真实业务数据。

## 复现

此目录独立构建，不引入仓库主构建，也不复制或修改第三方源码。

```sh
cmake -S benchmarks/json_compare -B build/json-compare \
  -DCMAKE_BUILD_TYPE=Release \
  -DMJSON_SOURCE_DIR=/home/krisw/projects/aimvision_projects/thor_thirdparty/analysis/mjson/reconstructed \
  -DNLOHMANN_INCLUDE_DIR="$PWD/build/windows-ninja/_deps/nlohmann_json-src/include"
cmake --build build/json-compare -j4
taskset -c 2 build/json-compare/json_compare > results.csv 2> metadata.txt
clang-format --dry-run --Werror benchmarks/json_compare/compare.cpp
```

使用 Linux GCC/Clang 的编译器屏障避免计时调用被消除。CPU 编号和依赖路径可按
本机调整；两库调用顺序交替，但频率变化及虚拟机调度仍可能造成波动。
