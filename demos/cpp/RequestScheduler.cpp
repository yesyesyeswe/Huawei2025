#include "RequestScheduler.hpp"

size_t RequestScheduler::calculate_dynamic_max(size_t disk_num) {
    size_t total_size = High_pq.size() + Median_pq.size() + Low_pq.size();
    if(total_size < 50) return total_size;
    // return static_cast<size_t>(total_size * 0.7);
    // 基础值：2 * disk_num 或队列剩余容量
    size_t base = static_cast<size_t>(total_size * 0.5);

    // 根据磁盘利用率调整
    // double utilization_factor = 1.0 - disk_utilization;
    // assert(utilization_factor < 1 + 1e-7);
    // size_t dynamic_max = static_cast<size_t>(base * utilization_factor);
    size_t dynamic_max = base;

    // 根据用时进一步调整
    if (avg_latency < last_time_cost) {
        dynamic_max = static_cast<size_t>(std::max(dynamic_max * 1.2, base * 1.0));
    } 
    else if(avg_latency > last_time_cost) {
        dynamic_max = std::min(static_cast<size_t>(dynamic_max / 1.5), base);
    }
    // 待处理请求太多了，多处理一点
    // 并且只有高优先级处理完了，才扩容低优先级的
    if(High_pq.size() >= 105) {
        dynamic_max = static_cast<size_t>(std::min(dynamic_max * 1.2, base * 1.0));
    }
    else if(Median_pq.size() >= 105) {
        dynamic_max = static_cast<size_t>(std::min(dynamic_max * 1.1, base * 1.0));
    }
    else if(Low_pq.size() >= 105) {
        dynamic_max = static_cast<size_t>(std::min(dynamic_max * 1.05, base * 1.0));
    }
    return dynamic_max;
}

void RequestScheduler::get_request_to_process(unordered_set<int>& new_request, int current_time, int disk_num, size_t current_max) {
    int req_to_get_num = current_max - plan.get_req_size();
    while (new_request.size() < req_to_get_num) {
        if(High_pq.empty() && Median_pq.empty() && Low_pq.empty()) break;
        while(!High_pq.empty() && new_request.size() < req_to_get_num) {
            auto [_, req_id] = High_pq.top();
            High_pq.pop();
            if(!active_requests.count(req_id)) continue;
            //  提前缓存，避免频繁访问
            auto& req = active_requests[req_id];
            // 记录时间差
            int time_gap = current_time - req.start_time;
            float current_size = (req.object_size + 1) * (1.05 - 0.001 * time_gap);
            float current_score = 0.5 *  current_size;
            if(req.is_completed()) {
                complete_request.push_back(req_id);
                n_rsp ++;
                continue;
            }
            // 忽略超时请求
            else if(time_gap > EXTRA_TIME) {
                //scheduler.active_requests.erase(req_id);
                continue;
            }
            // current_size [2.5, 4.5)
            else if (current_size >= 2.5 && current_size < 4.5) {
                Median_pq.emplace(current_score, req_id);
                continue;
            }
            // current_size [0, 2.5)
            else if(current_size > 0 && current_size < 2.5) {
                Low_pq.emplace(current_score, req_id);
                continue;
            }
            // current_size [4.5, 6]
            new_request.insert(req_id);
        }
        while(!Median_pq.empty() && new_request.size() < req_to_get_num) {
            auto [_, req_id] = Median_pq.top();
            Median_pq.pop();
            if(!active_requests.count(req_id)) continue;
            //  提前缓存，避免频繁访问
            auto& req = active_requests[req_id];
            // 记录时间差
            int time_gap = current_time - req.start_time;
            float current_size = (req.object_size + 1) * (1.05 - 0.001 * time_gap);
            float current_score = 0.5 *  current_size;
            if(req.is_completed()) {
                complete_request.push_back(req_id);
                n_rsp ++;
                continue;
            }
            // 忽略超时请求
            else if(time_gap > EXTRA_TIME) {
                //scheduler.active_requests.erase(req_id);
                continue;
            }
            // [0, 2.5)
            else if(current_size > 0 && current_size < 2.5) {
                Low_pq.emplace(current_score, req_id);
                continue;
            }
            // [2.5, 4.5)
            new_request.insert(req_id);
        }
        while(!Low_pq.empty() && new_request.size() < req_to_get_num) {
            auto [_, req_id] = Low_pq.top();
            Low_pq.pop();
            if(!active_requests.count(req_id)) continue;
            //  提前缓存，避免频繁访问
            auto& req = active_requests[req_id];
            // 记录时间差
            int time_gap = current_time - req.start_time;
            float current_size = (req.object_size + 1) * (1.05 - 0.001 * time_gap);
            float current_score = 0.5 *  current_size;
            if(req.is_completed()) {
                complete_request.push_back(req_id);
                n_rsp ++;
                continue;
            }
            // 忽略超时请求
            else if(time_gap > EXTRA_TIME) {
                //scheduler.active_requests.erase(req_id);
                continue;
            }
            // [0, 2.5)
            new_request.insert(req_id);
        }     
        // 如果此时已经没有 request 了，直接结束   
        break;
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
            if(req_it -> second.is_completed()) {
                complete_request.push_back(req_id);
                it = req_set.erase(it);
            }
            else
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
        // for(int i = 0; i < req.object_size; i ++) {
        //     // 选择未完成的块
        //     if(req.completed_blocks.count(i + 1)) continue;
        //     int best_disk = -1;
        //     int unit_id = obj.get_best_replica_units(i, disk_head_pos, capacity, best_disk, space_used);

        //     // 因为可能有重复的物品，那么可能有重复的单元，units_to_read[best_disk] 
        //     plan.units_to_read[best_disk].insert(unit_id);
        // }
        
        // 选择未完成的块
        int best_disk = -1;
        auto& read_set = req.completed_blocks;
        vector<int> unit_id = std::move(obj.get_best_replica_units(-1, disk_head_pos, capacity, best_disk, space_used, read_set, G));

        // 因为可能有重复的物品，那么可能有重复的单元，units_to_read[best_disk] 
        if(best_disk != -1)
        plan.units_to_read[best_disk].insert(unit_id.begin(), unit_id.end());
        
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