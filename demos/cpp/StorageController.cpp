#include "StorageController.hpp"

void StorageController::set_partition() {
    auto hot = tag_manager.hot_read_tags;
    auto cold = tag_manager.cold_read_tags;
    const auto& tag_units_need = tag_manager.tag_units_need;
    int total_units_need = std::accumulate(tag_units_need.begin(), tag_units_need.end(), 0);
    int tag_half_num = hot.size();
    int spare_units = static_cast<int>(0.95 * capacity) - 1;

    int disk_num = disks.size();
    for(int i = 1; i < disk_num; i ++) {
        std::random_shuffle(hot.begin(), hot.end());
        std::random_shuffle(cold.begin(), cold.end());
        int begin = 1;
        auto& disk = disks[i];
        for(int j = 0; j < tag_half_num; j ++) {
            int hot_tag_id = hot[j];
            int hot_need = static_cast<int>(tag_units_need[hot_tag_id] * 1.0 / total_units_need * spare_units);
            disk.add_partition(hot_tag_id, 2 * j + 1, begin, hot_need, false);
            begin += hot_need;

            int cold_tag_id = cold[j];
            int cold_need = static_cast<int>(tag_units_need[cold_tag_id] * 1.0 / total_units_need * spare_units);
            disk.add_partition(cold_tag_id, 2 * j + 2, begin, cold_need, true);
            begin += cold_need;
        }
        if(begin <= spare_units) {
            disk.reserve_blocks.emplace_front(begin, spare_units);
            disk.reserve_free_size += spare_units - begin + 1;
        }
    }
}

void StorageController::process_delete(vector<int>& deleted_object_id) {
    int n_abort = 0;
    vector<int> abort_reqs;
    vector<unordered_map<int, set<int>>> units_to_release;
    units_to_release.resize(disks.size());
    for(int obj_id : deleted_object_id) {
        StorageObject& obj = objects[obj_id];
        vector<ObjectReplica>& replicas = obj.replicas;
        int tag = obj.get_tag();
        // 磁盘清理
        for(auto& replica : replicas) {
            auto& units = replica.get_units();
            for(int unit_id : units) {
                units_to_release[replica.get_disk()][tag].insert(unit_id); 
            }
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
    // bug-fixed 必须在外面
    for(int i = 1; i < units_to_release.size(); i ++) {
        if(units_to_release[i].empty()) continue;
        disks[i].deallocate_main(units_to_release[i]);
    }
    assert(n_abort == abort_reqs.size());
    printf("%d\n", n_abort);
    for(int i = 0; i < n_abort; i ++) {
        printf("%d\n", abort_reqs[i]);
    }
}

void StorageController::process_write_main(vector<StorageObject>&new_objs) {
    
    // 按对象大小降序排序
    std::sort(new_objs.begin(), new_objs.end(), [](const auto& a, const auto& b) {
        return a.get_size() > b.get_size(); 
    });

    // 按序处理写入
    for (auto& obj : new_objs) {
        process_write(obj);
    }
}

void StorageController::process_write(StorageObject& obj) {
    int tag = obj.get_tag();
    int obj_id = obj.get_obj_id();
    int size = obj.get_size();
    // 选择目标磁盘
    vector<int> selected_disks;
    while(selected_disks.size() < REP_NUM) {
        selected_disks = tag_manager.select_disk(tag, obj_id, disks);
    }
    
    // 分配存储空间
    printf("%d\n", obj_id);
    for(int d : selected_disks) {
        int consecutive = 0;
        vector<int> units;
        bool success;
        if(tag_manager.is_hot_readTags(tag))
            success = disks[d].hot_allocate(tag, size, obj_id, consecutive, units);
        else 
            success = disks[d].cold_allocate(tag, size, obj_id, consecutive, units);
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
}

struct alignas(64) PaddedString {
    std::string data;
    char padding[64];
};


void StorageController::printf_actions(const int G) {
    auto& plan = scheduler.plan;
    vector<PaddedString> disk_actions(disks.size());
    
    // 查找繁忙磁盘
    get_busy_disks();
    
    if(busy_disks.size() == 0) {
        for(int i = 1; i < disks.size(); i ++) {
            printf("#\n");
        }
        fflush(stdout);
        return;
    }

    //任务分片参数
    const size_t num_threads = std::min(1UL, busy_disks.size());
    vector<std::thread> workers;
    // 动态计算分片参数
    const size_t total_disks = busy_disks.size();
    const size_t base_chunk = total_disks / num_threads;
    const size_t remainder = total_disks % num_threads;

    size_t start_idx = 0;
    for (size_t t = 0; t < num_threads; t ++) {
        // 计算当前线程分配数量
        const size_t chunk = base_chunk + (t < remainder ? 1 : 0);
        const size_t end_idx = start_idx + chunk;
        workers.emplace_back([&, local_t = t, local_start_idx = start_idx, local_end_idx = end_idx] {
            for (size_t i = local_start_idx; i < local_end_idx; i ++) {
                const int disk_id = busy_disks[i];
                if (!plan.units_to_read[disk_id].empty()) {
                    std::string actions;
                    actions.reserve(G + 1);
                    disks[disk_id].current_time = current_time;
                    disks[disk_id].schedule_moves(plan.units_to_read[disk_id], obj_info[local_t], actions);
                    disk_actions[disk_id].data = actions;
                    plan.disk_head_pos[disk_id] = disks[disk_id].get_head();
                } else {
                    disk_actions[disk_id].data = "#";
                }
            }
        });
        start_idx = end_idx; // 更新起始索引
    }

    // 等待所有任务完成
    for (auto& t : workers) t.join();

    // 合并输出缓冲
    std::string output_buffer;
    output_buffer.reserve(disk_actions.size() * 64);
    for (size_t i = 1; i < disk_actions.size(); i++) {
        if (disk_actions[i].data.empty() || disk_actions[i].data == "#") {
            output_buffer += "#\n";
        } else {
            output_buffer += disk_actions[i].data + "\n";
        }
    }
    printf("%s", output_buffer.c_str());
    fflush(stdout);

    // 直接使用线程局部数据更新请求
    for (size_t t = 0; t < num_threads; t ++) {
        for (auto& [obj_id, obj_blocks] : obj_info[t]) {
            auto& req_set = objects[obj_id].pending_requests;
            scheduler.update_req(req_set, obj_blocks);
        }
        obj_info[t].clear();
    }
    busy_disks.clear();
}

void StorageController::tick(const int G, const int capacity) {
    int disk_num = disks.size();
    int X = 1;
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
        for(int i = 1; i < disk_num; i ++) free_unit += disks[i].get_free();
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