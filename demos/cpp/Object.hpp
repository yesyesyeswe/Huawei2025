#ifndef OBJECT_HPP
#define OBJECT_HPP
#include<cmath>
#include<list>
#include "Disk.hpp"
#include <climits>
#include <utility>
#include <unordered_set>
using std::max;
using std::list;
using std::pair;
using std::unordered_set;

/******************** 对象副本类 ********************/
class ObjectReplica {
private:
    int disk_id;                // 数据存放硬盘号
    bool consecutive;           // 副本是否连续存放
    
public:
    vector<int> unit_ids;       // 数据存放单元号

    ObjectReplica(int disk, vector<int> units, int cons) 
        : disk_id(disk), unit_ids(std::move(units)), consecutive(cons) {}
        
    // 计算访问成本
    int access_cost(int head_pos, int capacity, const vector<int>& unit_ids) const;
    
    // Getter方法
    int get_disk() const { return disk_id; }
    const vector<int>& get_units() const { return unit_ids; }
    bool isconsecutive() const { return consecutive; }
};
    
/******************** 对象元数据类 ********************/
class StorageObject {
private:
    int object_id;                      // 物品 id
    int size;                           // 物品大小
    int tag;                            // 物品标签
    
public:
    vector<ObjectReplica> replicas;               // 物品副本
    unordered_set<int> pending_requests;          // 待处理请求列表

    StorageObject(int id, int sz, int tg) : object_id(id), size(sz), tag(tg) {
        replicas.reserve(3);
    }

    StorageObject() : object_id(-1), size(-1), tag(-1) {
        replicas.reserve(3);
    }
        
    // 添加副本
    // 直接接受构造参数（完美转发）
    void add_replica(int disk, vector<int> units, int cons) {
        replicas.emplace_back(disk, std::move(units), cons);
    }
    
    // 获取最佳访问副本
    int get_best_replica_units(int block_id, const vector<int>& head_pos, int capacity, int& best_disk, const vector<int>& space_used);
    
    // 请求管理
    void add_request(int req_id) { pending_requests.insert(req_id); }
    void complete_request(int req_id) { pending_requests.erase(req_id); }

    // Getter 方法
    int get_size() const { return size; };
    const vector<ObjectReplica>& get_replica() const { return replicas; }
};
#endif