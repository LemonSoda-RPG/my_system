#ifndef SYSCALL_H
#define SYSCALL_H

#define SYSCALL_PARAM_COUNT 5

#define SYS_sleep       0
#define SYS_getpid      1
#define SYS_fork		2
#define SYS_execve		3
#define SYS_yield		4
#define SYS_printmsg	100


// 定义我们压栈的数据结构

typedef struct _syscall_frame_t {
	int eflags;
	int gs, fs, es, ds;
	int edi, esi, ebp, dummy, ebx, edx, ecx, eax;
	int eip, cs;
	int func_id, arg0, arg1, arg2, arg3;
	int esp, ss;
}syscall_frame_t;


extern void exception_handler_syscall(void);





#endif