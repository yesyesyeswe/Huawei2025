#ifndef DISK_HPP
#define DISK_HPP
#include<vector>
#include<queue>
#include<string>
#include<cassert>
#include "Constant.hpp"
using std::vector;
using std::queue;
using std::string;

class DiskUnit {
    public:
        int unit_id;    // 磁盘单元号
        bool is_used;   // 是否使用
        int object_id;  // 该处存放物品
        
        DiskUnit(int id) : unit_id(id), is_used(false), object_id(-1) {}
};

class Disk {
private:
    int disk_id;                // 磁盘号
    int capacity;               // 容量
    int head_position;          // 磁头位置
    int current_tokens;         // 令牌
    vector<DiskUnit> units;     // 磁盘单元
    queue<int> free_units;      // 空闲磁盘单元
    
public:
    Disk(int id, int V) : disk_id(id), capacity(V), head_position(1), current_tokens(0) {
        for(int i = 1; i <= V; i ++) {
            units.emplace_back(i);
            free_units.push(i);
        }
    }

    // 分配指定大小的存储空间（优先连续）
    vector<int> allocate(int size);

    // 磁头移动调度
    string schedule_moves(const vector<int>& targets, const int G);
    
    // 更新令牌状态
    void reset_tokens() { current_tokens = 0; }
    bool can_perform(int cost, const int G) { return current_tokens + cost <= G; }
    void consume_tokens(int cost) { current_tokens += cost; }
    
    // Getter方法
    int get_head() const { return head_position; }
    int get_free() const { return free_units.size(); }
    const int get_capacity() const { return capacity; }
};
#endif