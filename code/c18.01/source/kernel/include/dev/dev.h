/**
 * 设备接口
 *
 * 创建时间：2022年7月5日
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 * 参考资料：https://wiki.osdev.org/Printing_To_Screen
 */
#ifndef DEV_H
#define DEV_H

#include "comm/types.h"

#define DEV_NAME_SIZE               32      // 设备名称长度

enum {
    DEV_UNKNOWN = 0,            // 未知类型
    DEV_TTY,                // TTY设备
    DEV_DISK,
};

struct _dev_desc_t;

/**
 * @brief 设备驱动接口  用于描述某个特定的设备
 */
struct _dev_desc_t;
typedef struct _device_t {
    struct _dev_desc_t * desc;      // 设备特性描述符    设备描述符
    int mode;                       // 操作模式    是只读还是读写
    int minor;                      // 次设备号    与主设备号结合使用  就可以知道 此设备是那种类型的设备 的哪一个设备
    void * data;                    // 设备参数
    int open_count;                 // 打开次数
}device_t;


/**
 * @brief 设备描述结构    用于描述某种类型的设备  这种类型的设备的操作都是一样的
 */
typedef struct _dev_desc_t {
    char name[DEV_NAME_SIZE];           // 设备名称
    int major;                          // 主设备号  
    
    // 以下是函数指针   函数名字是open  返回值是int型   传参是device_t指针
    int (*open) (device_t * dev) ;
    // 函数名字是read
    int (*read) (device_t * dev, int addr, char * buf, int size);
    // 同上
    int (*write) (device_t * dev, int addr, char * buf, int size);
    // 这就是 ioctl 吗
    int (*control) (device_t * dev, int cmd, int arg0, int arg1);
    void (*close) (device_t * dev);
}dev_desc_t;

int dev_open (int major, int minor, void * data);
int dev_read (int dev_id, int addr, char * buf, int size);
int dev_write (int dev_id, int addr, char * buf, int size);
int dev_control (int dev_id, int cmd, int arg0, int arg1);
void dev_close (int dev_id);

#endif // DEV_H
