#include "Disk.hpp"

void Disk::assert_not_used(int start, int end) {
    for(int i = start; i <= end; i ++) assert(!units[i].is_used);
}

list<Block> Disk::borrow_from_normal_zone(int need) {
    list<Block> borrowed;
    int total_borrowed = 0;
    
    // 优先借用普通区末尾空间（减少对已有分配的影响）
    for (auto it = free_blocks.begin(); it != free_blocks.end(); ) {
        int start = it -> start;
        int end = it -> end;
        int avail = end - start + 1;
        if (avail <= 0) { // 防御性编程：处理无效块
            it = free_blocks.erase(it);
            continue;
        }
        int take = min(avail, need - total_borrowed);
        
        
        if (take > 0) {
            borrowed.emplace_back(start, start + take - 1);
            //assert_not_used(start, start + take - 1);
            update_is_hot_unit(start, start + take - 1, true);
            it->start += take;
            total_borrowed += take;
            
            if (it->start > end) {
                // it 往前走一个，删除 it 本来的数据
                it = free_blocks.erase(it);
            }
            else {
                it ++;
            }
            
            if (total_borrowed >= need) break;
        }
        else {
            if (total_borrowed >= need) break;
            it ++;
        }
    }
    return borrowed;
}

void Disk::release_to_normal_zone(int release_size) {
    // 优先释放热区末尾的连续空间
    auto it = hot_zone_blocks.rbegin();
    list<Block> release_blocks;
    while (release_size > 0 && it != hot_zone_blocks.rend()) {
        int block_size = it->end - it->start + 1;
        
        if (block_size <= release_size) {
            // 整块释放
            release_blocks.emplace_front(*it);
            update_is_hot_unit(it->start, it->end, false);
            //assert_not_used(it->start, it->end);
            release_size -= block_size;
            it = decltype(it)(hot_zone_blocks.erase(std::next(it).base()));
        } else {
            // 切割块
            int new_start = it->end - release_size + 1;
            update_is_hot_unit(new_start, it->end, false);
            //assert_not_used(new_start, it->end);
            release_blocks.emplace_front(new_start, it->end);
            it->end = new_start - 1;
            release_size = 0;
            break;
        }
    }
    // 直接合并到 free_blocks（release_blocks. 已升序）
    free_blocks.merge(release_blocks, [](const Block& a, const Block& b) {
        return a.start < b.start;
    });
    
    // // 合并普通区的相邻块
    // merge_adjacent_blocks();
}


// 计算实际借用的单元数
int borrowed_units(const list<Block>& borrowed) {
    int count = 0;
    for (const auto& block : borrowed) {
        count += block.end - block.start + 1;
    }
    return count;
}

void Disk::adjust_hot_zone(int new_hot_demand) {
    // 当前热区已用空间
    int hot_used = hot_capacity - hot_free_size;

    // 约束2：需求不得超过的上限
    int borrow_max = min(new_hot_demand, max_hot_capacity - hot_capacity);
    
    // 计算需要调整的空间量
    int delta = new_hot_demand - hot_free_size;

    if (delta > 0) {
        // 需要从普通区借用空间
        // 约束3：需求不得超过上限
        int need_borrow = min(delta, borrow_max);
        if(need_borrow > 0) {
            auto borrowed = borrow_from_normal_zone(need_borrow);
            if (!borrowed.empty()) {
                int true_size = borrowed_units(borrowed);
                // 这里 borrowed 是从 free_blocks 从后往前取的
                // 并且每一个新块都插在前面,因此是有序的
                hot_zone_blocks.merge(borrowed, [](const Block& a, const Block& b) {
                    return a.start < b.start;
                });    
                hot_capacity += true_size; // 更新热区最大容量
                hot_free_size += true_size;
                free_size -= true_size;
            }
        }  
    } else if (delta < 0) {
        // 约束4：释放量不得使容量低于空闲空间
        int can_release = min(-delta, hot_free_size);
        // 约束5：释放量不得使容量低于下限
        can_release = min(can_release, hot_capacity - min_hot_capacity);

        // 可以释放到普通区的空间
        if (can_release > 0) {
            release_to_normal_zone(can_release);
            hot_capacity -= can_release; // 缩小热区容量
            hot_free_size -= can_release;
            free_size += can_release;
        }
    }
    
    // 最终约束：确保容量不低于下限且能容纳已用数据
    // 最终检查：不得超过上限
    assert(hot_capacity >= std::max(min_hot_capacity, hot_used));
    assert(hot_capacity <= max_hot_capacity);

    // 定期整合碎片
    merge_adjacent_blocks(hot_zone_blocks);
    merge_adjacent_blocks(free_blocks);
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

void Disk::set_obj_to_unit(int size, int obj_id, vector<int>& allocated_units) {
    assert(size == allocated_units.size());
    for(int i = 0; i < size; i ++) {
        assert(allocated_units[i] != 0);
        units[allocated_units[i]].object_id = obj_id;
        units[allocated_units[i]].object_block = i + 1;
        units[allocated_units[i]].is_used = true;
        if(is_hot_unit[allocated_units[i]]) hot_req_unit_size ++;
    }
}

bool Disk::allocate(int size, int obj_id, int& consecutive, vector<int>& allocated_units, list<Block>& blocks) {
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
            set_obj_to_unit(size, obj_id, allocated_units);
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
    set_obj_to_unit(size, obj_id, allocated_units);
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
    if(reverse_steps < direct_steps || direct_steps > max_tokens - 64) {
        if(can_perform(max_tokens)) { 
            // 如果热区需要读取的很多，则跳跃
            if(hot_req_unit_size >= hot_capacity * 0.5) {
                for(const auto& block : hot_zone_blocks) {
                    if(block.end - block.start + 1 > 5) {
                        actions = "j " + std::to_string(block.start);
                        save_status(block.start, MOVE, max_tokens);
                        return true;
                    }
                }
                int pos = hot_zone_blocks.front().start;
                actions = "j " + std::to_string(pos);
                save_status(pos, MOVE, max_tokens);
                return true;
            }

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

    // auto it = targets_set.lower_bound(head_position);
    // // 计算从 begin 到该迭代器的距离，即小于 head_position 的元素个数
    // int forward_num = std::distance(targets_set.begin(), it);
    // double forward_density = forward_num * 1.0 / head_position;
    // double backward_density = (targets_set.size() - forward_num) * 1.0 / (capacity - head_position + 1);

    // // 如果前面密度比较大，跳到第一个热区大连续块中
    // if(forward_density > backward_density && hot_req_unit_size >= hot_capacity * 0.7) {
    //     for(const auto& block : hot_zone_blocks) {
    //         if(block.end - block.start + 1 > 5) {
    //             actions = "j " + std::to_string(block.start);
    //             save_status(block.start, MOVE, max_tokens);
    //             return;
    //         }
    //     }
    //     int pos = hot_zone_blocks.front().start;
    //     actions = "j " + std::to_string(pos);
    //     save_status(pos, MOVE, max_tokens);
    //     return;
    // }

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
        targets_set.erase(unit_id);
        if(is_hot_unit[unit_id]) hot_req_unit_size --;
    }
    for(int unit_id : pass_away_units) {
        targets_set.erase(unit_id);
        if(is_hot_unit[unit_id]) hot_req_unit_size --;
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