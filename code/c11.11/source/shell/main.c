#include "lib_syscall.h"

#include <stdio.h>
int main(int argc,char** argv){

    // sbrk(0);
    // sbrk(100);
    // sbrk(200);
    // sbrk(4096*2 + 200);
    // sbrk(4096*4 + 200);
    



    printf("hahawhah\n");

    for(int i =0;i<argc;i++){
        printf("arg:%s\n",argv[i]); 
    }


    fork();
    yield();
    for(;;){
        print_msg("shell pid = %d",getpid());
        msleep(1000);
    }
}