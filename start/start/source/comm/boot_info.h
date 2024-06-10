#ifndef BOOT_INFO_H
#define BOOT_INFO_H
#define BOOT_ARM_REGION_MAX 10
#include "types.h"
typedef struct _boot_info_t{
    struct{
        uint32_t start;
        uint32_t size;
    }ram_region_cfg[BOOT_ARM_REGION_MAX];
    int ram_region_count;
}boot_info_t;

#endif