/*******************************************************************************
 * file           : i2c_vl53l1x.h
 * brief          : I2C VL53L1X definitions
 ******************************************************************************/
#ifndef I2C_VL53L1X_H
#define I2C_VL53L1X_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

hal_status_t VL53L1X_Init(QueueHandle_t I2cQueue);
hal_status_t VL53L1X_Start();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* I2C_VL53L1X_H */
