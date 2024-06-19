#include "core/task.h"
#include "tools/klib.h"


static int tss_init(task_t *task, uint32_t entry, uint32_t esp){



    
    return 0;
}   

//    task状态信息结构体       entry 入口？是函数的名字       esp指针
int task_init(task_t *task,uint32_t entry,uint32_t esp){
    ASSERT(task != (task_t*)0);   //task  不为空
    tss_init(task,entry,esp);
    return 0;


}