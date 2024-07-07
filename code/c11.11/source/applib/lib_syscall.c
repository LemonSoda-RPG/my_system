#include "applib/lib_syscall.h"

#include "comm/types.h"



int print_msg(char * fmt, int arg) {
    syscall_args_t args;
    args.id = SYS_printmsg;
    args.arg0 = (int)fmt;
    args.arg1 = arg;
    return sys_call(&args);
}

int fork() {
    syscall_args_t args;
    args.id = SYS_fork;
    return sys_call(&args);
}

int execve(const char *name, char * const *argv, char * const *env) {
    syscall_args_t args;
    args.id = SYS_execve;
    args.arg0 = (int)name;
    args.arg1 = (int)argv;
    args.arg2 = (int)env;
    return sys_call(&args);
}

int yield (void) {
    syscall_args_t args;
    args.id = SYS_yield;
    return sys_call(&args);
}

int open(const char *name, int flags, ...) {
    // 不考虑支持太多参数
    syscall_args_t args;
    args.id = SYS_open;
    args.arg0 = (int)name;
    args.arg1 = (int)flags;
    return sys_call(&args);
}

int read(int file, char *ptr, int len) {
    syscall_args_t args;
    args.id = SYS_read;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = len;
    return sys_call(&args);
}

int write(int file, char *ptr, int len) {
    syscall_args_t args;
    args.id = SYS_write;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = len;
    return sys_call(&args);
}

int close(int file) {
    syscall_args_t args;
    args.id = SYS_close;
    args.arg0 = (int)file;
    return sys_call(&args);
}

int lseek(int file, int ptr, int dir) {
    syscall_args_t args;
    args.id = SYS_lseek;
    args.arg0 = (int)file;
    args.arg1 = (int)ptr;
    args.arg2 = dir;
    return sys_call(&args);
}