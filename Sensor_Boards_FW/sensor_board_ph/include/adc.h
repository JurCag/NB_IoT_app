#ifndef __ADC_H__
#define __ADC_H__

#include <driver/adc.h>
#include "esp_adc_cal.h"
#include <stdlib.h>

uint8_t adcConfig(void);
uint32_t adcGetVoltage(void);
uint32_t adcGetRawValue(void);

#endif // __ADC_H__
