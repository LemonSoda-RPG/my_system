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
#include "comm/elf.h"
#include "core/memory.h"
#include "fs/fs.h"
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
    task->heap_end = 0;
    task->heap_start = 0;

    task->time_slice = TASK_TIME_SLICE_DEFAULT;
    task->slice_ticks = task->time_slice;
    task->pid = (uint32_t)task;
    list_node_init(&task->all_node);
    list_node_init(&task->run_node);
    list_node_init(&task->wait_node);

    // 插入就绪队列中和所有的任务队列中
    irq_state_t state = irq_enter_protection();
    list_insert_last(&task_manager.task_list, &task->all_node);
    irq_leave_protection(state);
    return 0;
}


void task_start(task_t *task){
    irq_state_t state = irq_enter_protection();
    task_set_ready(task);
    irq_leave_protection(state);
}
void task_uninit(task_t* task){
    if(task->tss_sel){
        gdt_free_sel(task->tss_sel);
    }
    if(task->tss.esp0){
        memory_free_page(task->tss.esp-MEM_PAGE_SIZE);
    }
    if(task->tss.cr3){
        memory_destroy_uvm(task->tss.cr3);
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
    // 初始化堆的起始和结束地址
    task_manager.first_task.heap_start = (uint32_t) e_first_task;
    task_manager.first_task.heap_end = (uint32_t) e_first_task;





    // 写TR寄存器，指示当前运行的第一个任务
    write_tr(task_manager.first_task.tss_sel);
    task_manager.curr_task = &task_manager.first_task;
    // 在这里对页表进行了切换  重新加载页表
    mmu_set_page_dir(task_manager.first_task.tss.cr3);

    // 在这里已经切换到第一个任务的页表了  我们可以为第一个任务来分配内存了
    // PTE_U 可以被用户模式访问
    
    memory_alloc_page_for(first_start, alloc_size,PTE_P|PTE_W| PTE_U);
    kernel_memcpy((void*)first_start,s_first_task,copy_size);
    task_start(&task_manager.first_task);
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
    task_manager.curr_task = (task_t *)0;

    // 空闲任务初始化
    // 空闲任务属于操作系统 我们将他的特权级设置为0  flag传递TASK_FLAGS_SYSTEM
    task_init(&task_manager.idle_task,
                "idle task", 
                TASK_FLAGS_SYSTEM,
                (uint32_t)idle_task_entry, 
                (uint32_t)(idle_task_stack + IDLE_STACK_SIZE));     // 里面的值不必要写
    task_start(&task_manager.idle_task);
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
    
    // 我们这里没有运行   mmu_set_page_dir  是因为运行这句函数  代表着我需要立即对页表进行切换 
    // 但是我其实并不需要，创建完子进程之后，它会被放入到就绪队列当中   当任务切换到他的时候 自然会对页表进行加载
    if((tss->cr3 = memory_copy_uvm(parent_task->tss.cr3))<0){
        goto fork_failed;
    }
    task_start(child_task);
    // 我们这里面运行的依旧是父进程   我们创建的子进程会被放到等待队列当中
    return child_task->pid;
fork_failed:
    if(child_task){
        task_uninit(child_task);
        free_task(child_task);
    }
    return -1;

}
/**
 * @brief 加载一个程序表头的数据到内存中
 */
static int load_phdr(int file, Elf32_Phdr * phdr, uint32_t page_dir) {
    // 生成的ELF文件要求是页边界对齐的
    ASSERT((phdr->p_vaddr & (MEM_PAGE_SIZE - 1)) == 0);

    // 分配空间
    int err = memory_alloc_for_page_dir(page_dir, phdr->p_vaddr, phdr->p_memsz, PTE_P | PTE_U | PTE_W);
    if (err < 0) {
        log_printf("no memory");
        return -1;
    }

    // 调整当前的读写位置
    if (sys_lseek(file, phdr->p_offset, 0) < 0) {
        log_printf("read file failed");
        return -1;
    }
    // 当前我们使用的  page_dir  还没有被加载  所以当前如果我们进行内存拷贝的话 依旧是按照之前的页表进行拷贝的
    // 为段分配所有的内存空间.后续操作如果失败，将在上层释放
    // 简单起见，设置成可写模式，也许可考虑根据phdr->flags设置成只读
    // 因为没有找到该值的详细定义，所以没有加上

    // 因此如果我们要拷贝 我们要先给我们当前这个新的页表分配内存 并返回对应的物理地址  
    // 然后就拷贝到返回的物理地址这个地方
    uint32_t vaddr = phdr->p_vaddr;
    uint32_t size = phdr->p_filesz;
    while (size > 0) {
        int curr_size = (size > MEM_PAGE_SIZE) ? MEM_PAGE_SIZE : size;
        // 在这里面分配并获得物理地址 
        uint32_t paddr = memory_get_paddr(page_dir, vaddr);

        // 注意，这里用的页表仍然是当前的
        // 读取并写入到paddr中
        // 我们这里是一页一页进行拷贝的  因为物理内存可能不是连续的   虽然虚拟内存肯定是连续的
        // size是剩余的大小  每次取出mem_page_size大小 当size小于mem_page_size大小时  就取最后的size的大小
        if (sys_read(file, (char *)paddr, curr_size) <  curr_size) {
            log_printf("read file failed");
            return -1;
        }

        size -= curr_size;
        vaddr += curr_size;
    }

    // bss区考虑由crt0和cstart自行清0，这样更简单一些
    // 如果在上边进行处理，需要考虑到有可能的跨页表填充数据，懒得写代码
    // 或者也可修改memory_alloc_for_page_dir，增加分配时清0页表，但这样开销较大
    // 所以，直接放在cstart哐crt0中直接内存填0，比较简单
    return 0;
}


extern uint8_t __bss_start__[],__bss_end__[];   
// 这里的结束地址确实和我们的elf文件的结束地址是一样的



static uint32_t load_elf_file(task_t *task,const char *name,uint32_t page_dir){

    Elf32_Ehdr elf_hdr;
    Elf32_Phdr elf_phdr;

    int file = sys_open(name,0);
    if(file<0){
        log_printf("open failed %s",name);
        goto load_failed; 
    }
    // 这里先将头文件的结构体读取出来  头文件不含真正的数据  真正的数据在头文件的后面
    int cnt = sys_read(file,(char*)&elf_hdr,sizeof(elf_hdr));
    if(cnt<sizeof(Elf32_Ehdr)){
        log_printf("elf  hdr too small ,size : %d",cnt);
        goto load_failed;
    }

    // 对elf文件进行检查
    // 做点必要性的检查。当然可以再做其它检查
    if ((elf_hdr.e_ident[0] != ELF_MAGIC) || (elf_hdr.e_ident[1] != 'E')
        || (elf_hdr.e_ident[2] != 'L') || (elf_hdr.e_ident[3] != 'F')) {
        log_printf("check elf indent failed.");
        goto load_failed;
    }

    // // 必须是可执行文件和针对386处理器的类型，且有入口
    // if ((elf_hdr.e_type != ET_EXEC) || (elf_hdr.e_machine != ET_386) || (elf_hdr.e_entry == 0)) {
    //     log_printf("check elf type or entry failed.");
    //     goto load_failed;
    // }

    // // 必须有程序头部
    // if ((elf_hdr.e_phentsize == 0) || (elf_hdr.e_phoff == 0)) {
    //     log_printf("none programe header");
    //     goto load_failed;
    // }


    // 接下来开始读取数据
    // 然后从中加载程序头，将内容拷贝到相应的位置
    // 一个elf文件中可能有多个程序头

    uint32_t e_phoff = elf_hdr.e_phoff;
    // 每个程序头的大小都是一样的 所以e_phoff += elf_hdr.e_phentsize 进行遍历
    for (int i = 0; i < elf_hdr.e_phnum; i++, e_phoff += elf_hdr.e_phentsize) {
        if (sys_lseek(file, e_phoff, 0) < 0) {
            log_printf("read file failed");
            goto load_failed;
        }

        // 读取程序头后解析，这里不用读取到新进程的页表中，因为只是临时使用下
        //
        cnt = sys_read(file, (char *)&elf_phdr, sizeof(Elf32_Phdr));
        if (cnt < sizeof(Elf32_Phdr)) {
            log_printf("read file failed");
            goto load_failed;
        }

        // 简单做一些检查，如有必要，可自行加更多
        // 主要判断是否是可加载的类型，并且要求加载的地址必须是用户空间
        if ((elf_phdr.p_type != PT_LOAD) || (elf_phdr.p_vaddr < MEMORY_TASK_BASE)) {
           continue;
        }

        // 加载当前程序头  从程序头中读取数据的位置 并进行拷贝
        int err = load_phdr(file, &elf_phdr, page_dir);
        if (err < 0) {
            log_printf("load program hdr failed");
            goto load_failed;
        }

        // 简单起见，不检查了，以最后的地址为bss的地址
        // 这里其实也可以使用我们在链接脚本当中获取的bss结束的值
        task->heap_start = elf_phdr.p_vaddr + elf_phdr.p_memsz;
        task->heap_end = task->heap_start;
        
   }

    sys_close(file);
    return elf_hdr.e_entry;



load_failed:
    return 0;


}
/**
 * @brief 复制进程参数到栈中。注意argv和env指向的空间在另一个页表里
 * to是栈顶指针
 */
static int copy_args (char * to, uint32_t page_dir, int argc, char **argv) {
    // 在stack_top中依次写入argc, argv指针，参数字符串
    task_args_t task_args;
    task_args.argc = argc;
    // 指针指向的位置
    task_args.argv = (char **)(to + sizeof(task_args_t));

    // 复制各项参数, 跳过task_args和参数表
    // 各argv参数写入的内存空间
    // sizeof(char *) * (argc) 是表的大小
    // 这是我们的目标地址  当前还没有拷贝
    char * dest_arg = to + sizeof(task_args_t) + sizeof(char *) * (argc);   // 留出结束符
    
    // argv表
    // dest_argv_tb  这里面存的是指针  不是数据 
    char ** dest_argv_tb = (char **)memory_get_paddr(page_dir, (uint32_t)(to + sizeof(task_args_t)));
    ASSERT(dest_argv_tb != 0);

    for (int i = 0; i < argc; i++) {
        char * from = argv[i];

        // 不能用kernel_strcpy，因为to和argv不在一个页表里
        int len = kernel_strlen(from) + 1;   // 包含结束符
        // 接下来进行拷贝
        // dest_arg 是虚拟地址   在函数中我们会找到对应的物理地址 进行拷贝
        int err = memory_copy_uvm_data((uint32_t)dest_arg, page_dir, (uint32_t)from, len);
        ASSERT(err >= 0);

        // 关联ar  将指针指向数据
        dest_argv_tb[i] = dest_arg;

        // 记录下位置后，复制的位置前移
        dest_arg += len;
    }

     // 写入task_args
    return memory_copy_uvm_data((uint32_t)to, page_dir, (uint32_t)&task_args, sizeof(task_args_t));
}


int sys_execve(char*name,char * * argv,char **env){
    // 将当前进程切换为name进行
    


    task_t *task = task_current();

    
  // 后面会切换页表，所以先处理需要从进程空间取数据的情况
  // get_file_name 获取可执行文件名
    kernel_strncpy(task->name, get_file_name(name), TASK_NAME_SIZE);
    // 我们转换之前的程序与转换之后的程序的内存映射会不一样
    // 因此我们的方法是将原来的页表释放  重新创建一个新的页表给程序使用
    // 获取原来的页表
    uint32_t old_page_dir = task->tss.cr3;

    uint32_t new_page_dir = memory_create_uvm();
    if(!new_page_dir){
        goto exec_failed;
    }
    // 任务切换
    uint32_t entry = load_elf_file(task,name,new_page_dir);
    // entry函数的入口地址
    if(entry==0){
        goto exec_failed;
    }

 // 准备用户栈空间，预留环境环境及参数的空间
    uint32_t stack_top = MEM_TASK_STACK_TOP - MEM_TASK_ARG_SIZE;    // 预留一部分参数空间
   
    int err = memory_alloc_for_page_dir(new_page_dir,
        MEM_TASK_STACK_TOP - MEM_TASK_STACK_SIZE,
        MEM_TASK_STACK_SIZE,
        PTE_P|PTE_U|PTE_W);
    if(err<0){
        goto exec_failed;
    }

    // 复制参数，写入到栈顶的后边
    // 获取参数数量
    int argc = strings_count(argv);
    err = copy_args((char *)stack_top, new_page_dir, argc, argv);
    if (err < 0) {
        goto exec_failed;
    }


    // 我们这里是将特权级为0  的栈进行修改   确保我们能够返回到正确的位置
    syscall_frame_t *frame = (syscall_frame_t*)(task->tss.esp0 - sizeof(syscall_frame_t));
    frame->eip = entry;
    frame->eax = frame->ebx = frame->ecx = frame->edx = 0;
    frame->esi = frame->edi = frame->ebp = 0;
    frame->eflags = EFLAGS_DEFAULT| EFLAGS_IF;  // 段寄存器无需修改

    // 修改用户栈的位置
    frame->esp= stack_top - sizeof(uint32_t)*SYSCALL_PARAM_COUNT;

    task->tss.cr3 = new_page_dir;  // 这并没有使页表切换生效
    mmu_set_page_dir(new_page_dir);  //因为我需要页表立即生效  因此这里直接加载新的页表

    //销毁原来的页表
    memory_destroy_uvm(old_page_dir);
    return 0;

exec_failed:
// 如果创建失败 那就还原吧
    if(new_page_dir){
        task->tss.cr3 = old_page_dir;
        mmu_set_page_dir(old_page_dir);
        memory_destroy_uvm(new_page_dir);
    }
    return -1;

}



