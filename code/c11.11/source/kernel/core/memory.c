/**
 * 内存管理
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "tools/klib.h"
#include "tools/log.h"
#include "core/memory.h"
#include "tools/klib.h"
#include "cpu/mmu.h"

static addr_alloc_t paddr_alloc;        // 物理地址分配结构
static pde_t kernel_page_dir[PDE_CNT] __attribute__((aligned(MEM_PAGE_SIZE))); // 内核页目录表

/**
 * @brief 初始化地址分配结构
 * 以下不检查start和size的页边界，由上层调用者检查
 */
static void addr_alloc_init (addr_alloc_t * alloc, uint8_t * bits,
                    uint32_t start, uint32_t size, uint32_t page_size) {
    mutex_init(&alloc->mutex);
    alloc->start = start;
    alloc->size = size;
    alloc->page_size = page_size;
    bitmap_init(&alloc->bitmap, bits, alloc->size / page_size, 0);
}

/**
 * @brief 分配多页内存
 */
static uint32_t addr_alloc_page (addr_alloc_t * alloc, int page_count) {
    uint32_t addr = 0;

    mutex_lock(&alloc->mutex);

    int page_index = bitmap_alloc_nbits(&alloc->bitmap, 0, page_count);
    if (page_index >= 0) {
        // 通过位图索引  计算分配的空间的起始地址
        addr = alloc->start + page_index * alloc->page_size;
    }

    mutex_unlock(&alloc->mutex);
    return addr;
}

/**
 * @brief 释放多页内存
 */
static void addr_free_page (addr_alloc_t * alloc, uint32_t addr, int page_count) {
    mutex_lock(&alloc->mutex);

    uint32_t pg_idx = (addr - alloc->start) / alloc->page_size;
    bitmap_set_bit(&alloc->bitmap, pg_idx, page_count, 0);

    mutex_unlock(&alloc->mutex);
}

static void show_mem_info (boot_info_t * boot_info) {
    log_printf("mem region:");
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        log_printf("[%d]: 0x%x - 0x%x", i,
                    boot_info->ram_region_cfg[i].start,
                    boot_info->ram_region_cfg[i].size);
    }
    log_printf("\n");
}

/**
 * @brief 获取可用的物理内存大小
 */
static uint32_t total_mem_size(boot_info_t * boot_info) {
    int mem_size = 0;

    // 简单起见，暂不考虑中间有空洞的情况
    for (int i = 0; i < boot_info->ram_region_count; i++) {
        mem_size += boot_info->ram_region_cfg[i].size;
    }
    return mem_size;
}

pte_t * find_pte (pde_t * page_dir, uint32_t vaddr, int alloc) {
    pte_t * page_table;

    pde_t *pde = page_dir + pde_index(vaddr);
    // pde中就包含了二级页表的各项信息
    // 如果二级页表不存在  那么就创建一个二级页表  
    // 一个二级页表能代表4M大小的数据  当然 页表本身的大小是  每一个页是4字节 4k大小  一个页表要代表4M  就需要1024个页，1024×4
    // 所以一个页表占用的空间是4kb大小
    // 所以我们这里只是为页表分配了空间，并没有真正的分配内存   既然没有分配内存 那么内存位图也没有发生改变

    if (pde->present) {
        page_table = (pte_t *)pde_paddr(pde);
    } else {
        // 如果不存在，则考虑分配一个
        if (alloc == 0) {
            return (pte_t *)0;
        }

        // 分配一个物理页表   我们这里分配的页  其实就是二级页表当中的页
        // 二级页表的地址
        uint32_t pg_paddr = addr_alloc_page(&paddr_alloc, 1);   //  一个pagetable的大小是4kb?   
        if (pg_paddr == 0) {
            return (pte_t *)0;
        }

        // 设置为用户可读写，将被pte中设置所覆盖
        // 将二级页表的地址以及各项权限保存到一级页表当中
        // 这里权限都给了   那么页表的权限控制主要是在一级页表当中实现的
        pde->v = pg_paddr |  PDE_P|PDE_W|PDE_U;

        // 为物理页表绑定虚拟地址的映射，这样下面就可以计算出虚拟地址了
        //kernel_pg_last[pde_index(vaddr)].v = pg_paddr | PTE_P | PTE_W;

        // 清空页表，防止出现异常
        // 这里虚拟地址和物理地址一一映射，所以直接写入
        // 
        page_table = (pte_t *)(pg_paddr);
        kernel_memset(page_table, 0, MEM_PAGE_SIZE);
    }
    //这里返回的是 我们分配的页的信息  在二级页表中所处地址
    return page_table + pte_index(vaddr);    // 返回所需的pte在二级页表中的地址
}

/**
 * @brief 将指定的地址空间进行一页的映射
 */
int memory_create_map (pde_t * page_dir, uint32_t vaddr, uint32_t paddr, int count, uint32_t perm) {
    for (int i = 0; i < count; i++) {
        // log_printf("create map: v-0x%x p-0x%x, perm: 0x%x", vaddr, paddr, perm);


        // 这里得到了当前的虚拟地址在二级页表中的页的地址，当然此时我们仅仅是知道了他在二级页表中的位置  还没有和物理内存联系起来
        pte_t * pte = find_pte(page_dir, vaddr, 1);
        if (pte == (pte_t *)0) {
            // log_printf("create pte failed. pte == 0");
            return -1;
        }

        // 创建映射的时候，这条pte应当是不存在的。
        // 如果存在，说明可能有问题
        // log_printf("\tpte addr: 0x%x", (uint32_t)pte);
        ASSERT(pte->present == 0);


        // 这里我们将物理地址和二级页表联系在了一起  将物理地址存储到了二级页表当中   这样之后我们就可以通过虚拟地址
        // 找到二级页中的页，进而读取其中保存的对应的物理地址   
        // 映射到此完成。
        pte->v = paddr | perm | PTE_P;

        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;
    }

    return 0;
}

/**
 * @brief 根据内存映射表，构造内核页表
 * 一个物理地址被一个页表映射之后  还能被另一个页表映射
 */
void create_kernel_table (void) {
    extern uint8_t s_text[], e_text[], s_data[], e_data[];
    extern uint8_t kernel_base[];
    /*
    typedef struct _memory_map_t {
    void * vstart;     // 虚拟地址
    void * vend;
    void * pstart;       // 物理地址
    uint32_t perm;      // 访问权限
    }memory_map_t;
    */
    // 地址映射表, 用于建立内核级的地址映射
    // 地址不变，但是添加了属性
    // 内核内存没有配置PTE_U权限 所以用户模式下不可访问。
    static memory_map_t kernel_map[] = {
        {kernel_base,   s_text,         0,             PTE_W },         // 内核栈区
        {s_text,        e_text,         s_text,         0},         // 内核代码区   只读的   
        {s_data,        (void *)(MEM_EBDA_START - 1),   s_data,        PTE_W},      // 内核数据区
        {(void*)MEM_EXT_START,(void*)MEM_EXT_END,(void*)MEM_EXT_START,PTE_W},
    };

    // 清空后，然后依次根据映射关系创建映射表
    for (int i = 0; i < sizeof(kernel_map) / sizeof(memory_map_t); i++) {
        memory_map_t * map = kernel_map + i;

        // 可能有多个页，建立多个页的配置
        // 简化起见，不考虑4M的情况
        int vstart = down2((uint32_t)map->vstart, MEM_PAGE_SIZE);
        int vend = up2((uint32_t)map->vend, MEM_PAGE_SIZE);
        int page_count = (vend - vstart) / MEM_PAGE_SIZE;

        memory_create_map(kernel_page_dir, vstart, (uint32_t)map->pstart, page_count, map->perm);
    }
}

/**
 * @brief 初始化内存管理系统
 * 该函数的主要任务：
 * 1、初始化物理内存分配器：将所有物理内存管理起来. 在1MB内存中分配物理位图
 * 2、重新创建内核页表：原loader中创建的页表已经不再合适
 */
void memory_init (boot_info_t * boot_info) {
    // 1MB内存空间起始，在链接脚本中定义
    extern uint8_t * mem_free_start;

    log_printf("mem init.");
    show_mem_info(boot_info);

    // 在内核数据后面放物理页位图
    uint8_t * mem_free = (uint8_t *)&mem_free_start;   // 2022年-10-1 经同学反馈，发现这里有点bug，改了下

    // 计算1MB以上空间的空闲内存容量，并对齐的页边界
    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;
    mem_up1MB_free = down2(mem_up1MB_free, MEM_PAGE_SIZE);   // 对齐到4KB页
    log_printf("Free memory: 0x%x, size: 0x%x", MEM_EXT_START, mem_up1MB_free);

    // 4GB大小需要总共4*1024*1024*1024/4096/8=128KB的位图, 使用低1MB的RAM空间中足够
    // 该部分的内存仅跟在mem_free_start开始放置
    addr_alloc_init(&paddr_alloc, mem_free, MEM_EXT_START, mem_up1MB_free, MEM_PAGE_SIZE);
    mem_free += bitmap_byte_count(paddr_alloc.size / MEM_PAGE_SIZE);

    // 到这里，mem_free应该比EBDA地址要小
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);

    // 创建内核页表并切换过去
    create_kernel_table();

    // 先切换到当前页表
    mmu_set_page_dir((uint32_t)kernel_page_dir);
}
uint32_t memory_create_uvm(void){
    // 分配一个页表  页表的大小是4k  一个页表项是4字节，因此有1k个页表项， 每个页表项映射4m的内存   1k*4m  = 4G
    pde_t *page_dir = (pde_t *)addr_alloc_page(&paddr_alloc,1);
    if(page_dir==0){
        return 0;
    }
    kernel_memset((void*)page_dir,0,MEM_PAGE_SIZE);
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);  //user_pde_start之前  都属于内核的地址空间
    // 直接复制user_pde_start之前的内存信息到新的进程的页表
    for(int i = 0;i<user_pde_start;i++){
        page_dir[i].v = kernel_page_dir[i].v;
    }
    return (uint32_t)page_dir;
}
int memory_alloc_for_page_dir(uint32_t page_dir,uint32_t vaddr,uint32_t size,int perm){
    uint32_t curr_vaddr = vaddr;
    // 计算要分配多少页内存
    int page_count = up2(size,MEM_PAGE_SIZE) / MEM_PAGE_SIZE;
    for(int i = 0;i<page_count ; i++){
        uint32_t paddr = addr_alloc_page(&paddr_alloc , 1);
        if(paddr ==0){
            log_printf("error");
            return 0;
        }
        // 分配成功之后 将虚拟内存与 物理地址之间进行映射 
        int err = memory_create_map((pde_t*) page_dir,curr_vaddr,paddr,1,perm);
        if(err<0){
            log_printf("mem alloc failed  no memory");
          
            return 0;
        }
        curr_vaddr +=MEM_PAGE_SIZE;
    }
    return 0;
}
int   memory_alloc_page_for(uint32_t addr,  uint32_t size,int perm){
    return memory_alloc_for_page_dir(task_current()->tss.cr3,addr,size,perm);
}
static void* curr_page_dir(void){
    return (pde_t*)(task_current()->tss.cr3);
}


/**
 * @brief 获取指定虚拟地址的物理地址
 * 如果转换失败，返回0。
 */
uint32_t memory_get_paddr (uint32_t page_dir, uint32_t vaddr) {
    pte_t * pte = find_pte((pde_t *)page_dir, vaddr, 0);
    // 这里我们返回的是二级页表的地址
    if (pte == (pte_t *)0) {
        return 0;
    }
    //我们通过 pte_paddr(pte) 获取的是这个二级页表代表的内存区域的起始地址
    // 将起始地址加上偏移量才是真正的物理地址   vaddr & (MEM_PAGE_SIZE - 1)  就是偏移量
    return pte_paddr(pte) + (vaddr & (MEM_PAGE_SIZE - 1));
}

uint32_t memory_alloc_page (void){
    uint32_t addr  = addr_alloc_page(&paddr_alloc,1);
    return addr;
}
void memory_free_page (uint32_t addr){

    if(addr<MEMORY_TASK_BASE){
        //2g以下的内存  我们使用的一对一映射
        addr_free_page(&paddr_alloc,addr,1);
    }
    else{
        // 2g以上的内存  我们要先找到他对应的物理内存  再进行释放
        pte_t * pte = find_pte(curr_page_dir(),addr,0);
        ASSERT((pte==(pte_t*)0)&&pte->present);
        addr_free_page(&paddr_alloc,pte_paddr(pte),1);
        pte->v = 0;
    }

}

/**
 * @brief 销毁用户空间内存
 */
void memory_destroy_uvm (uint32_t page_dir) {
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t * pde = (pde_t *)page_dir + user_pde_start;

    ASSERT(page_dir != 0);

    // 释放页表中对应的各项，不包含映射的内核页面
    for (int i = user_pde_start; i < PDE_CNT; i++, pde++) {
        if (!pde->present) {
            continue;
        }

        // 释放页表对应的物理页 + 页表
        pte_t * pte = (pte_t *)pde_paddr(pde);
        for (int j = 0; j < PTE_CNT; j++, pte++) {
            if (!pte->present) {
                continue;
            }

            addr_free_page(&paddr_alloc, pte_paddr(pte), 1);
        }

        addr_free_page(&paddr_alloc, (uint32_t)pde_paddr(pde), 1);
    }

    // 页目录表
    addr_free_page(&paddr_alloc, page_dir, 1);
}
uint32_t memory_copy_uvm(uint32_t page_dir){
    uint32_t to_page_dir = memory_create_uvm();   //子进程的一级页表
    if(to_page_dir==0){
        goto copy_uvm_failed;
    }

    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);
    pde_t *pde = (pde_t *)page_dir+user_pde_start;
    for(int i = user_pde_start ;i<PDE_CNT;i++,pde++){
        if(!pde->present){
            continue;
        }
        // 假如存在二级页表
        // 那么我们就获取二级页表的地址  并对二级页表进行遍历
        pte_t *pte = (pte_t*)pde_paddr(pde); 
        for(int j = 0;j<PTE_CNT;j++,pte++){
            if(!pte->present){
                continue;
            }
            // 如果父进程的二级页表当中的一页存在  我们就要把信息拷贝到子进程去
            // 那么当然要为子进程分配物理内存
            uint32_t page = addr_alloc_page(&paddr_alloc,1);
            if(page==0){
                goto copy_uvm_failed;
            }
            // 分配成功之后  将物理内存与子进程一级页表建立关系

            // 我们的这个虚拟地址 应该和父进程的虚拟地址是一样的
            uint32_t vaddr = (i<<22)|(j<<12);
            // 之前我们为子进程创建了一级页表  
            //  在memory_create_map 这个函数中 如果有需求 会创建二级页表  因为一个二级页表能容纳1024个物理页
            int err = memory_create_map((pde_t*)to_page_dir,vaddr,page,1,get_pte_perm(pte));
            if(err<0){
                goto copy_uvm_failed;
            }
            // 我们终于分配完空间了  且完成了映射  接下来将进行拷贝
            kernel_memcpy((void*)page,(void*)vaddr,MEM_PAGE_SIZE);
        }
    }
    return to_page_dir;

copy_uvm_failed:
    if(to_page_dir){
        memory_destroy_uvm(to_page_dir);
    }
    return 0;

}


/**
 * @brief 在不同的进程空间中拷贝字符串
 * page_dir为目标页表，当前仍为老页表
 */
int memory_copy_uvm_data(uint32_t to, uint32_t page_dir, uint32_t from, uint32_t size) {
    char *buf, *pa0;

    while(size > 0){
        // 获取目标的物理地址, 也即其另一个虚拟地址
        // to 是我们的虚拟地址  要知道他的物理地址是哪里
        // page_dir 在这个页表里面找
        uint32_t to_paddr = memory_get_paddr(page_dir, to);
        if (to_paddr == 0) {
            return -1;
        }

        // 计算当前可拷贝的大小
        uint32_t offset_in_page = to_paddr & (MEM_PAGE_SIZE - 1);
        uint32_t curr_size = MEM_PAGE_SIZE - offset_in_page;
        if (curr_size > size) {
            curr_size = size;       // 如果比较大，超过页边界，则只拷贝此页内的
        }

        kernel_memcpy((void *)to_paddr, (void *)from, curr_size);

        size -= curr_size;
        to += curr_size;
        from += curr_size;
  }

  return 0;
}