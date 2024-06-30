#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "core/task.h"
#include "comm/cpu_instr.h"
#include "tools/list.h"
#include "ipc/sem.h"
#include "core/memory.h"
// 定义两个结构体来描述进程的运行
// static task_t first_task;
static task_t init_task;

static uint32_t init_task_stack[1024];

static sem_t sem;
// boot_info 将用于内存的初始化
void kernel_init(boot_info_t *boot_info){

    ASSERT(boot_info->ram_region_count!=0);   
    cpu_init();   // cpu初始化

    memory_init(boot_info);
    log_init();   // 将log_init放在前面，后面的代码就可以调用来输出错误信息
    irq_init();   // 中断与异常初始化   （同样包括开启中断开关）
    time_init();
    task_manager_init();
}

void init_task_entry(void){
    int count = 0;
    for(;;)
    {  
        // sem_wait(&sem); 
        log_printf("init task:%d",count++);
        
    }
}

void init_main(void){


    // irq_enable_global();
    log_printf("kernel is running 1111......");
    log_printf("kernel version : %s , %s", OS_VERSION,"hahahah");
    log_printf("%d %d %x %c",123,-456,0x80000000,'a');

    task_first_init();
    // 最后一个参数 是传入的栈的指针   为什么要传入最后的地址呢  因为在压栈的时候  地址是从大到小增长的 
    task_init(&init_task,"init_task",(uint32_t)init_task_entry,(uint32_t)&init_task_stack[1024]);
    
    sem_init(&sem,1);
    
    irq_enable_global();
    int count = 0;
    for(;;)
    {
        
        log_printf("init main: %d",count++);
        // sys_sleep(900);
        // sem_notify(&sem);


    }
    

}