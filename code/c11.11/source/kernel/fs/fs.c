#include "fs/fs.h"
#include "comm/types.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"

#define TEMP_FILE_ID		100
static uint8_t TEMP_ADDR[100*1024];
static uint8_t * temp_pos;       // 当前位置

/**
 * 从磁盘上进行读写
 */
int sys_read(int file, char *ptr, int len) {
    if (file == TEMP_FILE_ID) {
        kernel_memcpy(ptr, temp_pos, len);
        temp_pos += len;
        return len;
    }
    return -1;
}

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

int sys_open(const char *name, int flags, ...){
    if (name[0] == '/') {
        // 暂时直接从扇区5000上读取, 读取大概40KB，足够了
        read_disk(5000, 80, (uint8_t *)TEMP_ADDR);   // 读取80个扇区  每个扇区的大小是512字节
        temp_pos = (uint8_t *)TEMP_ADDR;
        return TEMP_FILE_ID;   //返回一个文件描述符
    }

    return -1;


}
#include "tools/log.h"
int sys_write(int file, char *ptr, int len){
    ptr[len] = '\0';
    
    log_printf("%s", ptr);
    return len;

}   

/**
 * @brief 获取文件状态
 */
int sys_fstat(int file, struct stat *st) {
    kernel_memset(st, 0, sizeof(struct stat));
    st->st_size = 0;
    return 0;
}

int sys_isatty(int file) {
	return -1;
}
/**
 * 移动指针
 */
int sys_lseek(int file, int ptr, int dir){
    if(file==TEMP_FILE_ID){
        temp_pos = (uint8_t *)(TEMP_ADDR+ptr);
        return 0;
    }
    return -1;
} 
int sys_close(int file){

    return 0;

}


