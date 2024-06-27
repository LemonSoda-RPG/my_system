#include "ipc/mutex.h"
#include "core/task.h"
#include "cpu/irq.h"
void mutex_init(mutex_t *mutex){
    mutex->locked_count = 0;
    mutex->owner = (task_t*)0;
    list_init(&mutex->wait_list);

}

void mutex_lock(mutex_t *mutex){
    irq_state_t old_state = irq_enter_proctection();
    task_t *curr = task_current();
    if(mutex->locked_count==0){
        mutex->locked_count++;
        mutex->owner = curr;
    }else if(mutex->owner == curr)
    {
        mutex->locked_count++;
    }
    else{
        // 没抢到锁 就只能等待了
        // 从就绪队列中移除
        task_set_block(curr);
        // 插入到这把锁的等待队列当中
        list_insert_last(&mutex->wait_list,&curr->wait_node);
        // 切换任务
        task_dispatch();
    }
    irq_leave_proctection(old_state);
}
void mutex_unlock(mutex_t *mutex){
    irq_state_t old_state = irq_enter_proctection();

    task_t *curr = task_current();
    if(mutex->owner!=curr){
        irq_leave_proctection(old_state);
        return;
    }
    if(--mutex->locked_count==0){
        mutex->owner = (task_t*)0;
        if(list_count(&mutex->wait_list)>0){
            list_node_t *node = list_remove_first(&mutex->wait_list);
            task_t * task = list_node_parent(node,task_t,wait_node);
            task_set_ready(task);

            mutex->locked_count = 1;
            mutex->owner = task;
            task_dispatch();
        }
    }
    irq_leave_proctection(old_state);
}