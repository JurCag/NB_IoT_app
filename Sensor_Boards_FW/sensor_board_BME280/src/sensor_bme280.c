#include "sensor_bme280.h"

static s32 compTemp;
static u32 compHumi;
static u32 compPres;

static void sensorBme280Init(void);
static void taskDataAcq(void *pvParameters);
static void i2cMasterInit(void);
static s8 bme280I2cBusWrite(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
static s8 bme280I2cBusRead(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);


void sensorBme280CreateTaskDataAcq(void)
{
	xTaskCreate(
                taskDataAcq,           			/* Task function */
                "taskDataAcq",         			/* Name of task */
                2048,                           /* Stack size of task */
                NULL,                           /* Parameter of the task */
                tskIDLE_PRIORITY + 0,           /* Priority of the task */
                NULL                   			/* Handle of created task */
                );
}

static void sensorBme280Init(void)
{
    i2cMasterInit();
    BME280_delay_msek_register_CB(delay_msek_CB);
}

static void taskDataAcq(void *pvParameters)
{
	struct bme280_t bme280 = {
		.bus_write = bme280I2cBusWrite,
		.bus_read = bme280I2cBusRead,
		.dev_addr = BME280_I2C_ADDRESS1,
		.delay_msec = delay_msek_CB
	};

	s32 com_rslt;
	s32 v_uncomp_pressure_s32;
	s32 v_uncomp_temperature_s32;
	s32 v_uncomp_humidity_s32;
	
	sensorBme280Init();

	com_rslt = bme280_init(&bme280);

	com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_16X);
	com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_2X);
	com_rslt += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);

	com_rslt += bme280_set_standby_durn(BME280_STANDBY_TIME_1_MS);
	com_rslt += bme280_set_filter(BME280_FILTER_COEFF_16);

	com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);
	if (com_rslt == SUCCESS)
    {
		while(true) 
        {
			vTaskDelay(500/portTICK_PERIOD_MS);

			com_rslt = bme280_read_uncomp_pressure_temperature_humidity(
														&v_uncomp_pressure_s32, 
														&v_uncomp_temperature_s32, 
														&v_uncomp_humidity_s32);

			if (com_rslt == SUCCESS) 
            {
				ESP_LOGI(TAG_BME280, "%.2f degC / %.3f hPa / %.3f %%",
					bme280_compensate_temperature_double(v_uncomp_temperature_s32),
					bme280_compensate_pressure_double(v_uncomp_pressure_s32)/100, // Pa -> hPa
					bme280_compensate_humidity_double(v_uncomp_humidity_s32));

				compTemp = bme280_compensate_temperature_int32(v_uncomp_temperature_s32);
				compPres = bme280_compensate_pressure_int32(v_uncomp_pressure_s32);
				compHumi = bme280_compensate_humidity_int32(v_uncomp_humidity_s32);
				
			} 
            else 
            {
				ESP_LOGE(TAG_BME280, "measure error. code: %d", com_rslt);
			}
		}
	} 
    else 
    {
		ESP_LOGE(TAG_BME280, "init or setting error. code: %d", com_rslt);
	}

	ESP_LOGE(TAG_BME280, "DELETING THIS TASK");

	vTaskDelete(NULL);
}



static void i2cMasterInit(void)
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA_PIN,
		.scl_io_num = SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 1000000
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

static s8 bme280I2cBusWrite(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;

	esp_err_t espRc;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, reg_addr, true);
	i2c_master_write(cmd, reg_data, cnt, true);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		iError = SUCCESS;
	} else {
		iError = FAIL;
	}
	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}

static s8 bme280I2cBusRead(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;
	esp_err_t espRc;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

	if (cnt > 1) {
		i2c_master_read(cmd, reg_data, cnt-1, I2C_MASTER_ACK);
	}
	i2c_master_read_byte(cmd, reg_data+cnt-1, I2C_MASTER_NACK);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		iError = SUCCESS;
	} else {
		iError = FAIL;
	}

	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}

void delay_msek_CB(u32 msek)
{
	vTaskDelay(msek/portTICK_PERIOD_MS);
}


float sensorBme280GetTemperature(void)
{
	// ESP_LOGI(TAG_BME280, "Temp INT %d, float %.2f", compTemp, (compTemp / 100.0));
	return (compTemp / 100.0);
}

float sensorBme280GetHumidity(void)
{
	// ESP_LOGI(TAG_BME280, "Humi INT %d, float %.2f", compHumi, (compHumi / 1024.0));
	return (compHumi / 1024.0);
}

float sensorBme280GetPressure(void)
{
	// ESP_LOGI(TAG_BME280, "Pres INT %d, float %.2f", compPres, (compPres / 100.0));
	return (compPres / 100.0);
}