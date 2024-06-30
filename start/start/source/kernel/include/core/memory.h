#ifndef MEMORY_H
#define MEMORY_H
#include "comm/types.h"
#include "tools/bitmap.h"

#include "ipc/mutex.h"
#include "comm/boot_info.h"
#include "tools/klib.h"
#define MEM_EXT_START   (1024*1024)
#define MEM_PAGE_SIZE   4096
typedef struct _addr_alloc_t{
    
    bitmap_t bitmap;  //位图
    mutex_t mutex;
    uint32_t start;   // 内存的起始地址
    uint32_t size;   // 内存的大小
    uint32_t paga_size;  //每一页的大小

}addr_alloc_t;

void memory_init(boot_info_t *boot_info);

#endif