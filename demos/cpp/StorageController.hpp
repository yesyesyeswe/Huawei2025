#ifndef STORAGE_CONTROLLER_HPP
#define STORAGE_CONTROLLER_HPP
#include "RequestScheduler.hpp"
#include "TagManage.hpp"
#include <algorithm>

/******************** 主系统控制器 ********************/
class StorageController {
public:
    TagManager tag_manager;
    vector<Disk> disks; 
    RequestScheduler scheduler;
    int current_time = 0;

    // 记录 obj_id 和 对应的 obj
    unordered_map<int, StorageObject> objects;

    StorageController(int disk_num, int disk_cap) {
        scheduler = RequestScheduler(disk_num);
        for(int i = 0; i <= disk_num; i ++) {
            disks.emplace_back(i, disk_cap);
        }
    }
    
    // 处理删除请求
    void process_delete(vector<int>& deleted_object_id);

    // 处理写入请求
    void process_write(int obj_id, int size, int tag);
    
    // 处理读取请求
    void process_read(int req_id, int obj_id) {
        scheduler.add_request(req_id, obj_id, current_time, objects[obj_id].get_size());
        objects[obj_id].add_request(req_id);
    }
    
    // 执行时间片调度
    void tick(const int G);

};
#endif