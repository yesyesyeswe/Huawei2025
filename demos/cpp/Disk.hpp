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
#include <unordered_set>
#include "Constant.hpp"
#include <cmath>
#include <deque>
#include <numeric>
#include <list>
#include <thread>
#include <map>
#include <stack>
#include <tuple>
using std::vector;
using std::queue;
using std::deque;
using std::string;
using std::set;
using std::pair;
using std::unordered_map;
using std::unordered_set;
using std::ceil;
using std::list;
using std::min;
using std::max;
using std::stack;

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

struct DpResult {
    double profit = 0;
    string actions;
    vector<int> read_units;
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
    int hot_capacity;           // 热门区容量
    int free_size;              // 空闲容量
    int hot_free_size;          // 热门区空闲容量
    vector<int> units_read_id;  // 本次读取的单元
    struct DpKey {
        int pos;
        int token_remains;
        int contin_read_times;
        int time;
        bool has_jump;
        int read_count;

        bool operator<(const DpKey& other) const {
            return std::tie(pos, token_remains, contin_read_times, time, has_jump, read_count) <
                   std::tie(other.pos, other.token_remains, other.contin_read_times, 
                            other.time, other.has_jump, other.read_count);
        }
    };

public:
    vector<bool> is_hot_unit;       // 单元是否属于热区
    const int min_hot_capacity;
    const int max_hot_capacity;
    list<Block> free_blocks;        // 空闲磁盘块
    list<Block> hot_zone_blocks;    // 热门读取区
    vector<DiskUnit> units;         // 磁盘单元
    int prev_continue_read = 0; // 上次连续读取次数
    std::map<DpKey, DpResult> memo;
    
    Disk(int id, int G, int V) : 
        disk_id(id), 
        capacity(V), 
        head_position(1), 
        current_tokens(0), 
        prev_action(MOVE), 
        max_tokens(G), 
        prev_consum(-1), 
        free_size(V), 
        min_hot_capacity(static_cast<int>(V * 0.1)), 
        max_hot_capacity(static_cast<int>(V * 0.75))  
        {
            is_hot_unit.resize(V + 1);
            fill(is_hot_unit.begin(), is_hot_unit.end(), false);
            int hot_start = static_cast<int>(V * 0.3);
            int hot_end = static_cast<int>(V * 0.6);
            // [hot_start, hot_end] 标记为 true
            fill(is_hot_unit.begin() + hot_start, is_hot_unit.begin() + hot_end + 1, true);
            hot_capacity = hot_end - hot_start + 1;
            hot_free_size = hot_capacity;
            free_size = V - hot_free_size;

            hot_zone_blocks.emplace_back(hot_start, hot_end);
            free_blocks.emplace_back(1, hot_start - 1);
            free_blocks.emplace_back(hot_end + 1, V);

            for(int i = 0; i <= V; i ++) {
                units.emplace_back(i);
        }
    }

    // 热门空间调整
    void assert_not_used(int start, int end);
    void adjust_hot_zone(int new_hot_demand);
    list<Block> borrow_from_normal_zone(int need);
    void release_to_normal_zone(int release_size);
    void assert_enough_size() {
        int count = 0;
        for (const auto& block : free_blocks) {
            count += block.end - block.start + 1;
        }
        assert(count == free_size && "not enough free_blocks size!");
        count = 0;
        for (const auto& block : hot_zone_blocks) {
            count += block.end - block.start + 1;
        }
        assert(count == hot_free_size && "not enough hot_free_blocks size!");
    }

    // 更新热区 [start, end]
    void update_is_hot_unit(int start, int end, bool flag) {
        fill(is_hot_unit.begin() + start, is_hot_unit.begin() + end + 1, flag);
    }

    // 添加/删除请求
    void add_request(const vector<int>& units_id, int _start_time);
    void erase_request(const vector<int>& units_id, int _start_time);


    // 热门空间分配
    bool hot_allocate(int size, int obj_id, int& consecutive, vector<int>& allocated_units) {
        if(hot_free_size >= size && allocate(size, obj_id, consecutive, allocated_units, hot_zone_blocks)) {
            hot_free_size -= size;
            //assert_enough_size();
            return true;
        }
        if(free_size >= size && allocate(size, obj_id, consecutive, allocated_units, free_blocks)){
            free_size -= size;
            //assert_enough_size();
            return true;
        }
        assert(0);
        return true;
    }
    // 普通分配
    bool normal_allocate(int size, int obj_id, int& consecutive, vector<int>& allocated_units) {
        if(free_size >= size && allocate(size, obj_id, consecutive, allocated_units, free_blocks)){
            free_size -= size;
            //assert_enough_size();
            return true;
        }
        if(hot_free_size >= size && allocate(size, obj_id, consecutive, allocated_units, hot_zone_blocks)) {
            hot_free_size -= size;
            //assert_enough_size();
            return true;
        }
        assert(0);
        return false;
    }

    void deallocate_main(set<int>& units) {
        set<int> out_hot_units;
        set<int> in_hot_units;

        // // 预分配内存减少哈希冲突
        // out_hot_units.reserve(units.size());
        // in_hot_units.reserve(units.size());

        // 遍历分类
        for (int unit : units) {
            assert(unit >= 1 && unit < is_hot_unit.size());
            if (!is_hot_unit[unit]) {
                out_hot_units.insert(unit);
            } else {
                in_hot_units.insert(unit);
            }
        }
        if(!in_hot_units.empty()) {
            hot_deallocate(in_hot_units);
        }   
        if(!out_hot_units.empty()) {
            normal_deallocate(out_hot_units);
        }
    }

    // 热门空间回收
    void hot_deallocate(const set<int>& units) {
        deallocate(units, hot_zone_blocks);
        hot_free_size += units.size();
        //assert_enough_size();
    }
    // 普通回收
    void normal_deallocate(const set<int>& units) {
        deallocate(units, free_blocks);
        free_size += units.size();
        //assert_enough_size();
    }

    void merge_adjacent_blocks(list<Block>& blocks);
    void set_obj_to_unit(int size, int obj_id, vector<int>& allocated_units);

    // 动态规划获取路径
    DpResult dp(int pos, int token_remains, int contin_read_times, int time, const set<int>& targets, bool has_jump, int read_count);
    DpResult nr_dp(int pos, int token_remains, int contin_read_times, int time, const set<int>& targets, bool has_jump, int read_count);
    void dp_schedule_moves(set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions, int time);


    // 将需求按环状顺序整理
    void loop_requests(const set<int>& targets_set, vector<int>& targets);

    // 磁头移动调度
    void schedule_moves(set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions);
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
    int get_free() const { return free_size + hot_free_size; }
    int get_disk_id() const { return disk_id; }
    const int get_current_tokens() const { return current_tokens; }
    const int get_capacity() const { return capacity; }
    const int get_prev_action() const { return prev_action; }
    const int get_prev_consum() const { return prev_consum; }
    int calculate_read_consume() const;
    //bool inHotZone(int unit_id) const { return is_hot_unit[unit_id]; }

private:
    // 分配指定大小的存储空间（优先连续）
    bool allocate(int size, int obj_id, int& consecutive, vector<int>& allocated_units, list<Block>& blocks);
    void deallocate(const set<int>& units, list<Block>& blocks);

};
#endif