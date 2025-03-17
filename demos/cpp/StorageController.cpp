#include "StorageController.hpp"

void StorageController::process_delete(vector<int>& deleted_object_id) {
    int n_abort = 0;
    vector<int> abort_reqs;
    for(int obj_id : deleted_object_id) {
        StorageObject& obj = objects[obj_id];
        vector<ObjectReplica>& replicas = obj.replicas;
        for(auto& replica : replicas) {
            for(int unit_id : replica.get_units()) {
                disks[replica.get_disk()].deallocate_space(unit_id);
                disks[replica.get_disk()].set_unit_free(unit_id);
            }
        }
        for(int req_id : objects[obj_id].pending_requests) {
            if(scheduler.active_requests.count(req_id)) {
                n_abort ++;
                abort_reqs.push_back(req_id);

                // 删除请求
                scheduler.active_requests.erase(req_id);
                scheduler.get_plan().Requests_id.erase(req_id);
            }
        }
        objects.erase(obj_id);
    }
    assert(n_abort == abort_reqs.size());
    printf("%d\n", n_abort);
    for(int i = 0; i < n_abort; i ++) {
        printf("%d\n", abort_reqs[i]);
    }
}


void StorageController::process_write(int obj_id, int size, int tag) {
    // 选择目标磁盘
    vector<int> selected_disks;
    while(selected_disks.size() < REP_NUM) {
        int d = tag_manager.select_disk(tag, disks);
        if(find(selected_disks.begin(), selected_disks.end(), d) == selected_disks.end()) {
            selected_disks.push_back(d);
        }
    }
    
    // 分配存储空间
    StorageObject obj(obj_id, size, tag);
    printf("%d\n", obj_id);
    for(int d : selected_disks) {
        int consecutive = 0;
        auto units = disks[d].allocate(size, obj_id, consecutive);
        obj.add_replica(ObjectReplica(d, units, consecutive));
        printf("%d ", d);
        for(int i = 0; i < units.size(); i ++) {
            if(i != units.size() - 1) printf("%d ", units[i]);
            else printf("%d", units[i]);
        }
        printf("\n");
    }
    
    objects[obj_id] = obj;
}

void StorageController::tick(const int G) {
    // 重置磁盘令牌
    for(auto& d : disks) d.reset_tokens();

    BatchReadPlan& plan = scheduler.get_plan();
    int disk_num = disks.size();
    // 若 Request 太少，则增加
    while (plan.Requests_id.size() < std::min((size_t)disk_num, scheduler.pq.size())) {
        if(scheduler.pq.empty()) break;
        auto [_, req_id] = scheduler.pq.top();
        scheduler.pq.pop();
        assert(scheduler.active_requests.count(req_id));
        plan.Requests_id.insert(req_id);
    }

    // 执行请求调度
    scheduler.schedule_round(disks, objects, plan, G);
    scheduler.printf_actions(disks, objects, G);
    fflush(stdout);
    
    // 打印完成请求
    scheduler.printf_completed_request(plan, objects);
    scheduler.clean();

}