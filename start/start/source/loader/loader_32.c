#include "loader.h"
#include "comm/elf.h"
// static void read_disk(uint32_t sector,uint32_t sector_count,uint8_t * buf)
// {
//     outb(0x1F6, 0xE0);


//     outb(0x1F2, (uint8_t)(sector_count >> 8));
//     outb(0x1F3, (uint8_t)(sector >> 24));
//     outb(0x1F4,0);
//     outb(0x1F5,0);


//     // outb(0x1f6, 0xE0);
//     outb(0x1F2, (uint8_t)sector_count);
//     outb(0x1F3, (uint8_t)(sector));
//     outb(0x1F4, (uint8_t)(sector>>8));
//     outb(0x1F5, (uint8_t)(sector>>16));

//     outb(0x1F7, 0x24);

//     uint16_t *data_buf = (uint16_t *)buf;
//     while(sector_count--){
//         while((inb(0x1F7)&0x88)!=0x8){}

//         for(int i = 0;i<SECTOR_SIZE/2;i++)
//         {
//             *data_buf++=inw(0x1F0);
//         }
//     }
// }


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

static uint32_t reload_elf(uint8_t* file_buffer)
{
	Elf32_Ehdr *elf_hdr = (Elf32_Ehdr *)file_buffer;
	if((elf_hdr->e_ident[0] != 0x7F)||elf_hdr->e_ident[1]!='E'
	||elf_hdr->e_ident[2]!='L'||elf_hdr->e_ident[3]!='F'){
		return 0;
	}

	for(int i=0;i<elf_hdr->e_phnum;i++)
	{
		Elf32_Phdr *phdr = (Elf32_Phdr*)(file_buffer+elf_hdr->e_phoff)+i;
		if(phdr->p_type!=PT_LOAD)
		{
			continue;
		}

		uint8_t *src = file_buffer+phdr->p_offset;
		uint8_t *dest = (uint8_t*)phdr->p_paddr;
		//p_paddr 里面存储的就是我们想要拷贝到的地址 也就是0x10000
		for(int j = 0;j<phdr->p_filesz;j++)
		{
			*dest++ = *src++;
		}

		dest = (uint8_t*)phdr->p_paddr+phdr->p_filesz;
		for(int j = 0;j<phdr->p_memsz - phdr->p_filesz;j++)
		{
			*dest++ = 0;
		}
		return elf_hdr->e_entry;
	}
}

void die(uint8_t a)
{

}
void load_kernel(void)
{   
    // SYS_KERNEL_LOAD_ADDR 写入内存的位置 从第100扇区读取
    read_disk(100,500,(uint8_t*)SYS_KERNEL_LOAD_ADDR);
	// 这里我们将文件读取到了内存1M的位置 但是我们并不一定在这里运行他
	// 将他解析并拷贝到0x10000再运行
	uint32_t kernel_entry  = reload_elf((uint8_t*)SYS_KERNEL_LOAD_ADDR);
	if(kernel_entry==0)
	{
		die(2);
	}
	// ((void (*)(void))  这才是函数的类型 首先第一个void 说明函数的返回类型是void (*)说明是指针  第二个void 是传参
	// 
    ((void (*)(boot_info_t *))kernel_entry)(&boot_info); 
}