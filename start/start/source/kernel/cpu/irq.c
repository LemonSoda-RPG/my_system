#include "cpu/irq.h"
#include "cpu/cpu.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#include "tools/log.h"
#define IDT_TABLE_NR    128
static gate_sesc_t idt_table[IDT_TABLE_NR];


// 用于初始化8259可编程中断控制器（PIC）
static void init_pic(void) {
    // 边缘触发，级联，需要配置icw4, 8086模式
    outb(PIC0_ICW1, PIC_ICW1_ALWAYS_1 | PIC_ICW1_ICW4);

    // 对应的中断号起始序号0x20
    outb(PIC0_ICW2, IRQ_PIC_START);

    // 主片IRQ2有从片
    outb(PIC0_ICW3, 1 << 2);

    // 普通全嵌套、非缓冲、非自动结束、8086模式
    outb(PIC0_ICW4, PIC_ICW4_8086);

    // 边缘触发，级联，需要配置icw4, 8086模式
    outb(PIC1_ICW1, PIC_ICW1_ICW4 | PIC_ICW1_ALWAYS_1);

    // 起始中断序号，要加上8
    outb(PIC1_ICW2, IRQ_PIC_START + 8);

    // 没有从片，连接到主片的IRQ2上
    outb(PIC1_ICW3, 2);

    // 普通全嵌套、非缓冲、非自动结束、8086模式
    outb(PIC1_ICW4, PIC_ICW4_8086);

    // 禁止所有中断, 允许从PIC1传来的中断
    outb(PIC0_IMR, 0xFF & ~(1 << 2));
    outb(PIC1_IMR, 0xFF);
}

// 关闭中断
void irq_disable_global(void){
    cli();
}
void irq_enable_global(void){
    sti();
}

// 通过irqnum 开启指定中断
void irq_enable(int irq_num) {
    if (irq_num < IRQ_PIC_START) {
        return;
    }

    irq_num -= IRQ_PIC_START;
    if (irq_num < 8) {
        uint8_t mask = inb(PIC0_IMR) & ~(1 << irq_num);
        outb(PIC0_IMR, mask);
    } else {
        irq_num -= 8;
        uint8_t mask = inb(PIC1_IMR) & ~(1 << irq_num);
        outb(PIC1_IMR, mask);
    }
}

void irq_disable(int irq_num) {
    if (irq_num < IRQ_PIC_START) {
        return;
    }

    irq_num -= IRQ_PIC_START;
    if (irq_num < 8) {
        uint8_t mask = inb(PIC0_IMR) | (1 << irq_num);
        outb(PIC0_IMR, mask);
    } else {
        irq_num -= 8;
        uint8_t mask = inb(PIC1_IMR) | (1 << irq_num);
        outb(PIC1_IMR, mask);
    }
}


//  初始化异常处理
void irq_init(void)
{
    for(int i=0;i<IDT_TABLE_NR;i++)
    {
        gate_desc_set(idt_table+i,KERNEL_SELECTOR_CS,(uint32_t)exception_handler_unknown,
        GATE_P_PRESENT|GATE_DPL_0|GATE_TYPE_IDT);
    }
    // 配置中断处理函数
    irq_install(IRQ0_DE, (irq_handler_t)exception_handler_divider);
	irq_install(IRQ1_DB, (irq_handler_t)exception_handler_Debug);
	irq_install(IRQ2_NMI, (irq_handler_t)exception_handler_NMI);
	irq_install(IRQ3_BP, (irq_handler_t)exception_handler_breakpoint);
	irq_install(IRQ4_OF, (irq_handler_t)exception_handler_overflow);
	irq_install(IRQ5_BR, (irq_handler_t)exception_handler_bound_range);
	irq_install(IRQ6_UD, (irq_handler_t)exception_handler_invalid_opcode);
	irq_install(IRQ7_NM, (irq_handler_t)exception_handler_device_unavailable);
	irq_install(IRQ8_DF, (irq_handler_t)exception_handler_double_fault);
	irq_install(IRQ10_TS, (irq_handler_t)exception_handler_invalid_tss);
	irq_install(IRQ11_NP, (irq_handler_t)exception_handler_segment_not_present);
	irq_install(IRQ12_SS, (irq_handler_t)exception_handler_stack_segment_fault);
	irq_install(IRQ13_GP, (irq_handler_t)exception_handler_general_protection);
	irq_install(IRQ14_PF, (irq_handler_t)exception_handler_page_fault);
	irq_install(IRQ16_MF, (irq_handler_t)exception_handler_fpu_error);
	irq_install(IRQ17_AC, (irq_handler_t)exception_handler_alignment_check);
	irq_install(IRQ18_MC, (irq_handler_t)exception_handler_machine_check);
	irq_install(IRQ19_XM, (irq_handler_t)exception_handler_smd_exception);
	irq_install(IRQ20_VE, (irq_handler_t)exception_handler_virtual_exception);
    irq_install(IRQ21_CP, (irq_handler_t)exception_handler_control_exception);


    lidt((uint32_t)idt_table,sizeof(idt_table));


    init_pic();
}

// 个性化配置中断处理函数
int irq_install(int irq_num,irq_handler_t handler)
{
    if(irq_num >=IDT_TABLE_NR){
        return -1;
    }
    // IRQ_NUM  就是中断号  也就是下标  我们通过这个下标找到gdt选择子  
    gate_desc_set(idt_t able+irq_num,KERNEL_SELECTOR_CS,(uint32_t)handler,
        GATE_P_PRESENT|GATE_DPL_0|GATE_TYPE_IDT);
    return 0;
}

static void dump_core_regs(exception_frame_t*frame){
    log_printf("IRQ: %d, error code: %d", frame->num,frame->err_code);
    log_printf("cs:%d  ds:%d  es:%d  ss:%d  fs:%d  gs:%d",
        frame->cs,frame->ds,frame->es,frame->ds,frame->fs,frame->gs);   
    log_printf( "eax: 0x%x\n"
                "ebx: 0x%x\n"
                "ecx: 0x%x\n"
                "edx: 0x%x\n"
                "edi: 0x%x\n"
                "esi: 0x%x\n" 
                "ebp: 0x%x\n"
                "esp: 0x%x\n",
                frame->eax,frame->ebx,frame->ecx,frame->edi,frame->edi,frame->esi,
                frame->ebp,frame->esp);
    log_printf("eip:0x%x\neflags:0x%x\n",frame->eip,frame->eflags);
}

static void do_deafault_hanlder(exception_frame_t*frame,const char*msg){
    log_printf("-----------------");
    log_printf("IRQ/Exception happend:%s",msg);
    dump_core_regs(frame);
    for(;;){
        hlt();
    }
}


// 中断处理函数
void do_handler_unknown(exception_frame_t*frame){
    do_deafault_hanlder(frame,"unknown exception!");
}

void do_handler_divider(exception_frame_t*frame){
    do_deafault_hanlder(frame,"Divider exception!");
}

void do_handler_Debug(exception_frame_t*frame){
    do_deafault_hanlder(frame,"Debug exception!");
}
void do_handler_NMI(exception_frame_t*frame){
    do_deafault_hanlder(frame,"NMI exception!");
}
void do_handler_breakpoint(exception_frame_t*frame){
    do_deafault_hanlder(frame,"breakpoint exception!");
}
void do_handler_overflow(exception_frame_t*frame){
    do_deafault_hanlder(frame,"overflow exception!");
}
void do_handler_bound_range(exception_frame_t*frame){
    do_deafault_hanlder(frame,"bound_range exception!");
}
void do_handler_invalid_opcode(exception_frame_t*frame){
    do_deafault_hanlder(frame,"invalid_opcode exception!");
}
void do_handler_device_unavailable(exception_frame_t*frame){
    do_deafault_hanlder(frame,"device_unavailable exception!");
}
void do_handler_double_fault(exception_frame_t*frame){
    do_deafault_hanlder(frame,"double_fault exception!");
}
void do_handler_invalid_tss(exception_frame_t*frame){
    do_deafault_hanlder(frame,"invalid_tss exception!");
}
void do_handler_segment_not_present(exception_frame_t*frame){
    do_deafault_hanlder(frame,"segment_not_present exception!");
}
void do_handler_stack_segment_fault(exception_frame_t*frame){
    do_deafault_hanlder(frame,"stack_segment_fault exception!");
}
void do_handler_general_protection(exception_frame_t*frame){
    do_deafault_hanlder(frame,"general_protection exception!");
}
void do_handler_page_fault(exception_frame_t*frame){
    do_deafault_hanlder(frame,"page_fault exception!");
}
void do_handler_fpu_error(exception_frame_t*frame){
    do_deafault_hanlder(frame,"fpu_error exception!");
}
void do_handler_alignment_check(exception_frame_t*frame){
    do_deafault_hanlder(frame,"alignment_check exception!");
}
void do_handler_machine_check(exception_frame_t*frame){
    do_deafault_hanlder(frame,"machine_check exception!");
}
void do_handler_smd_exception(exception_frame_t*frame){
    do_deafault_hanlder(frame,"smd_exception exception!");
}
void do_handler_virtual_exception(exception_frame_t*frame){
    do_deafault_hanlder(frame,"virtual_exception exception!");
}
void do_handler_control_exception(exception_frame_t*frame){
    do_deafault_hanlder(frame,"control_exception exception!");
}


void irq_send_eoi(int irq_num){
    irq_num -= IRQ_PIC_START;
    if(irq_num >= 8)
    {
        outb(PIC1_OCW2,PIC_OCW2_EOI);
    }
    outb(PIC0_OCW2, PIC_OCW2_EOI);

}