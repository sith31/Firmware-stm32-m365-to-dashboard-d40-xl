# SmartESC STM32 v3 - Modificaciones para Cecotec Bongo D40 XL

## Resumen de cambios

Este fork adapta el firmware SmartESC (originalmente para Xiaomi M365) al **Cecotec Bongo D40 XL Connected** con pantalla Cecotec (UART 19200 8N1, trama 0x5B 0xA5).

---

## 🔋 Configuración KERS / Freno Regenerativo

### Estado actual: **e-ABS ACTIVO · Carga batería DESACTIVADA**

El Bongo D40 de serie tiene **freno motor e-ABS** (frena usando el motor como generador) pero **NO carga la batería por el conector XT60**. El BMS solo permite carga por el puerto de carga dedicado.

```c
// Core/Inc/config.h
#define REGEN_CURRENT      15000   // Corriente de fase motor para freno e-ABS (15A)
#define REGEN_CURRENT_MAX  0       // Corriente de BATERÍA = 0 (NO cargar por XT60)
```

### Comportamiento resultante

| Función | Estado | Detalle |
|---------|--------|---------|
| **Freno motor (e-ABS)** | ✅ ACTIVO | Par negativo progresivo al accionar maneta |
| **Carga batería por frenado** | ❌ BLOQUEADA | `REGEN_CURRENT_MAX = 0` impide corriente hacia XT60 |
| **Energía de frenado** | 🔥 Disipada | Calor en bobinados + MOSFETs (comportamiento stock) |
| **Parking lock anti-robo** | ✅ ACTIVO | Freno motor suave si mueven patinete bloqueado |
| **Luz freno trasera** | ✅ ACTIVA | Se enciende al detectar maneta de freno |

---

## 🔧 Cómo reactivar KERS (carga de batería por frenado)

**SOLO si verificas que TU batería D40 SÍ admite carga por XT60** (algunas revisiones de BMS lo permiten, o si cambiaste el BMS por uno que sí lo haga).

### Opción A: Reactivación completa (recomendada si BMS lo permite)

```c
// Core/Inc/config.h - Cambiar estas dos líneas:
#define REGEN_CURRENT      20000   // Corriente motor frenado (20A, valor original)
#define REGEN_CURRENT_MAX  10000   // Corriente batería carga (10A, valor original)
```

### Opción B: Carga conservadora (si BMS es sensible)

```c
// Core/Inc/config.h
#define REGEN_CURRENT      15000   // Motor: 15A (suave)
#define REGEN_CURRENT_MAX  5000    // Batería: 5A (conservador)
```

### Opción C: Solo e-ABS sin carga (configuración actual - segura)

```c
// Core/Inc/config.h - VALORES ACTUALES
#define REGEN_CURRENT      15000   // Motor: freno e-ABS activo
#define REGEN_CURRENT_MAX  0       // Batería: 0A = SIN CARGA
```

---

## ⚠️ Importante: Verificar antes de activar carga

Antes de poner `REGEN_CURRENT_MAX > 0`:

1. **Mide con multímetro** en el XT60 mientras frenas con el firmware original Cecotec
2. **O usa la app Web Bluetooth** del proyecto (`scooter_alarm_c3_app`) para ver `Corriente` en modo carga
3. Si ves corriente **negativa** (entrante) al frenar → **SÍ carga por XT60** → puedes subir `REGEN_CURRENT_MAX`
4. Si la corriente se queda en **0** al frenar → **NO carga por XT60** → déjalo en `0`

---

## 📝 Otros cambios en este fork

### `Core/Inc/config.h`
- `DISPLAY_TYPE_CECOTEC` seleccionado
- Baudrate 19200 para UART Cecotec
- Límites velocidad: Eco 6 / Normal 16 / Sport 25 km/h
- Corrientes fase: Eco 16A / Normal 28A / Sport 55A
- Protección térmica: aviso 70°C / corte 85°C / recuperación 65°C

### `Core/Src/Cecotec_Dashboard.c`
- Parser completo protocolo Cecotec (15 bytes, XOR checksum)
- Extracción gas (bytes 3-4), freno (bytes 5-6 + bit 4 byte 7), modo (byte 2)
- e-ABS progresivo: mapea posición maneta → torque motor negativo
- Protección sobretensión: reduce freno si batería > 41V
- Telemetría hacia pantalla: velocidad, SOC, voltaje, temp, modo, flags

### `Core/Src/main.c`
- Parking lock usa `MP.regen_current` para anti-theft (freno motor suave)
- Integración energía: `Battery_Current` calculado desde fase (Iq, Id, Uq, Ud)
- Autodetect hall sensors al arranque (pulsación larga botón)
- KV detection automático

---

## 🏗️ Compilar y flashear

### Método 1: GitHub Actions (recomendado)
1. Fork este repo en GitHub
2. Edita `Core/Inc/config.h` online
3. Actions → "Generate Bin" → Download zip
4. Flashea con app **downG** en móvil

### Método 2: STM32CubeIDE local
1. Instala STM32CubeIDE
2. Importa proyecto (File → Import → Git → Projects from Git)
3. Edita `Core/Inc/config.h`
4. Build (icono martillo)
5. Zip generado en `tools/zip-output/`

---

## 📁 Estructura del proyecto completo

```
cecotec/
├── scooter_alarm_c3_app/          # Web App + Servidor HTTPS (ESP32-C3 companion)
│   ├── index.html                 # Dashboard 4 pestañas (Panel, Carga, Viajes, Ajustes)
│   ├── servidor_https.py          # HTTPS autofirmado pto 8443 (Web Bluetooth Android)
│   └── abrir_app.py               # HTTP simple pto 8080
│
├── scooter_alarm_c3_firmware/     # ESP32-C3 Firmware (alarma + telemetría + BLE)
│   └── main/                      # Dual UART bridge, BLE security, NVS, ADC, Power
│
└── SmartESC_STM32_v3-0.5/         # ESTE REPO - Controladora STM32 (motor)
    └── SmartESC_STM32_v3-0.5/
        ├── Core/Inc/config.h      # ← CONFIGURAR AQUÍ
        ├── Core/Src/Cecotec_Dashboard.c
        └── Core/Src/main.c
```

---

## 🔗 Proyecto complementario: ESP32-C3 Alarm + Dashboard

El firmware `scooter_alarm_c3_firmware` en este mismo repo funciona **en paralelo** con esta controladora:

- **Puente Dual UART**: ESP32 intercepta Pantalla ↔ SmartESC
- **Alarma por proximidad**: Mi Band 9 / Móvil via BLE (whitelist NVS)
- **Corte MOSFETs low-side**: Aísla masa de la controladora al bloquear
- **Web App**: Dashboard completo en móvil (HTTPS para Web Bluetooth)

Ver `scooter_alarm_c3_app/README.md` y `scooter_alarm_c3_firmware/README.md` para detalles.

---

## 📄 Licencia

Basado en SmartESC (stancecoke/Koxx3) - BSD 3-Clause / GPL según archivos originales.
Modificaciones Cecotec D40 por @pepe_ (2025).