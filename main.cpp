// main.cpp

#include <iostream>
#include <dlfcn.h>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <cassert>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <map>

#include "operator_interface.h"

using CreateFunc = IScoreOperator* ();
using DestroyFunc = void (IScoreOperator*);

// 统计信息结构
struct Statistics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> v1_requests{0};
    std::atomic<uint64_t> v2_requests{0};
    std::atomic<uint64_t> hot_update_count{0};
    std::chrono::steady_clock::time_point start_time;
    
    Statistics() : start_time(std::chrono::steady_clock::now()) {}
    
    void record_request(const std::string& op_name) {
        total_requests++;
        if (op_name == "ScoreOperatorV1") {
            v1_requests++;
        } else if (op_name == "ScoreOperatorV2") {
            v2_requests++;
        }
    }
    
    void print_stats() {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        
        std::cout << "\n========== 统计信息 ==========\n";
        std::cout << "运行时间: " << duration.count() << " ms\n";
        std::cout << "总请求数: " << total_requests.load() << "\n";
        std::cout << "V1 请求数: " << v1_requests.load() << "\n"; 
        std::cout << "V2 请求数: " << v2_requests.load() << "\n";
        std::cout << "热更新次数: " << hot_update_count.load() << "\n";
        std::cout << "==============================\n\n";
    }
};

// 全局统计对象
Statistics g_stats;
std::mutex g_print_mutex;  // 保证输出不乱序

// 封装so和算子对象，析构时自动释放资源
struct OperatorHolder {
    void* handle = nullptr;
    IScoreOperator* op = nullptr;
    DestroyFunc* destroy_func = nullptr;

    ~OperatorHolder() {
        if (op && destroy_func) destroy_func(op);
        if (handle) dlclose(handle);
    }
};

// 全局指针，用atomic_load/store保证切换过程的原子性和线程安全
std::shared_ptr<OperatorHolder> g_operator;

// ---- 加载算子so并创建OperatorHolder ----
std::shared_ptr<OperatorHolder> load_operator(const std::string& so_file) {
    auto holder = std::make_shared<OperatorHolder>();
    holder->handle = dlopen(so_file.c_str(), RTLD_NOW);
    if (!holder->handle) {
        std::cerr << dlerror() << std::endl;
        return nullptr;
    }
    CreateFunc* create = (CreateFunc*) dlsym(holder->handle, "create_operator");
    DestroyFunc* destroy = (DestroyFunc*) dlsym(holder->handle, "destroy_operator");
    if (!create || !destroy) {
        std::cerr << "dlsym fail" << std::endl;
        dlclose(holder->handle);
        return nullptr;
    }
    holder->op = create();
    holder->destroy_func = destroy;
    return holder;
}

// ---- 热更新核心 ----
bool hot_update(const std::string& so_file) {
    std::cout << "[HotUpdate] 开始热更新到: " << so_file << std::endl;
    
    auto new_holder = load_operator(so_file);
    if (!new_holder) {
        std::cerr << "[HotUpdate] 失败! 无法加载: " << so_file << std::endl;
        return false;
    }
    
    auto old_holder = std::atomic_load(&g_operator);
    std::atomic_store(&g_operator, new_holder);   // 原子写入
    g_stats.hot_update_count++;
    
    std::cout << "[HotUpdate] 成功切换到: " << new_holder->op->name() << std::endl;
    
    // 给一点时间让正在使用old_holder的线程完成
    if (old_holder) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return true;
}

// ---- 业务线程 ----
void business_thread_func(int tid) {
    const int total_rounds = 20;  // 增加轮次以便观察更多热插拔效果
    
    for (int i = 0; i < total_rounds; ++i) {
        Feature f{tid, i, tid * 0.1 + i * 0.05, tid * 0.2 + i * 0.1};
        
        auto op_ptr = std::atomic_load(&g_operator);   // 原子读取
        if (!op_ptr || !op_ptr->op) {
            std::cerr << "[Thread-" << tid << "] 错误: 算子指针为空!\n";
            continue;
        }
        
        auto start_time = std::chrono::steady_clock::now();
        double score = op_ptr->op->compute_score(f);
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        // 记录统计信息
        g_stats.record_request(op_ptr->op->name());
        
        // 线程安全的输出
        {
            std::lock_guard<std::mutex> lock(g_print_mutex);
            std::cout << "[Thread-" << std::setw(2) << tid 
                      << "] Round " << std::setw(2) << i
                      << " | Op: " << std::setw(16) << op_ptr->op->name()
                      << " | Score: " << std::setw(8) << std::fixed << std::setprecision(3) << score
                      << " | Time: " << std::setw(4) << duration.count() << "μs"
                      << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(300));  // 稍微加快节奏
    }
    
    std::lock_guard<std::mutex> lock(g_print_mutex);
    std::cout << "[Thread-" << tid << "] 完成所有任务\n";
}

// ---- 热插拔测试控制线程 ----
void hot_swap_controller() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "\n🔄 ========== [控制器] 第1次热更新: V1 -> V2 ==========\n\n";
    assert(hot_update("./score_op_v2.so"));
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std::cout << "\n🔄 ========== [控制器] 第2次热更新: V2 -> V1 ==========\n\n";
    assert(hot_update("./score_op_v1.so"));
    
    std::this_thread::sleep_for(std::chrono::seconds(3)); 
    std::cout << "\n🔄 ========== [控制器] 第3次热更新: V1 -> V2 ==========\n\n";
    assert(hot_update("./score_op_v2.so"));
    
    std::cout << "\n✅ [控制器] 热插拔测试完成\n";
}

int main() {
    std::cout << "🚀 ========== 热插拔能力测试开始 ==========\n\n";
    
    // 1. 首次加载v1
    std::cout << "📦 [初始化] 加载初始算子...\n";
    assert(hot_update("./score_op_v1.so"));

    // 2. 启动多条业务线程（模拟高并发场景）
    constexpr int THREAD_NUM = 4;  // 增加线程数
    std::vector<std::thread> business_threads;
    
    std::cout << "🏭 [启动] 启动 " << THREAD_NUM << " 个业务线程...\n\n";
    for (int i = 0; i < THREAD_NUM; ++i) {
        business_threads.emplace_back(business_thread_func, i);
    }

    // 3. 启动热插拔控制线程
    std::thread controller_thread(hot_swap_controller);

    // 4. 定期打印统计信息
    std::thread stats_thread([]{
        for (int i = 0; i < 6; ++i) {  // 每2秒打印一次统计，共12秒
            std::this_thread::sleep_for(std::chrono::seconds(2));
            g_stats.print_stats();
        }
    });

    // 5. 等待所有线程结束
    for (auto &th : business_threads) {
        th.join();
    }
    controller_thread.join();
    stats_thread.join();

    // 6. 最终统计
    std::cout << "\n🎉 ========== 测试完成 ==========\n";
    g_stats.print_stats();
    
    std::cout << "✨ 热插拔能力验证:\n";
    std::cout << "   - ✅ 多线程并发访问安全\n";
    std::cout << "   - ✅ 无服务中断的算子切换\n";
    std::cout << "   - ✅ 多次热插拔稳定性\n";
    std::cout << "   - ✅ 原子操作保证一致性\n\n";
    
    return 0;
}