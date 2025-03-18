#include "Disk.hpp"

vector<int> Disk::allocate(int size, int obj_id, int& consecutive) {
    vector<int> allocated;
    vector<int> candidate;
    consecutive = 0;
    int size_copy = size;
    
    // 寻找连续空间
    queue<int> free_units_copy = free_units;
    while(!free_units_copy.empty()) {
        int u = free_units_copy.front();
        free_units_copy.pop();
        
        // 若候选集空，则添加
        if(candidate.empty() || u == candidate.back() + 1) {
            candidate.push_back(u);
            if(++consecutive >= size) {
                // 注意 candidate.end() - size != candidate.begin()
                // 因为分配是一直进行的
                allocated = vector<int>(candidate.end() - size, candidate.end());
                break;
            }
        } else {
            candidate.clear();
            consecutive = 0;
        }
    }
    
    // 没有足够连续空间则分配离散
    if(allocated.empty()) {
        while(!free_units.empty() && size -- ) {
            int u = free_units.front();
            free_units.pop();
            allocated.push_back(u);
            assert(u != 0);
            units[u].is_used = true;
        }
    }
    
    // 更新单元状态
    for(int u : allocated) {
        assert(u != 0);
        units[u].is_used = true;
        // 从空闲队列移除
        queue<int> new_queue;
        while(!free_units.empty()) {
            int x = free_units.front();
            free_units.pop();
            if(find(allocated.begin(), allocated.end(), x) == allocated.end()) {
                new_queue.push(x);
            }
        }
        free_units = new_queue;
    }
    
    if(size_copy != allocated.size()) {
        printf("%dK%ldK%d\n", size_copy, allocated.size(), size);
        fflush(stdout);
    }
    assert(size_copy == allocated.size());
    for(int i = 0; i < size; i ++) {
        assert(allocated[i] != 0);
        units[allocated[i]].object_id = obj_id;
        units[allocated[i]].object_block = i + 1;
        units[allocated[i]].is_used = true;
    }
    return allocated;
}

void Disk::save_status(const int pos, const int action, const int consum) {
    set_head_position(pos);
    set_prev_action(action);
    set_prev_consum(consum);
    return; 
}

bool Disk::get_actions(vector<int>& obj_index, string& actions, vector<int>& units_read_id) {
    // if(!can_perform(0)) return true;
    int current = get_head();
    for(int t : obj_index) {
        int steps = t - current;
        assert(steps >= 0);

        if(steps > 0) {
            if(!can_perform(steps)) {
                int new_steps = max_tokens - get_current_tokens();
                assert(new_steps >= 0);
                actions += string(new_steps, 'p');
                current += new_steps;
                move_step += new_steps;
                if(new_steps > 0) {
                    save_status(current, MOVE, 1);
                }
                else set_head_position(current);
                actions += '#';
                return true;
            }
            actions += string(steps, 'p');
            current += steps;
            move_step += steps;
            assert(get_head() + move_step == current);
            set_prev_action(MOVE);
            consume_tokens(steps);
            set_prev_consum(1);
        }
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
        assert(current == t);
        current = t + 1;
        move_step ++;
        assert(get_head() + move_step == current);
    }

    assert(get_head() + move_step == current);
    set_head_position(current);
    return false;
}

bool Disk::move_to_read(int destination, string& actions) {
    // if(!can_perform(0)) return true;
    int current = get_head();
    int steps = destination - current;
    assert(destination >= current);
    if(!can_perform(steps)) {
        // 没有操作过
        if(get_current_tokens() == 0) {
            assert(actions.empty());
            move_step += steps;
            save_status(destination, MOVE, max_tokens);
            actions = "j " + std::to_string(get_head());
            return true;
        }
        int new_steps = max_tokens - get_current_tokens();
        assert(new_steps >= 0);
        actions += string(new_steps, 'p');
        current += new_steps;
        move_step += new_steps;
        if(new_steps > 0) {
            save_status(current, MOVE, 1);   
        }
        else set_head_position(current);
        actions += '#';
        return true;
    }
    actions += string(steps, 'p');
    consume_tokens(steps);
    move_step += steps;
    set_head_position(destination);
    // 若移动了，设定 prev 情况
    if(destination > current) {    
        set_prev_consum(1);
        set_prev_action(MOVE);
    }
    return false;
}

std::pair<vector<int>, vector<int>> Disk::separate_requests(
    const vector<int>& targets) {
    int head = get_head();
    // 创建一个副本以避免修改原始数据
    vector<int> sorted_targets = targets;
    std::sort(sorted_targets.begin(), sorted_targets.end());

    // 使用 std::partition 分离左右请求
    auto pivot = std::partition(sorted_targets.begin(), sorted_targets.end(),
                                 [head](int t) { return t < head; });

    // 左侧请求：小于当前磁头位置
    vector<int> left(sorted_targets.begin(), pivot);

    // 右侧请求：大于或等于当前磁头位置
    vector<int> right(pivot, sorted_targets.end());

    assert((left.empty() || left.back() < head) && (right.empty() || head <= right.front()));
    return {left, right};
}

// 磁头移动调度
// To Improve
string Disk::schedule_moves(const set<int>& targets_set, unordered_map<int, vector<int>>& obj_info) {
    if (targets_set.empty()) return "#";
    // 保证 targets 里没有重复元素
    vector<int> targets(targets_set.begin(), targets_set.end());
    bool isdone = false;
    vector<int> units_read_id;
    
    // 使用 SCAN 算法规划路径
    string actions;

    // 分离左右请求
    auto [left, right] = separate_requests(targets);

    if(!right.empty()) {
        int begin = right.front();
        // 预移动
        isdone |= move_to_read(begin, actions);
        
        // 处理右半部分
        if(!isdone)
        isdone |= get_actions(right, actions, units_read_id);
    }
    
    // 处理左半部分
    if(!isdone && !left.empty() && left.back() < head_position) {
        // 预移动到头
        isdone |= move_to_read(capacity + 1, actions);
        // 处理左半部分
        if(!isdone)
        isdone |= get_actions(left, actions, units_read_id);
    }
    
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
    reset_move();

    return actions;
}