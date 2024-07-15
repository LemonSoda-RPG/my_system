#ifndef DEVFS_H
#define DEVFS_H

typedef struct _devfs_type_t{
    const char*name;   // 设备类型名字
    int dev_type;       // 类型
    int file_type;     // 文件类型？   不懂

}devfs_type_t;


#endif