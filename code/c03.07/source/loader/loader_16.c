/**
 * 自己动手写操作系统
 *
 * 16位引导代码
 * 二级引导，负责进行硬件检测，进入保护模式，然后加载内核，并跳转至内核运行
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */

// 16位代码，必须加上放在开头，以便有些io指令生成为32位
__asm__(".code16gcc");

void loader_entry(void) {
    for(;;) {}
}


