#ifndef REQUEST_SCHEDULER_HPP
#define REQUEST_SCHEDULER_HPP
#include "Disk.hpp"
#include "Object.hpp"
#include <unordered_map>
#include <queue>
#include <bitset>
using std::unordered_map;
using std::priority_queue;
using std::bitset;

/******************** 请求调度系统 ********************/
class RequestScheduler {
private:
    struct ReadRequest {
        int req_id;                 // 请求的 id
        int object_id;              // 请求物品的 id
        int start_time;             // 请求时间
        bitset<5> completed_blocks; // 物品大小最多 5 块
        
        bool is_completed(int total) const {
            return completed_blocks.count() == total;
        }
    };
    
    // 维护所有未完成的活跃读请求的哈希表（以req_id为键）
    unordered_map<int, ReadRequest> active_requests; 
    priority_queue<pair<float, int>> pq;            // 得分优先级队列
    
public:
    // 添加新请求
    void add_request(int req_id, int obj_id, int time) {
        active_requests[req_id] = {req_id, obj_id, time, bitset<5>()};
        pq.emplace(calc_priority(time), req_id);
    }
    
    // 生成调度指令
    void schedule_round(vector<Disk>& disks, const vector<StorageObject>& objects, const int G);
    
private:
    // 动态优先级计算
    float calc_priority(int start_time) const {
        const int current = /* 需要全局时间 */ 0; // 实际实现需获取当前时间
        const float x = current - start_time;
        float score = (x <= 10) ? (-0.005 * x + 1) : 
                        (x <= 105) ? (-0.01 * x + 1.05) : 0;
        return score * (1 + log(1 + current - start_time));
    }
};
#endif