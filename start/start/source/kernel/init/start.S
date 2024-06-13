#include "os_cfg.h"

    .text
    .extern kernel_init
    .global _start
    .extern init_main
    # ((void (*)(boot_info_t *))SYS_KERNEL_LOAD_ADDR)(&boot_info)
_start:
    # push %ebp
    # mov %esp, %ebp
    # 这里我们获得的起始是参数的地址
    # mov 0x8(%ebp), %eax
    mov 0x4(%esp), %eax  # 此时esp指向返回地址 在调用start之前 对参数进行了压栈，这里将参数取出，重新压栈
    push %eax
    # 在调用函数之前  将参数进行压栈
    call kernel_init

    jmp $KERNEL_SELECTOR_CS,$gdt_reload

gdt_reload:
    mov $KERNEL_SELECTOR_DS, %ax
	mov %ax, %ds
	mov %ax, %ss 
	mov %ax, %es 
	mov %ax, %fs 
	mov %ax, %gs 
    # 创建栈
    mov $(stack + KERNEL_STACK_SIZE), %esp
    jmp init_main

    .bss
.comm stack, KERNEL_STACK_SIZE

#   .macro exception_handler name
    .text 
    .extern do_handler_unknown
    .global exception_handler_unknown
exception_handler_unknown:

    # 在调用中断处理函数之前，有一些寄存器需要我们手动进行保存
    # 保存段寄存器  pusha和popa用于一次性保存和恢复所有通用寄存器的值
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    push %esp
    call do_handler_unknown

    pop %esp
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    iret 
