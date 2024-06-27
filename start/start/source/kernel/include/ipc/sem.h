#ifndef SEM_H
#define SEM_H
#include "tools/list.h"
typedef struct _sem_t
{
    int count;
    list_t wait_list;    /* data */
}sem_t;


void sem_init(sem_t*sem,int init_count);
// 等待信号
void sem_wait(sem_t *sem);

// 发送信号
void sem_notify(sem_t *sem);

// 查询信号量
int sem_count(sem_t *sem);


#endif