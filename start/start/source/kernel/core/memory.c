#include "core/memory.h"

static addr_alloc_t paddr_alloc;
// 1级页表的每个表项代表4M  因此需要1024个一级表项
static pde_t kernel_page_dir[PDE_PNT] __attribute__((aligned(MEM_PAGE_SIZE)));

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
uint32_t memory_create_uvm(void){
    // 分配一个页表  页表的大小是4k  一个页表项是4字节，因此有1k个页表项， 每个页表项映射4m的内存   1k*4m  = 4G
    pde_t *page_dir = (pde_t *)addr_alloc_page(&paddr_alloc,1);
    if(page_dir==0){
        return 0;
    }

    
    kernel_memset((void*)page_dir,0,MEM_PAGE_SIZE);
    uint32_t user_pde_start = pde_index(MEMORY_TASK_BASE);  //user_pde_start之前  都属于内核的地址空间
    for(int i = 0;i<user_pde_start;i++){
        page_dir[i].v = kernel_page_dir[i].v;
    }
    return (uint32_t)page_dir;
}

void memory_init(boot_info_t *boot_info){
    extern uint8_t * mem_free_start;   //内核代码的结束地址
    kernel_memset(kernel_page_dir,0,PDE_PNT*sizeof(pde_t));
    // 对内存进行初始化
    log_printf("mem init");
    show_mem_info(boot_info);    // 打印空闲内存的信息
     
    // 这个是指针  指向位图的起始内存地址   因为我们要将位图放到kernel的后面
    uint8_t * mem_free =(uint8_t *)& mem_free_start;  // kernel之后的地址  也是位图的起始地址

    // 这里我不明白   视频中说使用1M以上的空间给进程  但是这里的物理地址不是1M啊
    // 还是说 我们将空闲内存重新排序 ？ 
    uint32_t mem_up1MB_free = total_mem_size(boot_info) - MEM_EXT_START;
    // 将内存转化为pagesize的整数倍
    mem_up1MB_free = down2(mem_up1MB_free,MEM_PAGE_SIZE);
    log_printf("free memory: 0x%x, size: 0x%x",MEM_EXT_START,mem_up1MB_free);

    addr_alloc_init(&paddr_alloc,mem_free,MEM_EXT_START,mem_up1MB_free,MEM_PAGE_SIZE);

    //位图的结束地址
    mem_free += bitmap_byte_count(mem_up1MB_free/MEM_PAGE_SIZE);   
    
    ASSERT(mem_free < (uint8_t *)MEM_EBDA_START);
    create_kernel_table();
    mmu_set_page_dir((uint32_t)kernel_page_dir);

}
pte_t * find_pte(pde_t *page_dir,uint32_t vaddr,uint32_t alloc){

    pte_t * page_table;
    // pde_t 是一级页表  存储了二级页表的地址
    // 取出二级页表的地址
    // 这里的指针类型是pde_t   但是里面存储了  pte_t的地址  
    pde_t *pde = page_dir + pde_index(vaddr);
    if(pde->present){
        page_table = (pte_t*)pde_paddr(pde);
    }
    else{
        // 如果不存在 是否分配
        if(alloc==0)   
        {
            return (pte_t*)0;
        }
        else{

            // 我不明白  这样分配并不符合映射的规则啊   这里命名是查询空间 找到空闲的就存到这里面了
            // 如果二级页表不存在 我们这里就为他分配一个内存空间 并创建
            uint32_t pg_paddr =  addr_alloc_page(&paddr_alloc,1);
            
            if(pg_paddr ==0){
                return (pte_t*)0; 
            }
            kernel_memset((void*)pg_paddr,0,MEM_PAGE_SIZE);

            pde->v = pg_paddr | PDE_P | PDE_W ;
            // 分配完成之后  我们将其进行清空
            page_table = (pte_t*) pg_paddr;

        }
    }
    // 返回二级页表中  我们需要的项
    return page_table+pte_index(vaddr);
}




// 为页表建立映射关系    vaddr是虚拟内存的地址   pade_dir是页表的地址   二者需要配合才能进行映射
int memory_create_map(pde_t *page_dir,uint32_t vaddr,uint32_t paddr,int count,uint32_t perm){
    // 遍历页表中的每一项

    for(int i = 0 ; i<count ; i++){
        log_printf("create map : v-0x%x ,p-0x%x ,perm:0x%x",vaddr,paddr,perm);
        pte_t *pte = find_pte(page_dir,vaddr,1);    // 二级页表  
        if(pte == (pte_t*)0){
            log_printf("create pte failed");
            return -1;
        }

        log_printf("pte addr : 0x%x",(uint32_t)pte);
        ASSERT(pte->present == 0);
        pte->v = paddr | perm | PTE_P;
        vaddr += MEM_PAGE_SIZE;
        paddr += MEM_PAGE_SIZE;

    }
}

void create_kernel_table(){
    extern uint8_t s_text[],e_text[],s_data[];
    extern uint8_t kernel_base[];
    static memory_map_t kernel_map[] = {
        {kernel_base, s_text,kernel_base,PTE_W},
        {s_text,e_text,s_text,0},
        {s_data,(void*)MEM_EBDA_START,s_data,PTE_W},
        {(void*)MEM_EXT_START,(void*)MEM_EXT_END,(void*)MEM_EXT_START,PTE_W}
    };

    // 对表进行遍历  配置页表属性
    for(int i =0;i<sizeof(kernel_map)/sizeof(memory_map_t);i++){
        memory_map_t *map = kernel_map+i;

        uint32_t vstart = down2((uint32_t)map->vstart,MEM_PAGE_SIZE);
        uint32_t vend = up2((uint32_t)map->vend,MEM_PAGE_SIZE);
        uint32_t pstart = down2((uint32_t)map->pstart,MEM_PAGE_SIZE);
        int page_count = (vend - vstart) / MEM_PAGE_SIZE;

        // 对内存进行映射
        memory_create_map(kernel_page_dir, vstart,pstart,page_count,map->perm);

    }

}