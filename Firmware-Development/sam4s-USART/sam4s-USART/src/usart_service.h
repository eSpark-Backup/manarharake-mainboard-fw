/*
 * usart_service.h
 *
 * Created: 4/17/2024 2:28:12 PM
 *  Author: classicComputer
 */ 


#ifndef USART_SERVICE_H_
#define USART_SERVICE_H_

#include "serial.h"

void initializeUart(void);
void usartWritePacket(uint8_t* bufPtr);

#endif /* USART_SERVICE_H_ */