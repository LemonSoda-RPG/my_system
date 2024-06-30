#ifndef KLIB_H
#define KLIB_H
#include "comm/types.h"
#include <stdarg.h>

// size 空闲内存的总大小   bound 页大小  返回值是能够分多少页
// 此函数是将我们的空间向下取整  得到bound的整数倍  因为小于bound的空间，不应该被认为是一个合法的页
// 同理  我们也可以选择向下取整  这样的到的数值将会比size大
static inline uint32_t down2(uint32_t size,uint32_t bound){
    return size & ~(bound -1);
}

static inline uint32_t up2(uint32_t size,uint32_t bound){
    return (size + bound-1) & ~(bound -1);
}

void kernel_strcpy(char *dest,const char *src);
void kernel_strncpy(char *dest,const char *src,int size);
int  kernel_strncmp(const  char *s1,const char *s2,int size);
int  kernel_strlen(const char*str);

void kernel_memcpy(void * dest,void *src,int size);

void kernel_memset(void *dest,uint8_t v,int size);
int kernel_memcmp(void *d1,void*d2,int size);


void kernel_vsprintf(char *str_buf,const char* fmt,va_list args);
void kernel_sprintf(char *str_buf,const char* fmt, ... );

void kernel_itoa(char *buf,int num,int N);

#ifndef RELEASE

#define ASSERT(expr)    \
    if(!(expr)) pannic(__FILE__,__LINE__,__func__,#expr)   // 对expr进行判断
void pannic(const char*file,int line,const char* func,const char*cond);

#else
#define ASSERT(expr) ((void)0)
#endif


#endif