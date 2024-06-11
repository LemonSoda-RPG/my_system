    .text
    .extern kernel_init
    .global _start
    # ((void (*)(boot_info_t *))SYS_KERNEL_LOAD_ADDR)(&boot_info)
_start:
    # push %ebp
    # mov %esp, %ebp
    # 这里我们获得的起始是参数的地址
    # mov 0x8(%ebp), %eax
    mov 0x4(%esp), %eax
    push %eax
    call kernel_init
    jmp .

    