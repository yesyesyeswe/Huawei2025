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

template<typename... Maps>
std::unordered_map<int, std::vector<int>> merge_maps_efficient(const Maps&... maps) {
    std::unordered_map<int, std::vector<int>> result;
    size_t total_keys = 0;
    (..., (total_keys += maps.size()));  // 预估键的数量（C++17 折叠表达式）
    result.reserve(total_keys);

    // 显式遍历每个 map
    auto process_map = [&result](const auto& map) {
        for (const auto& [key, vec] : map) {
            auto& target_vec = result[key];
            target_vec.reserve(target_vec.size() + vec.size());
            target_vec.insert(target_vec.end(), vec.begin(), vec.end());
        }
    };

    // 逐个处理参数包中的 map
    (..., process_map(maps));  // C++17 折叠表达式
    return result;
}

void StorageController::get_busy_disks() {
    auto& plan = scheduler.plan;
    for(int i = 1; i < disks.size(); i ++) {
        if(!plan.units_to_read[i].empty()) {
            busy_disks.push_back(i);
        }
    }
    std::set<int> busy_disks_set(busy_disks.begin(), busy_disks.end());
    assert(busy_disks.size() == busy_disks_set.size());
}

void StorageController::printf_actions(const int G) {
    auto& plan = scheduler.plan;
    vector<string> disk_actions(disks.size());
    
    // 查找繁忙磁盘
    get_busy_disks();

    // 初始化动态任务队列
    

    //任务分片参数
    DynamicTaskQueue task_queue(busy_disks);
    const size_t num_threads = std::min(4UL, busy_disks.size());
    if(num_threads == 0) {
        for(int i = 1; i < disks.size(); i ++) {
            printf("#\n");
        }
        fflush(stdout);
        return;
    }

    std::vector<std::thread> workers;
    for (size_t t = 0; t < num_threads; t ++) {
        workers.emplace_back([&, local_t = t] {
            int disk_id;
            while (task_queue.try_get_task(disk_id)) {
                if (!plan.units_to_read[disk_id].empty()) {
                    std::string actions;
                    disks[disk_id].schedule_moves(plan.units_to_read[disk_id], obj_info[local_t], actions);
                    disk_actions[disk_id] = actions;
                    plan.disk_head_pos[disk_id] = disks[disk_id].get_head();
                } else {
                    disk_actions[disk_id] = "#";
                }
            }
        });
    }

    // 等待所有任务完成
    task_queue.set_done();
    for (auto& t : workers) t.join();

    for(size_t i = 1; i < disk_actions.size(); i ++) {
        if(disk_actions[i].empty()) {
            printf("#\n");
        }
        else printf("%s\n", disk_actions[i].c_str());
    }

    unordered_map<int, vector<int>> obj_infos = std::move(merge_maps_efficient(obj_info[0], obj_info[1], obj_info[2], obj_info[3]));
    
    // 更新请求信息
    if(!obj_infos.empty()) {
        for(auto& [obj_id, obj_blocks] : obj_infos) {
            auto& req_set = objects[obj_id].pending_requests;
            scheduler.update_req(req_set, obj_blocks);
        }
    }  

    for(size_t t = 0; t < num_threads; t ++) { 
        obj_info[t].clear();
    }
    busy_disks.clear();
}

void StorageController::tick(const int G, const int capacity) {
    int disk_num = disks.size();
    int X = 10;
    if(current_time % X == 0 && current_time < T + 51) {
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