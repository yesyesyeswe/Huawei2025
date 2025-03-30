#include "Disk.hpp"

void Disk::assert_not_used(int start, int end) {
    for(int i = start; i <= end; i ++) assert(!units[i].is_used);
}

void Disk::merge_adjacent_blocks(list<Block>& blocks) {
    if (blocks.size() == 0) return;

    list<Block> merged;
    auto it = blocks.begin();
    
    // 避免重复处理第一个块
    merged.push_back(*it++);
    
    while (it != blocks.end()) {
        Block& last = merged.back();
        Block current = *it;
        
        // 仅会相邻
        if (current.start == last.end + 1) {
            last.end = std::max(last.end, current.end);
        } else {
            merged.push_back(current);
        }
        ++it;
    }

    blocks.swap(merged);
}

void Disk::set_obj_to_unit(int tag, int size, int obj_id, vector<int>& allocated_units) {
    assert(size == allocated_units.size());
    auto it = tag_to_part.find(tag); 
        if (it == tag_to_part.end()) {
            assert(0);
        }
    int part_id = it->second; 
    for(int i = 0; i < size; i ++) {
        assert(allocated_units[i] != 0);
        units[allocated_units[i]].object_id = obj_id;
        units[allocated_units[i]].object_block = i + 1;
        units[allocated_units[i]].part_id = part_id;
        units[allocated_units[i]].is_used = true;
        part_req_unit_size[part_id] ++;
        on_heat_unit_num[part_id] ++;
        
    }
}

bool Disk::cold_allocate_Generic(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units, list<Block>& blocks) {
    // 先尝试分配连续空间
    for (auto it = blocks.rbegin(); it != blocks.rend(); it ++) {
        int start = it -> start;
        int end = it -> end;
        int block_size = end - start + 1;
        if (block_size >= size) {
            allocated_units.clear();
            allocated_units.resize(size);
            std::iota(allocated_units.begin(), allocated_units.end(), end - size + 1);
            it -> end = it -> end - size;
            if ((it -> end) < start) {
                blocks.erase(std::next(it).base());
            }
            set_obj_to_unit(tag, size, obj_id, allocated_units);
            return true;
        }
    }

    std::vector<int> discrete_units;
    discrete_units.reserve(size);

    // 遍历所有块，收集离散单元
    // 使用迭代器遍历，记录处理位置
    auto it = blocks.rbegin();
    while (it != blocks.rend() && discrete_units.size() < size) {
        Block& block = *it;
        int start = block.start;
        int end   = block.end;
        int available = end - start + 1;
        if (available <= 0) { // 防御性编程：处理无效块
            assert(0);
            continue;
        }
        int take = min(available, size - (int)discrete_units.size());

        // 收集离散单元：插入 end - take + 1 到 end 的连续值
        for (int i = 0; i < take; i ++) {
            // 逆向存入
            discrete_units.push_back(end - i);
        }

        // 更新当前块
        if (take == available) {
            // 整个块被分配完，删除当前块
            it = decltype(it)(blocks.erase(std::next(it).base()));
        } else {
            // 切割出剩余块，替换当前块
            Block remaining(start, end - take);
            *it = remaining;  // 直接修改原块
            it ++;
        }
    }

    // 检查是否分配成功
    if (discrete_units.size() < size) {
        assert(0);
        return false;
    }

    // 为了保证顺序
    std::reverse(discrete_units.begin(), discrete_units.end());
    allocated_units = std::move(discrete_units);
    set_obj_to_unit(tag, size, obj_id, allocated_units);
    return true; 
}

bool Disk::hot_allocate(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units) {
    int part_id = tag_to_part[tag];
    auto& part = Partitions[part_id];
    if(part.free_size >= size && allocate(tag, size, obj_id, consecutive, allocated_units, part.partition_blocks)) {
        part.free_size -= size;
        ////assert_enough_size_size(part.partition_blocks, part.free_size, part.capacity);
        return true;
    }
    // 借出去不还
    for(int i = 1; i <= tag_to_part.size(); i ++) {
        if(i == tag) continue;
        int part_id = tag_to_part[i % tag_to_part.size()];
        auto& part = Partitions[part_id];
        if(part.is_cold == true) continue;
        if(part.free_size >= size && allocate(tag, size, obj_id, consecutive, allocated_units, part.partition_blocks)) {
            part.free_size -= size;
            part.capacity -= size;
            Partitions[tag_to_part[tag]].capacity += size;
            ////assert_enough_size_size(part.partition_blocks, part.free_size, part.capacity);
            return true;
        }
    }
    for(int i = 1; i <= tag_to_part.size(); i ++) {
        if(i == tag) continue;
        int part_id = tag_to_part[i % tag_to_part.size()];
        auto& part = Partitions[part_id];
        if(part.is_cold == false) continue;
        if(part.free_size >= size && allocate(tag, size, obj_id, consecutive, allocated_units, part.partition_blocks)) {
            part.free_size -= size;
            part.capacity -= size;
            Partitions[tag_to_part[tag]].capacity += size;
            ////assert_enough_size_size(part.partition_blocks, part.free_size, part.capacity);
            return true;
        }
    }
    assert(0);
    return true;
}

bool Disk::cold_allocate(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units) {
    int part_id = tag_to_part[tag];
    auto& part = Partitions[part_id];
    if(part.free_size >= size && cold_allocate_Generic(tag, size, obj_id, consecutive, allocated_units, part.partition_blocks)) {
        part.free_size -= size;
        ////assert_enough_size_size(part.partition_blocks, part.free_size, part.capacity);
        return true;
    }
    // 借出去不还
    for(int i = 1; i <= tag_to_part.size(); i ++) {
        if(i == tag) continue;
        int part_id = tag_to_part[i % tag_to_part.size()];
        auto& part = Partitions[part_id];
        if(part.is_cold == false) continue;
        if(part.free_size >= size && cold_allocate_Generic(tag, size, obj_id, consecutive, allocated_units, part.partition_blocks)) {
            part.free_size -= size;
            part.capacity -= size;
            Partitions[tag_to_part[tag]].capacity += size;
            //assert_enough_size_size(part.partition_blocks, part.free_size, part.capacity);
            return true;
        }
    }
    for(int i = 1; i <= tag_to_part.size(); i ++) {
        if(i == tag) continue;
        int part_id = tag_to_part[i % tag_to_part.size()];
        auto& part = Partitions[part_id];
        if(part.is_cold == true) continue;
        if(part.free_size >= size && cold_allocate_Generic(tag, size, obj_id, consecutive, allocated_units, part.partition_blocks)) {
            part.free_size -= size;
            part.capacity -= size;
            Partitions[tag_to_part[tag]].capacity += size;
            //assert_enough_size_size(part.partition_blocks, part.free_size, part.capacity);
            return true;
        }
    }
    assert(0);
    return false;
}

bool Disk::allocate(int tag, int size, int obj_id, int& consecutive, vector<int>& allocated_units, list<Block>& blocks) {
    // 先尝试分配连续空间
    for (auto it = blocks.begin(); it != blocks.end(); it ++) {
        int start = it -> start;
        int end = it -> end;
        int block_size = end - start + 1;
        if (block_size >= size) {
            allocated_units.clear();
            allocated_units.resize(size);
            std::iota(allocated_units.begin(), allocated_units.end(), start);
            it -> start += size;
            if (it -> start > end) {
                blocks.erase(it);
            }
            set_obj_to_unit(tag, size, obj_id, allocated_units);
            return true;
        }
    }

    std::vector<int> discrete_units;
    discrete_units.reserve(size);

    // 遍历所有块，收集离散单元
    // 使用迭代器遍历，记录处理位置
    auto it = blocks.begin();
    while (it != blocks.end() && discrete_units.size() < size) {
        Block& block = *it;
        int start = block.start;
        int end   = block.end;
        int available = end - start + 1;
        if (available <= 0) { // 防御性编程：处理无效块
            assert(0);
            continue;
        }
        int take = min(available, size - (int)discrete_units.size());

        // 收集离散单元：插入 start 到 start+take-1 的连续值
        for (int i = 0; i < take; i ++) {
            discrete_units.push_back(start + i);
        }

        // 更新当前块
        if (take == available) {
            // 整个块被分配完，删除当前块
            it = blocks.erase(it);
        } else {
            // 切割出剩余块，替换当前块
            Block remaining(start + take, end);
            *it = remaining;  // 直接修改原块
            it ++;
        }
    }

    // 检查是否分配成功
    if (discrete_units.size() < size) {
        assert(0);
        return false;
    }

    allocated_units = std::move(discrete_units);
    set_obj_to_unit(tag, size, obj_id, allocated_units);
    return true; 
}


// 辅助函数：尝试合并到最后一个块
void merge_into(list<Block>& blocks, const Block& new_block) {
    if (blocks.empty()) {
        blocks.emplace_back(new_block);
        return;
    }

    Block& last = blocks.back();
    if (new_block.start > last.end + 1) {
        blocks.emplace_back(new_block);
    } else {
        last.end = std::max(last.end, new_block.end);
    }
}

void Disk::deallocate_main(unordered_map<int, set<int>>& units_id) {
    for(auto& [tag, units_id_part] : units_id) {
        int part_id = tag_to_part[tag];
        auto& part = Partitions[part_id];
        deallocate(units_id_part, part.partition_blocks);
        part.free_size += units_id_part.size();
        //assert_enough_size_size(part.partition_blocks, part.free_size, part.capacity);
    }
}

void Disk::deallocate(const set<int>& deallocate_units, list<Block>& blocks) {
    if (deallocate_units.empty()) return;

    // 1. 预处理输入：合并连续单元
    vector<Block> new_blocks;
    auto it = deallocate_units.begin();
    int current_start = *it;
    int current_end = current_start;
    units[current_start].reset();
    it ++;

    for (; it != deallocate_units.end(); it ++) {
        int unit_pos = *it;
        units[unit_pos].reset();
        if (unit_pos == current_end + 1) {
            current_end = unit_pos;
        } else {
            new_blocks.emplace_back(current_start, current_end);
            current_start = current_end = unit_pos;
        }
    }
    new_blocks.emplace_back(current_start, current_end);

    // 2. 合并新旧块列表
    list<Block> merged_blocks;
    auto old_it = blocks.begin();
    auto new_it = new_blocks.begin();

    while (old_it != blocks.end() && new_it != new_blocks.end()) {
        // 选择较小的起始块
        if (old_it->start < new_it->start) {
            merge_into(merged_blocks, *old_it ++);
        } else {
            merge_into(merged_blocks, *new_it ++);
        }
    }

    // 添加剩余块
    while (old_it != blocks.end()) merge_into(merged_blocks, *old_it ++);
    while (new_it != new_blocks.end()) merge_into(merged_blocks, *new_it ++);

    // 3. 最终合并相邻块
    blocks.swap(merged_blocks);
}

void Disk::save_status(const int pos, const int action, const int consum) {
    set_head_position(pos);
    set_prev_action(action);
    set_prev_consum(consum);
    return; 
}

bool Disk::move_to_read(int dest, string& actions) {
    int current = get_head();
    int direct_steps = (dest - current + capacity) % capacity;
    int reverse_steps = capacity - direct_steps;
    actions.reserve(actions.size() + direct_steps + 3);

    // 如果反向更快，直接跳跃
    if(reverse_steps < direct_steps && get_current_tokens() == 0) {
        assert(actions.empty());
        save_status(dest, MOVE, max_tokens);
        actions = "j " + std::to_string(get_head());
        return true;
    }
    if(!can_perform(direct_steps)) {
        // 没有操作过
        if(get_current_tokens() == 0) {
            assert(actions.empty());
            save_status(dest, MOVE, max_tokens);
            actions = "j " + std::to_string(get_head());
            return true;
        }
        int new_steps = max_tokens - get_current_tokens();
        assert(new_steps >= 0);
        if(new_steps > 0 && direct_steps - new_steps - max_tokens < 64) {
            // 提前移动是有意义的
            actions.append(new_steps, 'p');
            current += new_steps;
            save_status(current, MOVE, 1);   
        }
        else set_head_position(current);
        actions += '#';
        return true;
    }
    actions += string(direct_steps, 'p');
    consume_tokens(direct_steps);
    save_status(dest, MOVE, 1);
    return false;
}

bool Disk::smart_move(int dest, string& actions) {
    int current = get_head();
    int direct_steps = (dest - current + capacity) % capacity;
    int reverse_steps = (current - dest + capacity) % capacity;
    
    // 跳跃阈值：当逆向更短或直接移动代价过高时跳跃
    if(reverse_steps < direct_steps || direct_steps > 2 * max_tokens - 64 - 52 - 42) {
        if(can_perform(max_tokens)) { 
            // 如果分区需要读取的很多，则跳跃
            int current_part_id = (current - 1) / (capacity / part_num) + 1;
            if(part_req_unit_size[current_part_id] <= capacity / part_num * 0.5 && current_time > 9000) {
                 // 动态选择最优跳跃目标
                int best_part = -1;
                double max_score = -1;
                for (int pid = 1; pid <= part_num; ++pid) {
                    double score = get_partition_score(pid);
                    if (score > max_score && part_req_unit_size[pid] >= capacity / part_num * 0.6) {
                        max_score = score;
                        best_part = pid;
                    }
                }

                if (best_part != -1) {
                    int target = Partitions[best_part].partition_blocks.front().start;
                    actions = "j " + std::to_string(target);
                    save_status(target, MOVE, max_tokens);
                    return true;
                }
            }
        }
    }
    // 否则继续使用p移动
    return move_to_read(dest, actions);
}

int Disk::calculate_read_consume() const {
    return (get_prev_action() == MOVE) ? 64 : std::max(16, (get_prev_consum() * 8 + 9) / 10);
}

bool Disk::perform_read(int dest, string& actions, int read_consume) {
    if (!can_perform(read_consume)) {
        save_status(get_head(), get_prev_action(), get_prev_consum());
        actions += '#';
        return true;
    }
    actions += 'r';
    units_read_id.push_back(dest);
    set_prev_action(READ);
    consume_tokens(read_consume);
    set_prev_consum(read_consume);
    return false;
}

bool Disk::get_actions(vector<int>& targets, string& actions) {
    units_read_id.clear(); // 清空成员变量
    actions.reserve(actions.size() + targets.size() * 2);

    int current = get_head();
    for (int dest : targets) {
        if(current_time - units[dest].newest_time >= 105) {
            pass_away_units.push_back(dest);
            continue;
        }
        if (dest != current) {
            set_head_position(current);
            if (smart_move(dest, actions)) 
                return true; // 提前终止
            current = get_head(); // smart_move 已更新磁头位置
            assert(current == dest);
        }
        const int READ_CONSUME = calculate_read_consume();
        if (perform_read(dest, actions, READ_CONSUME)) {
            save_status(current, get_prev_action(), get_prev_consum());
            return true; // 资源不足，终止
        }
        current = dest + 1; // Read 操作会到达下一个位置
    }

    set_head_position(current);
    return false;
}

void Disk::loop_requests(const set<int>& targets_set, vector<int>& result) {
    // 找到第一个不小于head的位置
    int head = get_head();
    vector<int> targets_vector(targets_set.begin(), targets_set.end());  // 转换为 vector
    auto pivot = lower_bound(targets_vector.begin(), targets_vector.end(), head);  // 使用 std::lower_bound

    result.reserve(targets_vector.size());
    result.insert(result.end(), pivot, targets_vector.end());
    result.insert(result.end(), targets_vector.begin(), pivot);
}

// 磁头移动调度
void Disk::schedule_moves(set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions) {
    if (targets_set.empty()) {
        actions += '#';
        return;
    }

    vector<int> targets;
    targets.reserve(targets_set.size());

    // 转换为环形区域
    loop_requests(targets_set, targets);
    
    // 使用 SCAN 算法规划路径
    bool isdone = get_actions(targets, actions);
    if (!isdone) actions += '#';

    for(int unit_id : units_read_id) {
        int obj_id = units[unit_id].object_id;
        int obj_block_id = units[unit_id].object_block;
        obj_info[obj_id].emplace_back(obj_block_id);
        targets_set.erase(unit_id);
        int part_id = units[unit_id].part_id;
        part_req_unit_size[part_id] --;
        on_heat_unit_num[part_id] --;
    }
    for(int unit_id : pass_away_units) {
        targets_set.erase(unit_id);
        int part_id = units[unit_id].part_id;
        part_req_unit_size[part_id] --;
        on_heat_unit_num[part_id] --;
    }
    
    assert(get_current_tokens() <= max_tokens);
    assert(get_prev_action() == MOVE || get_prev_action() == READ);
    assert(get_head() <= capacity && get_head() >= 1);
    assert(actions.size() >= 1);

    reset_tokens();
    units_read_id.clear();
    pass_away_units.clear();

    return;
}


void Disk::set_units_time(const vector<int>& ids, int time) {
    for(int id : ids) {
        units[id].newest_time = time;
    }
}