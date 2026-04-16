#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mpu6050.h>
#include <stdio.h>

#include "esp_adc/adc_oneshot.h"

#define ADDR 0x68
#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21

#define FLEX_ADC_UNIT ADC_UNIT_1
#define FLEX_ADC_CHANNEL ADC_CHANNEL_6
#define FLEX_ADC_ATTEN ADC_ATTEN_DB_12 // Equivalent to the old DB_11

static const char *TAG = "force_glove";

void mpu6050_test(void *pvParameters) {
  mpu6050_dev_t dev = {0};
  ESP_ERROR_CHECK(
      mpu6050_init_desc(&dev, ADDR, 0, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO));
  ESP_ERROR_CHECK(mpu6050_init(&dev));

  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = FLEX_ADC_UNIT,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = FLEX_ADC_ATTEN,
  };
  ESP_ERROR_CHECK(
      adc_oneshot_config_channel(adc1_handle, FLEX_ADC_CHANNEL, &config));

  float z_offset = 0;
  int cal_samples = 100;

  ESP_LOGI(TAG, "CALIBRATING IMU...");
  for (int i = 0; i < cal_samples; i++) {
    mpu6050_rotation_t temp_rot;
    mpu6050_get_rotation(&dev, &temp_rot);
    z_offset += temp_rot.z;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  z_offset /= cal_samples;

  while (1) {
    // read IMU
    mpu6050_rotation_t rotation;
    mpu6050_get_rotation(&dev, &rotation);
    float clean_z = rotation.z - z_offset;

    // read Flex Sensor
    int flex_raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, FLEX_ADC_CHANNEL, &flex_raw));

    // print combined data
    if (clean_z > 5.0)
      printf("[ LEFT  ] ");
    else if (clean_z < -5.0)
      printf("[ RIGHT ] ");
    else
      printf("[ IDLE  ] ");

    // bend check (Values will drop with 1k resistor)
    if (flex_raw < 400) {
      printf("THROTTLE: MAX | ADC: %d\n", flex_raw);
    } else {
      printf("THROTTLE: OFF | ADC: %d\n", flex_raw);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void app_main() {
  ESP_ERROR_CHECK(i2cdev_init());
  xTaskCreate(mpu6050_test, "glove_task", configMINIMAL_STACK_SIZE * 6, NULL, 5,
              NULL);
}
