#ifndef CPU_H
#define CPU_H

#include "comm/types.h"

#define EFLAGS_DEFAULT  (1<<1)
#define EFLAGS_IF       (1<<9)  //

#pragma pack(1)
typedef struct _segment_desc_t{
    uint16_t limit15_0;  //16bit
    uint16_t base15_0;   //16
    uint8_t base23_16;   //8
    uint16_t attr;       //16
    uint8_t base31_24;   //8
}segment_desc_t;   //大小为8个字节



typedef struct _gate_sesc_t{

    uint16_t offset15_0;
    uint16_t selector;
    uint16_t attr;
    uint16_t offset31_16;
}gate_sesc_t;

#define GATE_TYPE_IDT   (0xE << 8)
#define GATE_P_PRESENT  (1<<15)
#define GATE_DPL_0      (0<<13)
#define GATE_DPL_3      (3<<13)



// 定义的结构体是一个任务状态段（Task State Segment，TSS）
// 主要包括保存任务的上下文（CPU寄存器状态）和切换任务的堆栈指针。
typedef struct _tss_t{
    uint32_t pre_link;
    uint32_t esp0,ss0,esp1,ss1,esp2,ss2;
    // 设置虚拟内存时会用到
    uint32_t cr3;
    // 通用寄存器  因为一个进程在开始之后  这些寄存器会被重新写入 因此不需要进行特定的初始化  写0就行了
    uint32_t eip,eflags,eax,ecx,edx,ebx,esp,ebp,esi,edi;
    // 我们使用的是平坦结构   写入段选择子   
    uint32_t es,cs,ss,ds,fs,gs;
    // 暂时用不到
    uint32_t ldt;
    // 暂时用不到
    uint32_t iomap;
}tss_t;



#pragma pack()

#define SEG_G           (1<<15)
#define SEG_D           (1<<14)
#define SEG_P_PRESENT   (1<<7)

#define SEG_DPL0        (0<<5)
#define SEG_DPL3        (3<<5)
#define SEG_S_SYSTEM    (0<<4)
#define SEG_S_NORMAL    (1<<4)

#define SEG_TYPE_CODE   (1<<3)
#define SEG_TYPE_DATA   (0<<3)
#define SEG_TYPE_TSS    (9<<0)

#define SEG_TYPE_RW     (1<<1)


void segment_desc_set(int selector, uint32_t base,uint32_t limit,uint16_t attr);
void gate_desc_set(gate_sesc_t* desc,uint16_t selector, uint32_t offset,uint16_t attr);

void init_gdt(void);
void cpu_init (void);


int gdt_alloc_desc(void);
void swith_to_tss(uint32_t tss_sel);
void gdt_free_sel(int sel);
#endif