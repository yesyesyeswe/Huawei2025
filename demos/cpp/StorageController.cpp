#include "StorageController.hpp"

void StorageController::process_delete(vector<int>& deleted_object_id) {
    int n_abort = 0;
    vector<int> abort_reqs;
    vector<set<int>> units_to_release;
    units_to_release.resize(disks.size());
    for(int obj_id : deleted_object_id) {
        StorageObject& obj = objects[obj_id];
        vector<ObjectReplica>& replicas = obj.replicas;
        // 磁盘清理
        for(auto& replica : replicas) {
            for(int unit_id : replica.get_units()) {
                units_to_release[replica.get_disk()].insert(unit_id); 
            }
        }
        for(int i = 1; i < units_to_release.size(); i ++) {
            disks[i].deallocate(units_to_release[i]);
        }
        // 请求清理
        for(int req_id : objects[obj_id].pending_requests) {
            if(scheduler.active_requests.count(req_id)) {
                n_abort ++;
                abort_reqs.push_back(req_id);

                // 删除请求
                scheduler.active_requests.erase(req_id);
                scheduler.plan.Requests_id.erase(req_id);
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
        selected_disks = tag_manager.select_disk(tag, disks);
    }
    
    // 分配存储空间
    StorageObject obj(obj_id, size, tag);
    printf("%d\n", obj_id);
    for(int d : selected_disks) {
        int consecutive = 0;
        vector<int> units;
        bool success = disks[d].allocate(size, obj_id, consecutive, units);
        assert(success);
        printf("%d ", d);
        for(int i = 0; i < units.size(); i ++) {
            if(i != units.size() - 1) printf("%d ", units[i]);
            else printf("%d", units[i]);
        }
        printf("\n");
        obj.add_replica(d, std::move(units), consecutive);
    }
    
    objects[obj_id] = obj;
}

void StorageController::printf_actions(const int G) {
    auto& plan = scheduler.plan;

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
            scheduler.update_req(req_set, obj_blocks);
        }
    }
}

void StorageController::tick(const int G, const int capacity) {
    int disk_num = disks.size();
    int X = 10;
    if(current_time % X == 0) {
        // 记录新请求
        unordered_set<int> new_request;
        new_request.reserve(2 * disk_num);

        // 若 Request 太少，则增加
        size_t current_max = scheduler.calculate_dynamic_max(disk_num);
        if(scheduler.should_accept_new_requests(current_max)) {
            scheduler.get_request_to_process(new_request, current_time, disk_num, current_max);
        }

        // 计算时间
        auto start = std::chrono::high_resolution_clock::now();

        // 执行请求调度
        scheduler.schedule_round(new_request, objects, current_time, G, capacity);
        printf_actions(G);
        fflush(stdout);

        // 打印完成请求
        scheduler.printf_completed_request(objects);

        auto end = std::chrono::high_resolution_clock::now();
        double time_cost = std::chrono::duration<double>(end - start).count();
        int free_unit = 0;
        for(auto& d : disks) free_unit += d.get_free();
        scheduler.record_metrics(time_cost, free_unit, capacity * (disk_num - 1));

        scheduler.clean();
    }
    else {
        // 不处理读取请求
        for(int i = 1; i < disk_num; i ++) {
            printf("#\n");
        }
        printf("%d\n", 0);
        fflush(stdout);
    }
}