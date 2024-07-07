#include "core/syscall.h"
#include "applib/lib_syscall.h"
#include "core/task.h"
#include "tools/log.h"
typedef int (*syscall_handler_t)(uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3);

void sys_print_msg(char*fmt,int arg){
	log_printf(fmt,arg);
}

// 系统调用函数表  
static const syscall_handler_t sys_table[] = {
    [SYS_sleep] = (syscall_handler_t)sys_msleep,
	[SYS_getpid] = (syscall_handler_t)sys_getpid,
	[SYS_printmsg] = (syscall_handler_t)sys_print_msg,
	[SYS_fork] = (syscall_handler_t)sys_fork,
};
void do_handler_syscall (syscall_frame_t * frame) {
	// 超出边界，返回错误
    if (frame->func_id < sizeof(sys_table) / sizeof(sys_table[0])) {
		// 查表取得处理函数，然后调用处理
		syscall_handler_t handler = sys_table[frame->func_id];
		if (handler) {
			int ret = handler(frame->arg0, frame->arg1, frame->arg2, frame->arg3);
			frame->eax = ret;  // 设置系统调用的返回值，由eax传递
            return;
		}
	}
	// 不支持的系统调用，打印出错信息
	task_t * task = task_current();
	log_printf("task: %s, Unknown syscall: %d", task->name,  frame->func_id);
    frame->eax = -1;  // 设置系统调用的返回值，由eax传递
}