#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

// Define our pins
#define PIN_PWMA 23
#define PIN_AIN2 22
#define PIN_AIN1 21
#define PIN_STBY 19
#define PIN_WHIPPER 33
#define ADC_CHAN ADC_CHANNEL_5 // GPIO 33

#define PIN_PWMB 14
#define PIN_BIN2 27
#define PIN_BIN1 18

#define SAFE_MAX_DUTY 600 // Cap at ~60% power to stay near 1A
#define RAMP_STEP 25      // How much to increase duty each step
#define RAMP_DELAY_MS 10  // Speed of the ramp

static int current_rear_duty = 0;
adc_oneshot_unit_handle_t adc1_handle;

adc_oneshot_unit_handle_t adc1_handle;

void init_hardware() {
  // 1. Setup Control Pins
  gpio_config_t io_conf = {
      .pin_bit_mask = (1ULL << PIN_AIN1) | (1ULL << PIN_AIN2) |
                      (1ULL << PIN_STBY) | (1ULL << PIN_BIN1) |
                      (1ULL << PIN_BIN2),
      .mode = GPIO_MODE_OUTPUT,
  };
  gpio_config(&io_conf);
  gpio_set_level(PIN_STBY, 1); // Enable Driver

  // 2. Setup PWM (Speed)
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_10_BIT,
      .timer_num = LEDC_TIMER_0,
      .freq_hz = 10000, // 10kHz for smooth motor movement
      .clk_cfg = LEDC_AUTO_CLK};
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t channel_a = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                     .channel = LEDC_CHANNEL_0,
                                     .timer_sel = LEDC_TIMER_0,
                                     .intr_type = LEDC_INTR_DISABLE,
                                     .gpio_num = PIN_PWMA,
                                     .duty = 0,
                                     .hpoint = 0};
  ledc_channel_config(&channel_a);

  ledc_channel_config_t channel_b = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                     .channel = LEDC_CHANNEL_1,
                                     .timer_sel = LEDC_TIMER_0,
                                     .intr_type = LEDC_INTR_DISABLE,
                                     .gpio_num = PIN_PWMB,
                                     .duty = 0,
                                     .hpoint = 0};
  ledc_channel_config(&channel_b);

  // 3. Setup ADC (Position Feedback)
  adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
  adc_oneshot_new_unit(&init_config1, &adc1_handle);
  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = ADC_ATTEN_DB_12, // Allows reading full 0-3.3V range
  };
  adc_oneshot_config_channel(adc1_handle, ADC_CHAN, &config);
}

void drive_whipper(int target_val) {
  int current_val;
  int tolerance = 40; // Stop within this range to prevent "jitter"

  while (1) {
    adc_oneshot_read(adc1_handle, ADC_CHAN, &current_val);
    int error = target_val - current_val;

    if (abs(error) < tolerance) {
      // Target Reached: Stop Motor
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
      break;
    }

    // Set Direction
    if (error > 0) {
      gpio_set_level(PIN_AIN1, 0);
      gpio_set_level(PIN_AIN2, 1);
    } else {
      gpio_set_level(PIN_AIN1, 1);
      gpio_set_level(PIN_AIN2, 0);
    }

    // Set Speed (Slow down as we get closer)
    int speed = abs(error) > 200 ? 700 : 400; // 0 to 1023 range
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void drive_rear(int target_speed) {
  // 1. Constrain to safety limits
  if (target_speed > SAFE_MAX_DUTY)
    target_speed = SAFE_MAX_DUTY;
  if (target_speed < -SAFE_MAX_DUTY)
    target_speed = -SAFE_MAX_DUTY;

  // 2. Set Direction Logic
  if (target_speed >= 0) {
    gpio_set_level(PIN_BIN1, 1);
    gpio_set_level(PIN_BIN2, 0);
  } else {
    gpio_set_level(PIN_BIN1, 0);
    gpio_set_level(PIN_BIN2, 1);
  }

  int target_duty = abs(target_speed);

  // 3. Ramping Logic: Gradually move current_duty toward target_duty
  while (current_rear_duty != target_duty) {
    if (current_rear_duty < target_duty) {
      current_rear_duty += RAMP_STEP;
      if (current_rear_duty > target_duty)
        current_rear_duty = target_duty;
    } else {
      current_rear_duty -= RAMP_STEP;
      if (current_rear_duty < target_duty)
        current_rear_duty = target_duty;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, current_rear_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    vTaskDelay(pdMS_TO_TICKS(RAMP_DELAY_MS));
  }
}

void app_main(void) {
  init_hardware();

  while (1) {
    drive_rear(128);
    drive_whipper(3500);

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Stop driving and steer left
    drive_rear(0);
    drive_whipper(150);

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
