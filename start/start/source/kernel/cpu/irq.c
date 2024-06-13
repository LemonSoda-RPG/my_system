#include "cpu/irq.h"
#include "cpu/cpu.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"
#include "os_cfg.h"
#define IDT_TABLE_NR    128


static gate_sesc_t idt_table[IDT_TABLE_NR];
extern void exception_handler_unknown(void);

void irq_init(void)
{
    for(int i=0;i<IDT_TABLE_NR;i++)
    {
        gate_desc_set(idt_table+i,KERNEL_SELECTOR_CS,(uint32_t)exception_handler_unknown,
        GATE_P_PRESENT|GATE_DPL_0|GATE_TYPE_INT);

    }

    lidt((uint32_t)idt_table,sizeof(idt_table));
}
static void do_deafault_hanlder(exception_frame_t*frame,const char*msg){
    for(;;){}
}
void do_handler_unknown(exception_frame_t*frame){
    do_deafault_hanlder(frame,"unknown exception!");
}