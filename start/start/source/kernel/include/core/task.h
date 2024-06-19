#ifndef TASK_H

#define TASK_H

#include "comm/types.h"
#include "cpu/cpu.h"
// 用于描述一个进程的运行
typedef struct _task_t{
    tss_t tss;
}task_t;

int task_init(task_t *task,uint32_t entry,uint32_t esp);