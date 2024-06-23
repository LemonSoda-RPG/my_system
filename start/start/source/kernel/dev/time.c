#include "dev/time.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include "cpu/irq.h"    
static uint32_t sys_tick;

// 当发生计时中断时触发此函数
void do_handler_time(exception_frame_t*frame){
    sys_tick++;
    pic_send_eoi(IRQ0_TIMER);  //  如果不发送结束命令  会一直卡在这个中断

}

// 初始化可编程间隔定时器（PIT）的，用于定期生成定时器中断
static void init_pit (void) {
    uint32_t reload_count = (PIT_OSC_FREQ) / (1000.0 / OS_TICKS_MS);
    // uint32_t reload_count = 0xFFFF;

    // 2023-3-18 写错了，应该是模式3或者模式2    配置定时器  一旦定时器配置完成，无论中断是否打开 定时器都会按照配置运行
    //outb(PIT_COMMAND_MODE_PORT, PIT_CHANNLE0 | PIT_LOAD_LOHI | PIT_MODE0);
    outb(PIT_COMMAND_MODE_PORT, PIT_CHANNEL0 | PIT_LOAD_LOHI | PIT_MODE3);
    outb(PIT_CHANNEL0_DATA_PORT, reload_count & 0xFF);   // 加载低8位
    outb(PIT_CHANNEL0_DATA_PORT, (reload_count >> 8) & 0xFF); // 再加载高8位
    // idt表已经加载了 这时再定义中断处理函数也是可以的
    irq_install(IRQ0_TIMER, (irq_handler_t)exception_handler_time);
    irq_enable(IRQ0_TIMER);
}


void time_init(void){
    sys_tick = 0;
    init_pit();
}

