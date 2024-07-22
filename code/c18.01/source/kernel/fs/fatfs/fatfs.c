#include "fs/fs.h"
#include "fs/fatfs/fatfs.h"
#include "dev/dev.h"
#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include <sys/fcntl.h>


// 为每一个硬件读写的时候  都创建一个缓冲区
int fatfs_mount (struct _fs_t * fs,int major, int minor){
    // dev_open 返回的是设备id    open返回的是文件描述符id
    int dev_id = dev_open(major,minor,(void*)0);
    if (dev_id < 0) {
        log_printf("open disk failed. major: %x, minor: %x", major, minor);
        return -1;
    }
     // 读取dbr扇区并进行检查
    // 分配一页缓存 足够了  因为一页有4k  一个扇区只有512
    dbr_t * dbr = (dbr_t *)memory_alloc_page();
    if (!dbr) {
        log_printf("mount fat failed: can't alloc buf.");
        goto mount_failed;
    }
    // 这里是读取一个扇区   这里的读取的具体的单位 要看当前设备的具体类型来确定 磁盘就是扇区
    int cnt = dev_read(dev_id, 0, (char *)dbr, 1);
    if (cnt < 1) {
        log_printf("read dbr failed.");
        goto mount_failed;
    }
     // 解析DBR参数，解析出有用的参数
    fat_t * fat = &fs->fat_data;
    fat->fat_buffer = (uint8_t *)dbr;
    fat->bytes_per_sec = dbr->BPB_BytsPerSec;
    fat->tbl_start = dbr->BPB_RsvdSecCnt;
    fat->tbl_sectors = dbr->BPB_FATSz16;
    fat->tbl_cnt = dbr->BPB_NumFATs;
    fat->root_ent_cnt = dbr->BPB_RootEntCnt;
    fat->sec_per_cluster = dbr->BPB_SecPerClus;
    fat->cluster_byte_size = fat->sec_per_cluster * dbr->BPB_BytsPerSec;
	fat->root_start = fat->tbl_start + fat->tbl_sectors * fat->tbl_cnt;
    fat->data_start = fat->root_start + fat->root_ent_cnt * 32 / SECTOR_SIZE;
    fat->curr_sector = -1;
    fat->fs = fs;
    mutex_init(&fat->mutex);
// 简单检查是否是fat16文件系统, 可以在下边做进一步的更多检查。此处只检查做一点点检查
	if (fat->tbl_cnt != 2) {
        log_printf("fat table num error, major: %x, minor: %x", major, minor);
		goto mount_failed;
	}
    if (kernel_memcmp(dbr->BS_FileSysType, "FAT16", 5) != 0) {
        log_printf("not a fat16 file system, major: %x, minor: %x", major, minor);
        goto mount_failed;
    }


    // 记录相关的打开信息
    fs->type = FS_FAT16;
    fs->data = &fs->fat_data;
    fs->dev_id = dev_id;
    return 0;
mount_failed:
    if (dbr) {
        memory_free_page((uint32_t)dbr);
    }
    dev_close(dev_id);
    return -1;
}



void fatfs_unmount(struct _fs_t * fs){
    fat_t * fat = (fat_t *)fs->data;
    dev_close(fs->dev_id);
    memory_free_page((uint32_t)fat->fat_buffer);
}



// 文件操作
int fatfs_open (struct _fs_t * fs, const char * path, file_t * file){
    return 0;

}
int fatfs_read (char * buf, int size, file_t * file){
    return 0;

}
int  fatfs_write (char * buf, int size, file_t * file){
    return 0;

}
void fatfs_close (file_t * file){
    return;

}
int fatfs_seek (file_t * file, uint32_t offset, int dir){
    return 0;

}
int fatfs_stat(file_t * file, struct stat *st){

    return 0;
}


// 目录操作
int fatfs_opendir(struct _fs_t * fs,const char * name, DIR * dir){

    // 在这里填充了索引
    dir->index = 0;
    return 0;

}
/**
 * @brief 缓存读取磁盘数据，用于目录的遍历等
 */
static int  bread_sector (fat_t * fat, int sector) {
    // 这个说明这个读的表项与上一次一样 在一个扇区里面
    if (sector == fat->curr_sector) {
        return 0;
    }

    int cnt = dev_read(fat->fs->dev_id, sector, fat->fat_buffer, 1);
    if (cnt == 1) {
        // 更新扇区号
        fat->curr_sector = sector;
        return 0;
    }
    return -1;
}
/**
 * @brief 获取文件类型
 */
file_type_t diritem_get_type (diritem_t * item) {
    file_type_t type = FILE_UNKNOWN;

    // 长文件名和volum id
    if (item->DIR_Attr & (DIRITEM_ATTR_VOLUME_ID | DIRITEM_ATTR_HIDDEN | DIRITEM_ATTR_SYSTEM)) {
        return FILE_UNKNOWN;
    }

    return item->DIR_Attr & DIRITEM_ATTR_DIRECTORY ? FILE_DIR : FILE_NORMAL;
}
/**
 * @brief 在root目录中读取diritem
 */
static diritem_t * read_dir_entry (fat_t * fat, int index) {

    // 通过下标  计算偏移量  进而得到目录项
    if ((index < 0) || (index >= fat->root_ent_cnt)) {
        return (diritem_t *)0;
    }
    // 偏移量计算
    int offset = index * sizeof(diritem_t);

    // 我们每一次读 至少读一个扇区 也就是说没必要每次都进行读磁盘
    // 假如前后两次读的恰好在同一个扇区  直接从上一次的内存中读取就好了
    
    // offset 单位是字节
    // fat->root_start + offset / fat->bytes_per_sec 这是在第几块扇区
    int err = bread_sector(fat, fat->root_start + offset / fat->bytes_per_sec);
    if (err < 0) {
        return (diritem_t *)0;
    }
    return (diritem_t *)(fat->fat_buffer + offset % fat->bytes_per_sec);
}


/**
 * @brief 获取diritem中的名称，转换成合适
 */
void diritem_get_name (diritem_t * item, char * dest) {
    char * c = dest;
    char * ext = (char *)0;

    kernel_memset(dest, 0, SFN_LEN + 1);     // 最多11个字符
    for (int i = 0; i < 11; i++) {
        if (item->DIR_Name[i] != ' ') {
            *c++ = item->DIR_Name[i];
        }

        if (i == 7) {
            ext = c;
            *c++ = '.';
        }
    }

    // 没有扩展名的情况
    if (ext && (ext[1] == '\0')) {
        ext[0] = '\0';
    }
}

/**
 * @brief 读取一个目录项
 */
int fatfs_readdir (struct _fs_t * fs,DIR* dir, struct dirent * dirent) {

    // 每次只读一个文件啊
     
    // * fat结构   这个就描述了fat整个文件系统的信息 同时记录了结构    但是他是在哪里写入的？？？？
    fat_t * fat = (fat_t *)fs->data;  

    // 做一些简单的判断，检查
    while (dir->index < fat->root_ent_cnt) {
        // 通过下标获取我们要读取的文件
        diritem_t * item = read_dir_entry(fat, dir->index);
        if (item == (diritem_t *)0) {
            return -1;
        }

        // 结束项，不需要再扫描了，同时index也不能往前走
        if (item->DIR_Name[0] == DIRITEM_NAME_END) {
            break;
        }

        // 只显示普通文件和目录，其它的不显示
        if (item->DIR_Name[0] != DIRITEM_NAME_FREE) {
            file_type_t type = diritem_get_type(item);
            if ((type == FILE_NORMAL) || (type == FILE_DIR)) {
                dirent->index = dir->index++;
                dirent->type = diritem_get_type(item);
                dirent->size = item->DIR_FileSize;
                diritem_get_name(item, dirent->name);
                return 0;
            }
        }

        dir->index++;
    }

    return -1;
}
int fatfs_closedir(struct _fs_t * fs,DIR *dir){
    return 0;

}
int fatfs_unlink (struct _fs_t * fs, const char * path){
    return 0;

}


fs_op_t fatfs_op = {
    .mount = fatfs_mount,
    .unmount = fatfs_unmount,
    .open = fatfs_open,
    .read = fatfs_read,
    .write = fatfs_write,
    .seek = fatfs_seek,
    .stat = fatfs_stat,
    .close = fatfs_close,

    .opendir = fatfs_opendir,
    .readdir = fatfs_readdir,
    .closedir = fatfs_closedir,
    .unlink = fatfs_unlink,
};