#include "TagManage.hpp"

vector<int> TagManager::select_disk(int tag, int obj_id, const vector<Disk>& disks) {
    
    set<int> disks_id;
    int disks_num = disks.size();
    
    // 选择空闲最多的三块磁盘
    vector<pair<int, int>> disk_status;
    disk_status.resize(disks_num); 
    for(int i = 1; i < disks_num; i ++) {
        auto& disk = disks[i];
        auto it = disk.tag_to_part.find(tag); // 使用find
        if (it == disk.tag_to_part.end()) {
            assert(0);
        }
        int part_id = it->second; // 获取值
        int free_size = disk.Partitions[part_id].free_size;
        if(free_size > 0) disk_status.emplace_back(free_size, i);
    }
    if(disk_status.size() < 3) disk_status.clear();
    for(int i = 1; i < disks_num; i ++) {
        auto& disk = disks[i];
        auto it = disk.tag_to_part.find(tag); // 使用find
        if (it == disk.tag_to_part.end()) {
            assert(0);
        }
        int part_id = it->second; // 获取值
        int free_size = disk.get_free();
        if(free_size > 0) disk_status.emplace_back(free_size, i);
    }
    sort(disk_status.rbegin(), disk_status.rend());
    for (int i = 0; i < 3; ++i) {
        disks_id.insert(disk_status[i].second);
    }
    
    return vector<int>(disks_id.begin(), disks_id.end());
}

void TagManager::process_flequency_info() {
    vector<int> tag_read_times(tag_num + 1, 0);

    // --- 计算每一个 tag 所需最大存储空间 ---
    for (int i = 1; i <= tag_num; i ++) {
        tag_units_need[i] = std::accumulate(fre_write[i].begin() + 1, fre_write[i].end(), 0);
        tag_units_need[i] -= std::accumulate(fre_del[i].begin() + 1, fre_del[i].end(), 0);
        tag_read_times[i] = std::accumulate(fre_read[i].begin() + 1, fre_read[i].end(), 0);

    }   
    
    // --- 对读取次数进行排序 ---
    vector<pair<int, int>> candidates;
    for (int i = 1; i <= tag_num; i++) {
        candidates.emplace_back(tag_read_times[i], i);
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    // 标签两两配对
    int count = 0;
    for(int i = 0; i < tag_num / 2; i ++) {
        hot_read_tags.push_back(candidates[i].second);
        cold_read_tags.push_back(candidates[tag_num - i - 1].second);
    }
    hot_read_tags_set = set<int>(hot_read_tags.begin(), hot_read_tags.end());
    cold_read_tags_set = set<int>(cold_read_tags.begin(), cold_read_tags.end());
}
