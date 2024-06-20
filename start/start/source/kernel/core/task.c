#include "core/task.h"
#include "tools/klib.h"
#include "cpu/cpu.h"
#include "tools/log.h"

static int tss_init(task_t *task, uint32_t entry, uint32_t esp){

    int tss_sel = gdt_alloc_desc();
    if(tss_sel<0){
        log_printf("gdt_alloc_desc err!");
        return -1;
    }
    //?  我们这里设置了gdt表  但是tss的地址没有改变啊  还是在原来的地方  这样的tss  是独立的吗 ？
    segment_desc_set(tss_sel,(uint32_t)&task->tss,sizeof(tss_t),
                    SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);
    task->tss_sel = tss_sel;

    // 设置TR寄存器！！！！！！！！！





    kernel_memset(task,0,sizeof(*task));

    // 下次运行时 开始运行到的位置 
    task->tss.eip = entry;     
    // 为进行分配一个栈
    task->tss.esp = task->tss.esp0 = esp; 
    task->tss.ss = task->tss.ss0 = KERNEL_SELECTOR_DS; 
    task->tss.es = task->tss.ds = task->tss.fs = task->tss.gs = KERNEL_SELECTOR_DS;
    task->tss.cs = KERNEL_SELECTOR_CS;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    
    return 0;
}   

//    task状态信息结构体       entry 入口？是函数的名字       esp指针
int task_init(task_t *task,uint32_t entry,uint32_t esp){
    
    ASSERT(task != (task_t*)0);   //task  不为空
    tss_init(task,entry,esp);
    return 0;

}