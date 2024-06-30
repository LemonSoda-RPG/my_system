#ifndef BITMAP_H
#define BITMAP_H
#include "comm/types.h"
typedef struct _bitmap_t{
    int bit_count;   // 页表的大小  共有多少页  就有多少个bit
    uint8_t *bits;   //位图数组  
}bitmap_t;


void bitmap_init(bitmap_t *bitmap,uint8_t *bits,int bit_count,int init_bit);
int bitmap_byte_count(int bitcount);
int bitmap_get_bit(bitmap_t *bitmap,int index);
// count 连续赋值  从index开始 
void bitmap_set_bit (bitmap_t *bitmap,int index,int count,int bit);
int bitmap_is_set(bitmap_t * bitmap,int index);
// 为进程分配多个页
int bitmap_alloc_nbits(bitmap_t *bitmap, int bit,int count);
#endif