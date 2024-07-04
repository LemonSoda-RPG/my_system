/**
 * 内核初始化以及测试代码
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "comm/boot_info.h"
#include "comm/cpu_instr.h"
#include "cpu/cpu.h"
#include "cpu/irq.h"
#include "dev/time.h"
#include "tools/log.h"
#include "core/task.h"
#include "os_cfg.h"
#include "tools/klib.h"
#include "tools/list.h"
#include "ipc/sem.h"
#include "core/memory.h"

static boot_info_t * init_boot_info;        // 启动信息


/**
 * 内核入口
 */
void kernel_init (boot_info_t * boot_info) {
    init_boot_info = boot_info;

    // 初始化CPU，再重新加载
    cpu_init();

    // 内存初始化要放前面一点，因为后面的代码可能需要内存分配
    memory_init(boot_info);
    log_init();
    irq_init();
    time_init();
    task_manager_init();
}

void move_to_first_task(void){
    task_t * curr  = task_current();
    ASSERT(curr!=0);
    tss_t * tss = &(curr->tss);
    __asm__ __volatile__(
        "jmp *%p[ip]"::[ip]"r"(tss->eip)
    );
}

void init_main(void) {
    log_printf("Kernel is running....");
    log_printf("Version: %s, name: %s", OS_VERSION, "tiny x86 os");
    log_printf("%d %d %x %c", -123, 123456, 0x12345, 'a');

    // 配置第一个任务的参数  
    task_first_init();
    // 跳转到第一个任务
    move_to_first_task();
}
