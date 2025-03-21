#include "RequestScheduler.hpp"

size_t RequestScheduler::calculate_dynamic_max(size_t disk_num) {
    // 基础值：2 * disk_num 或队列剩余容量
    size_t base = std::min(2 * disk_num, pq.size());

    // 根据磁盘利用率调整
    double utilization_factor = 1.0 - disk_utilization;
    size_t dynamic_max = static_cast<size_t>(base * utilization_factor);

    // 根据延迟进一步调整
    if (avg_latency > last_time_cost) {
        dynamic_max = std::max(dynamic_max * 2, base);
    } 
    else if(avg_latency < last_time_cost) {
        dynamic_max = std::max(static_cast<size_t>(dynamic_max / 1.5), base);
    }
    // 待处理请求太多了，多处理一点
    if(pq.size() >= 105) {
        dynamic_max = static_cast<size_t>(dynamic_max * 1.1);
    }

    return dynamic_max;
}

void RequestScheduler::get_request_to_process(unordered_set<int>& new_request, int current_time, int disk_num, size_t current_max) {
    vector<int> low_value_req;
    while (plan.get_req_size() + new_request.size() < current_max) {
        if(pq.empty()) break;
        auto [_, req_id] = pq.top();
        pq.pop();
        if(!active_requests.count(req_id)) continue;
        if(active_requests[req_id].is_completed()) {
            complete_request.push_back(req_id);
            n_rsp ++;
            continue;
        }
        // 忽略超时请求
        else if(current_time - active_requests[req_id].start_time > EXTRA_TIME) {
            //scheduler.active_requests.erase(req_id);
            continue;
        }
        else if (current_time - active_requests[req_id].start_time > EXTRA_TIME / 2) {
            low_value_req.push_back(req_id);
            continue;
        }
        new_request.insert(req_id);
    }
    // 如果此时还没有足够的 req
    int current_size = plan.get_req_size() + new_request.size();
    if(current_size + low_value_req.size() >= current_max) {
        new_request.insert(low_value_req.begin(), low_value_req.begin() + current_max - current_size);
    }
    else {
        new_request.insert(low_value_req.begin(), low_value_req.end());
    }
}

void RequestScheduler::update_req(unordered_set<int>& req_set, vector<int>& obj_blocks){
    auto it = req_set.begin();
    while(it != req_set.end()) {
        int req_id = *it;
        const auto req_it = active_requests.find(req_id);
        // 判断是否活跃
        if(req_it != active_requests.end()) {
            // 为已完成 request 更新状态
            req_it -> second.completed_blocks.insert(obj_blocks.begin(), obj_blocks.end());
            if(req_it -> second.is_completed()) complete_request.push_back(req_id);
            it ++;
        }
        else {
            it = req_set.erase(it);
        }
    }
}

void RequestScheduler::delete_complete_req(unordered_set<int>& reqs) {
    auto it = reqs.begin();
    while(it != reqs.end()) {
        const int req_id = *it;
        auto& req = active_requests[req_id];
        // 清理
        if(req.is_completed()) {
            complete_request.push_back(req_id);
            active_requests.erase(req_id);
            it = reqs.erase(it);
            n_rsp ++;
            continue;
        }
        it ++;   
    }
}
    
void RequestScheduler::schedule_round(unordered_set<int>& new_req, unordered_map<int, StorageObject>& objects, const int current_time, const int G, const int capacity) {
    auto it = new_req.begin();
    const auto& disk_head_pos = plan.disk_head_pos; // 缓存磁头位置
    int disk_num = plan.units_to_read.size();
    vector<int> space_used(disk_num);
    for(int i = 1; i < disk_num; i ++) {
        space_used[i] = plan.units_to_read[i].size();
    }

    // 遍历所有的新请求
    while(it != new_req.end()) {
        const int req_id = *it;
        auto& req = active_requests[req_id];
        const int obj_id = req.object_id;
        auto& obj = objects[obj_id];

        // 选择最佳副本
        for(int i = 0; i < req.object_size; i ++) {
            // 选择未完成的块
            if(req.completed_blocks.count(i + 1)) continue;
            int best_disk = -1;
            int unit_id = obj.get_best_replica_units(i, disk_head_pos, capacity, best_disk, space_used);

            // 因为可能有重复的物品，那么可能有重复的单元，units_to_read[best_disk] 为 set
            plan.units_to_read[best_disk].insert(unit_id);
        }
        it ++;
    }
    // 将 new_req 合并到旧的请求
    plan.insert_req(new_req);

    return;
}

void RequestScheduler::printf_completed_request(unordered_map<int, StorageObject>& objects) {
    // 已经分配过了
    //complete_request.reserve(plan.Requests_id.size()); // 预分配内存

    auto it = complete_request.begin();
    while (it != complete_request.end()) {
        const int req_id = *it;
        const auto req_it = active_requests.find(req_id);

        assert(req_it != active_requests.end());
        
        // 缓存对象引用，减少哈希查找
        auto& req = req_it -> second;
        auto& pending_reqs = objects[req.object_id].pending_requests;

        // 判断是否完成
        if (req.is_completed()) {
            // 从 active_requests 和 pending_requests 中删除
            active_requests.erase(req_it);
            pending_reqs.erase(req_id); // unordered_set::erase O(1)
            plan.Requests_id.erase(req_id); // 更新迭代器
        } 
        it ++;        
    }

    printf("%ld\n", complete_request.size());
    for(int req : complete_request) {
        printf("%d\n", req);
    }
    fflush(stdout);
    return;
}