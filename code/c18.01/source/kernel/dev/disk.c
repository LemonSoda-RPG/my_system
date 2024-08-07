#include "dev/disk.h"
#include "dev/dev.h"
#include "tools/klib.h"
#include "tools/log.h"
#include "comm/cpu_instr.h"
#include "cpu/irq.h"
#include "core/memory.h"
#include "core/task.h"
#include "ipc/sem.h"

// 支持多少个磁盘
static disk_t disk_buf[DISK_CNT];  // 通道结构
static mutex_t mutex;     // 通道信号量
static sem_t op_sem;      // 通道操作的信号量
static int task_on_op;



/**
 * 发送ata命令，支持多达16位的扇区，对我们目前的程序来书够用了。
 */
static void ata_send_cmd (disk_t * disk, uint32_t start_sector, uint32_t sector_count, int cmd) {
    outb(DISK_DRIVE(disk), DISK_DRIVE_BASE | disk->drive);		// 使用LBA寻址，并设置驱动器

	// 必须先写高字节
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count >> 8));	// 扇区数高8位
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 24));		// LBA参数的24~31位
	outb(DISK_LBA_MID(disk), 0);									// 高于32位不支持
	outb(DISK_LBA_HI(disk), 0);										// 高于32位不支持
	outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count));		// 扇区数量低8位
	outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 0));			// LBA参数的0-7
	outb(DISK_LBA_MID(disk), (uint8_t) (start_sector >> 8));		// LBA参数的8-15位
	outb(DISK_LBA_HI(disk), (uint8_t) (start_sector >> 16));		// LBA参数的16-23位

	// 选择对应的主-从磁盘
	outb(DISK_CMD(disk), (uint8_t)cmd);
}

/**
 * @brief 等待磁盘有数据到达
 */
static inline int ata_wait_data (disk_t * disk) {
    uint8_t status;
	do {
        // 等待数据或者有错误
        status = inb(DISK_STATUS(disk));
        if ((status & (DISK_STATUS_BUSY | DISK_STATUS_DRQ | DISK_STATUS_ERR))
                        != DISK_STATUS_BUSY) {
            break;
        }
    }while (1);

    // 检查是否有错误
    return (status & DISK_STATUS_ERR) ? -1 : 0;
}
/**
 * @brief 打印磁盘信息
 */
static void print_disk_info (disk_t * disk) {
    // 名字   总线序号   磁盘大小(M)   主盘还是从盘
    log_printf("%s:", disk->name);
    log_printf("  port_base: %x", disk->port_base);
    log_printf("  total_size: %d m", disk->sector_count * disk->sector_size / 1024 /1024);
    // log_printf("  drive: %s", disk->drive == DISK_DISK_MASTER ? "Master" : "Slave");

    // 显示分区信息
    log_printf("  Part info:");
    for (int i = 0; i < DISK_PRIMARY_PART_CNT; i++) {
        partinfo_t * part_info = disk->partinfo + i;
        if (part_info->type != FS_INVALID) {
            log_printf("    %s: type: %x, start sector: %d, count %d",
                    part_info->name, part_info->type,
                    part_info->start_sector, part_info->total_sector);
        }
    }
}

/**
 * 读取ATA数据端口
 */
static inline void ata_write_data (disk_t * disk, void * buf, int size) {
    uint16_t * c = (uint16_t *)buf;
    for (int i = 0; i < size / 2; i++) {
        outw(DISK_DATA(disk), *c++);
    }
}
/**
 * 读取ATA数据端口  从磁盘读取数据
 */
static inline void ata_read_data (disk_t * disk, void * buf, int size) {
    uint16_t * c = (uint16_t *)buf;
    for (int i = 0; i < size / 2; i++) {
        *c++ = inw(DISK_DATA(disk));
    }
}


/**
 * 获取指定序号的分区信息
 * 注意，该操作依赖物理分区分配，如果设备的分区结构有变化，则序号也会改变，得到的结果不同
 */
static int detect_part_info(disk_t * disk) {
    mbr_t mbr;

    // 读取mbr区
    // 参数解释  从第0扇区开始读 读一个扇区
    // 这里我们扫描的是从设备  也就是第二个磁盘的信息
    ata_send_cmd(disk, 0, 1, DISK_CMD_READ);
    int err = ata_wait_data(disk);
    if (err < 0) {
        log_printf("read mbr failed");
        return err;
    }
    ata_read_data(disk, &mbr, sizeof(mbr));

	// 遍历4个主分区描述，不考虑支持扩展分区
    // 指向分区表
	part_item_t * item = mbr.part_item;
    
    // 这个表是我们自己定义的  存储在磁盘结构体当中的  1+4的那个数组
    // 将item的信息拷贝到 part_info中
    partinfo_t * part_info = disk->partinfo + 1;  
    // 从下标1开始  因为0 我们用来表示整块磁盘了
	for (int i = 0; i < MBR_PRIMARY_PART_NR; i++, item++, part_info++) {
		part_info->type = item->system_id;

        // 没有分区，清空part_info
		if (part_info->type == FS_INVALID) {
			part_info->total_sector = 0;
            part_info->start_sector = 0;
            part_info->disk = (disk_t *)0;
        } else {
            // 在主分区中找到，复制信息
            kernel_sprintf(part_info->name, "%s%d", disk->name, i + 1);
            part_info->start_sector = item->relative_sectors;
            part_info->total_sector = item->total_sectors;
            part_info->disk = disk;
        }
	}
}

/**
 * @brief 检测磁盘相关的信息
 */
static int identify_disk (disk_t * disk) {
    // 发送命令  起始扇区号  扇区数量   命令
    // 命令不同 发送的那两个数字的含义也不同
    ata_send_cmd(disk, 0, 0, DISK_CMD_IDENTIFY);
    // 发送完命令之后进行读取 根据读取结果判断磁盘的有效性
    // 检测状态，如果为0，则控制器不存在
    int err = inb(DISK_STATUS(disk));
    if (err == 0) {
        log_printf("%s doesn't exist\n", disk->name);
        return -1;
    }

    // 等待数据就绪, 此时中断还未开启，因此暂时可以使用查询模式
    err = ata_wait_data(disk);
    if (err < 0) {
        log_printf("disk[%s]: read failed!\n", disk->name);
        return err;
    }

    // 读取返回的数据，特别是uint16_t 100 through 103
    // 测试用的盘： 总共102400 = 0x19000， 实测会多一个扇区，为vhd磁盘格式增加的一个扇区
    // 这里是512字节  因为16位是2字节
    uint16_t buf[256];
    ata_read_data(disk, buf, sizeof(buf));
    // 这里读取的数据就是这样分布的  磁盘的大小信息被保存在了这个位置
    disk->sector_count = *(uint32_t *)(buf + 100);  //扇区数量
    disk->sector_size = SECTOR_SIZE;            // 固定为512字节大小

    // sda sdb 这代表两个磁盘   sda0  sda1 代表不同的分区表
    // 分区0保存了整个磁盘的信息
    partinfo_t * part = disk->partinfo + 0;
    part->disk = disk;
    // part->name 目标内存
    kernel_sprintf(part->name, "%s%d", disk->name, 0);
    part->start_sector = 0;
    part->total_sector = disk->sector_count;
    part->type = FS_INVALID;

    // 接下来识别硬盘上的分区信息
    detect_part_info(disk);
    return 0;
}


void disk_init(void){
    log_printf("Checking disk...");

    // 清空所有disk，以免数据错乱。不过引导程序应该有清0的，这里为安全再清一遍
    kernel_memset(disk_buf, 0, sizeof(disk_buf));
    mutex_init(&mutex);
    sem_init(&op_sem,0);
    // 检测各个硬盘, 读取硬件是否存在，有其相关信息
    for (int i = 0; i < DISK_PER_CHANNEL; i++) {
        disk_t * disk = disk_buf + i;

        // 先初始化各字段  给磁盘取名字
        kernel_sprintf(disk->name, "sd%c", i + 'a');
        // 判断硬盘的主从
        disk->drive = (i == 0) ? DISK_DISK_MASTER : DISK_DISK_SLAVE;
        // 基地址
        disk->port_base = IOBASE_PRIMARY;
        disk->mutex = &mutex;
        disk->op_sem = &op_sem;

        // 识别磁盘，有错不处理，直接跳过  检测磁盘是否存在
        // 硬盘读取过程  包括发送命令 等待  读取命令
        int err = identify_disk(disk);
        if (err == 0) {
            print_disk_info(disk);
        }
    }
}


int disk_open (device_t * dev){
    //(dev->minor >> 4) 这个好像是从0xa开始的  我们要把他变成下标
    int disk_idx = (dev->minor >> 4) - 0xa;  // 硬盘号
    int part_idx = dev->minor & 0xF;    // 分区号 从1开始 0是特殊分区
    if ((disk_idx >= DISK_CNT) || (part_idx >= DISK_PRIMARY_PART_CNT)) {
        log_printf("device minor error: %d", dev->minor);
        return -1;
    }
    // 硬盘指针
    disk_t * disk = disk_buf + disk_idx;
    if (disk->sector_size == 0) {
        log_printf("disk not exist. device:sd%x", dev->minor);
        return -1;
    }  
    // 分区指针
    partinfo_t * part_info = disk->partinfo + part_idx;
    if (part_info->total_sector == 0) {
        log_printf("part not exist. device:sd%x", dev->minor);
        return -1;
    }
     // 磁盘存在，建立关联
    dev->data = part_info;  // 将分区信息保存到data当中
    irq_install(IRQ14_HARDDISK_PRIMARY, exception_handler_ide_primary);
    irq_enable(IRQ14_HARDDISK_PRIMARY);
    return 0;
}
// 函数名字是read
/**
 * @brief 读磁盘
 * start_sector  分区的第几个扇区
 */
int disk_read (device_t * dev, int start_sector, char * buf, int count) {
    // 取分区信息
    partinfo_t * part_info = (partinfo_t *)dev->data;
    if (!part_info) {
        log_printf("Get part info failed! device = %d", dev->minor);
        return -1;
    }
    // 获取分区对应的磁盘
    disk_t * disk = part_info->disk;
    if (disk == (disk_t *)0) {
        log_printf("No disk for device %d", dev->minor);
        return -1;
    }
    // 对总线加锁
    mutex_lock(disk->mutex);
    task_on_op = 1;

    int cnt;
    // part_info->start_sector 分区起始扇区下标
    ata_send_cmd(disk, part_info->start_sector + start_sector, count, DISK_CMD_READ);
    for (cnt = 0; cnt < count; cnt++, buf += disk->sector_size) {
        // 循环读取 每次读取一个扇区

        // 利用信号量等待中断通知，然后再读取数据
        // 我们发送命令之后需要进行等待
        // 为什么这里要判断task_current呢  是因为假如task_current 是空的 说明系统还没初始化完毕  此时调用sem会报错
        if (task_current()) {
            sem_wait(disk->op_sem);
        }

        // 这里虽然有调用等待，但是由于已经是操作完毕，所以并不会等
        // 这里主要检查是否出错
        int err = ata_wait_data(disk);
        if (err < 0) {
            log_printf("disk(%s) read error: start sect %d, count %d", disk->name, start_sector, count);
            break;
        }

        // 此处再读取数据
        ata_read_data(disk, buf, disk->sector_size);
    }

    mutex_unlock(disk->mutex);
    return cnt;
}
/**
 * @brief 写扇区
 */
int disk_write (device_t * dev, int start_sector, char * buf, int count) {
    // 取分区信息
    partinfo_t * part_info = (partinfo_t *)dev->data;
    if (!part_info) {
        log_printf("Get part info failed! device = %d", dev->minor);
        return -1;
    }

    disk_t * disk = part_info->disk;
    if (disk == (disk_t *)0) {
        log_printf("No disk for device %d", dev->minor);
        return -1;
    }

    mutex_lock(disk->mutex);
    task_on_op = 1;

    int cnt;
    ata_send_cmd(disk, part_info->start_sector + start_sector, count, DISK_CMD_WRITE);
    for (cnt = 0; cnt < count; cnt++, buf += disk->sector_size) {
        // 先写数据
        ata_write_data(disk, buf, disk->sector_size);

        // 利用信号量等待中断通知，等待写完成
        if (task_current()) {
            sem_wait(disk->op_sem);
        }

        // 这里虽然有调用等待，但是由于已经是操作完毕，所以并不会等
        int err = ata_wait_data(disk);
        if (err < 0) {
            log_printf("disk(%s) write error: start sect %d, count %d", disk->name, start_sector, count);
            break;
        }
    }

    mutex_unlock(disk->mutex);
    return cnt;
}
// 这就是 ioctl 吗
int disk_control(device_t * dev, int cmd, int arg0, int arg1){

    return -1;


}
void disk_close(device_t * dev){
    return;

}

/**
 * @brief 磁盘主通道中断处理
 */
void do_handler_ide_primary (exception_frame_t *frame)  {
    pic_send_eoi(IRQ14_HARDDISK_PRIMARY);
    if (task_on_op&&task_current()) {
        sem_notify(&op_sem);
    }
}

dev_desc_t dev_disk_desc = {
	.name = "disk",
	.major = DEV_DISK,
	.open = disk_open,
	.read = disk_read,
	.write = disk_write,
	.control = disk_control,
	.close = disk_close,
};