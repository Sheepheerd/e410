#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>

// --- PIN DEFINITIONS ---
#define PIN_PWMA 23
#define PIN_AIN2 22
#define PIN_AIN1 21
#define PIN_STBY 19
#define PIN_WHIPPER 33
#define ADC_CHAN ADC_CHANNEL_5

// --- SETTINGS ---
#define WHIPPER_TARGET 3430
#define MOTOR_SPEED 300
#define TIMEOUT_MS 3000

adc_oneshot_unit_handle_t adc1_handle;

void init_servo_hardware() {
  gpio_config_t io_conf = {
      .pin_bit_mask =
          (1ULL << PIN_AIN1) | (1ULL << PIN_AIN2) | (1ULL << PIN_STBY),
      .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io_conf);

  gpio_set_level(PIN_AIN1, 0);
  gpio_set_level(PIN_AIN2, 0);
  gpio_set_level(PIN_STBY, 1);

  ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                    .duty_resolution = LEDC_TIMER_10_BIT,
                                    .timer_num = LEDC_TIMER_0,
                                    .freq_hz = 10000,
                                    .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                        .channel = LEDC_CHANNEL_0,
                                        .timer_sel = LEDC_TIMER_0,
                                        .gpio_num = PIN_PWMA,
                                        .duty = 0};
  ledc_channel_config(&ledc_channel);

  adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
  adc_oneshot_new_unit(&init_config1, &adc1_handle);
  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = ADC_ATTEN_DB_12,
  };
  adc_oneshot_config_channel(adc1_handle, ADC_CHAN, &config);
}

void stop_motor() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  gpio_set_level(PIN_AIN1, 0);
  gpio_set_level(PIN_AIN2, 0);
}

void drive_until_greater_than(int target_val) {
  int current_val;
  uint32_t startTime = xTaskGetTickCount();

  printf("Starting drive... Waiting for ADC > %d\n", target_val);

  while (1) {
    adc_oneshot_read(adc1_handle, ADC_CHAN, &current_val);

    // EXIT CONDITION: Stop as soon as we are greater than the target
    if (current_val > target_val) {
      stop_motor();
      printf("THRESHOLD CROSSED: Final ADC is %d\n", current_val);
      break;
    }

    // SAFETY: Timeout if we don't hit the target (stalled)
    // if ((xTaskGetTickCount() - startTime) > pdMS_TO_TICKS(TIMEOUT_MS)) {
    //   stop_motor();
    //   printf("TIMEOUT: Failed to reach %d within 3 seconds.\n", target_val);
    //   break;
    // }

    // DRIVE DIRECTION: Move toward Left (Higher ADC numbers)
    // If it moves RIGHT instead, swap these levels (0/1 to 1/0)
    gpio_set_level(PIN_AIN1, 0);
    gpio_set_level(PIN_AIN2, 1);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, MOTOR_SPEED);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void app_main(void) {
  init_servo_hardware();

  // Small delay to let ADC settle
  vTaskDelay(pdMS_TO_TICKS(500));

  int start_val;
  adc_oneshot_read(adc1_handle, ADC_CHAN, &start_val);

  if (start_val > WHIPPER_TARGET) {
    printf("Already greater than %d (Current: %d). No move needed.\n",
           WHIPPER_TARGET, start_val);

  } else {
    drive_until_greater_than(WHIPPER_TARGET);
  }

  while (1) {
    int val;
    adc_oneshot_read(adc1_handle, ADC_CHAN, &val);
    printf("Current ADC: %d\n", val);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
