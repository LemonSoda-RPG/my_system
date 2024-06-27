#include "ipc/sem.h"
#include "tools/list.h"
#include "core/task.h"
#include "cpu/irq.h"
void sem_init(sem_t*sem,int init_count){
    sem->count = init_count;
    list_init(&sem->wait_list);
}
void sem_wait(sem_t *sem){
    irq_state_t old_state =  irq_enter_proctection();
    if(sem->count>0)
    {
        sem->count--;
    }
    else{
    // 当信号量为0  取出当前任务
        task_t *cur_task = task_current();
        // 从就绪队列中移除
        task_set_block(cur_task);
        list_insert_last(&sem->wait_list,&cur_task->wait_node);
        task_dispatch();
    }
    irq_leave_proctection(old_state);
}

// 发送信号
void sem_notify(sem_t *sem){
    irq_state_t old_state =  irq_enter_proctection();
    if(list_count(&sem->wait_list)){
        list_node_t* node = list_remove_first(&sem->wait_list);
        task_t *task= list_node_parent(node,task_t,wait_node);
        task_set_ready(task);
        task_dispatch();
    }
    else{
        sem->count++;
    }
    irq_leave_proctection(old_state);

}

int sem_count(sem_t *sem){
    irq_state_t old_state =  irq_enter_proctection();
    int count = sem->count;
    irq_leave_proctection(old_state);
    return count;

}