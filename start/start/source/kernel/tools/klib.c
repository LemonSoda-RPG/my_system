#include "tools/klib.h"
void kernel_strcpy(char *dest,const char *src){
    if(!dest || !src)
    {
        return;
    }
    while(*src && *dest)
    {
        *dest++ = *src++;
    }
    *dest = '\0';

    return;
}
void kernel_strncpy(char *dest,const char *src,int size){
    if(!dest || !src||!size)
    {
        return;
    }
    char * d = dest;
    const char * s = src;
    while ((size-- > 0) && (*s)) {
        *d++ = *s++;
    }
    if(size == 0)
    {
        *(d-1) = '\0';
    }
    else{
        *d = '\0';
    }

    return;
}
int  kernel_strncmp(const  char *s1,const char *s2,int size)
{
    if(!s1 || !s2)
    {
        return -1;
    }

    while(*s1 && *s2 && (*s1 == *s2) && size--){
        s1++;
        s2++;
    }
    return !((*s1=='\0')||(*s2=='\0')||(*s1==*s2));

}
int  kernel_strlen(const char*str){
    if(!str)
        return 0;
    int len = 0;
    const char *c = str;
    while(*c)
    {
        c++;
        len++;
    }
    return len;
}
void kernel_memcpy(void * dest,void *src,int size){
    if(!dest || !src||!size)
    {
        return;
    }
    uint8_t *d = (uint8_t *)dest;
    uint8_t *s = (uint8_t *)src;
    while(size--)
    {
        *d++=*s++;
    }
}

void kernel_memset(void *dest,uint8_t v,int size){
    if(!dest||!size){
        return;
    }
    uint8_t *d = (uint8_t*)dest;
    while(size--){
        *d++ = v;
    }

}
int kernel_memcmp(void *d1,void*d2,int size){
    if(!d1 || !d2||!size)
    {
        return 1;
    }
    uint8_t *p_d1 = (uint8_t *)d1;
    uint8_t *p_d2 = (uint8_t *)d2;
    while(size--)
    {
        if(*p_d1++ != *p_d2++){
            return 1;
        }
    }
    return 0;

}
void kernel_sprintf(char *str_buf,const char* fmt, ... ){

  
    va_list args;
    va_start(args,fmt);   // 将fmt后面的可变参数 保存到args中
    kernel_vsprintf(str_buf,fmt,args);
    va_end(args);

}
void kernel_itoa(char *buf,int num,int base)
{

    static const char * num2ch = {
        "0123456789ABCDEF"
    };
    char *p = buf;

    if(base!=2 && base!=8 && base!=10 && base!=16)
    {
        *p = '\0';
        return;
    }
    if(num<0&&base==10)
    {
        num*=-1;
        *p++ = '-';
    }
    do{
        char ch = num2ch[num % base];
        *p++ = ch;
        num = num / base;
    }while(num);
    *p-- = '\0';
    char* start = buf;
    while(start<p)
    {
        if(*start=='-')
        {
            start++;
        }
        char tmp = (*start);
        *start = (*p);
        *p = tmp;
        start++;
        p--;
    }
}



// 对fmt进行解析
void kernel_vsprintf(char *str_buf,const char* fmt,va_list args){
    enum{
        NORMAL,
        READ_FMT,
    } state = NORMAL;
    

    char * curr = str_buf;
    char ch;
    while((ch = *fmt++)){
        switch (state)
        {
        case NORMAL:
            if(ch=='%')
            {
                state = READ_FMT;
            }
            else{
                *curr++ = ch;
            }
            break;
        case READ_FMT:
            if(ch=='d')
            {
                int num = va_arg(args,int);
                kernel_itoa(curr,num,10);    //写入的字符串指针，数字，数字的进制
                curr+=kernel_strlen(curr);
            }
            else if(ch=='x'){
                int num = va_arg(args,int);
                kernel_itoa(curr,num,16);    //写入的字符串指针，数字，数字的进制
                curr+=kernel_strlen(curr);
            }
            else if(ch=='c'){

            }
            else if(ch=='s')
            {   
                const char * str = va_arg(args,char*);
                int len = kernel_strlen(str);
                while(len--){
                    *curr++ = *str++;
                }  
            }
            state = NORMAL;

        default:
            break;
        }
        
    }
}