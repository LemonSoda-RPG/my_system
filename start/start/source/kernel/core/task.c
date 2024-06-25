#include "core/task.h"
#include "tools/klib.h"
#include "cpu/cpu.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "tools/list.h"
#include "cpu/irq.h"
static uint32_t idle_task_stack[IDLE_STACK_SIZE];
extern void simple_switch(uint32_t **from,uint32_t *to);



static task_manager_t task_manager;

void task_switch_from_to(task_t *from,task_t *to){
    swith_to_tss(to->tss_sel);
    // simple_switch(&from->stack,to->stack);
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
int task_init(task_t *task,const char*name,uint32_t entry,uint32_t esp){
    ASSERT(task != (task_t*)0);   //task  不为空
    tss_init(task,entry,esp);


    kernel_strncpy(task->name,name,TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->slice_ticks = TASK_TIME_SLICE_DEFAULT;
    task->time_ticks = task->slice_ticks;
    task->sleep_ticks = 0;
    list_node_init(&task->all_node);  // 初始化节点
    list_node_init(&task->run_node);

    irq_state_t old_state = irq_enter_proctection();
    task_set_ready(task);
    list_insert_last(&task_manager.task_list,&task->all_node);
    irq_leave_proctection(old_state);
    return 0;
}
static void idle_task_entry(void){
    for(;;){
        hlt();
    }
}


void task_manager_init(void){
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);
    task_manager.curr_task = (task_t*)0;

    // 初始化idle任务
    task_init(&task_manager.idle_task,"idle_task",(uint32_t)idle_task_entry,(uint32_t)(idle_task_stack+IDLE_STACK_SIZE));
}


// 初始化第一个进程
void task_first_init(void){
    task_t *first_task = task_first_task();
    task_init(first_task,"first_task",0,0);

    // 写入的是gdt表当中的下标
    // tr寄存器  也就是 task register  保存当前正在运行的任务 通过gdt表进行索引  因为每一个task都有自己的tss段
    write_tr(first_task->tss_sel);
    task_manager.curr_task = first_task;

}


task_t * task_first_task(void){
    return &task_manager.first_task;
}

void task_set_ready(task_t *task){
    // 如果是空闲进程 直接返回
    if(task==&task_manager.idle_task){
        return;
    }
    list_insert_last(&task_manager.ready_list,&task->run_node);
    task->state = TASK_READY;
    
}
void task_set_block(task_t *task){
    if(task==&task_manager.idle_task){   // 如果是空闲进程  直接返回，因为他不属于任何队列
        return;
    }
    list_node_remove(&task_manager.ready_list,&task->run_node);
}



// 获取当前运行任务的结构所在位置
task_t * task_next_run(void){  
    // 当调用队列的节点时 发现是空的
    if(list_count(&task_manager.ready_list)==0){
        return &task_manager.idle_task;
    }
    list_node_t *task_node = list_first(&task_manager.ready_list);
    return list_node_parent(task_node,task_t,run_node);

}
task_t * task_current(void){

    return task_manager.curr_task;

}
// 手动切换任务
int sys_sched_yield(void){
    irq_state_t old_state = irq_enter_proctection();

    // 判断当前就绪队列还有没有别的任务 如果没有  那就没必要进行切换了
    if(list_count(&task_manager.ready_list)>1){
        task_t *curr_task = task_current();   // 查看当前正在运行的任务是哪一个
        // 将当前任务从队列中移除
        task_set_block(curr_task);
        // 再次添加到队列的尾部
        task_set_ready(curr_task);
        //任务切换
        task_dispatch();
    }
    irq_leave_proctection(old_state);
    return 0;

}
// 如果当前运行的是空闲进程，而且就绪队列此时还是空的
// 那么接下来还是运行空闲进程
void task_dispatch(void){   // 这个函数规定了不同的切换机制
    irq_state_t old_state = irq_enter_proctection();
    
    task_t *to = task_next_run();
    task_t *from = task_current();
    if(to!=from){
        // 不一样才会进行切换
        
        task_manager.curr_task = to;
        to->state = TASK_RUNNING;
        task_switch_from_to(from,to);
    }
    irq_leave_proctection(old_state);

}

// 根据时间片自动切换
void task_time_tick(void){
    task_t* task_curr = task_current();
    if(--(task_curr->slice_ticks) == 0)
    {   
        task_curr->slice_ticks = task_curr->time_ticks;
        task_set_block(task_curr);
        task_set_ready(task_curr);
        task_dispatch();
        // 这里如果满足条件就直接跳走了  不会再执行下面的代码
    }

    list_node_t *sleep_curr = list_first(&task_manager.sleep_list);
    while(sleep_curr){
        list_node_t *next = list_node_next(sleep_curr);
        task_t *curr_task = list_node_parent(sleep_curr,task_t,run_node);
        if(--curr_task->sleep_ticks==0){
            task_set_wakeup(curr_task);
            task_set_ready(curr_task);
        }
        sleep_curr = next;
    }
    // 这里的意思是  只要我有任务从延迟队列插入到就绪队列了  就执行一次任务切换
    // 不对  这里没有对队列进行出队操作，所以假如原本就有任务再执行，那么队首任务与当前执行的任务就是同一个，因此不会切换
    task_dispatch();
}

void task_set_sleep(task_t *task,uint32_t ticks){
    if(ticks==0)
    {
        return;
    }
    task->sleep_ticks = ticks;
    task->state = TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list,&task->run_node);

}
void task_set_wakeup(task_t *task){
    list_node_remove(&task_manager.sleep_list,&task->run_node);
}

void sys_sleep(uint32_t ms){
    irq_state_t old_state = irq_enter_proctection();

    task_set_block(task_manager.curr_task);
    
    task_set_sleep(task_manager.curr_task,(ms+(OS_TICKS_MS-1))/OS_TICKS_MS);

    task_dispatch();



    irq_leave_proctection(old_state);

}