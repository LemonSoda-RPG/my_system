/**
 * 任务管理
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */

#include "cpu/mmu.h"
#include "comm/cpu_instr.h"
#include "core/task.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "os_cfg.h"
#include "cpu/irq.h"
#include "core/syscall.h"
#include "core/memory.h"
static task_manager_t task_manager;     // 任务管理器
static uint32_t idle_task_stack[IDLE_STACK_SIZE];	// 空闲任务堆栈
static task_t task_table[TASK_NR];   // 存储所有的task

static mutex_t task_table_mutex;




static int tss_init (task_t * task, int flag, uint32_t entry, uint32_t esp) {
    // 为TSS分配GDT
    // 查找空闲的gdt下表
    int tss_sel = gdt_alloc_desc();
    if (tss_sel < 0) {
        log_printf("alloc tss failed.\n");
        return -1;
    }
    // 每个任务的tss段都是保存在gdt表中
    segment_desc_set(tss_sel, (uint32_t)&task->tss, sizeof(tss_t),
            SEG_P_PRESENT | SEG_DPL0 | SEG_TYPE_TSS);

    // tss段初始化
    kernel_memset(&task->tss, 0, sizeof(tss_t));

    // 为任务分配一个自己的栈
    uint32_t kernel_stack = memory_alloc_page();
    if(kernel_stack==0){
        goto tss_init_failed;
    }
    int code_sel,data_sel;
    // 
    if(flag&TASK_FLAGS_SYSTEM){
        // 高特权级就使用 内核段
        code_sel = KERNEL_SELECTOR_CS;
        data_sel = KERNEL_SELECTOR_DS;
    }else{
        code_sel = task_manager.app_code_sel | SEG_CPL3;
        data_sel = task_manager.app_date_sel | SEG_CPL3;
    }
    
    task->tss.eip   = entry;
    task->tss.esp   =  esp;
    task->tss.esp0  =  kernel_stack+MEM_PAGE_SIZE;

    task->tss.ss0 = KERNEL_SELECTOR_DS;
    task->tss.ss = data_sel;
    task->tss.eflags = EFLAGS_DEFAULT | EFLAGS_IF;
    task->tss.es =  task->tss.ds
            = task->tss.fs = task->tss.gs = data_sel;   // 暂时写死
    task->tss.cs = code_sel;    // 暂时写死
    task->tss.iomap = 0;


    // 每一个应用程序进程都有一个自己的页表
    uint32_t page_dir = memory_create_uvm();
    if(page_dir == 0){
        
        goto tss_init_failed;

    }
    task->tss.cr3 = page_dir;

    task->tss_sel = tss_sel;
    return 0;
tss_init_failed:
    gdt_free_sel(tss_sel);
    if(kernel_stack){
        memory_free_page(kernel_stack);
    }
    return -1;
}

/**
 * @brief 初始化任务
 */
int task_init (task_t *task, const char * name, int flag, uint32_t entry, uint32_t esp) {
    ASSERT(task != (task_t *)0);

    int err = tss_init(task,flag, entry, esp);
    if (err < 0) {
        log_printf("init task failed.\n");
        return err;
    }

    // 任务字段初始化
    kernel_strncpy(task->name, name, TASK_NAME_SIZE);
    task->state = TASK_CREATED;
    task->parent = (task_t*) 0;
    task->sleep_ticks = 0;
    task->time_slice = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_slice;
    task->pid = (uint32_t)task;
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    // 插入就绪队列中和所有的任务队列中
    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);
    return 0;
}

void task_uninit(task_t* task){
    if(task->tss_sel){
        gdt_free_sel(task->tss_sel);
    }
    if(task->tss.esp0){
        memory_free_page(task->tss.esp-MEM_PAGE_SIZE);
    }
    if(task->tss.cr3){
        memory_destory_uvm(task->tss.cr3);
    }
    kernel_memset(task,0,sizeof(task_t));
}


void simple_switch (uint32_t ** from, uint32_t * to);

/**
 * @brief 切换至指定任务
 */
void task_switch_from_to (task_t * from, task_t * to) {
     switch_to_tss(to->tss_sel);
    //simple_switch(&from->stack, to->stack);
}

void task_first_init (void) {

    extern void first_task_entry(void);
    // 第一个任务的开始和结束的物理地址
    extern uint8_t s_first_task[], e_first_task[];

    uint32_t copy_size = (uint32_t)(e_first_task - s_first_task);
    // 给我们拷贝的程序分配的空间
    // 这个其实就是我们给第一个任务分配的应用程序专用的栈
    uint32_t alloc_size = 10 * MEM_PAGE_SIZE;
    ASSERT(copy_size<alloc_size);

    uint32_t first_start = (uint32_t) first_task_entry;
    // 对任务进行初始化
    // 这个任务一开始是有汇编运行的  之后会跳转到C
    // 第一个任务属于应用程序  我们将他的特权级设置成3      因此flag传递为0   使用应用程序专用的段
    task_init(&task_manager.first_task, "first task",0, first_start, first_start+alloc_size);

    // 写TR寄存器，指示当前运行的第一个任务
    write_tr(task_manager.first_task.tss_sel);
    task_manager.curr_task = &task_manager.first_task;
    // 在这里对页表进行了切换  重新加载页表
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    // 在这里已经切换到第一个任务的页表了  我们可以为第一个任务来分配内存了
    // PTE_U 可以被用户模式访问
    
    memory_alloc_page_for(first_start, alloc_size,PTE_P|PTE_W| PTE_U);
    kernel_memcpy((void*)first_start,s_first_task,copy_size);
}

/**
 * @brief 返回初始任务
 */
task_t * task_first_task (void) {
    return &task_manager.first_task;
}

/**
 * @brief 空闲任务
 */
static void idle_task_entry (void) {
    for (;;) {
        hlt();
    }
}

/** 
 * @brief 任务分配
*/
static task_t * alloc_task(void){
    task_t *task = (task_t*)0;
    mutex_lock(&task_table_mutex);
    for(int i = 0;i<TASK_NR;i++){
        task_t *curr =  task_table+i;
        if(curr->name[0] == '\0'){
            task = curr;
            break;
        }
    }
    mutex_unlock(&task_table_mutex);
    return task;
}
/** 
 * @brief 任务释放
*/
static void free_task(task_t *task){
    mutex_lock(&task_table_mutex);
    task->name[0] = '\0';   
    mutex_unlock(&task_table_mutex);
}

/**
 * @brief 任务管理器初始化
 */
void task_manager_init (void) {


    kernel_memset(task_table,0,sizeof(task_table));
    mutex_init(&task_table_mutex);

    int sel = gdt_alloc_desc();
    // 配置应用程序的代码段与数据段

    segment_desc_set(sel,0,0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_DATA | SEG_TYPE_RW | SEG_D);
    task_manager.app_date_sel = sel;

    sel = gdt_alloc_desc();
        // 配置应用程序的代码段与数据段


    segment_desc_set(sel,0,0xFFFFFFFF,
        SEG_P_PRESENT | SEG_DPL3 | SEG_S_NORMAL | SEG_TYPE_CODE | SEG_TYPE_RW | SEG_D);
    task_manager.app_code_sel = sel;

    
    // 各队列初始化
    list_init(&task_manager.ready_list);
    list_init(&task_manager.task_list);
    list_init(&task_manager.sleep_list);

    // 空闲任务初始化
    // 空闲任务属于操作系统 我们将他的特权级设置为0  flag传递TASK_FLAGS_SYSTEM
    task_init(&task_manager.idle_task,
                "idle task", 
                TASK_FLAGS_SYSTEM,
                (uint32_t)idle_task_entry, 
                (uint32_t)(idle_task_stack + IDLE_STACK_SIZE));     // 里面的值不必要写

    task_manager.curr_task = (task_t *)0;
}

/**
 * @brief 将任务插入就绪队列
 */
void task_set_ready(task_t *task) {
    if (task != &task_manager.idle_task) {
        list_insert_last(&task_manager.ready_list, &task->run_node);
        task->state = TASK_READY;
    }
}

/**
 * @brief 将任务从就绪队列移除
 */
void task_set_block (task_t *task) {
    if (task != &task_manager.idle_task) {
        list_remove(&task_manager.ready_list, &task->run_node);
    }
}
/**
 * @brief 获取下一将要运行的任务
 */
static task_t * task_next_run (void) {
    // 如果没有任务，则运行空闲任务
    if (list_count(&task_manager.ready_list) == 0) {
        return &task_manager.idle_task;
    }
    
    // 普通任务
    list_node_t * task_node = list_first(&task_manager.ready_list);
    return list_node_parent(task_node, task_t, run_node);
}

/**
 * @brief 将任务加入睡眠状态
 */
void task_set_sleep(task_t *task, uint32_t ticks) {
    if (ticks <= 0) {
        return;
    }

    task->sleep_ticks = ticks;
    task->state = TASK_SLEEP;
    list_insert_last(&task_manager.sleep_list, &task->run_node);
}

/**
 * @brief 将任务从延时队列移除
 * 
 * @param task 
 */
void task_set_wakeup (task_t *task) {
    list_remove(&task_manager.sleep_list, &task->run_node);
}

/**
 * @brief 获取当前正在运行的任务
 */
task_t * task_current (void) {
    return task_manager.curr_task;
}

/**
 * @brief 当前任务主动放弃CPU
 */
int sys_yield (void) {
    irq_state_t state = irq_enter_protection();

    if (list_count(&task_manager.ready_list) > 1) {
        task_t * curr_task = task_current();

        // 如果队列中还有其它任务，则将当前任务移入到队列尾部
        task_set_block(curr_task);
        task_set_ready(curr_task);

        // 切换至下一个任务，在切换完成前要保护，不然可能下一任务
        // 由于某些原因运行后阻塞或删除，再回到这里切换将发生问题
        task_dispatch();
    }
    irq_leave_protection(state);

    return 0;
}

/**
 * @brief 进行一次任务调度
 */
void task_dispatch (void) {
    task_t * to = task_next_run();
    if (to != task_manager.curr_task) {
        task_t * from = task_manager.curr_task;
        task_manager.curr_task = to;

        to->state = TASK_RUNNING;
        task_switch_from_to(from, to);
    }
}

/**
 * @brief 时间处理
 * 该函数在中断处理函数中调用
 */
void task_time_tick (void) {
    task_t * curr_task = task_current();

    // 时间片的处理
    irq_state_t state = irq_enter_protection();
    if (--curr_task->slice_ticks == 0) {
        // 时间片用完，重新加载时间片
        // 对于空闲任务，此处减未用
        curr_task->slice_ticks = curr_task->time_slice;

        // 调整队列的位置到尾部，不用直接操作队列
        task_set_block(curr_task);
        task_set_ready(curr_task);
    }
    
    // 睡眠处理
    list_node_t * curr = list_first(&task_manager.sleep_list);
    while (curr) {
        list_node_t * next = list_node_next(curr);

        task_t * task = list_node_parent(curr, task_t, run_node);
        if (--task->sleep_ticks == 0) {
            // 延时时间到达，从睡眠队列中移除，送至就绪队列
            task_set_wakeup(task);
            task_set_ready(task);
        }
        curr = next;
    }

    task_dispatch();
    irq_leave_protection(state);
}

/**
 * @brief 任务进入睡眠状态
 * 
 * @param ms 
 */
void sys_msleep (uint32_t ms) {
    // 至少延时1个tick
    if (ms < OS_TICK_MS) {
        ms = OS_TICK_MS;
    }

    irq_state_t state = irq_enter_protection();

    // 从就绪队列移除，加入睡眠队列
    task_set_block(task_manager.curr_task);
    task_set_sleep(task_manager.curr_task, (ms + (OS_TICK_MS - 1))/ OS_TICK_MS);
    
    // 进行一次调度
    task_dispatch();

    irq_leave_protection(state);
}

int sys_getpid(void){
    task_t *task = task_current();
    return task->pid;
}

int sys_fork(void){

    task_t *parent_task = task_current();
    task_t *child_task = alloc_task();
    if(child_task ==(task_t*)0 ){
        goto fork_failed;
    }

    // 我们要获取父进程执行系统调用时的信息   然后并且复制到子进程中  那么子进程就可以和父进程一样了
    // 获取父进程进行系统调用时往栈中压入的信息
    // 当前我们已经处在系统调用当中了  也就说父进程的信息现在已经在栈当中了 我们要将信息读出来

    // esp0就是指向的特权级0的栈顶  但是我们想要得到的是结构体的起始位置  因此要使用栈顶指针减去结构体的大小
    // 得到了结构体之后 就可以通过eip得到父进程当时运行到的位置
    syscall_frame_t *frame =(syscall_frame_t *)( parent_task->tss.esp0 - sizeof(syscall_frame_t));
    // 给子进程进行初始化
    // 这个入口地址是一个难点
    // 当前父进程的栈顶指针指向了特权级为0的栈   但是我们的子进程只需要从特权级为3的任务开始就行了
    // frame当中保存的都是父进程在系统调用之前的信息  所以这个里面保存的esp也是指向特权级3 的  
    // 但是父进程在系统调用之前进行了很多压栈  因此如果子进程如果要使用父进程的esp指针，要将esp恢复到压栈之前的位置
    int err = task_init(child_task,parent_task->name,0,frame->eip,frame->esp+sizeof(uint32_t)*SYSCALL_PARAM_COUNT);
    if(err<0){
        goto fork_failed;
    }
    // 我们这里设置的是子进程的tss     eax存储的是函数的返回值
    tss_t *tss = &child_task->tss;
    tss->eax = 0;
    tss->ebx = frame->ebx;
    tss->ecx = frame->ecx;
    tss->edx = frame->edx;
    tss->esi = frame->esi;
    tss->edi = frame->edi;
    tss->ebp = frame->ebp;

    tss->cs = frame->cs;
    tss->ds = frame->ds;
    tss->es = frame->es;
    tss->fs = frame->fs;
    tss->gs = frame->gs;
    tss->eflags = frame->eflags;
    // 指定父进程pid
    child_task->parent = parent_task;


    // 为进程新建一个页表
    if((tss->cr3 = memory_copy_uvm(parent_task->tss.cr3))<0){
        goto fork_failed;
    }




    // 我们这里面运行的依旧是父进程   我们创建的子进程会被放到等待队列当中
    return child_task->pid;
fork_failed:
    if(child_task){
        task_uninit(child_task);
        free_task(child_task);
    }
    return -1;

}