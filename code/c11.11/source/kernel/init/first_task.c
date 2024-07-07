#include "tools/log.h"
#include "tools/klib.h"
#include "core/task.h"
#include "applib/lib_syscall.h"
int first_task_main(void){

    for(;;){
        int pid = getpid();
        for(;;){
            msleep(1000);
            print_msg("task id = %d",pid);
        }
    }
    return 0;

}