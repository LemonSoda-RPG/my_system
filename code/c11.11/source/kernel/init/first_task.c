#include "tools/log.h"
#include "tools/klib.h"
#include "core/task.h"
#include "applib/lib_syscall.h"
int first_task_main(void){
    int count = 3;
    int pid = fork();
    if(pid<0){
        print_msg("failed",0);
    }
    else if(pid>0){
        print_msg("father",0);
        print_msg("CHILD ID %d",pid);

    }
    else if(pid==0) {
        print_msg("child",0);

        char * argv[] = {"arg0","arg1","arg2","arg3"};
        execve("/shell.elf",argv,0);
    }
    for(;;){
        int pid = getpid();
        for(;;){
            msleep(1000);
            print_msg("task id = %d",pid);
        }
    }
    return 0;

}