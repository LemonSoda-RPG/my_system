#include "fs/fs.h"
#include "fs/fatfs/fatfs.h"
#include "dev/dev.h"
#include "core/memory.h"
#include "tools/log.h"
#include "tools/klib.h"
#include <sys/fcntl.h>



int fatfs_mount (struct _fs_t * fs,int major, int minor){
    return 0;


}
void fatfs_unmount(struct _fs_t * fs){
    return ;

}
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
// int fatfs_opendir(struct _fs_t * fs,const char * name, DIR * dir){
//     return 0;

// }
// int fatfs_readdir(struct _fs_t * fs, DIR* dir, struct dirent * dirent){
//     return 0;

// }
// int fatfs_closedir(struct _fs_t * fs,DIR *dir){
//     return 0;

// }
// int fatfs_unlink (struct _fs_t * fs, const char * path){
//     return 0;

// }


fs_op_t fatfs_op = {
    .mount = fatfs_mount,
    .unmount = fatfs_unmount,
    .open = fatfs_open,
    .read = fatfs_read,
    .write = fatfs_write,
    .seek = fatfs_seek,
    .stat = fatfs_stat,
    .close = fatfs_close,

    // .opendir = fatfs_opendir,
    // .readdir = fatfs_readdir,
    // .closedir = fatfs_closedir,
    // .unlink = fatfs_unlink,
};