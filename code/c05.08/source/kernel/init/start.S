/**
 * 自己动手写操作系统
 *
 * 二级引导，负责进行硬件检测，进入保护模式，然后加载内核，并跳转至内核运行
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
 #include "os_cfg.h"

  	// 不必加.code32因默认就是32位
 	.text
 	.global _start
	.extern kernel_init
	.extern init_main
_start:
    # 第一种方法
    # push %ebp
    # mov %esp, %ebp
    # mov 0x8(%ebp), %eax
    # push %eax

    # 第二种方法
    # mov 4(%esp), %eax
    # push %eax

    # 第三种方法
    push 4(%esp)

    # kernel_init(boot_info)
    call kernel_init

	// 重新加载GDT
	jmp $KERNEL_SELECTOR_CS, $gdt_reload

gdt_reload:
	mov $KERNEL_SELECTOR_DS, %ax		// 16为数据段选择子
	mov %ax, %ds
    mov %ax, %ss
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

	// 栈设置
	mov $(stack + KERNEL_STACK_SIZE), %esp

	// 栈和段等沿用之前的设置
	jmp init_main

	.bss
.comm stack, KERNEL_STACK_SIZE    // comm 声明未初始化的通用内存区域，以字节计


// 中断发生时，会自动切换到特权级0对应的栈中去执行
// 并且只保存ss,esp,cs,eip,flags寄存器
// 所以需要在中断中自行保存其它寄存器

    .text
	.extern do_handler_unknown
	.global exception_handler_unknown
exception_handler_unknown:
	// 保存所有寄存器
	pusha
	push %ds
	push %es
	push %fs
	push %gs

	// 调用中断处理函数
	push %esp
	call do_handler_unknown
	add $(1*4), %esp		// 丢掉esp

	// 恢复保存的寄存器
	pop %gs
	pop %fs
	pop %es
	pop %ds
	popa
	iret

