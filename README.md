# C++ 动态库热插拔能力测试

这是一个展示C++动态库热插拔技术的完整示例，实现了在不中断服务的情况下动态切换算法实现。

## 🎯 项目目标

- 验证动态库热插拔的可行性和稳定性
- 展示多线程环境下的安全热插拔机制
- 提供可复用的热插拔框架设计

## 🏗️ 项目架构

```
demo/
├── README.md              # 项目文档
├── build.sh              # 构建脚本
├── main.cpp              # 主程序（热插拔框架）
├── operator_interface.h  # 算子接口定义
├── score_op_v1.cpp       # 算子实现版本1
├── score_op_v2.cpp       # 算子实现版本2
└── 生成文件：
    ├── demo              # 可执行文件
    ├── score_op_v1.so    # 算子V1动态库
    └── score_op_v2.so    # 算子V2动态库
```

## 🔧 技术特性

### 核心技术栈
- **C++11+**: 标准库支持
- **动态链接**: `dlopen`/`dlsym`/`dlclose`
- **原子操作**: `std::atomic_load`/`std::atomic_store`
- **多线程**: `std::thread`、`std::mutex`
- **智能指针**: `std::shared_ptr` 自动内存管理

### 关键特性
- ✅ **零停机热插拔**: 业务线程无感知切换
- ✅ **线程安全**: 原子操作保证并发安全
- ✅ **内存安全**: RAII自动资源管理
- ✅ **多次切换**: 支持V1↔V2任意切换
- ✅ **统计监控**: 实时性能和切换统计
- ✅ **错误处理**: 完善的异常处理机制

## 🚀 快速开始

### 1. 环境要求
```bash
# Linux环境
sudo apt-get update
sudo apt-get install build-essential

# 或者CentOS/RHEL
sudo yum install gcc-c++
```

### 2. 编译运行
```bash
# 赋予执行权限
chmod +x build.sh

# 编译项目
./build.sh

# 运行热插拔测试
./demo
```

### 3. 预期输出
```
🚀 ========== 热插拔能力测试开始 ==========

📦 [初始化] 加载初始算子...
[HotUpdate] 开始热更新到: ./score_op_v1.so
[HotUpdate] 成功切换到: ScoreOperatorV1

🏭 [启动] 启动 4 个业务线程...

[Thread- 0] Round  0 | Op: ScoreOperatorV1 | Score:    0.020 | Time:    1μs
[Thread- 1] Round  0 | Op: ScoreOperatorV1 | Score:    0.070 | Time:    0μs
...

🔄 ========== [控制器] 第1次热更新: V1 -> V2 ==========

[HotUpdate] 开始热更新到: ./score_op_v2.so
[HotUpdate] 成功切换到: ScoreOperatorV2

[Thread- 0] Round  7 | Op: ScoreOperatorV2 | Score:    2.475 | Time:    1μs
...

========== 统计信息 ==========
运行时间: 4032 ms
总请求数: 27
V1 请求数: 21
V2 请求数: 6
热更新次数: 2
==============================
```

## 📋 详细设计

### 1. 接口设计

#### 算子接口 (`operator_interface.h`)
```cpp
struct Feature {
    int user_id;        // 用户ID
    int item_id;        // 物品ID  
    double user_feature; // 用户特征
    double item_feature; // 物品特征
};

struct IScoreOperator {
    virtual ~IScoreOperator() = default;
    virtual double compute_score(const Feature& feature) = 0;
    virtual const char* name() const = 0;
};
```

#### 动态库导出接口
```cpp
extern "C" {
    IScoreOperator* create_operator();
    void destroy_operator(IScoreOperator* op);
}
```

### 2. 核心组件

#### OperatorHolder
```cpp
struct OperatorHolder {
    void* handle = nullptr;           // dlopen句柄
    IScoreOperator* op = nullptr;     // 算子实例
    DestroyFunc* destroy_func = nullptr; // 销毁函数
    
    ~OperatorHolder() {              // RAII自动清理
        if (op && destroy_func) destroy_func(op);
        if (handle) dlclose(handle);
    }
};
```

#### 原子切换机制
```cpp
std::shared_ptr<OperatorHolder> g_operator;

// 原子读取（业务线程）
auto op_ptr = std::atomic_load(&g_operator);

// 原子写入（热更新线程）  
std::atomic_store(&g_operator, new_holder);
```

#### 统计监控
```cpp
struct Statistics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> v1_requests{0};
    std::atomic<uint64_t> v2_requests{0};
    std::atomic<uint64_t> hot_update_count{0};
    // ...统计方法
};
```

### 3. 算子实现差异

#### V1算子 (简单线性)
```cpp
double compute_score(const Feature& feature) override {
    return feature.user_feature * 0.7 + feature.item_feature * 0.3;
}
```

#### V2算子 (复杂非线性)
```cpp
double compute_score(const Feature& feature) override {
    double base_score = feature.user_feature * 0.4 + feature.item_feature * 0.6;
    return base_score * (1.0 + 0.1 * sin(feature.user_id * 0.1)) + 2.0;
}
```

## 🧪 测试场景

### 多线程并发测试
- **业务线程数**: 4个
- **每线程请求数**: 20轮
- **请求间隔**: 300ms
- **并发模式**: 高频读取、无锁设计

### 热插拔测试序列
1. **T+0s**: 启动，加载V1算子
2. **T+2s**: 第1次热更新 V1→V2
3. **T+5s**: 第2次热更新 V2→V1  
4. **T+8s**: 第3次热更新 V1→V2
5. **T+12s**: 测试结束

### 验证指标
- ✅ **功能正确性**: 算子切换后计算结果变化
- ✅ **线程安全性**: 无数据竞争、无段错误
- ✅ **性能影响**: 热更新时延<100ms
- ✅ **资源管理**: 无内存泄漏、无句柄泄漏

## 🎛️ 配置选项

### 编译选项
```bash
# 默认编译
g++ -fPIC -shared -o score_op_v1.so score_op_v1.cpp
g++ -fPIC -shared -o score_op_v2.so score_op_v2.cpp  
g++ -std=c++11 -o demo main.cpp -ldl -pthread

# Debug模式
g++ -g -O0 -DDEBUG -std=c++11 -o demo main.cpp -ldl -pthread

# Release模式
g++ -O2 -DNDEBUG -std=c++11 -o demo main.cpp -ldl -pthread
```

### 运行时参数
可修改`main.cpp`中的常量：
```cpp
constexpr int THREAD_NUM = 4;        // 业务线程数
const int total_rounds = 20;         // 每线程请求轮次
std::chrono::milliseconds(300)       // 请求间隔
```

## 🐛 故障排除

### 常见问题

#### 1. 动态库加载失败
```
错误：dlopen: cannot open shared object file
解决：检查.so文件是否存在，路径是否正确
```

#### 2. 符号未找到
```  
错误：dlsym fail
解决：确保算子实现了extern "C"导出函数
```

#### 3. 段错误
```
错误：Segmentation fault
解决：检查算子指针是否为空，确保线程安全
```

#### 4. 编译错误
```
错误：undefined reference to `dlopen'
解决：添加-ldl链接选项
```

### 调试方法

#### 启用详细日志
```cpp
// 在load_operator中添加
std::cout << "Loading: " << so_file << std::endl;
std::cout << "Handle: " << holder->handle << std::endl;
```

#### 内存检查
```bash
# 使用valgrind检查内存泄漏
valgrind --leak-check=full ./demo

# 使用gdb调试
gdb ./demo
(gdb) run
(gdb) bt  # 查看调用栈
```

## 🎨 扩展应用

### 1. 生产环境部署
- 添加配置文件支持
- 实现优雅的信号处理
- 增加日志系统集成
- 添加监控指标上报

### 2. 功能扩展
- 支持多个算子并存
- 实现算子版本回滚
- 添加算子预热机制
- 支持动态配置更新

### 3. 性能优化  
- 实现无锁数据结构
- 添加内存池管理
- 优化切换时机选择
- 实现渐进式切换

## 📚 相关资料

### 技术文档
- [dlopen手册](https://man7.org/linux/man-pages/man3/dlopen.3.html)
- [C++原子操作](https://en.cppreference.com/w/cpp/atomic)
- [RAII设计模式](https://en.cppreference.com/w/cpp/language/raii)

### 最佳实践
- [动态链接最佳实践](https://tldp.org/HOWTO/Program-Library-HOWTO/)
- [多线程编程指南](https://computing.llnl.gov/tutorials/pthreads/)
- [C++智能指针使用](https://docs.microsoft.com/en-us/cpp/cpp/smart-pointers-modern-cpp)

## 👨‍💻 贡献

欢迎提交Issue和Pull Request来改进这个项目！

### 开发环境
```bash
git clone <repository>
cd demo
chmod +x build.sh
./build.sh
```

### 提交规范
- 代码风格：遵循Google C++ Style Guide
- 提交信息：使用Conventional Commits格式
- 测试：确保所有测试用例通过

## 📄 许可证

MIT License - 详见LICENSE文件

---

**🎉 享受你的热插拔之旅！**
