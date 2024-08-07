/**
 * 自己动手写操作系统
 *
 * 系统引导部分，启动时由硬件加载运行，然后完成对二级引导程序loader的加载
 * 该部分程序存储于磁盘的第1个扇区，在计算机启动时将会由BIOS加载到0x7c00处
 * 之后，将由BIOS跳转至0x7c00处开始运行
 * 
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */

  	// 16位代码，务必加上
  	.code16
 	.text
	.global _start
	.extern loader_entry
	


_start:
	jmp loader_entry

	.code32
	.text
	.global protect_mode_entry
	.extern load_kernel
	// 有了global修饰  那么外部代码也能调用这个函数  
	// 和extern的用法有点相似
protect_mode_entry:
	// 16 是数据段
	mov $16, %ax
	mov %ax, %ds
	mov %ax, %ss 
	mov %ax, %es 
	mov %ax, %fs 
	mov %ax, %gs 
	// 8 代码段
	jmp $8, $load_kernel