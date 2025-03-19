#include "Disk.hpp"

void Disk::merge_adjacent_blocks() {
    if (free_blocks.size() == 0) return;

    list<Block> merged;
    merged.emplace_back(free_blocks.front());

    for (auto it = free_blocks.begin(); it != free_blocks.end(); it ++) {
        int start = (*it).start;
        int end = (*it).end;
        if (merged.back().end + 1 == start) {
            merged.back().end = end;
        } else {
            merged.emplace_back(start, end);
        }
    }

    free_blocks.swap(merged);
}

void Disk::set_obj_to_unit(int size, int obj_id, vector<int>& allocated_units) {
    assert(size == allocated_units.size());
    for(int i = 0; i < size; i ++) {
        assert(allocated_units[i] != 0);
        units[allocated_units[i]].object_id = obj_id;
        units[allocated_units[i]].object_block = i + 1;
        units[allocated_units[i]].is_used = true;
    }
}

bool Disk::allocate(int size, int obj_id, int& consecutive, vector<int>& allocated_units) {
    // 先尝试分配连续空间
    for (auto it = free_blocks.begin(); it != free_blocks.end(); it ++) {
        int block_size = it->end - it->start + 1;
        if (block_size >= size) {
            allocated_units.clear();
            allocated_units.resize(size);
            std::iota(allocated_units.begin(), allocated_units.end(), it->start);
            it->start += size;
            if (it->start > it->end) {
                free_blocks.erase(it);
            }
            set_obj_to_unit(size, obj_id, allocated_units);
            free_size -= size;
            return true;
        }
    }

    std::vector<int> discrete_units;
    std::vector<Block> new_blocks;  // 记录切割后的剩余块

    // 遍历所有块，收集离散单元
    // 使用迭代器遍历，记录处理位置
    auto it = free_blocks.begin();
    while (it != free_blocks.end() && discrete_units.size() < size) {
        Block& block = *it;
        int available = block.end - block.start + 1;
        int take = std::min(available, size - (int)discrete_units.size());

        // 收集离散单元
        for (int i = 0; i < take; i ++) {
            discrete_units.push_back(block.start + i);
        }

        // 更新当前块
        if (take == available) {
            // 整个块被分配完，删除当前块
            it = free_blocks.erase(it);
        } else {
            // 切割出剩余块，替换当前块
            Block remaining(block.start + take, block.end);
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
    set_obj_to_unit(size, obj_id, allocated_units);
    free_size -= size;
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

void Disk::deallocate(const vector<int>& deallocate_units) {
    if (deallocate_units.empty()) return;
    free_size += deallocate_units.size();

    // 1. 预处理输入：排序、合并连续单元
    vector<int> sorted_units(deallocate_units);
    std::sort(sorted_units.begin(), sorted_units.end());

    vector<Block> new_blocks;
    int current_start = sorted_units[0];
    int current_end = sorted_units[0];
    units[sorted_units[0]].reset();

    for (size_t i = 1; i < sorted_units.size(); ++i) {
        units[sorted_units[i]].reset();
        if (sorted_units[i] == current_end + 1) {
            current_end = sorted_units[i];
        } else {
            new_blocks.emplace_back(current_start, current_end);
            current_start = current_end = sorted_units[i];
        }
    }
    new_blocks.emplace_back(current_start, current_end);

    // 2. 合并新旧块列表
    list<Block> merged_blocks;
    auto old_it = free_blocks.begin();
    auto new_it = new_blocks.begin();

    while (old_it != free_blocks.end() && new_it != new_blocks.end()) {
        // 选择较小的起始块
        if (old_it->start < new_it->start) {
            merge_into(merged_blocks, *old_it++);
        } else {
            merge_into(merged_blocks, *new_it++);
        }
    }

    // 添加剩余块
    while (old_it != free_blocks.end()) merge_into(merged_blocks, *old_it++);
    while (new_it != new_blocks.end()) merge_into(merged_blocks, *new_it++);

    // 3. 最终合并相邻块
    free_blocks.swap(merged_blocks);
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
    int reverse_steps = (current - dest + capacity) % capacity;
    // 如果反向更快，直接跳跃
    if(reverse_steps < direct_steps && get_current_tokens() == 0) {
        assert(actions.empty());
        save_status(dest, MOVE, max_tokens);
        actions = "j " + std::to_string(dest);
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
        actions += string(new_steps, 'p');
        current += new_steps;
        if(new_steps > 0) {
            save_status(current, MOVE, 1);   
        }
        else set_head_position(current);
        actions += '#';
        return true;
    }
    actions += string(direct_steps, 'p');
    consume_tokens(direct_steps);
    set_head_position(dest);
    // 若移动了，设定 prev 情况
    if(dest > current) {    
        set_prev_consum(1);
        set_prev_action(MOVE);
    }
    return false;
}

bool Disk::smart_move(int dest, string& actions) {
    int current = get_head();
    int direct_steps = (dest - current + capacity) % capacity;
    int reverse_steps = (current - dest + capacity) % capacity;
    
    // 跳跃阈值：当逆向更短或直接移动代价过高时跳跃
    if(reverse_steps < direct_steps || direct_steps > max_tokens / 2) {
        if(can_perform(max_tokens)) { // 至少保留读取令牌
            dest = dest > capacity ? dest % capacity : dest;
            actions = "j " + std::to_string(dest);
            save_status(dest, MOVE, max_tokens);
            return true;
        }
    }
    // 否则继续使用p移动
    return move_to_read(dest, actions);
}


bool Disk::get_actions(vector<int>& obj_index, string& actions, vector<int>& units_read_id) {
    // if(!can_perform(0)) return true;
    int current = get_head();
    for(int dest : obj_index) {
        if(dest != current) {
            set_head_position(current);
            bool isdone = smart_move(dest, actions);
            if(isdone) return true;
            current = get_head();
        }
        assert(current == dest);

        int READ_CONSUME = (get_prev_action() == MOVE) ? 64 : std::max(16, (get_prev_consum() * 8 + 9) / 10);

        if(!can_perform(READ_CONSUME)) {
            save_status(current, get_prev_action(), get_prev_consum());
            actions += '#';
            return true;
        }
        actions += 'r';
        units_read_id.push_back(current);
        set_prev_action(READ);
        consume_tokens(READ_CONSUME);
        set_prev_consum(READ_CONSUME);
        assert(current == dest);
        current = dest + 1;
    }

    set_head_position(current);
    return false;
}

void Disk::loop_requests(vector<int>& targets) {
    int head = get_head();
    // targets 是从 set<int> 出来的，天生有序
    //std::sort(targets.begin(), targets.end());

    // 找到第一个不小于head的位置
    auto pivot = lower_bound(targets.begin(), targets.end(), head);
    vector<int> forward(pivot, targets.end());
    vector<int> backward(targets.begin(), pivot);
    
    // 环状处理：将后半部分接到前面
    if(!forward.empty()) {
        forward.insert(forward.end(), backward.begin(), backward.end());
        targets.swap(forward);
        return;
    }
    targets.swap(backward);
    return;
}

// 磁头移动调度
string Disk::schedule_moves(const set<int>& targets_set, unordered_map<int, vector<int>>& obj_info) {
    if (targets_set.empty()) return "#";
    // 保证 targets 里没有重复元素
    vector<int> targets(targets_set.begin(), targets_set.end());
    bool isdone = false;
    vector<int> units_read_id;
    
    // 使用 SCAN 算法规划路径
    string actions;

    loop_requests(targets);

    if(!targets.empty()) isdone |= get_actions(targets, actions, units_read_id);
    
    if(!isdone) {
        actions += '#';
        isdone = true;
    }
    
    for(int unit_id : units_read_id) {
        int obj_id = units[unit_id].object_id;
        int obj_block_id = units[unit_id].object_block;
        obj_info[obj_id].push_back(obj_block_id);
    }
    
    assert(get_current_tokens() <= max_tokens);
    assert(get_prev_action() == MOVE || get_prev_action() == READ);
    assert(get_head() <= capacity && get_head() >= 1);
    assert(actions.size() >= 1);

    reset_tokens();

    return actions;
}