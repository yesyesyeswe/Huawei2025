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
            units[u].is_used = true;
        }
    }
    
    // 更新单元状态
    for(int u : allocated) {
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

bool Disk::get_actions(vector<int>& obj_index, string& actions, vector<int>& units_read_id, const int G) {
    if(!can_perform(1, G)) return true;
    int current = get_head();
    for(int t : obj_index) {
        int steps = t - current;

        if(steps > 0) {
            if(!can_perform(steps, G)) {
                int new_steps = G - get_current_tokens();
                assert(new_steps >= 0);
                if(new_steps > 0) {
                    actions += string(new_steps, 'p');
                    current += new_steps;
                    save_status(current, MOVE, 1);
                }
                actions += '#';
                return true;
            }
            actions += string(steps, 'p');
            current += steps;
            set_prev_action(MOVE);
            consume_tokens(steps);
            set_prev_consum(1);
        }
        assert(steps >= 0);
        int READ_CONSUME = (get_prev_action() == MOVE) ? 64 : std::max(16, (get_prev_consum() * 8 + 9) / 10);

        if(!can_perform(READ_CONSUME, G)) {
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
    }

    set_head_position(current);
    //set_prev_consum(get_current_tokens());
    return false;
}

/*
(gdb) p current
$2 = 3041
(gdb) p t
$3 = 3040
*/

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

bool Disk::move_to_read(int destination, string& actions, const int G, const int V) {
    if(!can_perform(1, G)) return true;
    int current = get_head();
    assert(destination >= current);
    if(!can_perform(destination - current, G)) {
        // 没有操作过
        if(get_current_tokens() == 0) {
            assert(actions.empty());
            destination = (destination > V) ? (destination % V) : destination;
            save_status(destination, MOVE, G);
            actions = "j " + std::to_string(destination);
            return true;
        }
        int new_steps = G - get_current_tokens();
        assert(new_steps >= 0);
        if(new_steps > 0) {
            actions += string(new_steps, 'p');
            current += new_steps;
            current = (current > V) ? (current % V) : current;
            save_status(current, MOVE, 1);   
        }
        actions += '#';
        return true;
    }
    // 若移动了，设定 prev 情况
    if(destination > current) {
        actions += string(destination - current, 'p');
        consume_tokens(destination - current);
        set_prev_consum(1);
        set_prev_action(MOVE);
        destination = (destination > V) ? (destination % V) : destination;
        set_head_position(destination);
    }
    return false;
}

// 磁头移动调度
// To Improve
string Disk::schedule_moves(const vector<int>& targets, unordered_map<int, vector<int>>& obj_info, const int G) {
    if (targets.empty()) return "#";
    bool isdone = false;
    vector<int> units_read_id;
    
    // 使用 SCAN 算法规划路径
    string actions;

    // 分离左右请求
    auto [left, right] = separate_requests(targets);

    const int V = get_capacity();
    if(!right.empty()) {
        int begin = right.front();
        // 预移动
        isdone |= move_to_read(begin, actions, G, V);
        
        // 处理右半部分
        if(!isdone)
        isdone |= get_actions(right, actions, units_read_id, G);
    }
    
    // 处理左半部分
    if(!isdone && !left.empty() && left.back() < head_position) {
        // 预移动到头
        isdone |= move_to_read(V + 1, actions, G, V);
        // 处理左半部分
        if(!isdone)
        isdone |= get_actions(left, actions, units_read_id, G);
    }
    

    if(!isdone) actions += '#';

    if(!units_read_id.empty()) {
        for(int unit_id : units_read_id) {
            int obj_id = units[unit_id].object_id;
            int obj_block_id = units[unit_id].object_block;
            obj_info[obj_id].push_back(obj_block_id);
        }
    }

    assert(get_current_tokens() <= G);
    assert(get_prev_action() == MOVE || get_prev_action() == READ);
    assert(get_head() <= V && get_head() >= 1);
    assert(actions.size() >= 1);

    reset_tokens();

    return actions;
}