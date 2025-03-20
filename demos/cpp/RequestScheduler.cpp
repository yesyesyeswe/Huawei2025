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


void RequestScheduler::schedule_round(unordered_set<int>& new_req, unordered_map<int, StorageObject>& objects, const int current_time, const int G, const int capacity) {
    complete_request.reserve(plan.Requests_id.size() + new_req.size());
    auto it = new_req.begin();
    const auto& disk_head_pos = plan.disk_head_pos; // 缓存磁头位置

    // 遍历所有的新请求
    while(it != new_req.end()) {
        const int req_id = *it;
        auto& req = active_requests[req_id];
        const int obj_id = req.object_id;
        auto& obj = objects[obj_id];

        // 清理（对于那些在 pq 中被动完成的请求）
        if(req.is_completed(obj.get_size())) {
            complete_request.push_back(req_id);
            active_requests.erase(req_id);
            it = new_req.erase(it);
            n_rsp ++;
            continue;
        }
        it ++;

        
        // 为每一个块选择一个最佳的副本
        for(int i = 0; i < obj.get_size(); i ++) {
            // 选择未完成的块
            if(req.completed_blocks.count(i + 1)) continue;
            
            int best_disk = -1, min_cost = INT_MAX;
            const ObjectReplica* best_replica = nullptr;

            for(int rep = 0; rep < REP_NUM; rep ++) {
                const auto& replica = obj.replicas[rep];
                const int disk_id = replica.get_disk();
                int cost = replica.access_cost(disk_head_pos[disk_id], capacity, {replica.unit_ids[i]});
                // 优先选择连续存放的副本
                if(replica.isconsecutive()) cost -= 50;
                if(cost < min_cost) {
                    min_cost = cost;
                    best_replica = &replica;
                    best_disk = disk_id;
                }
            }
            assert(best_disk >= 0);
            // 因为可能有重复的物品，那么可能有重复的单元，units_to_read[best_disk] 为 set
            plan.units_to_read[best_disk].insert(best_replica -> unit_ids[i]);
        }
    }
    // 将 new_req 合并到旧的请求
    plan.insert_req(new_req);

    return;
}

void RequestScheduler::printf_actions(vector<Disk>& disks, unordered_map<int, StorageObject>& objects, const int G) {
    // 记录读取的 obj 信息
    unordered_map<int, vector<int>> obj_info;
    // 生成磁盘指令
    for(size_t i = 1; i < disks.size(); i ++) {
        if(!plan.units_to_read[i].empty()) {
            string actions;
            disks[i].schedule_moves(plan.units_to_read[i], obj_info, actions);
            printf("%s\n" , actions.c_str());
            plan.disk_head_pos[i] = disks[i].get_head();
        } else {
            printf("#\n");
        }
    }
    // 更新请求信息
    if(!obj_info.empty()) {
        for(auto& [obj_id, obj_blocks] : obj_info) {
            auto& req_set = objects[obj_id].pending_requests;
            auto it = req_set.begin();
            while(it != req_set.end()) {
                const auto req_it = active_requests.find(*it);
                // 判断是否活跃
                if(req_it != active_requests.end()) {
                    assert(obj_id == req_it -> second.object_id);
                    // 为已完成 request 更新状态
                    req_it -> second.completed_blocks.insert(obj_blocks.begin(), obj_blocks.end());
                    it ++;
                }
                else {
                    it = req_set.erase(it);
                }
            }
        }
    }
}

void RequestScheduler::printf_completed_request(unordered_map<int, StorageObject>& objects) {
    // 已经分配过了
    //complete_request.reserve(plan.Requests_id.size()); // 预分配内存

    auto it = plan.Requests_id.begin();
    while (it != plan.Requests_id.end()) {
        const int req_id = *it;
        const auto req_it = active_requests.find(req_id);

        assert(req_it != active_requests.end());
        
        // 缓存对象引用，减少哈希查找
        auto& req = req_it -> second;
        auto& storage_obj = objects[req.object_id];
        auto& pending_reqs = storage_obj.pending_requests;

        // 判断是否完成
        if (req.is_completed(storage_obj.get_size())) {
            // 添加到完成列表
            complete_request.push_back(req_id);
            // 从 active_requests 和 pending_requests 中删除
            active_requests.erase(req_it);
            pending_reqs.erase(req_id); // unordered_set::erase O(1)
            it = plan.Requests_id.erase(it); // 更新迭代器
        } else {
            ++it;
        }
    }

    printf("%ld\n", complete_request.size());
    for(int req : complete_request) {
        printf("%d\n", req);
    }
    fflush(stdout);
    return;
}