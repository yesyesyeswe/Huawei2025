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
        new_request.insert(req_id);
    }
}


void RequestScheduler::schedule_round(unordered_set<int>& new_req, unordered_map<int, StorageObject>& objects, const int current_time, const int G, const int capacity) {
    // bug-fixed: 迭代过程中小心删除元素
    std::vector<int> requests_to_remove;

    // 遍历所有的新请求
    for(int req_id : new_req) {
        // 清理（对于那些在 pq 中被动完成的请求）
        auto& req = active_requests[req_id];
        if(req.is_completed(objects[req.object_id].get_size())) {
            complete_request.push_back(req_id);
            n_rsp ++;
            active_requests.erase(req_id);
            requests_to_remove.push_back(req_id);
            continue;
        }

        const auto& obj = objects[req.object_id];
        
        // 为每一个块选择一个最佳的副本
        for(int i = 0; i < obj.get_size(); i ++) {
            // 选择未完成的块
            if(req.completed_blocks.count(i + 1)) continue;
            
            int best_disk = -1, min_cost = INT_MAX;
            ObjectReplica best_replica = obj.replicas[0];
            for(int rep = 0; rep < REP_NUM; rep ++) {
                auto replica = obj.replicas[rep];
                int disk_id = replica.get_disk();
                int cost = replica.access_cost(plan.disk_head_pos[disk_id], capacity, {replica.unit_ids[i]});
                // 优先选择连续存放的副本
                if(replica.isconsecutive()) cost -= 50;
                if(cost < min_cost) {
                    min_cost = cost;
                    best_replica = replica;
                    best_disk = best_replica.get_disk();
                }
            }
            assert(best_disk >= 0);
            // 因为可能有重复的物品，那么可能有重复的单元，units_to_read[best_disk] 为 set
            plan.units_to_read[best_disk].insert(best_replica.unit_ids[i]);
        }
    }

    // 统一从 plan.Requests_id 中删除这些请求 ID
    for (auto& req_id : requests_to_remove) {
        new_req.erase(req_id);
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
            printf("%s\n" , disks[i].schedule_moves(plan.units_to_read[i], obj_info).c_str());
            plan.disk_head_pos[i] = disks[i].get_head();
        } else {
            printf("#\n");
        }
    }
    // 更新请求信息
    if(!obj_info.empty()) {
        for(auto& obj_pair : obj_info) {
            int obj_id = obj_pair.first;
            vector<int>& obj_blocks = obj_pair.second;
            auto req_set = objects[obj_id].pending_requests;
            for(int req_id : req_set) {
                // 判断是否活跃
                if(active_requests.count(req_id)) {
                    assert(obj_id == active_requests[req_id].object_id);
                    // 为已完成 request 更新状态
                    active_requests[req_id].completed_blocks.insert(obj_blocks.begin(), obj_blocks.end());
                }
                else {
                    objects[obj_id].pending_requests.erase(req_id);
                }
            }
        }
    }
    return;
}

void RequestScheduler::printf_completed_request(unordered_map<int, StorageObject>& objects) {
    vector<int> requests_to_remove;

    for (int req_id : plan.Requests_id) {
        auto& req = active_requests[req_id];
        if (req.is_completed(objects[req.object_id].get_size())) {
            requests_to_remove.push_back(req_id);
            complete_request.push_back(req_id);
            n_rsp++;
            active_requests.erase(req_id);
            assert(objects[req.object_id].pending_requests.count(req_id));
            objects[req.object_id].pending_requests.erase(req_id);
        }
    }

    // 统一从 plan.Requests_id 中删除这些请求 ID
    for (int req_id : requests_to_remove) {
        plan.Requests_id.erase(req_id);
    }
    
    assert(n_rsp == complete_request.size());
    printf("%d\n", n_rsp);
    for(int i = 0; i < n_rsp; i ++) {
        printf("%d\n", complete_request[i]);
    }
    fflush(stdout);
    return;
}