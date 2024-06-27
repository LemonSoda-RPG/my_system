#ifndef TASK_H
#define TASK_H
#include "comm/types.h"
#include "cpu/cpu.h"
#include "os_cfg.h"
#include "tools/list.h"
#define TASK_NAME_SIZE 32
#define TASK_TIME_SLICE_DEFAULT 10
#define IDLE_STACK_SIZE 64
// 用于描述一个进程的运行
typedef struct _task_t{
// 设置当前任务状态
    enum{
        TASK_CREATED,
        TASK_RUNNING,
        TASK_SLEEP,
        TASK_READY,
        TASK_WAITTING
    }state;

    int sleep_ticks;   // 延时市场
    int slice_ticks;    
    int time_ticks;//每个任务的最多的运行时间  单位是每次中断的事件

    char name[TASK_NAME_SIZE];
    list_node_t run_node;   //  存放到就绪队列
    list_node_t all_node;   // 存放到task队列
    list_node_t wait_node;   // 存放到等待队列

    tss_t tss;
    int tss_sel;
}task_t;

typedef struct _task_manager_t{
    task_t * curr_task;   // 指向当前运行的进程
    list_t ready_list;   // 就绪队列
    list_t task_list;    // 进程队列  保存所有已经创建的进程
    list_t sleep_list;   // 睡眠队列
    task_t first_task;
    task_t idle_task;   // 空闲任务只会加入到all_node队列  
}task_manager_t;



// 对task进行初始化
int task_init(task_t *task,const char*name,uint32_t entry,uint32_t esp);
void task_switch_from_to(task_t *from,task_t *to);


void task_manager_init(void);  //对_task_manager_t 进行初始化
void task_first_init(void);
task_t * task_first_task(void);

void task_set_ready(task_t* task);
void task_set_block(task_t *task);

int sys_sched_yield(void);
void task_dispatch(void);
task_t * task_current(void);
task_t * task_next_run(void);
void task_time_tick(void);

void sys_sleep(uint32_t ms);
void task_set_sleep(task_t *task,uint32_t ticks);
void task_set_wakeup(task_t *task);

#endif