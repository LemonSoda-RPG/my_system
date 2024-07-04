	.text
	.global first_task_entry
	.extern first_task_main
first_task_entry:
	// 重新加载所有的数据段描述符
	mov %ss, %ax		// 16为数据段选择子
	mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    // 长跳转进入到32位内核加载模式中
    jmp first_task_main
