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
    int tag_num = 0;                                        // 标签个数
    int period = 0;                                         // 流程个数
    unordered_map<int, set<int>> stage_hot_tags_read;       // 热门读取标签
    unordered_map<int, set<int>> stage_hot_tags_delete;     // 热门读取标签
    
public:

    // 预处理数据存储
    vector<vector<int>> fre_del;
    vector<vector<int>> fre_write;
    vector<vector<int>> fre_read;

    TagManager(int M, int _period) : tag_num(M), period(_period) {
        fre_del.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        fre_write.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        fre_read.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        stage_hot_tags_read.reserve(period / 4);
        stage_hot_tags_delete.reserve(period / 4);
    }

    // 获取推荐磁盘
    vector<int> select_disk(int stage, int tag, int obj_id, const vector<Disk>& disks);
    // 处理读取数据
    void selectHotTagsRead(int max_num, int slice, int future_steps) {
        selectHotTagsGeneric(fre_read, stage_hot_tags_read, max_num, slice, future_steps);
    }
    // 处理删除数据
    void selectHotTagsDelete(int max_num, int slice, int future_steps) {
        selectHotTagsGeneric(fre_del, stage_hot_tags_delete, max_num, slice, future_steps);
    }
    bool isHotReadTags(int stage, int tag) { 
        return stage_hot_tags_read[stage].count(tag); 
    }
    bool isHotDeleteTags(int stage, int tag) { 
        return stage_hot_tags_delete[stage].count(tag); 
    }

private:
    void selectHotTagsGeneric(
        const vector<vector<int>>& data, 
        unordered_map<int, set<int>>& result_store, 
        int max_num, 
        int slice, 
        int future_steps
    );
    void Get_all_tag_accsums(
        const vector<vector<int>>& data,
        vector<vector<int>>& all_tag_sums, 
        int slice,
        int future_steps,
        double weight
    ); 
    
};
#endif