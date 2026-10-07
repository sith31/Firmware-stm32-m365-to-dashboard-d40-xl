/*
 * Cecotec_Dashboard.c
 *
 *  Integration for Cecotec Bongo D40 XL Connected Display
 *  Protocol: 19200 Baud 8N1 | Fixed 15-byte frame | Header 0x5B 0xA5 | Checksum: XOR
 */

#include "Cecotec_Dashboard.h"
#include "main.h"
#include "config.h"
#include "print.h"
#include "button_processing.h"
#include <string.h>

static uint8_t ui8_rx_buffer[128];
static uint8_t ui8_packet[CECOTEC_FRAME_LEN];
static uint16_t ui16_old_ptr = 0;
static uint32_t ui32_last_valid_tick = 0;

uint8_t calculate_Cecotec_XOR(const uint8_t *pkt, uint8_t len) {
	uint8_t xor_chk = 0;
	// XOR of payload bytes [02] through [len - 2]
	for (uint8_t i = 2; i < len - 1; i++) {
		xor_chk ^= pkt[i];
	}
	return xor_chk;
}

void CecotecDashboard_init(UART_HandleTypeDef huart) {
	memset(ui8_rx_buffer, 0, sizeof(ui8_rx_buffer));
	ui16_old_ptr = 0;
	ui32_last_valid_tick = HAL_GetTick();

	if (HAL_UART_Receive_DMA(&huart, (uint8_t*) ui8_rx_buffer, sizeof(ui8_rx_buffer)) != HAL_OK) {
		Error_Handler();
	}
}

void search_CecotecMessage(MotorState_t *MS, MotorParams_t *MP, UART_HandleTypeDef huart) {
	uint32_t now = HAL_GetTick();

	// Safety Watchdog: If no valid packet received for more than 200 ms, cut motor power
	if ((now - ui32_last_valid_tick) > 200) {
		MS->i_q_setpoint_temp = 0;
		MS->brake_active = false;

		// If disconnected for more than 1000 ms, re-arm DMA reception to recover from framing glitches
		if ((now - ui32_last_valid_tick) > 1000) {
			CLEAR_BIT(DMA1_Channel5->CCR, DMA_CCR_EN);
			DMA1_Channel5->CNDTR = sizeof(ui8_rx_buffer);
			SET_BIT(DMA1_Channel5->CCR, DMA_CCR_EN);
			HAL_UART_Receive_DMA(&huart, (uint8_t*) ui8_rx_buffer, sizeof(ui8_rx_buffer));
			ui16_old_ptr = 0;
			ui32_last_valid_tick = now - 250; // give next chance
		}
	}

	uint16_t current_ptr = sizeof(ui8_rx_buffer) - (uint16_t)(DMA1_Channel5->CNDTR);

	// Parse stream in circular buffer
	while (ui16_old_ptr != current_ptr) {
		// Check for 0x5B header
		if (ui8_rx_buffer[ui16_old_ptr] == CECOTEC_HEADER_0) {
			uint16_t next_idx = (ui16_old_ptr + 1) % sizeof(ui8_rx_buffer);

			// Check for 0xA5 second header byte
			if (ui8_rx_buffer[next_idx] == CECOTEC_HEADER_1) {
				// Calculate available bytes in circular buffer from ui16_old_ptr to current_ptr
				uint16_t available;
				if (current_ptr >= ui16_old_ptr) {
					available = current_ptr - ui16_old_ptr;
				} else {
					available = (sizeof(ui8_rx_buffer) - ui16_old_ptr) + current_ptr;
				}

				if (available >= CECOTEC_FRAME_LEN) {
					// Copy 15 bytes into linear packet buffer
					for (uint8_t i = 0; i < CECOTEC_FRAME_LEN; i++) {
						ui8_packet[i] = ui8_rx_buffer[(ui16_old_ptr + i) % sizeof(ui8_rx_buffer)];
					}

					// Validate XOR checksum
					uint8_t calculated_xor = calculate_Cecotec_XOR(ui8_packet, CECOTEC_FRAME_LEN);
					if (calculated_xor == ui8_packet[CECOTEC_FRAME_LEN - 1]) {
						// Valid frame detected!
						process_CecotecMessage(MS, MP, ui8_packet);
						ui32_last_valid_tick = HAL_GetTick();

						// Advance pointer past this frame
						ui16_old_ptr = (ui16_old_ptr + CECOTEC_FRAME_LEN) % sizeof(ui8_rx_buffer);
						continue;
					}
				} else {
					// Not enough bytes yet for a complete frame; wait for next DMA chunk
					break;
				}
			}
		}
		ui16_old_ptr = (ui16_old_ptr + 1) % sizeof(ui8_rx_buffer);
	}
}

void process_CecotecMessage(MotorState_t *MS, MotorParams_t *MP, const uint8_t *pkt) {
	// Mode synchronization & parking check from Cecotec display (Byte[02] and Byte[08])
	uint8_t mode_cmd = pkt[2];
	if (mode_cmd == 0 || mode_cmd == 4 || mode_cmd == 5 || mode_cmd == 'P' || (pkt[8] & 0x02)) {
		MS->parking_locked = true;
	}

	// Parking Lock Interlock: If active, throttle is forced to 0 and motor resists any wheel movement
	// e-ABS motor braking ACTIVE (energy dissipated in motor, NOT sent to battery - REGEN_CURRENT_MAX=0)
	if (MS->parking_locked) {
		if (MS->Speed > 0) {
			// Anti-theft: gentle motor braking to resist movement (e-ABS)
			MS->i_q_setpoint_temp = -MP->regen_current;
			MS->brake_active = true;
			MS->beep = 1; // Sound alarm when moved in locked state
		} else {
			// Standstill rest: prevent continuous power draw and coil heating
			MS->i_q_setpoint_temp = 0;
			MS->brake_active = false;
		}
		return;
	}

	// 1. Throttle extraction: Byte[03] and Byte[04] (Little-Endian 16-bit word)
	uint16_t raw_throttle = (uint16_t)pkt[3] | ((uint16_t)pkt[4] << 8);

	// 2. Progressive e-ABS Brake extraction (MOTOR BRAKING ONLY - no battery charging):
	//    The brake sensor on the handlebar is an analog/progressive Hall sensor.
	//    It can arrive as:
	//    - 16-bit ADC value: pkt[5] | (pkt[6] << 8), similar to throttle (685 to 2870 mV)
	//    - 8-bit scaled value: pkt[5] (0 to 255)
	//    - Lever contact bit: pkt[7] bit 4 (0x10)
	uint16_t raw_brake_16 = (uint16_t)pkt[5] | ((uint16_t)pkt[6] << 8);
	uint8_t brake_val_8 = pkt[5];
	bool brake_flag_bit = ((pkt[7] & 0x10) != 0);

	int32_t brake_demand = 0; // Proportional braking demand: 0 to MP->regen_current

	if (pkt[6] > 0 && raw_brake_16 >= 500) {
		// 16-bit ADC reading mode (685 mV resting to 2870 mV full squeeze)
		if (raw_brake_16 >= (CECOTEC_THROTTLE_MIN + CECOTEC_THROTTLE_DEADBAND)) {
			if (raw_brake_16 > CECOTEC_THROTTLE_MAX) {
				raw_brake_16 = CECOTEC_THROTTLE_MAX;
			}
			brake_demand = map(raw_brake_16,
							   CECOTEC_THROTTLE_MIN + CECOTEC_THROTTLE_DEADBAND,
							   CECOTEC_THROTTLE_MAX,
							   0,
							   MP->regen_current);
		}
	} else {
		// 8-bit proportional mode (0 in resting position up to 255 at full squeeze)
		// Deadband: below 20 is resting
		if (brake_val_8 > 20) {
			if (brake_val_8 > 240) {
				brake_val_8 = 240;
			}
			brake_demand = map(brake_val_8, 20, 240, 0, MP->regen_current);
		}
	}

	// If the contact switch flag is active but analog reading is at initial touch,
	// apply a minimum gentle initial regen force (~15% of max regen)
	if (brake_flag_bit && brake_demand < (MP->regen_current >> 3)) {
		brake_demand = (MP->regen_current >> 3);
	}

	if (brake_demand > 0 || brake_flag_bit) {
		MS->brake_active = true;

		// e-ABS Motor Braking (D40 stock behavior):
		// Active when vehicle is rolling (Speed > 2 km/h).
		// Proportional to the physical position of the brake lever.
		// NOTE: REGEN_CURRENT_MAX=0 in config.h prevents battery charging via XT60
		if (MS->Speed > 2) {
			int32_t regen = brake_demand;

			// Battery overvoltage protection: ramp down regen strength near max battery voltage
			if (MS->Voltage > (BATTERYVOLTAGE_MAX - 1000)) {
				regen = map(MS->Voltage, BATTERYVOLTAGE_MAX - 1000, BATTERYVOLTAGE_MAX, regen, 0);
			}

			MS->i_q_setpoint_temp = -regen;  // Negative torque = motor braking (e-ABS)
		} else {
			// Smooth standstill transition: cancel motor braking below 2 km/h
			MS->i_q_setpoint_temp = 0;
		}
	} else {
		MS->brake_active = false;

		// Mode synchronization from Cecotec display (Byte[02]: 1=Eco/Peatón, 2=Normal/D, 3=Sport)
		uint8_t mode_cmd = pkt[2];
		if (mode_cmd == 1 && (MS->mode & 0x07) != eco) {
			MS->mode = (MS->mode & ~0x07) | eco;
			set_mode(MP, MS);
		} else if (mode_cmd == 2 && (MS->mode & 0x07) != normal) {
			MS->mode = (MS->mode & ~0x07) | normal;
			set_mode(MP, MS);
		} else if (mode_cmd == 3 && (MS->mode & 0x07) != sport) {
			MS->mode = (MS->mode & ~0x07) | sport;
			set_mode(MP, MS);
		}

		// Progressive throttle mapping with exponential curve and Kick-to-start
		if (raw_throttle >= (CECOTEC_THROTTLE_MIN + CECOTEC_THROTTLE_DEADBAND)) {
			if (raw_throttle > CECOTEC_THROTTLE_MAX) {
				raw_throttle = CECOTEC_THROTTLE_MAX;
			}
			int32_t norm_throttle = map(raw_throttle,
										CECOTEC_THROTTLE_MIN + CECOTEC_THROTTLE_DEADBAND,
										CECOTEC_THROTTLE_MAX,
										0,
										1000);

#if (THROTTLE_CURVE_EXP == 1)
			// Quadratic progressive curve: smoother low-speed control while delivering full peak power
			norm_throttle = (350 * norm_throttle + 650 * ((norm_throttle * norm_throttle) / 1000)) / 1000;
#endif

			int32_t target_iq = (norm_throttle * MP->phase_current_limit) / 1000;

			// Safety Kick-to-start: require minimal speed before motor torque activates
#if (KICK_TO_START_KMH > 0)
			if (MS->Speed < KICK_TO_START_KMH) {
				target_iq = 0;
			}
#endif

			// Speed Limiter & Absolute 25 km/h cap (never exceed 25 km/h)
			uint8_t effective_limit = MP->speed_limit;
			if (effective_limit > 25) {
				effective_limit = 25;
			}

			if (MS->Speed >= effective_limit) {
				target_iq = 0;
			} else if (MS->Speed > (effective_limit - 2)) {
				// Smooth proportional taper over the last 2 km/h to prevent abrupt cutoff
				target_iq = map(MS->Speed, effective_limit - 2, effective_limit, target_iq, 0);
			}

			MS->i_q_setpoint_temp = target_iq;
		} else {
			MS->i_q_setpoint_temp = 0;
		}
	}
}

static uint8_t ui8_tx_buffer[CECOTEC_FRAME_LEN];
static uint8_t ui8_tx_seq = 0;

void send_CecotecTelemetry(MotorState_t *MS, MotorParams_t *MP, UART_HandleTypeDef huart) {
	ui8_tx_buffer[0] = CECOTEC_HEADER_0;
	ui8_tx_buffer[1] = CECOTEC_HEADER_1;
	ui8_tx_buffer[2] = 0x02; // Response ID: ESC telemetry frame

	// Speed in tenths of km/h: Byte[03] (Low), Byte[04] (High)
	uint16_t speed_x10 = (uint16_t)(MS->Speed * 10);
	ui8_tx_buffer[3] = speed_x10 & 0xFF;
	ui8_tx_buffer[4] = (speed_x10 >> 8) & 0xFF;

	// Battery SOC (0 to 100%): Byte[05]
	int32_t soc = map(MS->Voltage, BATTERYVOLTAGE_MIN, BATTERYVOLTAGE_MAX, 0, 100);
	if (soc < 0) soc = 0;
	if (soc > 100) soc = 100;
	ui8_tx_buffer[5] = (uint8_t)soc;

	// Battery Voltage in cV (e.g. 4080 cV = 40.80V): Byte[06] (Low), Byte[07] (High)
	uint16_t volt_cv = (uint16_t)(MS->Voltage / 10);
	ui8_tx_buffer[6] = volt_cv & 0xFF;
	ui8_tx_buffer[7] = (volt_cv >> 8) & 0xFF;

	// Temperature in °C: Byte[08]
	ui8_tx_buffer[8] = (uint8_t)MS->Temperature;

	// Active Mode: Byte[09] (1 = Eco, 2 = Confort, 3 = Sport, 0 = Parking/Lock)
	uint8_t active_mode = 2;
	if (MS->parking_locked) {
		active_mode = 0; // Display shows 'P' (Parking Mode)
	} else if ((MS->mode & 0x07) == eco) {
		active_mode = 1;
	} else if ((MS->mode & 0x07) == sport) {
		active_mode = 3;
	}
	ui8_tx_buffer[9] = active_mode;

	// Lights, Brake, Lock & Beep status: Byte[10]
	uint8_t status_flags = 0;
	if (MS->light) status_flags |= 0x01;
	if (MS->brake_active) status_flags |= 0x02;
	if (MS->parking_locked) status_flags |= 0x04; // Bit 2: Locked
	if (MS->beep) {
		status_flags |= 0x08; // Bit 3: Buzzer beep trigger
		MS->beep = 0;
	}
	ui8_tx_buffer[10] = status_flags;

	// Error code: Byte[11]
	ui8_tx_buffer[11] = (uint8_t)MS->error_state;

	// Current in Amperes: Byte[12]
	int16_t curr_amps = (int16_t)(abs(MS->Battery_Current) / 1000);
	ui8_tx_buffer[12] = (uint8_t)curr_amps;

	// Sequence counter: Byte[13]
	ui8_tx_buffer[13] = ui8_tx_seq++;

	// Checksum XOR: Byte[14]
	ui8_tx_buffer[14] = calculate_Cecotec_XOR(ui8_tx_buffer, CECOTEC_FRAME_LEN);

	// Transmit telemetry frame to display
	HAL_UART_Transmit_DMA(&huart, ui8_tx_buffer, CECOTEC_FRAME_LEN);
}