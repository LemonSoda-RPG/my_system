/**
 * 文件系统相关接口的实现
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#ifndef FILE_H
#define FILE_H
#include <sys/stat.h>
#include "file.h"
#include "tools/list.h"
#include "applib/lib_syscall.h"
#include "fs/fatfs/fatfs.h"
#include "ipc/mutex.h"
/**
 * @brief 文件系统操作接口
 */
struct _fs_t;
typedef struct _fs_op_t {
    // 
	int (*mount) (struct _fs_t * fs,int major, int minor);
    void (*unmount) (struct _fs_t * fs);
    int (*open) (struct _fs_t * fs, const char * path, file_t * file);
    int (*read) (char * buf, int size, file_t * file);
    int (*write) (char * buf, int size, file_t * file);
    void (*close) (file_t * file);
    int (*seek) (file_t * file, uint32_t offset, int dir);
    int (*stat)(file_t * file, struct stat *st);
    int (*ioctl) (file_t * file, int cmd, int arg0, int arg1);

    int (*opendir)(struct _fs_t * fs,const char * name, DIR * dir);
    int (*readdir)(struct _fs_t * fs, DIR* dir, struct dirent * dirent);
    int (*closedir)(struct _fs_t * fs,DIR *dir);
    int (*unlink) (struct _fs_t * fs, const char * path);

}fs_op_t;
 
#define FS_MOUNTP_SIZE 512    //名字的最大长度
typedef enum _fs_type_t
{
    FS_DEVFS,
    FS_FAT16,
}fs_type_t;

// 一个fs就是一种文件类型   文件类型通过链表进行保存
typedef struct _fs_t{
    char mount_point[FS_MOUNTP_SIZE];
    fs_op_t *op;    // 回调函数表
    fs_type_t type;  // 因为这里使用的是enum类型  初始化时这里的初始值是0  所以默认是fs_devfs类型
    void *data;  // 保存临时的数据
    int dev_id;  // 设备分区
    list_node_t node; // 链表节点    链表存储的是各种文件类型  例如 dev  fat16
    mutex_t *mutex;
    union {
        fat_t fat_data;         // 文件系统相关数据  // 大概是保存了目录项相关的东西
    };
}fs_t;

const char* path_next_child(const char*path);
void fs_init (void);
int path_to_num (const char * path, int * num);
int sys_open(const char *name, int flags, ...);
int sys_read(int file, char *ptr, int len);
int sys_write(int file, char *ptr, int len);
int sys_lseek(int file, int ptr, int dir);
int sys_close(int file);
void fs_protect(fs_t *fs);
void fs_unprotect(fs_t *fs);

int sys_isatty(int file);
int sys_fstat(int file, struct stat *st);

int sys_dup (int file);


int sys_opendir(const char * name, DIR * dir);
int sys_readdir(DIR* dir, struct dirent * dirent);
int sys_closedir(DIR *dir);
int sys_unlink (const char * path);



#endif // FILE_H

