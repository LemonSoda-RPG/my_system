#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "os_cfg.h"
// boot_info 将用于内存的初始化
void kernel_init(boot_info_t *boot_info){
    cpu_init();   // cpu初始化
    log_init();   // 将log_init放在前面，后面的代码就可以调用来输出错误信息
    irq_init();   // 中断与异常初始化   （同样包括开启中断开关）
    time_init();
}

void init_main(void){
    log_printf("kernel is running 1111......");
    log_printf("kernel version : %s , %s", OS_VERSION,"hahahah");
    log_printf("%d %d ",123,-456);
    // log_printf("%d %d %x %c",123,-456,0x12345,'a');
    // log_printf("version is %s",OS_VERSION);

    // irq_enable_global();
    // int a = 3/0;

    for(;;){}
}