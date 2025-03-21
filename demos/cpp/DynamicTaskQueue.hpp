#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <vector>

class DynamicTaskQueue {
public:
    DynamicTaskQueue(const std::vector<int>& tasks) : done(false) {
        for (int task : tasks) task_queue.push(task);
    }

    // 线程安全获取任务
    bool try_get_task(int& task) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !task_queue.empty() || done; });
        
        if (task_queue.empty()) return false;
        
        task = task_queue.front();
        task_queue.pop();
        return true;
    }

    // 通知所有线程任务完成
    void set_done() {
        std::lock_guard<std::mutex> lock(mutex_);
        done = true;
        cv_.notify_all();
    }

private:
    std::queue<int> task_queue;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool done;
};