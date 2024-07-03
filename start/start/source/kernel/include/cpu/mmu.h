#ifndef MMU_H
#define MMU_H
#include "comm/types.h"
#include "comm/cpu_instr.h"
#define PDE_PNT 1024
#define PTE_P  (1<<0)
#define PDE_P   (1<<0)


#define PDE_W   (1<<1)
#define PTE_W   (1<<1)
#define PDE_U   (1<<2)




static  inline uint32_t pde_index(uint32_t vaddr){
    int index = (vaddr >> 22);
    return index;
}

static  inline uint32_t pte_index(uint32_t vaddr){
    int index = (vaddr >> 12) & 0x3FFF;
    return index;
}



// 因为我们使用的是二级页表 因此需要两种结构体   一种用于映射一级页表到二级页表  一种映射二级页表到物理地址
typedef union _pde_t{
    uint32_t v;
    struct{
        uint32_t present : 1;
        uint32_t write_enbale : 1;
        uint32_t user_mode_acc : 1;
        uint32_t write_through : 1;
        uint32_t cache_dieable : 1;
        uint32_t accessed : 1;
        uint32_t  : 1;
        uint32_t ps : 1;
        uint32_t  : 4;
        uint32_t phy_pt_paddr : 20;
    };
}pde_t;



typedef union _pte_t{
    uint32_t v;
    struct{
        uint32_t present : 1;
        uint32_t write_enbale : 1;
        uint32_t user_mode_acc : 1;
        uint32_t write_through : 1;
        uint32_t cache_dieable : 1;
        uint32_t accessed : 1;
        uint32_t dirty : 1;
        uint32_t pat : 1;
        uint32_t global : 1;
        uint32_t  : 3;
        uint32_t phy_pt_paddr : 20;
    };
}pte_t;


static inline void mmu_set_page_dir(uint32_t paddr){
    write_cr3(paddr);
}

static inline uint32_t pde_paddr(pde_t * pde){
    return pde->phy_pt_paddr<<12;    // pde是一级页表中的一个成员    返回他存储的二级页表的地址
    // 左移12位是为了补全32位
}
#endif
