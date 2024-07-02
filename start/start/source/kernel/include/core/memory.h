#ifndef MEMORY_H
#define MEMORY_H
#include "comm/types.h"
#include "tools/bitmap.h"
#include "cpu/mmu.h"
#include "tools/log.h"
#include "ipc/mutex.h"
#include "comm/boot_info.h"
#include "tools/klib.h"
#define MEM_EXT_START   (1024*1024)
#define MEM_PAGE_SIZE   4096
#define MEM_EBDA_START  0x80000
typedef struct _addr_alloc_t{
    
    bitmap_t bitmap;  //位图
    mutex_t mutex;
    uint32_t start;   // 内存的起始地址
    uint32_t size;   // 内存的大小
    uint32_t paga_size;  //每一页的大小

}addr_alloc_t;

typedef struct _memory_map_t
{
    void *vstart;   // 虚拟内存起始
    void *vend;     // 虚拟内存结束
    void *pstart;   // 映射的物理地址
    uint32_t perm;  // 空间权限
}memory_map_t;

int memory_create_map(pde_t *page_dir,uint32_t vaddr,uint32_t paddr,int count,uint32_t perm);
void create_kernel_table();
pte_t * find_pte(pde_t *page_dir,uint32_t vaddr,uint32_t alloc);
void memory_init(boot_info_t *boot_info);
void show_mem_info(boot_info_t *boot_info);
#endif