    .text
    .global _start
    .extern cstart
_start:
    mov %ss, %ax		// 16为数据段选择子
	mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    // 长跳转进入到32位内核加载模式中
    jmp cstart