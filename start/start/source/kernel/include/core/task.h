#ifndef TASK_H
#define TASK_H
#include "comm/types.h"
#include "cpu/cpu.h"
#include "os_cfg.h"
// 用于描述一个进程的运行
typedef struct _task_t{
    uint32_t * stack;
    tss_t tss;
    int tss_sel;
}task_t;


// 对task进行初始化
int task_init(task_t *task,uint32_t entry,uint32_t esp);
void task_switch_from_to(task_t *from,task_t *to);
#endif