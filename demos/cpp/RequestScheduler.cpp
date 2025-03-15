#include "RequestScheduler.hpp"

void RequestScheduler::schedule_round(vector<Disk>& disks, const vector<StorageObject>& objects, const int G) {
    vector<vector<int>> disk_targets(disks.size());
    
    while(!pq.empty()) {
        auto [priority, req_id] = pq.top();
        pq.pop();
        
        auto& req = active_requests[req_id];
        if(req.is_completed(objects[req.object_id].get_size())) {
            active_requests.erase(req_id);
            continue;
        }
        

        const auto& obj = objects[req.object_id];
        
        // 为每一个块选择一个最佳的副本
        for(int i = 0; i < obj.get_size(); i ++) {
            // 选择未完成的块
            if(req.completed_blocks[i]) continue;
            
            int best_disk = -1, min_cost = INT_MAX;
            const ObjectReplica* best_replica = &obj.get_replica()[0];
            for(int rep = 0; rep < REP_NUM; rep ++) {
                auto& replica = obj.get_replica()[rep];
                Disk& disk = disks[replica.get_disk()];
                int cost = replica.access_cost(disk, {replica.get_units()[i]});
                if(cost < min_cost) {
                    min_cost = cost;
                    best_replica = &replica;
                    best_disk = (*best_replica).get_disk();
                }
            }
            
            assert(best_disk != -1);
            disk_targets[best_disk].push_back((*best_replica).get_units()[i]);
        }

        
        if(!req.is_completed(obj.get_size())) {
            pq.emplace(calc_priority(req.start_time), req_id);
        }
    }
    
    // 生成磁盘指令
    for(int i = 1; i < disks.size(); ++i) {
        if(!disk_targets[i].empty()) {
            printf("%s\n" ,disks[i].schedule_moves(disk_targets[i], G));
        } else {
            printf("#\n");
        }
    }
}