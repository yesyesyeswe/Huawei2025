#include "Object.hpp"

int ObjectReplica::access_cost(const Disk& disk, const vector<int>& unit_ids) const {
    if(unit_ids.empty()) return INT_MAX;
    
    // 使用SCAN算法计算最优路径
    vector<int> positions = { disk.get_head() };
    positions.insert(positions.end(), unit_ids.begin(), unit_ids.end());
    sort(positions.begin() + 1, positions.end(), [](int a, int b) { return a > b; });
    
    // 计算单向移动总距离
    int cost = 0;
    int prev = positions[0];
    for(int p : positions) {
        if(p >= prev) {
            cost += p - prev;
        } else {
            cost += (disk.get_capacity() - prev) + p;
        }
        prev = p;
    }
    return cost * 64; // 估算令牌消耗
}

const ObjectReplica& StorageObject::get_best_replica(const vector<Disk>& disks) const {
    int min_cost = INT_MAX;
    const ObjectReplica* best = nullptr;
    
    for(const auto& rep : replicas) {
        int cost = rep.access_cost(disks[rep.get_disk()], rep.get_units());
        if(cost < min_cost) {
            min_cost = cost;
            best = &rep;
        }
    }
    return *best;
}
