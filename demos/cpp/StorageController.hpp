#ifndef STORAGE_CONTROLLER_HPP
#define STORAGE_CONTROLLER_HPP
#include "RequestScheduler.hpp"
#include "TagManage.hpp"

/******************** 主系统控制器 ********************/
class StorageController {
private:
    vector<Disk> disks;
    vector<StorageObject> objects;
    TagManager tag_manager;
    RequestScheduler scheduler;
    int current_time = 0;
    
public:
    StorageController(int disk_num, int disk_cap) {
        for(int i = 0; i <= disk_num; i ++) {
            disks.emplace_back(i, disk_cap);
        }
    }
    
    // 处理写入请求
    void process_write(int obj_id, int size, int tag);
    
    // 处理读取请求
    void process_read(int req_id, int obj_id) {
        scheduler.add_request(req_id, obj_id, current_time);
    }
    
    // 执行时间片调度
    void tick(const int G) {
        current_time ++;
        // 重置磁盘令牌
        for(auto& d : disks) d.reset_tokens();
        // 执行请求调度
        scheduler.schedule_round(disks, objects, const int G);
    }
};
#endif