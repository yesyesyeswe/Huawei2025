#ifndef STORAGE_CONTROLLER_HPP
#define STORAGE_CONTROLLER_HPP
#include "RequestScheduler.hpp"
#include "TagManage.hpp"
#include "DynamicTaskQueue.hpp"
#include <utility>
#include <algorithm>
#include <chrono>  // 引入时间库功能
using std::array;


/******************** 主系统控制器 ********************/
class StorageController {
public:
    TagManager tag_manager;
    vector<Disk> disks; 
    RequestScheduler scheduler;
    int current_time = 0;
    int T = 0;
    int prev_stage = 1;
    int capacity;

    // 记录 obj_id 和 对应的 obj
    unordered_map<int, StorageObject> objects;
    // 记录读取的 obj 信息
    vector<unordered_map<int, vector<int>>> obj_info;
    vector<int> busy_disks;

    StorageController(int disk_num, int token_max, int disk_capacity, int _T, int tag_num, int period) : scheduler(disk_num), T(_T), tag_manager(tag_num, period), capacity(disk_capacity) {
        for(int i = 0; i < disk_num; i ++) {
            disks.emplace_back(i, token_max, disk_capacity, tag_num);
        }
        busy_disks.reserve(disk_num);
        obj_info.resize(10);
    }
    
    // 磁盘分区
    void set_partition();

    // 处理删除请求
    void process_delete(vector<int>& deleted_object_id);

    // 处理写入请求
    void process_write_main(vector<StorageObject> &new_objs);
    void process_write(StorageObject& obj);
    
    // 处理读取请求
    void process_read(int req_id, int obj_id) {
        scheduler.add_request(
            req_id, 
            obj_id, 
            current_time, 
            objects[obj_id].get_size(), 
            false
        );
        objects[obj_id].add_request(req_id);
        for(int i = 0; i < REP_NUM; i ++) {
            const auto& replica = objects[obj_id].replicas[i];
            int disk_id = replica.get_disk();
            const auto& units_id = replica.get_units();
            disks[disk_id].set_units_time(units_id, current_time);
        }
    }
    // 获取繁忙磁盘
    void get_busy_disks();
    void printf_actions(const int G);
    //void merge_shards(unordered_map<int, vector<int>>& global_map);
    
    // 执行时间片调度
    void tick(const int G, const int capacity);

};
#endif