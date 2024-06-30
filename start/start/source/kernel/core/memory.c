#include "core/memory.h"
#include "tools/log.h"




static addr_alloc_t paddr_alloc;
// size 空闲内存的总大小
static void addr_alloc_init(addr_alloc_t* alloc,uint8_t* bits,
    uint32_t start,uint32_t size,uint32_t page_size){
    mutex_init(&alloc->mutex);
    alloc->start = start;
    alloc->paga_size = page_size;
    alloc->size = size;
    bitmap_init(&alloc->bitmap,bits,alloc->size/page_size,0);
}


// 分配页表  page_count 分配的页表数目
static uint32_t addr_alloc_page (addr_alloc_t *alloc,int page_count){
    uint32_t addr = 0;
    mutex_lock(&alloc->mutex);
    // 页表的起始下标
    int page_index = bitmap_alloc_nbits(&alloc->bitmap,0,page_count);
    if(page_index>=0){
        // 根据页表的下标计算出实际的物理内存地址
        addr = alloc->start + page_index * alloc->paga_size;
    }
    mutex_unlock(&alloc->mutex);
    return addr;
}

static void addr_free_page (addr_alloc_t *alloc,uint32_t addr,int page_count){
    mutex_lock(&alloc->mutex);
    // 当前地址减去起始地址  再除以页表的大小  就是当前地址所在的页表的下标
    uint32_t pg_index = (addr-alloc->start)/alloc->paga_size;
    bitmap_set_bit(&alloc->bitmap,pg_index,page_count,0);
    mutex_unlock(&alloc->mutex);
}
void show_mem_info(boot_info_t *boot_info){
    log_printf("mem region:");
    for(int i = 0;i<boot_info->ram_region_count;i++){
        log_printf("[%d]:0x%x - 0x%x",i,
        boot_info->ram_region_cfg[i].start,
        boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}
static uint32_t total_mem_size(boot_info_t *boot_info){
    uint32_t mem_size =0;
    for(int i = 0;i<boot_info->ram_region_count;i++){
        mem_size+=boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}
void memory_init(boot_info_t *boot_info){
    extern uint8_t * mem_free_start;
    // 对内存进行初始化
    log_printf("mem init");
    show_mem_info(boot_info);
     
    //? 这里为什么是 8位 不是32位呢   我就用32位试试
    uint8_t * mem_free =(uint8_t *)& mem_free_start;  // kernel之后的地址  也是位图的起始地址


    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;
    // 将内存转化为pagesize的整数倍
    mem_up1MB_free = down2(mem_up1MB_free,MEM_PAGE_SIZE);
    log_printf("free memory: 0x%x, size: 0x%x",MEM_EXT_START,mem_up1MB_free);

    addr_alloc_init(&paddr_alloc,mem_free,MEM_EXT_START,mem_up1MB_free,MEM_PAGE_SIZE);

    //位图的结束地址
    mem_free += bitmap_byte_count(mem_up1MB_free/MEM_PAGE_SIZE);   
    

}
