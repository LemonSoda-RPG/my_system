#include "cpu/irq.h"
#include "cpu/cpu.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#define IDT_TABLE_NR    128


static gate_sesc_t idt_table[IDT_TABLE_NR];


void irq_init(void)
{
    for(int i=0;i<IDT_TABLE_NR;i++)
    {
        gate_desc_set(idt_table+i,KERNEL_SELECTOR_CS,(uint32_t)exception_handler_unknown,
        GATE_P_PRESENT|GATE_DPL_0|GATE_TYPE_INT);
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
    irq_install(IRQ21_CP,(irq_handler_t)exception_handler_control_exception);
    lidt((uint32_t)idt_table,sizeof(idt_table));
}

// 个性化配置中断处理函数
int irq_install(int irq_num,irq_handler_t handler)
{
    if(irq_num >=IDT_TABLE_NR){
        return -1;
    }
    gate_desc_set(idt_table+irq_num,KERNEL_SELECTOR_CS,(uint32_t)handler,
        GATE_P_PRESENT|GATE_DPL_0|GATE_TYPE_INT);
    return 0;
}



static void do_deafault_hanlder(exception_frame_t*frame,const char*msg){
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