#ifndef STORAGE_CONTROLLER_HPP
#define STORAGE_CONTROLLER_HPP
#include "RequestScheduler.hpp"
#include "TagManage.hpp"
#include "DynamicTaskQueue.hpp"
#include <utility>
#include <algorithm>
#include <chrono>  // 引入时间库功能


/******************** 主系统控制器 ********************/
class StorageController {
public:
    TagManager tag_manager;
    vector<Disk> disks; 
    RequestScheduler scheduler;
    int current_time = 0;
    int T = 0;

    // 记录 obj_id 和 对应的 obj
    unordered_map<int, StorageObject> objects;
    // 记录读取的 obj 信息
    vector<unordered_map<int, vector<int>>> obj_info;
    vector<int> busy_disks;

    StorageController(int disk_num, int disk_cap, int disk_units_num, int _T, int tag_num, int period) : scheduler(disk_num), T(_T), tag_manager(tag_num, period) {
        for(int i = 0; i < disk_num; i ++) {
            disks.emplace_back(i, disk_cap, disk_units_num);
        }
        obj_info.resize(4);
        busy_disks.reserve(disk_num);
    }
    
    // 处理删除请求
    void process_delete(vector<int>& deleted_object_id);

    // 处理写入请求
    void process_write(int stage, int obj_id, int size, int tag);
    
    // 处理读取请求
    void process_read(int req_id, int obj_id, int stage) {
        scheduler.add_request(
            req_id, 
            obj_id, 
            current_time, 
            objects[obj_id].get_size(), 
            tag_manager.isHotDeleteTags(stage, objects[obj_id].get_tag())
        );
        objects[obj_id].add_request(req_id);
    }
    // 获取繁忙磁盘
    void get_busy_disks();
    void printf_actions(const int G);
    
    // 执行时间片调度
    void tick(const int G, const int capacity);

};
#endif