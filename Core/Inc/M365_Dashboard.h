/*
 * M365_Dashboard.h
 *
 *  Created on: Nov 27, 2021
 *      Author: stancecoke
 *  Modified for ESP32 Bridge on USART3 (Half-Duplex)
 */

#ifndef INC_M365_DASHBOARD_H_
#define INC_M365_DASHBOARD_H_

#include "main.h"

void M365Dashboard_init(void);

void search_DashboardMessage(MotorState_t *MS, MotorParams_t *MP);

void send_DashboardMessage(uint8_t page, MotorState_t *MS, MotorParams_t *MP);

void process_DashboardMessage(MotorState_t *MS, MotorParams_t *MP, uint8_t *message, uint8_t length);

void addCRC(uint8_t * message, uint8_t size);

int16_t checkCRC(uint8_t * message, uint8_t size);

#endif /* INC_M365_DASHBOARD_H_ */
