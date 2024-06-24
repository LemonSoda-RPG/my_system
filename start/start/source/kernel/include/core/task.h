#ifndef TASK_H
#define TASK_H
#include "comm/types.h"
#include "cpu/cpu.h"
#include "os_cfg.h"
#include "tools/list.h"
// 用于描述一个进程的运行
typedef struct _task_t{
    // uint32_t * stack;
    tss_t tss;
    int tss_sel;
}task_t;


// 对task进行初始化
int task_init(task_t *task,uint32_t entry,uint32_t esp);
void task_switch_from_to(task_t *from,task_t *to);



typedef struct _task_manager_t{
    task_t * curr_task;   // 指向当前运行的进程
    list_t ready_list;   // 就绪队列
    list_t task_list;    // 进程队列
    task_t first_task;

}task_manager_t;

void task_manager_init(void);  //对_task_manager_t 进行初始化
void task_first_init(void);
task_t * task_first_task(void);
#endif