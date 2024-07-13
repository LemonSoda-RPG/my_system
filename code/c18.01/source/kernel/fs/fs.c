/**
 * 文件系统相关接口的实现
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "core/task.h"
#include "comm/cpu_instr.h"
#include "tools/klib.h"
#include "fs/fs.h"
#include "comm/boot_info.h"
#include <sys/stat.h>
#include "dev/console.h"
#include "fs/file.h"
#include "tools/log.h"
#include "dev/dev.h"
#define FS_TABLE_SIZE		10   // 最多支持10种文件类型
#define TEMP_FILE_ID		100
#define TEMP_ADDR        	(8*1024*1024)      // 在0x800000处缓存原始

static uint8_t * temp_pos;       // 当前位置
static list_t mounted_list;		// 文件类型链表  只有指针
static list_t free_list;		// 初始化的时候  先将文件类型存储到空闲列表
// 用于存放我们文件类型的结构的数组
static fs_t fs_table[FS_TABLE_SIZE];
extern fs_op_t devfs_op;



/**
* 使用LBA48位模式读取磁盘
*/
static void read_disk(int sector, int sector_count, uint8_t * buf) {
    outb(0x1F6, (uint8_t) (0xE0));

	outb(0x1F2, (uint8_t) (sector_count >> 8));
    outb(0x1F3, (uint8_t) (sector >> 24));		// LBA参数的24~31位
    outb(0x1F4, (uint8_t) (0));					// LBA参数的32~39位
    outb(0x1F5, (uint8_t) (0));					// LBA参数的40~47位

    outb(0x1F2, (uint8_t) (sector_count));
	outb(0x1F3, (uint8_t) (sector));			// LBA参数的0~7位
	outb(0x1F4, (uint8_t) (sector >> 8));		// LBA参数的8~15位
	outb(0x1F5, (uint8_t) (sector >> 16));		// LBA参数的16~23位

	outb(0x1F7, (uint8_t) 0x24);

	// 读取数据
	uint16_t *data_buf = (uint16_t*) buf;
	while (sector_count-- > 0) {
		// 每次扇区读之前都要检查，等待数据就绪
		while ((inb(0x1F7) & 0x88) != 0x8) {}

		// 读取并将数据写入到缓存中
		for (int i = 0; i < SECTOR_SIZE / 2; i++) {
			*data_buf++ = inw(0x1F0);
		}
	}
}

/**
 * @brief 对文件类型链表进行初始化
 */
static void mount_list_init(void){
	list_init(&free_list);
	for(int i = 0;i<FS_TABLE_SIZE;i++){
		list_insert_first(&free_list,&fs_table[i].node);
	}
	list_init(&mounted_list);

}

static fs_op_t *get_fs_op(fs_type_t type,int major){
	switch (type)
	{
	case FS_DEVFS:
		return &(devfs_op);
		break;
	
	default:
		return (fs_op_t*)0;
		break;
	}
}
static fs_t *mount(fs_type_t type,char *mount_point,int dev_major,int dev_minor){
	fs_t* fs = (fs_t*)0;
	log_printf("mount file system,name :%s, dev:%x",mount_point,dev_major);
	// 判断当前文件类型是否已经挂载了
	
	list_node_t *curr = list_first(&mounted_list);
	while(curr){
		// 查找当前节点的文件类型
		fs_t* fs= list_node_parent(curr,fs_t,node);
		// 进行对比判断
		if(kernel_strncmp(mount_point,fs->mount_point,FS_MOUNTP_SIZE)==0){
			log_printf("fs already mounted");
			goto mount_failed;
		}
		// 获取下一个节点
		curr = list_node_next(curr);
	}


	//  free_node 就是将要保存我们类型信息的节点  
	// 我们将必要的信息填入之后  就要将他放到mounted的链表当中
	list_node_t *free_node =list_remove_first(&free_list);
	if(!free_node){
		log_printf("no free fs,mount failed");
		goto mount_failed;
	}

	fs = list_node_parent(free_node,fs_t,node);

	// 我们要在这里指定回调函数
	// 根据传入的type字段  选定对应的回调函数
	fs_op_t *op = get_fs_op(type,dev_major);
	if(!op){
		log_printf("unsupport fs type:%d",type);
		goto mount_failed;
	}
	kernel_memset(fs,0,sizeof(fs_t));
	kernel_memcpy(fs->mount_point,mount_point,FS_MOUNTP_SIZE);
	fs->op = op;
	// 进行特定的挂载操作
	if(fs->op->mount(fs,dev_major,dev_minor)<0){
		log_printf("mount fs %s failed",mount_point);
	}
	list_insert_last(&mounted_list,&fs->node);
	return fs;
mount_failed:
	if(fs){
		list_insert_last(&free_list,&fs->node);
	}
	return (fs_t*)0;
}
/**
 * @brief 文件系统初始化
 */
void fs_init (void) {
	// 初始化文件描述符表与中断
    file_table_init();
	mount_list_init();
	
	// 挂载文件类型
	fs_t *fs = mount(FS_DEVFS,"/dev",0,0);
	ASSERT(fs!=(fs_t*)0);
}

/**
 * @brief 检查路径是否正常
 */
static int is_path_valid (const char * path) {
	if ((path == (const char *)0) || (path[0] == '\0')) {
		return 0;
	}

    return 1;
}

/**
 * 打开文件
 */
int sys_open(const char *name, int flags, ...) {
	if (kernel_strncmp(name, "tty", 3) == 0) {
        if (!is_path_valid(name)) {
            log_printf("path is not valid.");
            return -1;
        }

        // 分配文件描述符链接。这个过程中可能会被释放
        int fd = -1;
        file_t * file = file_alloc();
        if (file) {
            fd = task_alloc_fd(file);  // 分配文件描述符
            if (fd < 0) {
                goto sys_open_failed;
            }
        }

		if (kernel_strlen(name) < 5) {
			goto sys_open_failed;
		}

		int num = name[4] - '0';
		int dev_id = dev_open(DEV_TTY, num, 0);
		if (dev_id < 0) {
			goto sys_open_failed;
		}

		file->dev_id = dev_id;
		file->mode = 0;
		file->pos = 0;
		file->ref = 1;   // 有几个指针指向自己
		file->type = FILE_TTY;
		kernel_strncpy(file->file_name, name, FILE_NAME_SIZE);
		return fd;

sys_open_failed:
		if (file) {
			file_free(file);
		}

		if (fd >= 0) {
			task_remove_fd(fd);
		}
		return -1;
	} else {
		if (name[0] == '/') {
            // 暂时直接从扇区5000上读取, 读取大概40KB，足够了
            read_disk(5000, 80, (uint8_t *)TEMP_ADDR);
            temp_pos = (uint8_t *)TEMP_ADDR;
            return TEMP_FILE_ID;
        }
	}
}

/**
 * 复制一个文件描述符
 */
int sys_dup (int file) {
	// 超出进程所能打开的全部，退出
	if ((file < 0) && (file >= TASK_OFILE_NR)) {
        log_printf("file(%d) is not valid.", file);
		return -1;
	}

	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}

	int fd = task_alloc_fd(p_file);	// 新fd指向同一描述符
	if (fd >= 0) {
		p_file->ref++;		// 增加引用
		return fd;
	}

	log_printf("No task file avaliable");
    return -1;
}

/**
 * 读取文件api
 */
int sys_read(int file, char *ptr, int len) {
    if (file == TEMP_FILE_ID) {
        kernel_memcpy(ptr, temp_pos, len);
        temp_pos += len;
        return len;
    } else {
		file_t * p_file = task_file(file);
		if (!p_file) {
			log_printf("file not opened");
			return -1;
		}

		return dev_read(p_file->dev_id, 0, ptr, len);
	}
    return -1;
}

#include "tools/log.h"

/**
 * 写文件
 */
int sys_write(int file, char *ptr, int len) {
	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}

	return dev_write(p_file->dev_id, 0, ptr, len);
}

/**
 * 文件访问位置定位
 */
int sys_lseek(int file, int ptr, int dir) {
    if (file == TEMP_FILE_ID) {
        temp_pos = (uint8_t *)(ptr + TEMP_ADDR);
        return 0;
    }
    return -1;
}

/**
 * 关闭文件
 */
int sys_close(int file) {
}


/**
 * 判断文件描述符与tty关联
 */
int sys_isatty(int file) {
	return -1;
}

/**
 * @brief 获取文件状态
 */
int sys_fstat(int file, struct stat *st) {
    kernel_memset(st, 0, sizeof(struct stat));
    st->st_size = 0;
    return 0;
}

