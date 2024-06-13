#ifndef IRQ_H
#define IRQ_H
#include "comm/types.h"
void irq_init(void);

typedef struct _exception_frame_t
{
    /* data */
    uint32_t gs,fs,es,ds;
    uint32_t edi,esi,ebp,esp,ebx,edx,ecx,eax;
    uint32_t eip,cs,eflags;
}exception_frame_t;

void do_handler_unknown(exception_frame_t*frame);

#endif