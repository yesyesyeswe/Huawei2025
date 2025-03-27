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
#include <cmath>
#include <deque>
#include <numeric>
#include <list>
#include <thread>
using std::vector;
using std::queue;
using std::deque;
using std::string;
using std::set;
using std::pair;
using std::unordered_map;
using std::ceil;
using std::list;

#define READ 999
#define MOVE 998

class DiskUnit {
    public:
        int unit_id;        // 磁盘单元号
        bool is_used;       // 是否使用
        int object_id;      // 该处存放物品
        int object_block;   // 该处存放的物品块号
        int obj_size;       // 存储的物品大小
        set<int> start_time;     // 加入的时间（多请求）
        
        DiskUnit(int id) : unit_id(id), is_used(false), object_id(-1), object_block(-1), obj_size(-1) {}
        void reset() {
            is_used = false;
            object_id = -1;
            object_block = -1;
            obj_size = -1;
            start_time.clear();
        }
};

struct Block {
    int start;  // 起始单元号
    int end;     // 结束单元号（闭区间）
    
    Block(int s, int e) : start(s), end(e) {}
};


class Disk {
private:
    int disk_id;                // 磁盘号
    int head_position;          // 磁头位置
    int current_tokens;         // 令牌
    int max_tokens;             // 最大令牌数
    int prev_action;            // 磁头上一步操作
    int prev_consum;            // 上一步消耗的令牌数
    int capacity;               // 容量
    int free_size;              // 空闲容量
    vector<int> units_read_id;  // 本次读取的单元
    
public:
    list<Block> free_blocks;    // 空闲磁盘块
    vector<DiskUnit> units;     // 磁盘单元
    
    

    Disk(int id, int G, int V) : disk_id(id), capacity(V), head_position(1), current_tokens(0), prev_action(MOVE), max_tokens(G), prev_consum(-1), free_size(V) {
        free_blocks.emplace_back(1, V);
        for(int i = 0; i <= V; i ++) {
            units.emplace_back(i);
        }
    }

    // 分配指定大小的存储空间（优先连续）
    bool allocate(int size, int obj_id, int& consecutive, vector<int>& allocated_units);
    void deallocate(const set<int>& units);
    void merge_adjacent_blocks();
    
    // 分配磁盘
    void set_obj_to_unit(int size, int obj_id, vector<int>& allocated_units);

    // 添加/删除请求
    void add_request(const vector<int>& units_id, int _start_time);
    void erase_request(const vector<int>& units_id, int _start_time);

    // 动态规划获取路径
    double dp(int pos, int token_remains, int contin_read_times, int time, const set<int>& targets, string& actions, bool has_jump);
    void dp_schedule_moves(const set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions, int time);


    // 将需求按环状顺序整理
    void loop_requests(const set<int>& targets_set, vector<int>& targets);

    // 磁头移动调度
    void schedule_moves(const set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions);
    bool move_to_read(int dest, string& actions); 
    bool get_actions(vector<int>& obj_index, string& actions);
    bool smart_move(int dest, string& actions);
    bool perform_read(int dest, string& actions, int read_consume);

    // Setter 方法
    // 保存最后状态
    void save_status(const int pos, const int action, const int consum);
    // 更新磁头位置
    void set_head_position(const int pos) { 
        head_position = (pos > capacity) ? pos % capacity : pos;
    }
    // 更新上一步操作
    void set_prev_action(const int action) { prev_action = action; };
    // 更新上一次消耗的令牌数
    void set_prev_consum(const int consum) { prev_consum = consum; }


    // 更新令牌状态
    void reset_tokens() { current_tokens = 0; }
    bool can_perform(int cost) { return current_tokens + cost <= max_tokens; }
    void consume_tokens(int cost) { current_tokens += cost; }
    
    // Getter 方法
    int get_head() const { return head_position; }
    int get_free() const { return free_size; }
    int get_disk_id() const { return disk_id; }
    const int get_current_tokens() const { return current_tokens; }
    const int get_capacity() const { return capacity; }
    const int get_prev_action() const { return prev_action; }
    const int get_prev_consum() const { return prev_consum; }
    int calculate_read_consume() const;

};
#endif