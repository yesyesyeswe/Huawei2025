#ifndef TAGMANAGE_HPP
#define TAGMANAGE_HPP
#include "Disk.hpp"
#include <unordered_map>
#include <utility>
#include <map>
using std::map;
using std::unordered_map;
using std::pair;

/******************** 标签管理类 ********************/
class TagManager {
private:
    struct TagProfile {
        vector<int> preferred_disks;    // 该标签推荐优先使用的磁盘 ID 列表
        map<int, int> write_history;    // time_slice -> write_count
    };
    
    unordered_map<int, TagProfile> tag_profiles;   // 以标签ID为键，存储每个标签的专属磁盘配置和写入历史
    
public:
    // 获取推荐磁盘
    int select_disk(int tag, const vector<Disk>& disks);
};
#endif