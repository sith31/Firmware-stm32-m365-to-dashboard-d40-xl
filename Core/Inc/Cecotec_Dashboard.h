/*
 * Cecotec_Dashboard.h
 *
 *  Integration for Cecotec Bongo D40 XL Connected Display
 *  UART 19200 Baud 8N1 | Fixed 15-byte frame | Header 0x5B 0xA5 | Checksum: XOR
 */

#ifndef INC_CECOTEC_DASHBOARD_H_
#define INC_CECOTEC_DASHBOARD_H_

#include "main.h"
#include "config.h"
#include "stm32f1xx_hal.h"

#define CECOTEC_FRAME_LEN   15
#define CECOTEC_HEADER_0    0x5B
#define CECOTEC_HEADER_1    0xA5

void CecotecDashboard_init(UART_HandleTypeDef huart);
void search_CecotecMessage(MotorState_t *MS, MotorParams_t *MP, UART_HandleTypeDef huart);
void process_CecotecMessage(MotorState_t *MS, MotorParams_t *MP, const uint8_t *pkt);
uint8_t calculate_Cecotec_XOR(const uint8_t *pkt, uint8_t len);
void send_CecotecTelemetry(MotorState_t *MS, MotorParams_t *MP, UART_HandleTypeDef huart);

#endif /* INC_CECOTEC_DASHBOARD_H_ */
