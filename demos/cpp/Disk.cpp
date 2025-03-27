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
        units[allocated_units[i]].obj_size = size;
        units[allocated_units[i]].is_used = true;
    }
}

void Disk::add_request(const vector<int>& units_id, int _start_time) {
    for(int id : units_id) {
        units[id].start_time.insert(_start_time);
    }
}
void Disk::erase_request(const vector<int>& units_id, int _start_time) {
    for(int id : units_id) {
        units[id].start_time.erase(_start_time);
    }
}

bool Disk::allocate(int size, int obj_id, int& consecutive, vector<int>& allocated_units) {
    // 先尝试分配连续空间
    for (auto it = free_blocks.begin(); it != free_blocks.end(); it ++) {
        int start = it -> start;
        int end = it -> end;
        int block_size = end - start + 1;
        if (block_size >= size) {
            allocated_units.clear();
            allocated_units.resize(size);
            std::iota(allocated_units.begin(), allocated_units.end(), start);
            it -> start += size;
            if (it -> start > end) {
                free_blocks.erase(it);
            }
            set_obj_to_unit(size, obj_id, allocated_units);
            free_size -= size;
            return true;
        }
    }

    std::vector<int> discrete_units;
    discrete_units.reserve(size);

    // 遍历所有块，收集离散单元
    // 使用迭代器遍历，记录处理位置
    auto it = free_blocks.begin();
    while (it != free_blocks.end() && discrete_units.size() < size) {
        Block& block = *it;
        int start = block.start;
        int end   = block.end;
        int available = end - start + 1;
        int take = std::min(available, size - (int)discrete_units.size());

        // 收集离散单元
        discrete_units.insert(discrete_units.end(), start, start + take);

        // 更新当前块
        if (take == available) {
            // 整个块被分配完，删除当前块
            it = free_blocks.erase(it);
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

void Disk::deallocate(const set<int>& deallocate_units) {
    if (deallocate_units.empty()) return;
    free_size += deallocate_units.size();

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
    auto old_it = free_blocks.begin();
    auto new_it = new_blocks.begin();

    while (old_it != free_blocks.end() && new_it != new_blocks.end()) {
        // 选择较小的起始块
        if (old_it->start < new_it->start) {
            merge_into(merged_blocks, *old_it ++);
        } else {
            merge_into(merged_blocks, *new_it ++);
        }
    }

    // 添加剩余块
    while (old_it != free_blocks.end()) merge_into(merged_blocks, *old_it ++);
    while (new_it != new_blocks.end()) merge_into(merged_blocks, *new_it ++);

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
    int reverse_steps = capacity - direct_steps;
    actions.reserve(actions.size() + direct_steps + 3);
    assert(direct_steps > 0);

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
        if(new_steps > 0 && direct_steps - new_steps < 64) {
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

bool Disk::get_actions(vector<int>& obj_index, string& actions) {
    units_read_id.clear(); // 清空成员变量
    actions.reserve(actions.size() + obj_index.size() * 2);

    int current = get_head();
    
    for (int dest : obj_index) {
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

double calculate_time_profit(int time_gap) {
    assert(time_gap >= 0);
    if(time_gap >= 0 && time_gap <= 10) {
        return -0.005 * time_gap + 1; 
    }
    else if(time_gap > 10 && time_gap <= 105) {
        return -0.01 * time_gap + 1.05;
    }
    else {
        return 0;
    }
    assert(0);
    return 0;
}


double Disk::dp(int pos, int token_remains, int contin_read_times, int time, const set<int>& targets, string& actions, bool has_jump) {
    if(token_remains <= 0) return 0;
    if(units_read_id.size() == targets.size()) return 0;
    if(pos > capacity) pos = pos % capacity;
    double jump = -1, pass = -1, read = -1;
    int jump_target = 0;
    if(token_remains == max_tokens && !has_jump) {
        double current_max = -99999;
        for(int target : targets) {
            if(target - pos <= max_tokens - 64) continue;
            double current_score = dp(target, max_tokens, 0, time + 1, targets, actions, true);
            if(current_score > current_max) {
                current_max = current_score;
                jump_target = target;
            }
        }
        // 0.5 为惩罚项
        jump = current_max * 0.5;
    }
    // 有需求
    int read_consume = 64;
    if(targets.count(pos)) {
        read_consume = (contin_read_times >= 8) ? 16 : require_token[contin_read_times];
        if(token_remains >= read_consume) {
            double profit = 0;
            const auto& unit = units[pos];
            const auto& start_times =  unit.start_time;
            int size = unit.obj_size;
            for(int start_time : start_times) {
                profit += 0.5 * (size + 1) * calculate_time_profit(time - start_time) / size;
            }
            units_read_id.push_back(pos);
            read = profit + dp(pos + 1, token_remains - read_consume, contin_read_times + 1, time, targets, actions, false);
        }
        if(token_remains > 64) {
            pass = dp(pos + 1, token_remains - 1, 0, time, targets, actions, false);
        }
    }
    else {
        if(token_remains >= 1) {
            pass = dp(pos + 1, token_remains - 1, 0, time, targets, actions, false);
        }
    }

    double max_profit = std::max({jump, read, pass});
    if(token_remains >= read_consume && std::abs(max_profit - read) < 1e-7) {
        actions = "r" + actions;
    }
    else if(token_remains >= 1 && std::abs(max_profit - pass) < 1e-7) {
        actions = "p" + actions;
    }
    else if(token_remains == max_tokens && std::abs(max_profit - jump) < 1e-7) {
        actions = "j " + std::to_string(jump_target);
        set_head_position(jump_target);
        return -1; // 表明要跳跃
    }
    return max_profit;
}


// 磁头移动调度（版本 2）
void Disk::dp_schedule_moves(const set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions, int time) {
    if (targets_set.empty()) {
        actions += '#';
        return;
    }
    int head = get_head();

    double profit = dp(head, max_tokens, 0, time, targets_set, actions, false);

    // 非跳跃
    if(profit >= 0) {
        int new_head = head + actions.size();
        if(new_head > capacity) new_head %= capacity;
        set_head_position(new_head);
        actions += "#";
    }
    for(int unit_id : units_read_id) {
        int obj_id = units[unit_id].object_id;
        int obj_block_id = units[unit_id].object_block;
        obj_info[obj_id].emplace_back(obj_block_id);
    }
    
    assert(get_head() <= capacity && get_head() >= 1);
    assert(actions.size() >= 1);

    units_read_id.clear();

    return;
}



// 磁头移动调度
void Disk::schedule_moves(const set<int>& targets_set, unordered_map<int, vector<int>>& obj_info, string& actions) {
    if (targets_set.empty()) {
        actions += '#';
        return;
    }

    vector<int> targets;
    targets.reserve(targets_set.size());

    // 转换为环形区域
    loop_requests(targets_set, targets);
    
    // 使用 SCAN 算法规划路径
    bool isdone = targets.empty() || get_actions(targets, actions);
    if (!isdone) actions += '#';

    for(int unit_id : units_read_id) {
        int obj_id = units[unit_id].object_id;
        int obj_block_id = units[unit_id].object_block;
        obj_info[obj_id].emplace_back(obj_block_id);
    }
    
    assert(get_current_tokens() <= max_tokens);
    assert(get_prev_action() == MOVE || get_prev_action() == READ);
    assert(get_head() <= capacity && get_head() >= 1);
    assert(actions.size() >= 1);

    reset_tokens();
    units_read_id.clear();

    return;
}