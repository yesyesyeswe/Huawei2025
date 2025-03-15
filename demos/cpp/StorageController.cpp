#include "StorageController.hpp"

void StorageController::process_write(int obj_id, int size, int tag) {
    // 选择目标磁盘
    vector<int> selected_disks;
    while(selected_disks.size() < REP_NUM) {
        int d = tag_manager.select_disk(tag, disks);
        if(find(selected_disks.begin(), selected_disks.end(), d) == selected_disks.end()) {
            selected_disks.push_back(d);
        }
    }
    
    // 分配存储空间
    StorageObject obj(obj_id, size, tag);
    for(int d : selected_disks) {
        auto units = disks[d].allocate(size);
        obj.add_replica(ObjectReplica(d, units));
    }
    
    objects[obj_id] = obj;
}