#ifndef REQUEST_SCHEDULER_HPP
#define REQUEST_SCHEDULER_HPP
#include "Disk.hpp"
#include "Object.hpp"
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <bitset>
#include <set>
#include <map>
#include <iostream>
using std::unordered_map;
using std::unordered_set;
using std::priority_queue;
using std::bitset;
using std::set;
using std::map;

/******************** 请求调度系统 ********************/
class ReadRequest {
public:
    int req_id;                                 // 请求的 id
    int object_id;                              // 请求物品的 id
    int object_size;                            // 请求物品的大小
    int start_time;                             // 请求时间
    unordered_set<int> completed_blocks;        // 物品大小最多 5 块
    
    ReadRequest(int _req_id, int obj_id, int time, int size) : req_id(_req_id) , object_id(obj_id), object_size(size), start_time(time) {
        completed_blocks.reserve(5);
    }
    ReadRequest() : req_id(0), object_id(0), start_time(0) {
        completed_blocks.reserve(5);
    }

    bool is_completed() const {
        return completed_blocks.size() == object_size;
    }

    bool operator<(const ReadRequest& other) const {
        // 根据你的需求定义比较逻辑，例如按 req_id 排序
        return req_id < other.req_id;
    }
};

struct BatchReadPlan {
    vector<set<int>> units_to_read;              // 本次要处理的单元
    vector<int> disk_head_pos;                      // 当前磁头位置
    unordered_set<int> Requests_id;                 // 一次处理的所有 Requests
    int total_tokens;                               // 预计消耗令牌
    float total_score;                              // 预期收益

    BatchReadPlan(int disk_num) : total_tokens(0), total_score(0.0f), disk_head_pos(disk_num, 1) {
        units_to_read.resize(disk_num);
    }
    void insert_req(unordered_set<int>& new_req) { Requests_id.insert(new_req.begin(), new_req.end()); };
    int get_req_size() { return Requests_id.size(); }
};

class RequestScheduler {  
public:
    BatchReadPlan plan;
    // 维护所有未完成的活跃读请求的哈希表（以req_id为键）
    unordered_map<int, ReadRequest> active_requests; 
    priority_queue<pair<float, int>> High_pq;             // 高质量得分优先级队列
    priority_queue<pair<float, int>> Median_pq;           // 中质量得分优先级队列
    priority_queue<pair<float, int>> Low_pq;              // 低质量得分优先级队列
    int n_rsp = 0;
    vector<int> complete_request;

    RequestScheduler(int disk_num) : plan(disk_num) {
        // 10000 是随意选择的
        active_requests.reserve(10000);
        complete_request.reserve(7 *  disk_num);
    }

    void get_request_to_process(unordered_set<int>& new_request, int current_time, int disk_num, size_t current_max);

    // 添加新请求
    void add_request(int req_id, int obj_id, int time, int size, bool isHot_delete) {
        active_requests[req_id] = {req_id, obj_id, time, size};
        float base_score = calc_priority(size);
        if(isHot_delete) {
            if(size == 4) base_score *= 1.02;
            else if(size == 2 || size == 3) base_score *= 1.1;
        }
        switch (size)
        {
        case 5:
            High_pq.emplace(base_score, req_id);
            break;
        case 4:
        case 3:
            Median_pq.emplace(base_score, req_id);
            break;
        case 2:
        case 1:
            Low_pq.emplace(base_score, req_id);
            break;
        default:
            assert(0);
            break;
        }
    }
    
    // 处理本次所需请求
    bool should_accept_new_requests(size_t current_max) {
        // 判断是否接受新请求
        return (plan.get_req_size() < current_max);
        //return true;
    }

    size_t calculate_dynamic_max(size_t disk_num);

    // 为每一个 req 的每一个 obj 中的块选择合适的副本
    void schedule_round(unordered_set<int>& new_req, unordered_map<int, StorageObject>& objects, const int current_time, const int G, const int capacity);
    
    // 更新请求信息
    void update_req(unordered_set<int>& req_set, vector<int>& obj_blocks);
    void delete_complete_req(unordered_set<int>& reqs);

    void printf_completed_request(unordered_map<int, StorageObject>& objects);
    void clean() { n_rsp = 0; complete_request.clear(); }
 
    
private:
    double avg_latency = 0.0;       // 平均延迟
    double disk_utilization = 0.0;  // 磁盘利用率
    int total_processed = 0;        // 总处理请求数   
    double last_time_cost = 0.0;    // 上一次时间片耗时  

public:
    // 记录每轮处理的请求数和耗时
    void record_metrics(double time, int free_units_num, const int total_capacity) {
        // 更新平均延迟
        avg_latency = (avg_latency * total_processed + time) / plan.get_req_size();
        total_processed = plan.get_req_size();

        // 更新磁盘利用率（假设总时间已知）
        disk_utilization = 1 - free_units_num / (double)total_capacity;
        last_time_cost = time;
    }

private:
    // 动态优先级计算
    float calc_priority(float size) const {
        return 1.0 / 2 * (size + 1);
    }
};
#endif