#include "Disk.hpp"

vector<int> Disk::allocate(int size) {
    vector<int> allocated;
    vector<int> candidate;
    int consecutive = 0;
    
    // 寻找连续空间
    queue<int> free_units_copy = free_units;
    while(!free_units_copy.empty()) {
        int u = free_units_copy.front();
        free_units_copy.pop();
        
        // 若候选集空，则添加
        if(candidate.empty() || u == candidate.back() + 1) {
            candidate.push_back(u);
            if(++consecutive >= size) {
                // 注意 candidate.end() - size != candidate.begin()
                // 因为分配是一直进行的
                allocated = vector<int>(candidate.end() - size, candidate.end());
                break;
            }
        } else {
            candidate.clear();
            consecutive = 0;
        }
    }
    
    // 没有足够连续空间则分配离散
    if(allocated.empty()) {
        while(size-- && !free_units.empty()) {
            int u = free_units.front();
            free_units.pop();
            allocated.push_back(u);
            units[u].is_used = true;
        }
    }
    
    // 更新单元状态
    for(int u : allocated) {
        units[u].is_used = true;
        // 从空闲队列移除
        queue<int> new_queue;
        while(!free_units.empty()) {
            int x = free_units.front();
            free_units.pop();
            if(find(allocated.begin(), allocated.end(), x) == allocated.end()) {
                new_queue.push(x);
            }
        }
        free_units = new_queue;
    }
    return allocated;
}

// 磁头移动调度
// To Improve
string Disk::schedule_moves(const vector<int>& targets, const int G) {
    vector<int> sorted(targets);
    sort(sorted.begin(), sorted.end());
    
    // 使用SCAN算法规划路径
    string actions;
    int current = head_position;

    // 分离左右请求
    auto right = sorted;  // t >= head_position 的请求
    auto left = sorted;   // t < head_position 的请求（反向遍历）
    right.erase(remove_if(right.begin(), right.end(), [&](int t){ return t < current; }), right.end());
    left.erase(remove_if(left.begin(), left.end(), [&](int t){ return t >= current; }), left.end());
    
    // 处理向右移动
    for(int t : right) {
        int steps = t - current;
        if(steps > 0) {
            actions += string(steps, 'p');
        }
        actions += 'r';
        current = t + 1;
    }
    
    // 处理绕转情况
    const int V = get_capacity();
    if(!sorted.empty() && sorted.back() < head_position) {
        actions += string(V - current + 1, 'p');
        current = 1;
        for(int t : left) {
            int steps = t - current;
            actions += string(steps, 'p') + 'r';
            current = t + 1;
        }
    }
    
    actions += '#';

    // 令牌消耗检查
    int tokens = 64 + (actions.size() - 1) * 16; // 估算值
    if(tokens > G) {
        // 需要拆分到多个时间片
        return "j " + std::to_string(sorted.front());
    }


    return actions;
}