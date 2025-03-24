#include "Object.hpp"

int ObjectReplica::access_cost(int head_pos, int capacity, const vector<int>& unit_ids, const int G) const {
    if(unit_ids.empty()) return INT_MAX;
    
    // 使用 SCAN 算法计算最优路径
    vector<int> positions = { head_pos };
    positions.insert(positions.end(), unit_ids.begin(), unit_ids.end());
    sort(positions.begin() + 1, positions.end(), [](int a, int b) { return a > b; });
    
    // 计算单向移动总距离
    int cost = 0;
    int prev = positions[0];
    for(int p : positions) {
        if(p >= prev) {
            if(p - prev > G) {
                cost += G;
            }
            else {
                cost += p - prev;
            }
        } else {
            if((capacity - prev) + p > G) {
                cost += G;
            }
            else {
                cost += (capacity - prev) + p;
            }
        }
        prev = p;
    }
    return cost; // 估算令牌消耗
}


vector<int> StorageObject::get_best_replica_units(int block_id, const vector<int>& head_pos, int capacity, int& best_disk, const vector<int>& space_used, const unordered_set<int>&read_set, const int G) {
    best_disk = 0;
    int min_cost = INT_MAX;
    const ObjectReplica* best_replica = nullptr;
    if(block_id != -1 && !read_set.empty() && read_set.count(block_id)) return{};

    for(int rep = 0; rep < REP_NUM; rep ++) {
        const auto& replica = replicas[rep];
        const int disk_id = replica.get_disk();
        int cost = 0;
        // 整体处理或分开处理
        if(block_id == -1) {
            if(read_set.empty()) {
                cost = replica.access_cost(head_pos[disk_id], capacity, replica.unit_ids, G);
            }
            else {
                vector<int> units_not_read;
                units_not_read.reserve(5);
                for(int i = 0 ; i < get_size(); i ++) {
                    if(!read_set.count(i + 1)) units_not_read.push_back(replica.unit_ids[i]);
                }
                if(units_not_read.empty()) return {};
                cost = replica.access_cost(head_pos[disk_id], capacity, units_not_read, G);
            }
        }
        else {
            cost = replica.access_cost(head_pos[disk_id], capacity, {replica.unit_ids[block_id]}, G);
        }
        // 磁盘要处理的数据比较多时，倾向于不选择
        cost += space_used[disk_id];
        // 优先选择连续存放的副本
        if(replica.isconsecutive()) cost -= 50;
        if(cost < min_cost) {
            min_cost = cost;
            best_replica = &replica;
            best_disk = disk_id;
        }
    }
    assert(best_disk >= 1);
    if(block_id != -1) return {(*best_replica).unit_ids[block_id]};
    return (*best_replica).unit_ids;
}

// const ObjectReplica& StorageObject::get_best_replica(const vector<Disk>& disks) const {
//     int min_cost = INT_MAX;
//     const ObjectReplica* best = nullptr;
    
//     for(const auto& rep : replicas) {
//         int cost = rep.access_cost(disks[rep.get_disk()], rep.get_units());
//         if(cost < min_cost) {
//             min_cost = cost;
//             best = &rep;
//         }
//     }
//     return *best;
// }
