#include "tools/bitmap.h"
#include "tools/klib.h"

int bitmap_is_set(bitmap_t * bitmap,int index){
    return bitmap_get_bit(bitmap,index) ? 1 : 0;
}

// 为进程分配多个页
int bitmap_alloc_nbits(bitmap_t *bitmap, int bit,int count){
    int search_idx = 0;
    int ok_index = -1;
    int i = -1;
    while(search_idx < bitmap->bit_count){
        if(bitmap_get_bit(bitmap,search_idx)!=bit){
            search_idx++;
            continue;
        }
        ok_index = search_idx;
        for(i = 1;(i < count)&&(search_idx<bitmap->bit_count);i++){
            if(bitmap_get_bit(bitmap,search_idx++)!=bit){
                ok_index = -1;
                break;
            }
        }
        if(i>=count){
            bitmap_set_bit(bitmap,ok_index,count,~bit);
            return ok_index;
        }
    }
    return -1;
}


int bitmap_get_bit(bitmap_t *bitmap,int index){
    int bit_index = index/8;
    int off = index % 8;
    return (bitmap->bits[bit_index]) & (1<<off);
}

void bitmap_set_bit (bitmap_t *bitmap,int index,int count,int bit){
    for(int i=0 ; (i<count) && (index < bitmap->bit_count) ; i++ , index++){
        if(bit){
            bitmap->bits[index/8] |= (1<<(index%8));
        }else{
            bitmap->bits[index/8] &= (1<<(index%8));
        }
    }
}
int bitmap_byte_count(int bitcount){
    if(bitcount%8>0)
        return bitcount/8+1;
    else    
        return bitcount/8;
}

// bitcount 有效的页数
void bitmap_init(bitmap_t *bitmap,uint8_t *bits,int bit_count,int init_bit){
    bitmap->bit_count = bit_count;
    bitmap->bits = bits;
    // 计算位图结构体占用的内存大小
    int bytes = bitmap_byte_count(bitmap->bit_count);
    kernel_memset(bits,init_bit ? 0xFF:0,bytes);    
}

