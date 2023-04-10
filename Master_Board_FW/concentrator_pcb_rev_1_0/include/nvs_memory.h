#ifndef __NVS_MEMORY_H__
#define __NVS_MEMORY_H__

#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

#define NVS_MAX_KEY_LEN (15)

void nvsInit(void);
uint8_t nvsWriteItem(char* key, uint32_t item);
uint8_t nvsReadItem(char* key, uint32_t* item);
void nvsEraseAll(void);

#endif // __NVS_MEMORY_H__
