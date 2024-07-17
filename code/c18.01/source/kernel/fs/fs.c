/**
 * 文件系统相关接口的实现
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "core/task.h"
#include "comm/cpu_instr.h"
#include "tools/klib.h"
#include "dev/disk.h"
#include "fs/fs.h"
#include "comm/boot_info.h"
#include <sys/stat.h>
#include "dev/console.h"
#include "fs/file.h"
#include "tools/log.h"
#include "os_cfg.h"
#include <sys/file.h>
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
extern fs_op_t fatfs_op;
static fs_t * root_fs;				// 根文件系统



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
	case FS_FAT16:
		return &(fatfs_op);
		break;
	default:
		return (fs_op_t*)0;
		break;
	}
}
// 这里是总的文件系统的mount   需要在其中指定回调函数组  然后再调用特定的mount进行挂载
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
	disk_init();
	// 挂载文件类型
	fs_t *fs = mount(FS_DEVFS,"/dev",0,0);

	ASSERT(fs!=(fs_t*)0);
	root_fs = mount(FS_FAT16,"/home",ROOT_DEV);
	ASSERT(root_fs!= (fs_t *)0);
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
 * @brief 转换目录为数字
 */
int path_to_num (const char * path, int * num) {
	int n = 0;

	const char * c = path;
	while (*c && *c != '/') {
		n = n * 10 + *c - '0';
		c++;
	}
	*num = n;
	return 0;
}

void fs_protect(fs_t *fs){
	if(fs->mutex){
		mutex_lock(fs->mutex);
	}
}
void fs_unprotect(fs_t *fs){
	if(fs->mutex){
		mutex_unlock(fs->mutex);
	}
}
const char * path_next_child(const char*path){
	const char *c = path;
	// 假如传入的是 /dev/tty0  那么最后返回的就是tty0   假如传入的是 /dev/123/tty0  返回的就是 123/tty0
	while(*c && (*c++)=='/'){}
	while(*c && (*c++)!='/'){}
	return *c ? c:(const char*)0;
}
// str 比path要短
int path_begin_with(const char*path,const char*str){
	const char*s1 = path;
	const char*s2 = str;
	while(*s1&&*s2){
		if(*s1==*s2){
			s1++;
			s2++;
		}else
		{
			break;
		}
	}
	return *s2=='\0';

}
/**
 * 打开文件  多次调用open会创建新的函数表 所以会造成冲突
 * 
 * 打开文件  我们会传入文件的名字   有了名字 我们会对名字进行切片 获取一级路径  通过一级路径判断文件的类型
 * 然后我们会对已经挂载的文件设备类型进行遍历，看看有没有符合这个文件类型的文件管理系统，假如有 我们就能指定文件管理类型
 * 比如说我们在这里判断他为dev文件  之后我们会使用devopen 接口函数将他打开  在devFS_open函数中 我们会进行进一步的类型判断
 * 当判断是tty类型之后  将tty类型以及相关参数传递给dev_open函数   最终在dev_open 中调用相关回调函数 
 */
int sys_open(const char *name, int flags, ...) {
	if(kernel_strncmp(name,"/shell.elf",3)==0){
		// 暂时直接从扇区5000上读取, 读取大概40KB，足够了
		// read_disk(5000, 80, (uint8_t *)TEMP_ADDR);
		int dev_id = dev_open(DEV_DISK,0xa0,(void*)0);
		dev_read(dev_id,5000,(uint8_t *)TEMP_ADDR,80);
		temp_pos = (uint8_t *)TEMP_ADDR;

		return TEMP_FILE_ID;
	}


    int fd = -1;
	file_t * file = file_alloc();  // 分配文件表
	if(!file){
		return -1;
	}
	fd = task_alloc_fd(file);  // 分配文件描述符  这个表里面存的是指针
	if (fd < 0) {
		goto sys_open_failed;
	}
	
	// 对字符串进行切片
	fs_t *fs = (fs_t*)0;
	list_node_t *node = list_first(&mounted_list);
	while(node){
		fs_t *curr = list_node_parent(node,fs_t,node);
		// 判断name是否以mount_point开头
		if(path_begin_with(name,curr->mount_point)){
			fs=curr;
			break;
		}
		node = list_node_next(node);
	}

	if(fs){
		// 通过函数判断tty的序号  这里原本是使用的dev_open  我们现在使用devfs_open
		name = path_next_child(name);
	}else{
		// 假如没有找到我们要的文件结构  可能没有挂载？？
		fs = root_fs;
	}
	
	file->fs = fs;  // 文件系统类型
	file->mode = flags;   // 操作模式   只读？ 只写？
	// file->ref = 1;   // 有几个指针指向自己   这里不进行操作了  在别的地方更改
	// file->type = FILE_TTY;
	kernel_strncpy(file->file_name, name, FILE_NAME_SIZE);
	// 我们目前依旧是直接使用了dev的类型 没有判断
	fs_protect(fs);
	int err=  fs->op->open(fs,name,file);
	if(err<0){
		fs_unprotect(fs);
		log_printf("open %s failed",name);
		goto sys_open_failed;
	}
	fs_unprotect(fs);
	
	return fd;

sys_open_failed:
	file_free(file);
	if (fd >= 0) {
		task_remove_fd(fd);
	}
	return -1;
} 

static int is_fd_bad(int file){
	if((file<0||file>=TASK_OFILE_NR)){
		return 1;
	}
	return 0;

}
/**
 * 复制一个文件描述符
 */
int sys_dup (int file) {
	// 我们要先对file进行判断是否合法 合法在进行复制

	if(is_fd_bad(file)){
		log_printf("file %d is not valid.",file);
		return -1;
	}


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
 * 我们这里面只实现了dev_read
 */
int sys_read(int file, char *ptr, int len) {
    if (file == TEMP_FILE_ID) {
        kernel_memcpy(ptr, temp_pos, len);
        temp_pos += len;
        return len;
    }
	if(is_fd_bad(file)||!ptr||!len){
		return 0;
	}

	
	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}
	// 判断操作权限
	if(p_file->mode==O_WRONLY){		
		log_printf("%d:write only",file);
		return 0;
	}

	fs_t *fs = p_file->fs;
	fs_protect(fs);
	int err = fs->op->read(ptr,len,p_file);
	fs_unprotect(fs);
	
	return err;
}


/**
 * 写文件
 */
int sys_write(int file, char *ptr, int len) {

	if(is_fd_bad(file)||!ptr||!len){
			return 0;
	}
	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}
	// 判断操作权限
	if(p_file->mode==O_RDONLY){		
		log_printf("%d:read only",file);
		return 0;
	}

	fs_t *fs = p_file->fs;
	fs_protect(fs);
	int err = fs->op->write(ptr,len,p_file);
	fs_unprotect(fs);
	return err;
}

/**
 * 文件访问位置定位
 */
int sys_lseek(int file, int ptr, int dir) {

    if (file == TEMP_FILE_ID) {
        temp_pos = (uint8_t *)(ptr + TEMP_ADDR);
        return 0;
    }
	if(is_fd_bad(file)){
		return 0;
	}

	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}


	fs_t *fs = p_file->fs;
	fs_protect(fs);
	int err = fs->op->seek(p_file,ptr,dir);
	fs_unprotect(fs);
	return err;
}

/**
 * 关闭文件
 */
int sys_close(int file) {
	if (file == TEMP_FILE_ID) {
		return 0;
	}
	if(is_fd_bad(file)){
		return 0;
	}

	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}
	ASSERT(p_file->ref>0);
	
	

	// 当某个文件表被指针指向的次数变为0  才是真正的关闭
	if(p_file->ref--==1){
		fs_t *fs = p_file->fs;
		fs_protect(fs);
		fs->op->close(p_file);
		fs_unprotect(fs);
		file_free(p_file);
	}
	





	return 0;
}


/**
 * 判断文件描述符与tty关联
 */
int sys_isatty(int file) {
	if(is_fd_bad(file)){
		return 0;
	}
	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}
	return p_file->type==FILE_TTY;
}

/**
 * @brief 获取文件状态
 */
int sys_fstat(int file, struct stat *st) {
 
	if(is_fd_bad(file)){
		return 0;
	}
	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}
	fs_t *fs = p_file->fs;
	kernel_memset(st,0,sizeof(struct stat));
	fs_protect(fs);
	int err = fs->op->stat(p_file,st);
	fs_unprotect(fs);
    return err;
}

