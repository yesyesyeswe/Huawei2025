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
public:
    // 预处理数据存储
    vector<vector<int>> fre_del;
    vector<vector<int>> fre_write;
    vector<vector<int>> fre_read;

    vector<int> tag_units_need;     // 每个 tag 需要的最大空间
    vector<int> hot_read_tags;      // 热门读取标签
    vector<int> cold_read_tags;     // 冷门读取标签
    set<int> hot_read_tags_set;      // 热门读取标签
    set<int> cold_read_tags_set;     // 冷门读取标签
    int tag_num = 0;                // 总标签个数
    int period = 0;                 // 流程个数

    TagManager(
        int M, 
        int _period
    ) : tag_num(M),
        period(_period)
    {
        fre_del.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        fre_write.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        fre_read.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        hot_read_tags.reserve(tag_num / 2 + 1);
        cold_read_tags.reserve(tag_num / 2 + 1);
        tag_units_need.resize(tag_num + 1);
    }

    // 获取推荐磁盘
    vector<int> select_disk(int tag, int obj_id, const vector<Disk>& disks);
    // 预处理信息
    void process_flequency_info();

    bool is_hot_readTags(int tag) { return hot_read_tags_set.count(tag); }
    bool is_cold_readTags(int tag) { return cold_read_tags_set.count(tag); }

};
#endif