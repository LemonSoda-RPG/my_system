#include "os_cfg.h"

    .text
    .extern kernel_init
    .global _start
    .extern init_main

_start:
    
    push 4(%esp)
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

    .text
# 需要传递参数   名字  异常序号  错误码
# 并不是所有的错误都会有错误码  因此这里需要特殊处理
.macro exception_handler name num with_err_code
        .extern do_handler_\name
        .global exception_handler_\name
    exception_handler_\name:

        # 在调用中断处理函数之前，有一些寄存器需要我们手动进行保存
        # 保存段寄存器  pusha和popa用于一次性保存和恢复所有通用寄存器的值
        .if \with_err_code == 0 
            push $0
        .endif


        push $\num
        pushal
        push %ds
        push %es
        push %fs
        push %gs
        
        push %esp     // frame的指针
        call do_handler_\name
        add $(1*4),%esp

        pop %gs
        pop %fs
        pop %es
        pop %ds
        popal
        
        add $(2*4),%esp      //跳过err_code和num
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



exception_handler time, 0x20, 0

// simple_switch(&from,to)
    .text
    .global simple_switch
simple_switch:
//整个过程应该是 将寄存器压到当前任务的栈里面  然后切换到另一个任务  再从另一个任务的栈里面取出所需的寄存器

// 取出传参   调用函数传参是从右到左压栈  所以最右的参数先压入  地址最大
// from
    mov 4(%esp), %eax
// to 
    mov 8(%esp), %edx


// 将寄存器压入到当前任务的栈
    push %ebp
    push %ebx
    push %esi
    push %edi
    
// 保存这个栈的指针
    mov %esp, (%eax)

/*
    mov %esp, %eax      将 %esp 的值复制到 %eax 寄存器。
    mov %esp, (%eax)    将 %esp 的值存储到 %eax 指定的内存地址。
*/


// 这里进行任务的切换  既然要切任务  那就得知道下一个任务的栈的位置  因此需要传参
    mov %edx, %esp
    
// 切换之后 将当前栈中的寄存器弹出
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret

