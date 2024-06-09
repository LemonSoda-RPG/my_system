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
#include <sys/file.h>
#include "dev/disk.h"

#define FS_TABLE_SIZE		10		// 文件系统表数量

static list_t mounted_list;			// 已挂载的文件系统
static list_t free_list;				// 空闲fs列表
static fs_t fs_tbl[FS_TABLE_SIZE];		// 空闲文件系统列表大小

extern fs_op_t devfs_op;

#define TEMP_FILE_ID		100
#define TEMP_ADDR        	(8*1024*1024)      // 在0x800000处缓存原始

static uint8_t * temp_pos;       // 当前位置

/**
 * @brief 判断文件描述符是否正确
 */
static int is_fd_bad (int file) {
	if ((file < 0) && (file >= TASK_OFILE_NR)) {
		return 1;
	}

	return 0;
}

/**
 * @brief 获取指定文件系统的操作接口
 */
static fs_op_t * get_fs_op (fs_type_t type, int major) {
	switch (type) {
	case FS_DEVFS:
		return &devfs_op;
	default:
		return (fs_op_t *)0;
	}
}

/**
 * @brief 挂载文件系统
 */
static fs_t * mount (fs_type_t type, char * mount_point, int dev_major, int dev_minor) {
	fs_t * fs = (fs_t *)0;

	log_printf("mount file system, name: %s, dev: %x", mount_point, dev_major);

	// 遍历，查找是否已经有挂载
 	list_node_t * curr = list_first(&mounted_list);
	while (curr) {
		fs_t * fs = list_node_parent(curr, fs_t, node);
		if (kernel_strncmp(fs->mount_point, mount_point, FS_MOUNTP_SIZE) == 0) {
			log_printf("fs alreay mounted.");
			goto mount_failed;
		}
		curr = list_node_next(curr);
	}

	// 分配新的fs结构
	list_node_t * free_node = list_remove_first(&free_list);
	if (!free_node) {
		log_printf("no free fs, mount failed.");
		goto mount_failed;
	}
	fs = list_node_parent(free_node, fs_t, node);

	// 检查挂载的文件系统类型：不检查实际
	fs_op_t * op = get_fs_op(type, dev_major);
	if (!op) {
		log_printf("unsupported fs type: %d", type);
		goto mount_failed;
	}

	// 给定数据一些缺省的值
	kernel_memset(fs, 0, sizeof(fs_t));
	kernel_strncpy(fs->mount_point, mount_point, FS_MOUNTP_SIZE);
	fs->op = op;
	fs->mutex = (mutex_t *)0;

	// 挂载文件系统
	if (op->mount(fs, dev_major, dev_minor) < 0) {
		log_printf("mount fs %s failed", mount_point);
		goto mount_failed;
	}
	list_insert_last(&mounted_list, &fs->node);
	return fs;
mount_failed:
	if (fs) {
		// 回收fs
		list_insert_first(&free_list, &fs->node);
	}
	return (fs_t *)0;
}

/**
 * @brief 初始化挂载列表
 */
static void mount_list_init (void) {
	list_init(&free_list);
	for (int i = 0; i < FS_TABLE_SIZE; i++) {
		list_insert_first(&free_list, &fs_tbl[i].node);
	}
	list_init(&mounted_list);
}

/**
 * @brief 文件系统初始化
 */
void fs_init (void) {
	mount_list_init();
    file_table_init();

	// 磁盘检查
	disk_init();

	// 挂载设备文件系统，待后续完成。挂载点名称可随意
	fs_t * fs = mount(FS_DEVFS, "/dev", 0, 0);
	ASSERT(fs != (fs_t *)0);
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

/**
 * @brief 判断路径是否以xx开头
 */
int path_begin_with (const char * path, const char * str) {
	const char * s1 = path, * s2 = str;
	while (*s1 && *s2 && (*s1 == *s2)) {
		s1++;
		s2++;
	}

	return *s2 == '\0';
}

/**
 * @brief 获取下一级子目录
 */
const char * path_next_child (const char * path) {
   const char * c = path;

    while (*c && (*c++ == '/')) {}
    while (*c && (*c++ != '/')) {}
    return *c ? c : (const char *)0;
}

static void fs_protect (fs_t * fs) {
	if (fs->mutex) {
		mutex_lock(fs->mutex);
	}
}

static void fs_unprotect (fs_t * fs) {
	if (fs->mutex) {
		mutex_unlock(fs->mutex);
	}
}

/**
 * 打开文件
 */
int sys_open(const char *name, int flags, ...) {
	// 临时使用，保留shell加载的功能
	if (kernel_strncmp(name, "/shell.elf", 4) == 0) {
        // 暂时直接从扇区1000上读取, 读取大概40KB，足够了
		int dev_id = dev_open(DEV_DISK, 0xa0, (void *)0);
		dev_read(dev_id, 5000, (uint8_t *)TEMP_ADDR, 80);
        //read_disk(5000, 80, (uint8_t *)TEMP_ADDR);
        temp_pos = (uint8_t *)TEMP_ADDR;
        return TEMP_FILE_ID;
    }

	// 分配文件描述符链接
	file_t * file = file_alloc();
	if (!file) {
		return -1;
	}

	int fd = task_alloc_fd(file);
	if (fd < 0) {
		goto sys_open_failed;
	}

	// 检查名称是否以挂载点开头，如果没有，则认为name在根目录下
	// 即只允许根目录下的遍历
	fs_t * fs = (fs_t *)0;
	list_node_t * node = list_first(&mounted_list);
	while (node) {
		fs_t * curr = list_node_parent(node, fs_t, node);
		if (path_begin_with(name, curr->mount_point)) {
			fs = curr;
			break;
		}
		node = list_node_next(node);
	}

	if (fs) {
		name = path_next_child(name);
	} else {
		//fs = root_fs;
	}

	file->mode = flags;
	file->fs = fs;
	kernel_strncpy(file->file_name, name, FILE_NAME_SIZE);

	fs_protect(fs);
	int err = fs->op->open(fs, name, file);
	if (err < 0) {
		fs_unprotect(fs);

		log_printf("open %s failed.", name);
		return -1;
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

/**
 * 复制一个文件描述符
 */
int sys_dup (int file) {
	// 超出进程所能打开的全部，退出
	if (is_fd_bad(file)) {
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
		file_inc_ref(p_file);
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
    }

    if (is_fd_bad(file) || !ptr || !len) {
		return 0;
	}

	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}

	if (p_file->mode == O_WRONLY) {
		log_printf("file is write only");
		return -1;
	}

	// 读取文件
	fs_t * fs = p_file->fs;
	fs_protect(fs);
	int err = fs->op->read(ptr, len, p_file);
	fs_unprotect(fs);
	return err;
}

/**
 * 写文件
 */
int sys_write(int file, char *ptr, int len) {
	if (is_fd_bad(file) || !ptr || !len) {
		return 0;
	}

	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}

	if (p_file->mode == O_RDONLY) {
		log_printf("file is write only");
		return -1;
	}

	// 写入文件
	fs_t * fs = p_file->fs;
	fs_protect(fs);
	int err = fs->op->write(ptr, len, p_file);
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

	if (is_fd_bad(file)) {
		return -1;
	}

	file_t * p_file = task_file(file);
	if (!p_file) {
		log_printf("file not opened");
		return -1;
	}

	// 写入文件
	fs_t * fs = p_file->fs;

	fs_protect(fs);
	int err = fs->op->seek(p_file, ptr, dir);
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

	if (is_fd_bad(file)) {
		log_printf("file error");
		return -1;
	}

	file_t * p_file = task_file(file);
	if (p_file == (file_t *)0) {
		log_printf("file not opened. %d", file);
		return -1;
	}

	ASSERT(p_file->ref > 0);

	if (p_file->ref-- == 1) {
		fs_t * fs = p_file->fs;

		fs_protect(fs);
		fs->op->close(p_file);
		fs_unprotect(fs);
	    file_free(p_file);
	}

	task_remove_fd(file);
	return 0;
}


/**
 * 判断文件描述符与tty关联
 */
int sys_isatty(int file) {
	if (is_fd_bad(file)) {
		return 0;
	}

	file_t * pfile = task_file(file);
	if (pfile == (file_t *)0) {
		return 0;
	}

	return pfile->type == FILE_TTY;
}

/**
 * @brief 获取文件状态
 */
int sys_fstat(int file, struct stat *st) {
	if (is_fd_bad(file)) {
		return -1;
	}

	file_t * p_file = task_file(file);
	if (p_file == (file_t *)0) {
		return -1;
	}

	fs_t * fs = p_file->fs;

    kernel_memset(st, 0, sizeof(struct stat));

	fs_protect(fs);
	int err = fs->op->stat(p_file, st);
	fs_unprotect(fs);
	return err;
}
