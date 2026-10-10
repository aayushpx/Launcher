#ifndef MPU6050_H
#define MPU6050_H

#include "esp_err.h"

typedef struct {
  float accel_x;       // m/s^2
  float accel_y;
  float accel_z;
  float gyro_x;        // degrees/second
  float gyro_y;
  float gyro_z;
  float temperature_c; // degrees Celsius
} mpu6050_reading_t;

esp_err_t mpu6050_init(void);
esp_err_t mpu6050_read(mpu6050_reading_t *reading);
void mpu6050_scan_bus(void);
void mpu6050_diagnose(void);

#endif

