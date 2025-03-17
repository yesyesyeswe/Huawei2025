#ifndef DISK_HPP
#define DISK_HPP
#include<vector>
#include<queue>
#include<string>
#include<cassert>
#include <algorithm>
#include <set>
#include <utility>
#include <unordered_map>
#include "Constant.hpp"
using std::vector;
using std::queue;
using std::string;
using std::set;
using std::pair;
using std::unordered_map;

#define READ 999
#define MOVE 998

class DiskUnit {
    public:
        int unit_id;        // 磁盘单元号
        bool is_used;       // 是否使用
        int object_id;      // 该处存放物品
        int object_block;   // 该处存放的物品块号
        
        DiskUnit(int id) : unit_id(id), is_used(false), object_id(-1), object_block(-1) {}
        void reset() {
            assert(unit_id != 0);
            is_used = false;
            object_id = -1;
            object_block = -1;
        }
};

class Disk {
private:
    int disk_id;                // 磁盘号
    int capacity;               // 容量
    int head_position;          // 磁头位置
    int current_tokens;         // 令牌
    queue<int> free_units;      // 空闲磁盘单元
    int prev_action;            // 磁头上一步操作
    int prev_consum;            // 上一步消耗的令牌数
    
public:

    vector<DiskUnit> units;     // 磁盘单元

    Disk(int id, int V) : disk_id(id), capacity(V), head_position(1), current_tokens(0), prev_action(MOVE), prev_consum(-1) {
        for(int i = 1; i <= V; i ++) {
            units.emplace_back(i);
            free_units.push(i);
        }
    }

    // 分配指定大小的存储空间（优先连续）
    vector<int> allocate(int size, int obj_id, int& consecutive);

    // 将需求整理为两部分
    std::pair<vector<int>, vector<int>> separate_requests(const vector<int>& targets);

    // 磁头移动调度
    string schedule_moves(const vector<int>& targets,  unordered_map<int, vector<int>>& obj_info, const int G);
    bool move_to_read(int destination, string& actions, const int G, const int V); 
    bool get_actions(vector<int>& obj_index, string& actions, vector<int>& units_read_id, const int G);

    // Setter 方法
    // 保存最后状态
    void save_status(const int pos, const int action, const int consum);
    // 更新磁头位置
    void set_head_position(const int pos) { head_position = pos; }
    // 更新上一步操作
    void set_prev_action(const int action) { prev_action = action; };
    // 更新上一次消耗的令牌数
    void set_prev_consum(const int consum) { prev_consum = consum; }
    void deallocate_space(const int id) { free_units.push(id); }
    void set_unit_free(const int id) { units[id].reset(); }


    // 更新令牌状态
    void reset_tokens() { current_tokens = 0; }
    bool can_perform(int cost, const int G) { return current_tokens + cost <= G; }
    void consume_tokens(int cost) { current_tokens += cost; }
    
    // Getter 方法
    int get_head() const { return head_position; }
    int get_free() const { return free_units.size(); }
    int get_disk_id() const { return disk_id; }
    const int get_current_tokens() const { return current_tokens; }
    const int get_capacity() const { return capacity; }
    const int get_prev_action() const { return prev_action; }
    const int get_prev_consum() const { return prev_consum; }

};
#endif