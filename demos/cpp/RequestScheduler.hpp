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
using std::unordered_map;
using std::unordered_set;
using std::priority_queue;
using std::bitset;
using std::set;
using std::map;

/******************** 请求调度系统 ********************/
class ReadRequest {
public:
    int req_id;                     // 请求的 id
    int object_id;                  // 请求物品的 id
    int start_time;                 // 请求时间
    set<int> completed_blocks;      // 物品大小最多 5 块
    
    ReadRequest(int _req_id, int obj_id, int time) : req_id(_req_id) , object_id(obj_id), start_time(time) {}
    ReadRequest() : req_id(0), object_id(0), start_time(0) {}

    bool is_completed(int total) const {
        return completed_blocks.size() == total;
    }
};

struct BatchReadPlan {
    set<int> Object_to_read;            // 本批次要读取的对象
    vector<vector<int>> units_to_read;  // 本次要处理的单元
    set<ReadRequest> Requests;          // 一次处理的所有 Requests
    int total_tokens;                   // 预计消耗令牌
    float total_score;                  // 预期收益

    BatchReadPlan() : total_tokens(0), total_score(0.0f) {}
};

class RequestScheduler {
private: 
    BatchReadPlan plan;
       
public:

    // 维护所有未完成的活跃读请求的哈希表（以req_id为键）
    unordered_map<int, ReadRequest> active_requests; 
    priority_queue<pair<float, int>> pq;            // 得分优先级队列
    int n_rsp = 0;
    vector<int> complete_request;

    RequestScheduler(int disk_num) {
        plan.units_to_read.resize(disk_num);
    }
    RequestScheduler() {} 

    // 添加新请求
    void add_request(int req_id, int obj_id, int time, int size) {
        active_requests[req_id] = {req_id, obj_id, time};
        pq.emplace(calc_priority(size), req_id);

        // 将来也许可以进行动态更新
        /*
        if(!req.is_completed(obj.get_size())) {
            pq.emplace(calc_priority(req.start_time), req_id);
        }
        */
    }
    
    // 为每一个 req 的每一个 obj 中的块选择合适的副本
    void schedule_round(vector<Disk>& disks, unordered_map<int, StorageObject>& objects, BatchReadPlan& plan, const int G);
    
    void printf_actions(vector<Disk>& disks, unordered_map<int, StorageObject>& objects, const int G);
    void printf_completed_request(BatchReadPlan& plan, unordered_map<int, StorageObject>& objects);
    void clean() { n_rsp = 0; complete_request.clear(); }


    BatchReadPlan& get_plan() { return plan; }
    
private:
    // 动态优先级计算
    float calc_priority(int size) const {
        return 1.0 / 2 * (size + 1) * (1 - 0.005 * size);
    }
};
#endif