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
    int tag_num = 0;                    // 标签个数
    int period = 0;                     // 流程个数
    
public:

    // 预处理数据存储
    vector<vector<int>> fre_del;
    vector<vector<int>> fre_write;
    vector<vector<int>> fre_read;

    TagManager(int M, int _period) : tag_num(M), period(_period) {
        fre_del.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        fre_write.resize(tag_num + 1, std::vector<int>(period + 1, 0));
        fre_read.resize(tag_num + 1, std::vector<int>(period + 1, 0));
    }

    // 获取推荐磁盘
    vector<int> select_disk(int tag, const vector<Disk>& disks);
    
};
#endif