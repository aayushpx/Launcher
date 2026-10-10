#include "mpu6050.h"

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MPU6050_ADDRESS       0x68
#define MPU6050_REG_WHO_AM_I  0x75
#define MPU6050_REG_PWR_MGMT  0x6B
#define MPU6050_REG_GYRO_CFG  0x1B
#define MPU6050_REG_ACCEL_CFG 0x1C
#define MPU6050_REG_DATA      0x3B

#define MPU6050_SDA GPIO_NUM_16
#define MPU6050_SCL GPIO_NUM_21

static const char *TAG = "MPU6050";

static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t dev_handle;

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
  uint8_t data[2] = {reg, value};

  return i2c_master_transmit(
      dev_handle, data, sizeof(data), 100);

}

static esp_err_t read_registers(
    uint8_t start_reg, uint8_t *data, size_t length)
{
  return i2c_master_transmit_receive(
      dev_handle, &start_reg, 1, data, length, 100);
}

static int16_t read_int16(const uint8_t *data)
{
  return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}


void mpu6050_diagnose(void)
{
  const uint8_t registers[] = {
    0x75, // WHO_AM_I
    0x6B, // PWR_MGMT_1
    0x1B, // GYRO_CONFIG
    0x1C, // ACCEL_CONFIG
  };

  for (size_t i = 0;
      i < sizeof(registers) / sizeof(registers[0]);
      i++) {
    uint8_t value = 0;
    esp_err_t err = read_registers(
        registers[i], &value, 1);

    if (err == ESP_OK) {
      ESP_LOGI(TAG, "Register 0x%02X = 0x%02X",
          registers[i], value);
    } else {
      ESP_LOGE(TAG, "Register 0x%02X read failed: %s",
          registers[i], esp_err_to_name(err));
    }
  }
}


void mpu6050_scan_bus(void)
{
  ESP_LOGI(TAG, "Scanning I2C bus...");

  for (uint8_t address = 1; address < 127; address++) {
    esp_err_t err = i2c_master_probe(
        bus_handle, address, 100);

    if (err == ESP_OK) {
      ESP_LOGI(TAG, "I2C device found at 0x%02X", address);
    }
  }

  ESP_LOGI(TAG, "I2C scan complete");
}

esp_err_t mpu6050_init(void)
{
  i2c_master_bus_config_t bus_cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = MPU6050_SDA,
    .scl_io_num = MPU6050_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
  };

  esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C bus setup failed: %s",
        esp_err_to_name(err));
    return err;
  }

  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = MPU6050_ADDRESS,
    .scl_speed_hz = 50000,
  };

  err = i2c_master_bus_add_device(
      bus_handle, &dev_cfg, &dev_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C device setup failed: %s",
        esp_err_to_name(err));
    return err;
  }

  uint8_t id = 0;
  err = read_registers(MPU6050_REG_WHO_AM_I, &id, 1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "WHO_AM_I read failed: %s",
        esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "WHO_AM_I = 0x%02X", id);

  // Wake the sensor using its internal oscillator.
  err = write_register(MPU6050_REG_PWR_MGMT, 0x00);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Wake-up write failed: %s",
        esp_err_to_name(err));
    return err;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  uint8_t power = 0xFF;
  err = read_registers(MPU6050_REG_PWR_MGMT, &power, 1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Power register read failed: %s",
        esp_err_to_name(err));
    return err;
  }

  ESP_LOGI(TAG, "PWR_MGMT_1 after wake = 0x%02X", power);

  err = write_register(MPU6050_REG_ACCEL_CFG, 0x00);
  if (err != ESP_OK) return err;

  err = write_register(MPU6050_REG_GYRO_CFG, 0x00);
  if (err != ESP_OK) return err;

  ESP_LOGI(TAG,
      "Sensor responds over I2C; identity reported as 0x%02X",
      id);

  return ESP_OK;
}

esp_err_t mpu6050_read(mpu6050_reading_t *reading)
{
  if (reading == NULL || dev_handle == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t data[14];
  esp_err_t err = read_registers(
      MPU6050_REG_DATA, data, sizeof(data));

  if (err != ESP_OK) {
    return err;
  }

  int16_t ax = read_int16(&data[0]);
  int16_t ay = read_int16(&data[2]);
  int16_t az = read_int16(&data[4]);
  int16_t raw_temp = read_int16(&data[6]);
  int16_t gx = read_int16(&data[8]);
  int16_t gy = read_int16(&data[10]);
  int16_t gz = read_int16(&data[12]);

  reading->accel_x = ((float)ax / 16384.0f) * 9.80665f;
  reading->accel_y = ((float)ay / 16384.0f) * 9.80665f;
  reading->accel_z = ((float)az / 16384.0f) * 9.80665f;

  reading->gyro_x = (float)gx / 131.0f;
  reading->gyro_y = (float)gy / 131.0f;
  reading->gyro_z = (float)gz / 131.0f;

  reading->temperature_c = ((float)raw_temp / 340.0f) + 36.53f;

  return ESP_OK;
}


