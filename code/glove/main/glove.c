#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mpu6050.h>
#include <stdio.h>

#define ADDR 0x68

#define I2C_MASTER_SCL_IO 22 // GPIO for SCL
#define I2C_MASTER_SDA_IO 21 // GPIO for SDA

static const char *TAG = "mpu6050_test";

void mpu6050_test(void *pvParameters) {
  mpu6050_dev_t dev = {0};
  ESP_ERROR_CHECK(
      mpu6050_init_desc(&dev, 0x68, 0, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO));
  ESP_ERROR_CHECK(mpu6050_init(&dev));

  float z_offset = 0;
  int cal_samples = 100;

  ESP_LOGI(TAG, "CALIBRATING...");
  for (int i = 0; i < cal_samples; i++) {
    mpu6050_rotation_t temp_rot;
    mpu6050_get_rotation(&dev, &temp_rot);
    z_offset += temp_rot.z;
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  z_offset /= cal_samples;
  ESP_LOGI(TAG, "Calibration Done. Offset: %.2f", z_offset);

  while (1) {
    mpu6050_rotation_t rotation;
    mpu6050_get_rotation(&dev, &rotation);

    float clean_z = rotation.z - z_offset;

    if (clean_z > 5.0) {
      ESP_LOGI(TAG, "[ <--- LEFT ]  Speed: %.1f", clean_z);
    } else if (clean_z < -5.0) {
      ESP_LOGI(TAG, "[ RIGHT ---> ] Speed: %.1f", clean_z);
    } else {
      // ESP_LOGI(TAG, "      STILL      ");
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
void app_main() {
  ESP_ERROR_CHECK(i2cdev_init());

  xTaskCreate(mpu6050_test, "mpu6050_test", configMINIMAL_STACK_SIZE * 6, NULL,
              5, NULL);
}
