#ifndef CPU_INSTR_H
#define CPU_INSTR_H

#include "comm/types.h"
#include "cpu/irq.h"






//内联函数  会被插入到c代码中 而不是进入函数  我们是在这里定义的 如果不加static 那么引用他的c文件，都会定义一次这个函数  可能会造成冲突 因此添加static  只定义一次
static inline void cli(){
    __asm__ __volatile__("cli");  //cli指令用于清除中断标志  也就是关闭中断
    
}

static inline void sti(){
    __asm__ __volatile__("sti");  //开启中断
    
}


static inline uint8_t inb(uint16_t port){
    uint8_t rv;
    //inb  al, bx
    __asm__ __volatile__("inb %[p], %[v]":[v]"=a"(rv):[p]"d"(port));
    return rv;
}

// 大改
static inline uint16_t inw(uint16_t port){
    uint16_t rv;
	__asm__ __volatile__("in %1, %0" : "=a" (rv) : "dN" (port));
	return rv;
}


static inline void outb(uint16_t port,uint8_t data){
    //outb  al, bx
    __asm__ __volatile__("outb %[v], %[p]": :[p]"d"(port),[v]"a"(data));
}

static inline void lgdt(uint32_t start,uint32_t size){
    struct {
        uint16_t limit;
        uint16_t start15_0;
        uint16_t start31_16;
    }gdt;

    // 将gdt_table转换为了gdt结构体
    gdt.start31_16 = start >> 16;
    gdt.start15_0 = start & 0xFFFF;
    gdt.limit = size-1;
    __asm__ __volatile__("lgdt %[g]"::[g]"m"(gdt));
}

//读取cr0
static inline uint16_t read_cr0(void){
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0,%[v]":[v]"=r"(cr0));
    return cr0;

}


//写入cr0
// 大改
static inline void write_cr0(uint32_t v){

    __asm__ __volatile__("mov %[v],%%cr0"::[v]"r"(v));

}
static inline void far_jump(uint32_t selector,uint32_t offset){
    
    uint32_t addr[] = {offset,selector};
    __asm__ __volatile__("ljmpl *(%[a])"::[a]"r"(addr));

}
//加载idt表
static inline void lidt(uint32_t start,uint32_t size){
    struct {
        uint16_t limit;
        uint16_t start15_0;
        uint16_t start31_16;
    }idt;

    idt.start31_16 = start >> 16;
    idt.start15_0 = start & 0xFFFF;
    idt.limit = size-1;
    __asm__ __volatile__("lidt %0"::"m"(idt));
}

static inline void hlt(void){
    __asm__ __volatile__("hlt");
}

static inline void write_tr (uint32_t tss_selector) {
    __asm__ __volatile__("ltr %%ax"::"a"(tss_selector));
}

static inline irq_state_t read_eflags(){
    uint32_t eflags;
    __asm__ __volatile__("pushf\n\tpop %%eax":"=a"(eflags));
    return eflags;
}
static inline void write_eflags(irq_state_t eflags){
    __asm__ __volatile__("push %%eax\n\tpopf"::"a"(eflags));
}
#endif