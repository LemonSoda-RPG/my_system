#ifndef LIB_SYSCALL_H
#define LIB_SYSCALL_H

#include "core/syscall.h"
#include "os_cfg.h"
#include "comm/types.h"
typedef struct _syscall_args_t{
    int id;  //通过id判断使用哪个函数
    int arg0;  //四个参数
    int arg1;
    int arg2;
    int arg3;
}syscall_args_t;
static inline int sys_call(syscall_args_t *args){
	uint32_t addr[] = {0, SELECTOR_SYSCALL|0};
    int ret;

    // 采用调用门, 这里只支持5个参数
    // 用调用门的好处是会自动将参数复制到内核栈中，这样内核代码很好取参数
    // 而如果采用寄存器传递，取参比较困难，需要先压栈再取

    // lcall指令能够判断调用的是不是门函数
    __asm__ __volatile__(
            "push %[arg3]\n\t"
            "push %[arg2]\n\t"
            "push %[arg1]\n\t"
            "push %[arg0]\n\t"
            "push %[id]\n\t"
            "lcalll *(%[gate])\n\n"   //在这里跳到了exception_handler_syscall 函数  也就是这这里发生了系统调用
            :"=a"(ret)       // a 是调用的处理函数的返回值  
            :[arg3]"r"(args->arg3), [arg2]"r"(args->arg2), [arg1]"r"(args->arg1),
    [arg0]"r"(args->arg0), [id]"r"(args->id),
    [gate]"r"(addr));
    return ret;

}


static inline void msleep(int ms){
    if(ms<=0){
        return;
    }
    syscall_args_t args;
    args.id = SYS_sleep;
    args.arg0 = ms;
    sys_call(&args);  // 通用的系统调用接口函数
}

static inline int getpid() {
    syscall_args_t args;
    args.id = SYS_getpid;
    return sys_call(&args);
}

static inline void print_msg(char * fmt, int arg) {
    syscall_args_t args;
    args.id = SYS_printmsg;
    args.arg0 = (int)fmt;
    args.arg1 = arg;
    sys_call(&args);
}



static inline int fork(){
    syscall_args_t args;
    args.id = SYS_fork;
    return sys_call(&args);
}

static inline int execve(const char*name,char * const * argv,char *const*env){
    syscall_args_t args;
    args.id = SYS_execve;
    args.arg0 = (int)name;
    args.arg1 = (int)argv;
    args.arg2 = (int)env;
    return sys_call(&args);
}

static inline int yield(void){
    syscall_args_t args;
    args.id = SYS_yield;
  
    return sys_call(&args);
}


#endif