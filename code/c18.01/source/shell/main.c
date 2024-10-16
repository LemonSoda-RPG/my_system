/**
 * 简单的命令行解释器
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include <stdio.h>
#include <string.h>
#include "lib_syscall.h"
#include "main.h"
#include <getopt.h>
#include <stdlib.h>
#include <sys/file.h>
#include "fs/file.h"


static cli_t cli;    // 定义一个shell解释器
static const char * promot = "sh >>";       // 命令行提示符

/**
 * 显示命令行提示符
 */
static void show_promot(void) {
    printf("%s", cli.promot);    
    fflush(stdout);  //刷新缓存   // printf的缓存是通过c库实现的  不是系统调用
}
/**
 * 清屏命令
 */
static int do_clear (int argc, char ** argv) {
    printf("%s", ESC_CLEAR_SCREEN);
    printf("%s", ESC_MOVE_CURSOR(0, 0));
    return 0;
}
/**
 * help命令
 */
static int do_help(int argc, char **argv) {
    const cli_cmd_t * start = cli.cmd_start;
    // 这个函数的作用就是输出所有的内置命令 并打印用法
    // 循环打印名称及用法
    while (start < cli.cmd_end) {
        printf("%s %s\n",  start->name, start->useage);
        start++;
    }
    return 0;
}
/**
 * 程序退出命令
 */
static int do_exit (int argc, char ** argv) {
    exit(0);
    return 0;
}

/**
 * 回显命令
 *   getopt需要多次调用，需要重置
 *   哪怕外部多次调用echo 她也不会自动重值
 */
static int do_echo (int argc, char ** argv) {
    // 只有一个参数，需要先手动输入，再输出
    if (argc == 1) {
        char msg_buf[128];
        // 假如只有一个参数  则继续读取后续的信息
        fgets(msg_buf, sizeof(msg_buf), stdin);
        msg_buf[sizeof(msg_buf) - 1] = '\0';
        puts(msg_buf);
        return 0;
    }
    // https://www.cnblogs.com/yinghao-liu/p/7123622.html
    // optind是下一个要处理的元素在argv中的索引
    // 当没有选项时，变为argv第一个不是选项元素的索引。
    int count = 1;    // 缺省只打印一次
    int ch;
    // getopt对参数进行解析
    // 通过while循环进行解析
    while ((ch = getopt(argc, argv, "n:h")) != -1) {
        switch (ch) {
            case 'h':
                puts("echo echo any message");
                puts("Usage: echo [-n count] msg");
                optind = 1;        // getopt需要多次调用，需要重置
                return 0;
            case 'n':
                count = atoi(optarg);
                break;
            case '?':
                if (optarg) {
                    fprintf(stderr, "Unknown option: -%s\n", optarg);
                }
                optind = 1;        // getopt需要多次调用，需要重置
                return -1;
        }
    }

    // 索引已经超过了最后一个参数的位置，意味着没有传入要发送的信息
    if (optind > argc - 1) {
        fprintf(stderr, "Message is empty \n");
        optind = 1;        // getopt需要多次调用，需要重置
        return -1;
    }

    // 循环打印消息
    // optind是下一个要处理的元素在argv中的索引
    char * msg = argv[optind];
    for (int i = 0; i < count; i++) {
        puts(msg);
    }
    optind = 1;        // getopt需要多次调用，需要重置
    return 0;
}












/**
 * @brief 列出目录内容   对目录内容进行遍历
 */
static int do_ls (int argc, char ** argv) {
    // 打开目录
	DIR * p_dir = opendir("temp");
	if (p_dir == NULL) {
		printf("open dir failed\n");
		return -1;
	}

    // 然后进行遍历
	struct dirent * entry;
	while((entry = readdir(p_dir)) != NULL) {
        strlwr(entry->name);
		printf("%c %s %d\n",
                entry->type == FILE_DIR ? 'd' : 'f',
                entry->name,
                entry->size);
	}
	closedir(p_dir);

    return 0;
}


// 命令列表
static const cli_cmd_t cmd_list[] = {
    {
        .name = "help",
		.useage = "help -- list support command",
		.do_func = do_help,
    },
    {
        .name = "clear",
		.useage = "clear -- clear the screen",
		.do_func = do_clear,
    },
	{
		.name = "echo",
		.useage = "echo [-n count] msg  -- echo something",
		.do_func = do_echo,
	},
    {
        .name = "ls",
        .useage = "ls [dir] -- list director",
        .do_func = do_ls,
    },
    // {
    //     .name = "less",
    //     .useage = "less [file] -- open file",
    //     .do_func = do_less,
    // },
    {
        .name = "quit",
        .useage = "quit from shell",
        .do_func = do_exit,
    }
};

/**
 * 命令行初始化
 */
static void cli_init(const char * promot, const cli_cmd_t * cmd_list, int cnt) {
    cli.promot = promot;
    
    // 输入缓存清空
    memset(cli.curr_input, 0, CLI_INPUT_SIZE);
    // 内置命令起始地址
    cli.cmd_start = cmd_list;
    // 结束地址
    cli.cmd_end = cmd_list + cnt;
}
/**
 * 运行内部命令
 */
static void run_builtin (const cli_cmd_t * cmd, int argc, char ** argv) {
    int ret = cmd->do_func(argc, argv);
    if (ret < 0) {
        fprintf(stderr,"error: %d\n", ret);
    }
}

/**
 * 在内部命令中搜索
 */
static const cli_cmd_t * find_builtin (const char * name) {
    for (const cli_cmd_t * cmd = cli.cmd_start; cmd < cli.cmd_end; cmd++) {
        if (strcmp(cmd->name, name) != 0) {
            continue;
        }

        return cmd;
    }

    return (const cli_cmd_t *)0;
}




static void run_exec_file(const char *path,int argc,char**argv){
    int pid = fork();
    if(pid<0){
        fprintf(stderr,"fork failed %s",path);
    }else if(pid==0){
        // 子进程
        for(int i = 0;i<argc;i++){
            printf("arg %d = %s\n",i ,argv[i]);

        }
        exit(-1);
    }
    else{
        int status;
        // 父进程等待收尸
        int pid = wait(&status);
        fprintf(stderr, "cmd %s result: %d, pid = %d\n", path, status, pid);

    }
}
char temp[256];
int main (int argc, char **argv) {

	open(argv[0], O_RDWR);      // 不会创建额外的文件描述符     所以读写不会发生冲突
    dup(0);     // 标准输出
    dup(0);     // 标准错误输出

    // int pid = fork();
    // if(fork>0){
    //     wait(NULL);
    //     exit(0);
    // }
    // printf("ssssss\n");
    // int a = 10;
    // int b = a/0;
    void *addr = sbrk(10);
    // 对shell解释器进行初始化
   	cli_init(promot, cmd_list, sizeof(cmd_list) / sizeof(cli_cmd_t));
    for (;;) {
        show_promot();  //打印提示符
        // 在这里调用了  read  假如没有数据  就会在这里陷入阻塞
        // 我们这里遇到的问题时 fgets把换行符也读进来了
        char *str = fgets(cli.curr_input,CLI_INPUT_SIZE,stdin);

        // 这里的str和cli.curr_input起始是一样的
        if(!str){
            continue;
        }
        // 读取的字符串中结尾可能有换行符，去掉之
        char * cr = strchr(cli.curr_input, '\n');
        if (cr) {
            *cr = '\0';
        }
        cr = strchr(cli.curr_input, '\r');
        if (cr) {
            *cr = '\0';
        }
        // 提取出命令，找命令表
        // 通过空格  对字符串进行切片
        // 进而得到多个命令
        const char * space = " ";  // 字符分割器
        char *token = strtok(cli.curr_input, space);
        int argc = 0;
        char * argv[CLI_MAX_ARG_COUNT];
        memset(argv, 0, sizeof(argv));

        // 对得到的字符串数组进行遍历
        while (token) {
            // 记录参数
            argv[argc++] = token;
            // 不要问为什么这里用null   strtok就是这么用的

            // 先获取下一位置
            token = strtok(NULL, space);
        }
        if(argc==0){
            continue;
        }
        // 接下来对任务进行解析
        // 这里返回任务列表当中的某个任务的指针
        const cli_cmd_t *cmd = find_builtin(argv[0]);
        if(cmd){
            run_builtin(cmd,argc,argv);
            continue;
        }
        // exec  磁盘加载

        run_exec_file("",argc,argv);

        fprintf(stderr, ESC_COLOR_ERROR"Unknown command: %s\n"ESC_COLOR_DEFAULT, cli.curr_input);

   
    }

    return 0;
}