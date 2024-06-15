#ifndef TIME_H
#define TIME_H
#include "cpu/irq.h"

#define PIT_OSC_FREQ            1193182
#define PIT_COMMAND_MODE_PORT   0X43         // 定时器配置端口
#define PIT_CHANNEL0_DATA_PORT  0X40

#define PIT_CHANNEL0             (0<<6)
#define PIT_LOAD_LOHI           (3<<4)
#define PIT_MODE3               (3<<1)
// 中断
void do_handler_time(exception_frame_t*frame);
//中断
extern void exception_handler_time(void);
void time_init(void);
#endif