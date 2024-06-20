#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "core/task.h"


// boot_info 将用于内存的初始化
void kernel_init(boot_info_t *boot_info){
    ASSERT(boot_info->ram_region_count!=0);
    cpu_init();   // cpu初始化
    log_init();   // 将log_init放在前面，后面的代码就可以调用来输出错误信息
    irq_init();   // 中断与异常初始化   （同样包括开启中断开关）
    time_init();
}

// 定义两个结构体来描述进程的运行
static task_t first_task;
static task_t init_task;
static uint32_t init_task_stack[1024];

void init_task_entry(void){
    int count = 0;
    for(;;)
    {
        log_printf("int task:%d",count++);
    }
}

void init_main(void){
    log_printf("kernel is running 1111......");
    log_printf("kernel version : %s , %s", OS_VERSION,"hahahah");
    log_printf("%d %d %x %c",123,-456,0x80000000,'a');
    // log_printf("%d %d %x %c",123,-456,0x12345,'a');
    


    // 最后一个参数 是传入的栈的指针   为什么要传入最后的地址呢  因为在压栈的时候  地址是从大到小增长的 
    task_init(&init_task,(uint32_t)init_task_entry,(uint32_t)&init_task_stack[1024]);

    task_init(&first_task,0,0);
    
    int count = 0;
    for(;;)
    {
        log_printf("int main: %d",count++);
        // 设定一个小程序  能够切换到另一个进程
    }
    // init_task_entry();

}