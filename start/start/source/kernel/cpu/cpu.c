#include "cpu/cpu.h"
#include "os_cfg.h"
#include "comm/cpu_instr.h"
static segment_desc_t gdt_table[GDT_TABLE_SIZE];  //这是一个数组
void segment_desc_set(int selector, uint32_t base,uint32_t limit,uint16_t attr)
{
    // 这里为什么要偏移3呢   这是因为segment_desc_t结构体的大小为8个字节的大小    
    // 右移3位相当于  除以8   也就是我们将偏移量从以字节为单位  转换为了以结构体segment_desc_t为单位   
    segment_desc_t * desc = gdt_table + (selector >> 3);

    if(limit > 0xFFFFF){
        attr |= 0x8000;
        limit /= 0x1000;   //将limit字段的单位设置为4kb
    }
    desc->limit15_0 = limit & 0xFFFF;
    desc->base15_0 = base & 0xFFFF;
    desc->base23_16 = (base>>16) &0xFF;
    desc->attr = attr | (((limit >> 16) & 0xF)<<8);
    desc->base31_24 = (base>>24)&0xFF;
}

void gate_desc_set(gate_sesc_t* desc,uint16_t selector, uint32_t offset,uint16_t attr)
{
    desc->offset15_0 = offset&0xFFFF;
    desc->selector = selector;
    desc->attr = attr;
    desc->offset31_16 = (offset >> 16) &0xFFFF;
}
 
// 遍历gdt表  查询空余位置
int gdt_alloc_desc(void){
    for(int i = 1;i<GDT_TABLE_SIZE;i++)
    {
        segment_desc_t *desc = gdt_table+i;
        if(desc->attr == 0){    // == 0 说明这个位置是空的  可以使用
            return i
        }


    }


}



void init_gdt(void)
{
    for(int i=0;i<GDT_TABLE_SIZE;i++)
    {
        segment_desc_set(i<<3,0,0,0);
    }


    segment_desc_set(KERNEL_SELECTOR_DS,0x00000000,0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_DATA | SRG_TYPE_RW | SEG_D |SEG_G);

    segment_desc_set(KERNEL_SELECTOR_CS,0,0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL0 | SEG_S_NORMAL | SEG_TYPE_CODE | SRG_TYPE_RW | SEG_D|SEG_G);

    lgdt((uint32_t)gdt_table,sizeof(gdt_table));
    // 重新加载gdt表  开启分段内存


}

void cpu_init (void){
    init_gdt();
}