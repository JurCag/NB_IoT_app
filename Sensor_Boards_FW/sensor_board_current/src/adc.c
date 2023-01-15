#include "adc.h"

static esp_adc_cal_characteristics_t adc1Chars; // adc1 characteristics

#define ADC_CHANNEL         ADC_CHANNEL_0
#define ADC_ATTEN_DB        ADC_ATTEN_DB_11

// voltage mV value when adc rawVal = 4095
#if (ADC_ATTEN_DB == ADC_ATTEN_DB_11)
    #define ADC_MAX_VOLTAGE_MV      (2790)
#elif (DC_ATTEN_DB == ADC_ATTEN_DB_6)
    #define ADC_MAX_VOLTAGE_MV      (1531)
#elif (DC_ATTEN_DB == ADC_ATTEN_DB_2_5)
    #define ADC_MAX_VOLTAGE_MV      (1100)
#elif (DC_ATTEN_DB == ADC_ATTEN_DB_0)
    #define ADC_MAX_VOLTAGE_MV      (821)
#else 
    #error "Define 'ADC_ATTEN_DB' macro as one of adc_atten_t enum!"
#endif

uint8_t adcConfig(void)
{
    if (adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB) != ESP_OK) // GPIO 0 -> ADC1 channel 0 
        return EXIT_FAILURE;

    if(adc1_config_width(ADC_WIDTH_BIT_12) != ESP_OK)
        return EXIT_FAILURE;

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB, ADC_WIDTH_BIT_12, 0, &adc1Chars);

    return EXIT_SUCCESS;
}

uint32_t adcGetVoltage(void)
{
    uint32_t rawVal =  adc1_get_raw(ADC_CHANNEL);
    uint32_t mV = esp_adc_cal_raw_to_voltage(rawVal, &adc1Chars);

    return mV;
}

uint32_t adcGetRawValue(void)
{
    uint32_t rawVal =  adc1_get_raw(ADC_CHANNEL);

    return rawVal;
}