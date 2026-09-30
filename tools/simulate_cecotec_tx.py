#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
========================================================================================
   SIMULADOR DE PANTALLA CECOTEC BONGO D40 XL PARA SMARTESC STM32
   Envía tramas de 15 bytes a 19200 baudios 8N1 por puerto COM (FTDI / USB-TTL)
   conectado al pin PB6 (línea de pantalla) de la controladora M365.
========================================================================================
"""

import sys
import time
import serial
import serial.tools.list_ports

def calc_xor(pkt):
    x = 0
    for b in pkt[2:14]:
        x ^= b
    return x

def make_packet(throttle_raw, brake_val=0, mode=1, lock_bit=False):
    # Formato: 5B A5 ID B03 B04 B05 B06 B07 B08 B09 B10 B11 B12 SUB XOR
    pkt = bytearray(15)
    pkt[0] = 0x5B
    pkt[1] = 0xA5
    pkt[2] = mode # 1 = Eco, 2 = Confort, 3 = Sport, 0 = Parking/Lock
    pkt[3] = throttle_raw & 0xFF
    pkt[4] = (throttle_raw >> 8) & 0xFF
    if brake_val > 255:
        # Modo 16-bit ADC (ej: 685..2870 mV)
        pkt[5] = brake_val & 0xFF
        pkt[6] = (brake_val >> 8) & 0xFF
    else:
        # Modo 8-bit analógico (0..255)
        pkt[5] = brake_val & 0xFF
        pkt[6] = 0x00
    pkt[7] = 0x1C if brake_val > 20 else 0x0D
    pkt[8] = 0x02 if lock_bit else 0x00
    pkt[9] = 0x00
    pkt[10] = 0x00
    pkt[11] = 0x00
    pkt[12] = 0x00
    pkt[13] = 0x47 # Sub ID
    pkt[14] = calc_xor(pkt)
    return bytes(pkt)

def main():
    ports = [p for p in serial.tools.list_ports.comports() if "bluetooth" not in p.description.lower()]
    default_port = ports[0].device if ports else "COM3"
    port_name = sys.argv[1].upper() if len(sys.argv) > 1 else default_port

    print("=" * 70)
    print("  SIMULADOR DE PANTALLA CECOTEC BONGO D40 (19200 BAUD)")
    print(f"  Abriendo puerto: {port_name}")
    print("=" * 70)

    try:
        ser = serial.Serial(port_name, baudrate=19200, timeout=0.05)
    except Exception as e:
        print(f"Error abriendo {port_name}: {e}")
        return

    print("Transmisión activa a ~60 Hz (16.6 ms).")
    print("Teclas: [0] Reposo | [1] 25% Gas | [2] 50% Gas | [3] 100% Gas | [B] Freno | [Ctrl+C] Salir")

    current_gas = 685 # reposo
    current_brake = False

    try:
        while True:
            pkt = make_packet(current_gas, current_brake)
            ser.write(pkt)
            time.sleep(0.016)
    except KeyboardInterrupt:
        print("\nSimulación finalizada.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
