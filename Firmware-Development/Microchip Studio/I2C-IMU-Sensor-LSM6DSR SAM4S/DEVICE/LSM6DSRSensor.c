/*
 * CFile1.c
 *
 * Created: 11/7/2022 8:07:09 PM
 *  Author: Hp
 */ 


#include "LSM6DSRSensor.h"

/**

void i2cInit(Twi *dev_i2c, uint8_t address, lsm6dsr_ctx_t* reg_ctx,void* handle) 
{
	reg_ctx->write_reg = LSM6DSR_io_write;
	reg_ctx->read_reg = LSM6DSR_io_read;
	reg_ctx->handle = (void *)handle;
	acc_is_enabled = 0U;
	gyro_is_enabled = 0U;
}


uint8_t IO_Write(Twi *dev_i2c, uint8_t* pBuffer, uint8_t RegisterAddr, uint16_t NumByteToWrite)
   {  
      if (dev_i2c) {
        //dev_i2c->beginTransmission(((uint8_t)(((address) >> 1) & 0x7F)));

        //dev_i2c->write(RegisterAddr);
        for (uint16_t i = 0 ; i < NumByteToWrite ; i++) {
          //dev_i2c->write(pBuffer[i]);
        }

        //dev_i2c->endTransmission(true);

        return 0;
      }

      return 1;
    }

int32_t LSM6DSR_io_write(uint8_t WriteAddr, uint8_t *pBuffer, uint16_t nBytesToWrite)
{
	return ( IO_Write(pBuffer, WriteAddr, nBytesToWrite) );
}

**/

