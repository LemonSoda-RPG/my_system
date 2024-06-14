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


# 需要传递参数   名字  异常序号  错误码
# 并不是所有的错误都会有错误码  因此这里需要特殊处理
.macro exception_handler name num with_err_code
    .text 
    .extern do_handler_\name
    .global exception_handler_\name
exception_handler_\name:

    # 在调用中断处理函数之前，有一些寄存器需要我们手动进行保存
    # 保存段寄存器  pusha和popa用于一次性保存和恢复所有通用寄存器的值
    .if \with_err_code == 0 
        push $0
    .endif


    push $\num
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    
    push %esp
    call do_handler_\name
    add $(1*4),%esp

    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    
    add $(2*4),%esp
    iret 
.endm


exception_handler unknown, -1, 0
exception_handler divider, 0, 0

exception_handler Debug, 1, 0
exception_handler NMI, 2, 0
exception_handler breakpoint, 3, 0
exception_handler overflow, 4, 0
exception_handler bound_range, 5, 0
exception_handler invalid_opcode, 6, 0
exception_handler device_unavailable, 7, 0
exception_handler double_fault, 8, 1
exception_handler invalid_tss, 10, 1
exception_handler segment_not_present, 11, 1
exception_handler stack_segment_fault, 12, 1
exception_handler general_protection, 13, 1
exception_handler page_fault, 14, 1
exception_handler fpu_error, 16, 0
exception_handler alignment_check, 17, 1
exception_handler machine_check, 18, 0
exception_handler smd_exception, 19, 0
exception_handler virtual_exception, 20, 0
exception_handler control_exception, 21, 1
