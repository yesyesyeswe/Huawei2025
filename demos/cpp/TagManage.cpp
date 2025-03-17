#include "TagManage.hpp"

vector<int> TagManager::select_disk(int tag, const vector<Disk>& disks) {
    //auto& profile = tag_profiles[tag];
    
    // 初始化：选择空闲最多的三块磁盘
    vector<pair<int, int>> disk_status;
    for(int i = 1; i < disks.size(); i ++) {
        disk_status.emplace_back(disks[i].get_free(), i);
    }
    sort(disk_status.rbegin(), disk_status.rend());
    
    // for(int i = 0; i < 3 && i < disk_status.size(); i ++) {
    //     profile.preferred_disks.push_back(disk_status[i].second);
    // }
    
    // 轮询选择
    // static int rr_ptr[MAX_DISK_NUM] = {0};
    // return profile.preferred_disks[rr_ptr[tag]++ % profile.preferred_disks.size()];
    vector<int> disks_id;
    for (int i = 0; i < 3; ++i) {
        disks_id.push_back(disk_status[i].second);
    }
    return disks_id;
    
}