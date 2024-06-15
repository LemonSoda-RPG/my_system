#include "init.h"
#include "comm/boot_info.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"

// boot_info 将用于内存的初始化
void kernel_init(boot_info_t *boot_info){
    cpu_init();   // cpu初始化
    irq_init();   // 中断初始化
    time_init();
}

void init_main(void){
    irq_enable_global();
    // int a = 3/0;
    for(;;){}
}