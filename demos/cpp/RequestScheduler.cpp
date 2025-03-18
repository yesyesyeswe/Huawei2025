#include "RequestScheduler.hpp"

void RequestScheduler::schedule_round(vector<Disk>& disks, unordered_map<int, StorageObject>& objects, BatchReadPlan& plan, const int current_time, const int G) {
    // bug-fixed: 迭代过程中小心删除元素
    std::vector<int> requests_to_remove;

    // 遍历所有的待处理请求
    for(auto& req_id : plan.Requests_id) {
        // 清理（对于那些在 pq 中被动完成的请求）
        auto& req = active_requests[req_id];
        if(req.is_completed(objects[req.object_id].get_size())) {
            complete_request.push_back(req_id);
            n_rsp ++;
            active_requests.erase(req_id);
            requests_to_remove.push_back(req_id);
            continue;
        }
        
        // 不判断是否是新物品
        plan.Object_to_read.insert(req.object_id);

        const auto& obj = objects[req.object_id];
        
        // 为每一个块选择一个最佳的副本
        for(int i = 0; i < obj.get_size(); i ++) {
            // 选择未完成的块
            if(req.completed_blocks.count(i + 1)) continue;
            
            int best_disk = -1, min_cost = INT_MAX;
            ObjectReplica best_replica = obj.replicas[0];
            for(int rep = 0; rep < REP_NUM; rep ++) {
                auto replica = obj.replicas[rep];
                Disk disk = disks[replica.get_disk()];
                int cost = replica.access_cost(disk, {replica.unit_ids[i]});
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
        plan.Requests_id.erase(req_id);
    }
    return;
}

void RequestScheduler::printf_actions(vector<Disk>& disks, unordered_map<int, StorageObject>& objects, const int G) {
    // 记录读取的 obj 信息
    unordered_map<int, vector<int>> obj_info;
    // 生成磁盘指令
    for(size_t i = 1; i < disks.size(); i ++) {
        if(!plan.units_to_read[i].empty()) {
            printf("%s\n" , disks[i].schedule_moves(plan.units_to_read[i], obj_info).c_str());
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
                if(active_requests.count(req_id)) {
                    assert(obj_id == active_requests[req_id].object_id);
                    active_requests[req_id].completed_blocks.insert(obj_blocks.begin(), obj_blocks.end());
                    // To fix
                }
                else {
                    objects[obj_id].pending_requests.erase(req_id);
                }
            }
        }
    }
    return;
}

void RequestScheduler::printf_completed_request(BatchReadPlan& plan, unordered_map<int, StorageObject>& objects) {
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