#include "core/task.h"
#include "tools/klib.h"
#include "cpu/cpu.h"
#include "tools/log.h"
extern void simple_switch(uint32_t **from,uint32_t *to);

void task_switch_from_to(task_t *from,task_t *to){
    // swith_to_tss(to->tss_sel);
    simple_switch(&from->stack,to->stack);
}


static int tss_init(task_t *task, uint32_t entry, uint32_t esp){

    int tss_sel = gdt_alloc_desc();
    if(tss_sel<0){
        log_printf("gdt_alloc_desc err!");
        return -1;
    }
    // 同一片物理区域  可以被设置成不同的段  例如我们创建的tss结构体  
    // 她一开始是数据段以及代码段   但是我们仍然可以将他设置为tss段  内存区域被设置成段 之后  会有保护机制 来防止
    // 没有权限的程序进行读写  相当于独立出来了
    //?  我们这里设置了gdt表  但是tss的地址没有改变啊  还是在原来的地方  这样的tss  是独立的吗 ？
    segment_desc_set(tss_sel,(uint32_t)&task->tss,sizeof(tss_t),
                    SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
    


    kernel_memset(&task->tss, 0, sizeof(tss_t));

    // 下次运行时 开始运行到的位置 
    task->tss.eip = entry;     
    // 为进行分配一个栈
    task->tss.esp = task->tss.esp0 = esp; 
    task->tss.ss = task->tss.ss0 = KERNEL_SELECTOR_DS; 
    task->tss.es = task->tss.ds = task->tss.fs = task->tss.gs = KERNEL_SELECTOR_DS;
    task->tss.cs = KERNEL_SELECTOR_CS;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.iomap = 0;
    task->tss_sel = tss_sel;
    
    return 0;
}   

//    task状态信息结构体       entry 入口？是函数的名字       esp指针
int task_init(task_t *task,uint32_t entry,uint32_t esp){
    
    ASSERT(task != (task_t*)0);   //task  不为空
    // tss_init(task,entry,esp);

    uint32_t * pesp = (uint32_t*) esp;
    if(pesp){
        *(--pesp) = entry;
        *(--pesp) = 0;
        *(--pesp) = 0;
        *(--pesp) = 0;
        *(--pesp) = 0;
        task->stack = pesp;
    }




    return 0;

}