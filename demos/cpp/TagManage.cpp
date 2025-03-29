#include "TagManage.hpp"

vector<int> TagManager::select_disk(int stage, int tag, int obj_id, const vector<Disk>& disks) {
    
    set<int> disks_id;
    int disks_num = disks.size();
    if(isHotReadTags(stage, tag)) {
        // 随机分片
        int i = 0;
        std::hash<string> tag_hash;
        while(disks_id.size() < 3) {
            size_t hash_val = tag_hash(std::to_string(tag) + std::to_string(obj_id) + std::to_string(i ++));
            int id = 1 + hash_val % (disks_num - 1); // 使得 id = 1:disks_num-1
            disks_id.insert(id);
        }
    }
    else {
        // 选择空闲最多的三块磁盘
        vector<pair<int, int>> disk_status;
        disk_status.resize(disks_num); 
        for(int i = 1; i < disks_num; i ++) {
            disk_status.emplace_back(disks[i].get_free(), i);
        }
        sort(disk_status.rbegin(), disk_status.rend());
        for (int i = 0; i < 3; ++i) {
            disks_id.insert(disk_status[i].second);
        }
    }
    return vector<int>(disks_id.begin(), disks_id.end());
}

void TagManager::Get_all_tag_accsums(
    const vector<vector<int>>& data,
    vector<vector<int>>& all_tag_sums, 
    int slice,
    int future_steps,
    const double _weight
) {
    int small_stages = period / slice + 1;
    all_tag_sums.resize(small_stages);
    
    for (int j_start = 1; j_start <= period; j_start += slice) {
        vector<int> tag_sums(tag_num + 1, 0);
        int j_end = std::min(j_start + slice - 1, period);   
        // --- 计算总和 ---
        for (int i = 1; i <= tag_num; i ++) {
            tag_sums[i] = std::accumulate(data[i].begin() + j_start, data[i].begin() + j_end + 1, 0);
        }   
        // 每个 tag 在 4 个 period 交互次数总和
        all_tag_sums[(j_start - 1) / slice + 1].swap(tag_sums);
    }

    // 计算 accumulate_sum
    double weight = _weight;
    for(int stage = 1; stage < small_stages; stage ++) {
        // 遍历未来 1-future_steps 个阶段
        // 不用遍历自己
        for (int offset = 1; offset < future_steps; offset ++) {
            int actual_stage = stage + offset;
            if (actual_stage >= small_stages) break; // 边界保护
            // 计算该大阶段的加权总和 
            for(int i = 1; i <= tag_num; i ++) {
                all_tag_sums[stage][i] += static_cast<int>(all_tag_sums[actual_stage][i] * weight);
            }
            weight *= _weight; // 衰减系数
        }
    }
    fflush(stdout);
}

void TagManager::selectHotTagsGeneric(
    const vector<vector<int>>& data, 
    unordered_map<int, set<int>>& result_store, 
    int max_num,
    int slice,
    int future_steps,
    double rate,
    unordered_map<int, set<int>>& cold_result_store
) {
    int small_stages = period / slice + 1;
    vector<vector<int>> all_tag_sums;

    Get_all_tag_accsums(data, all_tag_sums, slice, future_steps, rate);

    for(int stage = 1; stage < small_stages; stage ++) {
        // --- 计算平均值 ---
        int total_sum = std::accumulate(all_tag_sums[stage].begin() + 1, all_tag_sums[stage].end(), 0);
        double avg = static_cast<double>(total_sum) / tag_num;

        // --- 筛选热门标签 ---
        vector<pair<int, int>> candidates;
        for (int i = 1; i <= tag_num; i++) {
            candidates.emplace_back(all_tag_sums[stage][i], i);
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        // --- 选择最多max_num个标签 ---
        set<int> hot_tags;
        set<int> cold_tags;
        int count = 0;
        for (const auto& [sum, idx] : candidates) {
            if (count >= max_num) break;
            hot_tags.insert(idx);
            count ++;
        }
        count = 0;
        for(auto it = candidates.rbegin(); it != candidates.rend(); it ++) {
            if (count >= max_num) break;
            cold_tags.insert(it -> second);
            count ++;
        }
        cold_result_store[stage].swap(cold_tags);
    }
}