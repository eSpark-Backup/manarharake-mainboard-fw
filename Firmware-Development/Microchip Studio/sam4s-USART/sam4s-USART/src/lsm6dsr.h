/*
 * lsm6dsr.h
 *
 * Created: 4/17/2024 11:13:33 AM
 *  Author: classicComputer
 */ 
#include "lsm6dsr_reg.h"
#include "twi.h"


#ifndef LSM6DSR_H_
#define LSM6DSR_H_

/* IMU Related definitions */
#define LSM6DSR_ACC_SENSITIVITY_FS_2G   0.061f
#define LSM6DSR_ACC_SENSITIVITY_FS_4G   0.122f
#define LSM6DSR_ACC_SENSITIVITY_FS_8G   0.244f
#define LSM6DSR_ACC_SENSITIVITY_FS_16G  0.488f

#define LSM6DSR_GYRO_SENSITIVITY_FS_125DPS    4.370f
#define LSM6DSR_GYRO_SENSITIVITY_FS_250DPS    8.750f
#define LSM6DSR_GYRO_SENSITIVITY_FS_500DPS   17.500f
#define LSM6DSR_GYRO_SENSITIVITY_FS_1000DPS  35.000f
#define LSM6DSR_GYRO_SENSITIVITY_FS_2000DPS  70.000f
#define LSM6DSR_GYRO_SENSITIVITY_FS_4000DPS 140.000f

typedef struct
{
	lsm6dsr_odr_xl_t acc_odr;
	lsm6dsr_odr_g_t gyro_odr;
	
	uint8_t acc_is_enabled;
	uint8_t gyro_is_enabled;
}LSM6DSR;

typedef enum
{
	LSM6DSR_OK = 0,
	LSM6DSR_ERROR = 1
} LSM6DSRStatusTypeDef;

struct imuData_s {
	int32_t accelerometer[3];
	int32_t gyroscope[3];
};

typedef struct imuData_s imuData_t;

/**** TWI Parameters ****/
#define TWI_SPEED			400000
#define BOARD_ID_TWI_IMU    ID_TWI0
#define BOARD_BASE_TWI_IMU  TWI0
uint8_t data_received;
uint8_t data_transmitted;

/** LSM6DSR Registers **/
#define IMU_BUS_ADDR	0x6B // 0x6B (based on SDO/SA0 pin)
#define IMU_WHO_AM_I	0x0F
#define STATUS_REG		0x1E
lsm6dsr_ctx_t reg_ctx;

/** TWI & LSM6DSR Functions **/
int32_t LSM6DSR_io_read(void *handle, uint8_t ReadAddr, uint8_t *pBuffer, uint16_t nBytesToRead);
int32_t LSM6DSR_io_write(void *handle, uint8_t WriteAddr, uint8_t *pBuffer, uint16_t nBytesToWrite);

void initializeImu(void);
void getImuData(imuData_t* imuData);

#endif /* LSM6DSR_H_ */