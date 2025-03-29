#include <cstdio>
#include <cassert>
#include <cstdlib>

#include "Constant.hpp"
#include "Disk.hpp"
#include "Object.hpp"
#include "RequestScheduler.hpp"
#include "TagManage.hpp"
#include "StorageController.hpp"

// #include <iostream>
// #include <fstream>
// #include <sstream>
// using namespace std;

/*
T：代表本次数据有 𝑇+105个时间片，后续输入第二阶段将循环交互 𝑇+105次。 
M：代表对象标签数。对象标签编号为 1 ~ 𝑀。
N：代表存储系统中硬盘的个数，硬盘编号为 1 ~ 𝑁。
V：代表存储系统中每个硬盘的存储单元个数。存储单元编号为1 ~ 𝑉。
G：代表每个磁头每个时间片最多消耗的令牌数。 
*/

int timestamp_action()
{
    int timestamp = 0;
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
    return timestamp;
}

int main()
{
    // ifstream file("output.txt");
    // if (!file.is_open()) {
    //     cerr << "无法打开文件 output.txt" << endl;
    //     return 1;
    // }

    // unordered_map<int, int> dataMap;
    // string line;
    // while (getline(file, line)) {
    //     istringstream iss(line);
    //     int timestamp, disk3;
    //     if (iss >> timestamp >> disk3) {
    //         dataMap[timestamp] = disk3;
    //     } else {
    //         cerr << "格式错误: " << line << endl;
    //     }
    // }
    //
    // file.close();


    int T, M, N, V, G;
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    int read_max_tags_choose = 6;
    int delete_max_tags_choose = 5;
    int read_slice = 4;
    int read_future_steps = 2;
    int delete_slice = 12;
    int delete_future_steps = 3;

    StorageController controller(N + 1, G, V, T, M, (T - 1) / FRE_PER_SLICING + 1);
    TagManager& Tag = controller.tag_manager;
    Tag.read_max_tags_choose = read_max_tags_choose;
    Tag.delete_max_tags_choose = delete_max_tags_choose;
    Tag.read_slice = read_slice;
    Tag.read_future_steps = read_future_steps;
    Tag.delete_slice = delete_slice;
    Tag.delete_future_steps = delete_future_steps;

     // 预处理数据加载
     auto load_fre = [M, T](vector<vector<int>>& dest) {
        //dest.resize(M + 1); 已提前初始化
        for(int i = 1; i <= M; i ++) {
            int period = (T - 1) / FRE_PER_SLICING + 1;
            //dest[i].resize(period + 1); 已提前初始化
            for(int j = 1; j <= period; j ++)
                scanf("%d", &dest[i][j]);
        }
    };
    
    load_fre(Tag.fre_del);
    load_fre(Tag.fre_write); 
    load_fre(Tag.fre_read);

    Tag.selectHotTagsDelete(delete_max_tags_choose, delete_slice, delete_future_steps);
    Tag.selectHotTagsRead(read_max_tags_choose, read_slice, read_future_steps);

    printf("OK\n");
    fflush(stdout);

    for(int t = 1; t <= T + EXTRA_TIME; t ++) {
        // 处理时间片对齐
        controller.current_time = timestamp_action();
        // if(controller.current_time > 1 && dataMap[controller.current_time - 1] != controller.disks[10].get_head()) {
        //     printf("%d!=%d\n", dataMap[controller.current_time - 1], controller.disks[10].get_head());
        //     assert(0);
        // }
        
        // 处理删除事件
        int n_delete;
        scanf("%d", &n_delete);
        // 处理删除逻辑 To do
        vector<int> deleted;
        for(int i = 0; i < n_delete; i ++) {
            int obj_id;
            scanf("%d", &obj_id);
            deleted.push_back(obj_id);
        }
        controller.process_delete(deleted);
        fflush(stdout);

        
        // 处理写入请求
        int n_write;
        scanf("%d", &n_write);
        vector<StorageObject> new_objs;
        if(n_write) new_objs.reserve(n_write);
        int stage = (controller.current_time - 1) / (FRE_PER_SLICING * read_slice) + 1;
        for(int i = 0; i < n_write; i ++) {
            int id, size, tag;
            scanf("%d %d %d", &id, &size, &tag);
            new_objs.emplace_back(id, size, tag);
        }
        if(n_write) controller.process_write_main(stage, new_objs);
        fflush(stdout);
        
        // 处理读取请求
        int n_read;
        scanf("%d", &n_read);
        for(int i = 0; i < n_read; i ++) {
            int req_id, obj_id;
            scanf("%d %d", &req_id, &obj_id);
            controller.process_read(req_id, obj_id, (controller.current_time - 1) / (FRE_PER_SLICING * delete_slice) + 1);
        }
        
        // 推进时间
        controller.tick(G, V);
        fflush(stdout);
    }

    return 0;
}