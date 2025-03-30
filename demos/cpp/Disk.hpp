#ifndef DISK_HPP
#define DISK_HPP
#include<vector>
#include<queue>
#include<string>
#include<cassert>
#include <algorithm>
#include <set>
#include <map>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include "Constant.hpp"
#include <cmath>
#include <deque>
#include <numeric>
#include <list>
#include <thread>
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
using std::tuple;
using std::max;
using std::min;

#define READ 999
#define MOVE 998

class DiskUnit {
    public:
        int unit_id;        // 磁盘单元号
        int part_id;        // 磁盘分区号
        bool is_used;       // 是否使用
        int object_id;      // 该处存放物品
        int object_block;   // 该处存放的物品块号
        int newest_time;    // 最新请求的时间
        
        DiskUnit(int id) : unit_id(id), is_used(false), object_id(-1), object_block(-1), newest_time(0), part_id(-1) {}
        void reset() {
            is_used = false;
            object_id = -1;
            object_block = -1;
        }
};

struct Block {
    int start;  // 起始单元号
    int end;     // 结束单元号（闭区间）
    
    Block(int s, int e) : start(s), end(e) {}
};


class Disk {
private:
    // 磁盘属性
    int disk_id;                    // 磁盘号
    int capacity;                   // 容量
    int free_size;                  // 空闲容量

    // 读取相关
    int head_position;              // 磁头位置
    int current_tokens;             // 令牌
    int max_tokens;                 // 最大令牌数
    int prev_action;                // 磁头上一步操作
    int prev_consum;                // 上一步消耗的令牌数

    vector<int> units_read_id;                  // 读取的单元
    vector<int> pass_away_units;                // 过时单元
    vector<int> partition_pending_units;        // 分区待处理 units 数量
    
public:
    vector<DiskUnit> units;            // 磁盘单元
    int current_time;                  // 当前时间

    int reserve_free_size;          // 预留空余容量
    list<Block> reserve_blocks;     // 预留磁盘块
    
    // 分区相关
    struct Partition {
        int part_id;                   // 分区号
        list<Block> partition_blocks;  // 分区块
        int capacity;                  // 分区容量
        int free_size;                 // 分区空闲空间
        bool is_cold;                  // 冷分区标记
        int part_begin = 0;
        int part_end = 0;
        
        Partition(int _part_id, int begin, int end) 
            : part_id(_part_id), capacity(end - begin + 1), free_size(end - begin + 1), part_begin(begin), part_end(end) {
                partition_blocks.emplace_back(begin, end);
            }

        Partition() : part_id(0), capacity(0), free_size(0) {}
    };
    vector<Partition> Partitions;     // 分区
    std::map<int, int> tag_to_part;   // tag 到分区的映射
    vector<int> on_heat_unit_num;     // 每个分区带读取单元
    vector<int> part_req_unit_size;  // 每个 part 的 req 请求数量
    const int part_num = 16;         // 分为 16 个 tag, 17 part    
    double last_hit_rate = 0.0;     // 上一次命中率

    deque<int> recent_requests;      // 最近请求记录
    int PREDICT_WINDOW = 64;    // 可调整的预测窗口
    
    Disk(int id, int G, int V, int tag_num) : 
        disk_id(id), capacity(V), 
        head_position(1), current_tokens(0), 
        prev_action(MOVE), max_tokens(G), 
        prev_consum(-1), free_size(V)
    {
        for(int i = 0; i <= V; i ++) {
            units.emplace_back(i);
        }

        int begin = static_cast<int>(0.95 * V);
        reserve_free_size = V - begin + 1;
        reserve_blocks.emplace_back(begin, V);

        Partitions.resize(tag_num + 1);
        Partitions[0] = Partition(0, begin, V);
        tag_to_part[0] = 0;

        on_heat_unit_num = vector<int>(tag_num + 1, 0);
        pass_away_units.reserve(100);

        // 每一个 part 大小为 capacity / part_num
        part_req_unit_size.resize(part_num + 1);
        std::fill(part_req_unit_size.begin(), part_req_unit_size.end(), 0);

    }

    // 添加新分区
    void add_partition(int tag, int part_id, int begin, int size, bool _is_cold) {
        tag_to_part[tag] = part_id;
        Partitions[part_id] = Partition(part_id, begin, begin + size - 1);
        Partitions[part_id].is_cold = _is_cold;
    }


    // 热门空间调整
    void assert_not_used(int start, int end);
    void adjust_hot_zone(int new_hot_demand);
    list<Block> borrow_from_normal_zone(int need);
    void release_to_normal_zone(int release_size);
    void assert_enough_size(const list<Block>& blocks, int size, int capacity) {
        int count = 0;
        assert(size <= capacity && "free_size is more than capacity !");
        for (const auto& block : blocks) {
            count += block.end - block.start + 1;
        }
        assert(count == size && "not enough part_blocks size!");
        count = 0;
        for (const auto& block : reserve_blocks) {
            count += block.end - block.start + 1;
        }
        assert(count == reserve_free_size && "not enough reserve_blocks size!");
    }


    // 热门空间分配（正向分配）
    bool hot_allocate(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units);

    // 冷门分配（倒向分配）
    bool cold_allocate(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units);

    void deallocate_main(unordered_map<int, set<int>>& units_id);

    void merge_adjacent_blocks(list<Block>& blocks);
    void set_obj_to_unit(int tag, int size, int obj_id, vector<int>& allocated_units);

    // 将需求按环状顺序整理
    void loop_requests(const set<int>& targets_set, vector<int>& targets);

    // 磁头移动调度
    void schedule_moves(set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions);
    bool move_to_read(int dest, string& actions); 
    bool get_actions(vector<int>& obj_index, string& actions);
    bool smart_move(int dest, string& actions, int part_id);
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
    void set_units_time(const vector<int>& id, int time);


    // 更新令牌状态
    void reset_tokens() { current_tokens = 0; }
    bool can_perform(int cost) { return current_tokens + cost <= max_tokens; }
    void consume_tokens(int cost) { current_tokens += cost; }
    
    // Getter 方法
    int get_head() const { return head_position; }
    int get_free() const { 
        int size = 0;
        for(const auto& part : Partitions)  size += part.free_size;
        return size;
    }
    int get_disk_id() const { return disk_id; }
    const int get_current_tokens() const { return current_tokens; }
    const int get_capacity() const { return capacity; }
    const int get_prev_action() const { return prev_action; }
    const int get_prev_consum() const { return prev_consum; }
    int calculate_read_consume() const;
    //bool inHotZone(int unit_id) const { return is_hot_unit[unit_id]; }

private:
    // 冷门分配
    bool cold_allocate_Generic(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units, list<Block>& blocks);
    // 分配指定大小的存储空间（优先连续）
    bool allocate(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units, list<Block>& blocks);
    void deallocate(const set<int>& units, list<Block>& blocks);

    private:
    // 新增：热度评分参数
    const double SPATIAL_BONUS = 0.3;      // 连续区块奖励系数

    // 计算单个单元的热度得分
    double calculate_unit_score(int unit_id) const {
        double TIME_CRITICAL_FACTOR = 0.1; // 时间敏感系数
        double FREQUENCY_WEIGHT = 1.2;      // 访问频率权重
        const DiskUnit& unit = units[unit_id];
        
        // 时间敏感度：剩余生存时间倒计时
        int ttl = 105 - (current_time - unit.newest_time);
        double time_score = 1.0 / (1.0 + exp(-TIME_CRITICAL_FACTOR * ttl));
        
        // 空间连续性奖励
        int block_size = 1;
        for (int i = unit_id + 1; i <= capacity; ++i) {
            if (units[i].object_id == unit.object_id) block_size++;
            else break;
        }
        double spatial_score = 1.0 + SPATIAL_BONUS * log1p(block_size);
        
        // 访问频率加权
        double freq_score = part_req_unit_size[units[unit_id].part_id] * FREQUENCY_WEIGHT;
        
        return time_score * spatial_score * freq_score;
    }

    // 计算分区的综合热度
    double get_partition_score(int part_id) const {
        int start = Partitions[part_id].part_begin;
        int end = Partitions[part_id].part_end;
        double score = 0.0;
        for (int unit = start; unit <= end; ++unit) {
            if(units[unit].is_used)
            score += calculate_unit_score(unit);
        }
        return score; // 平均得分
    }

public:
    vector<int> predict_hotspots(const deque<int>& history) const {
        // 统计频率
        unordered_map<int, int> freq_map;
        for (int unit : history) {
            freq_map[unit]++;
        }

        // 转换为可排序的vector
        vector<pair<int, int>> freq_vec(freq_map.begin(), freq_map.end());
        
        // 按频率降序排序
        sort(freq_vec.begin(), freq_vec.end(), 
            [](auto& a, auto& b) { return a.second > b.second; });

        // 提取Top3热点
        vector<int> hotspots;
        for (int i = 0; i < 3 && i < freq_vec.size(); ++i) {
            hotspots.push_back(freq_vec[i].first);
        }
        
        return hotspots;
    }

    void process_request(int unit) {
        // 更新请求历史
        recent_requests.push_back(unit);
        if (recent_requests.size() > PREDICT_WINDOW) {
            recent_requests.pop_front();
        }
        
        // 每100个时间单位调整窗口
        if (current_time % 100 == 0) {
            adjust_predict_window();
        }
    }

    void adjust_predict_window() {
        double current_hit_rate = calculate_hit_rate();
        
        // 动态调整逻辑
        if (current_hit_rate > last_hit_rate + 0.1) {
            PREDICT_WINDOW = std::min(128, PREDICT_WINDOW + 8);
        } else if (current_hit_rate < last_hit_rate - 0.1) {
            PREDICT_WINDOW = std::max(32, PREDICT_WINDOW - 8);
        }
        
        last_hit_rate = current_hit_rate;
    }

    double calculate_hit_rate() const {
        if (recent_requests.empty()) return 0.0;
        
        // 获取预测热点
        auto hotspots = predict_hotspots(recent_requests);
        
        // 计算实际命中数
        int hit_count = 0;
        for (int unit : recent_requests) {
            if (find(hotspots.begin(), hotspots.end(), unit) != hotspots.end()) {
                hit_count++;
            }
        }
        
        return hit_count * 1.0 / recent_requests.size();
    }

};
#endif