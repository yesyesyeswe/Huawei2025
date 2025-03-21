#include "Object.hpp"

int ObjectReplica::access_cost(int head_pos, int capacity, const vector<int>& unit_ids) const {
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
            cost += p - prev;
        } else {
            cost += (capacity - prev) + p;
        }
        prev = p;
    }
    return cost; // 估算令牌消耗
}


int StorageObject::get_best_replica_units(int block_id, const vector<int>& head_pos, int capacity, int& best_disk, const vector<int>& space_used) {
    best_disk = 0;
    int min_cost = 9999999;
    const ObjectReplica* best_replica = nullptr;

    for(int rep = 0; rep < REP_NUM; rep ++) {
        const auto& replica = replicas[rep];
        const int disk_id = replica.get_disk();
        int cost = replica.access_cost(head_pos[disk_id], capacity, {replica.unit_ids[block_id]});
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
    assert(best_disk >= 0);
    return (*best_replica).unit_ids[block_id];
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
