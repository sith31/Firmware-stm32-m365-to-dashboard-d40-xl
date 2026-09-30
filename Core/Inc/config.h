/*
 * config.h
 *
 *  Automatically created by Lishui Parameter Configurator
 *  Author: stancecoke
 */

#ifndef CONFIG_H_
#define CONFIG_H_
#include "stdint.h"

//Dangerzone, do not touch!!
#define DISPLAY_TYPE_M365DASHBOARD (1<<1)
#define DISPLAY_TYPE_DEBUG (1<<0)							// For ASCII-Output in Debug mode);
#define DISPLAY_TYPE_CECOTEC (1<<2)

#define TRIGGER_OFFSET_ADC 50
#define TRIGGER_DEFAULT 2020
#define _T 2028

#define SPEEDFILTER 3

//#define ADCTHROTTLE
//#define FAST_LOOP_LOG
//#define DISABLE_DYNAMIC_ADC

// choose your display here
#define DISPLAY_TYPE DISPLAY_TYPE_CECOTEC

// Cecotec Bongo D40 calibration settings
#define CECOTEC_BAUDRATE 19200
#define CECOTEC_THROTTLE_MIN 685   // Raw mV in resting position (~0.685V)
#define CECOTEC_THROTTLE_MAX 2870  // Raw mV at 100% throttle (~2.870V)
#define CECOTEC_THROTTLE_DEADBAND 40 // Deadband threshold above min

// Safety: Kick-to-start minimum speed in km/h (set 0 to disable)
#define KICK_TO_START_KMH 3

// Acceleration curve & slew rate ramp
#define ACCEL_RAMP_STEP 15         // Max Iq step up per loop iteration (~300 mA per step)
#define THROTTLE_CURVE_EXP 1       // 1 = Progressive exponential/quadratic curve, 0 = Linear

// Thermal protection settings (MOSFET NTC in °C)
#define OVERTEMP_WARN_DEG   70     // Start linear current rollback at 70°C
#define OVERTEMP_CUTOFF_DEG 85     // Cut motor power completely at 85°C
#define OVERTEMP_RECOVER_DEG 65    // Recover full power once cooled below 65°C

// calibration factors for voltage and current
#define CAL_BAT_V 14 	// ADC counts * CAL_BAT_V = Battery voltage in mV
#define CAL_I 38		// ADC counts * CAL_I = current in mA

// gains for PI controls
#define P_FACTOR_I_Q 100
#define I_FACTOR_I_Q 2
#define P_FACTOR_I_D 100
#define I_FACTOR_I_D 10

// min and max values of throttle and brake signals in ADC counts
#define THROTTLEOFFSET 45
#define THROTTLEMAX 175
#define BRAKEOFFSET 50
#define BRAKEMAX 190

// parameters for speed calculation
#define WHEEL_CIRCUMFERENCE 800 // 800 mm para rueda de 10" (Cecotec Bongo D40 XL). [690 mm para 8.5" M365]
#define GEAR_RATIO 15           // 15 pares de polos (30 imanes) para motor de cubo directo Cecotec / M365

// speed limits for individual modes in kph
#define SPEEDLIMIT_ECO 6        // Modo Peatón (máx 6 km/h)
#define SPEEDLIMIT_NORMAL 16    // Modo D / Eco (máx 16 km/h)
#define SPEEDLIMIT_SPORT 25     // Modo Sport (máx 25 km/h - estricto legal)

// motor current limits for invividual modes in mA, see default settings at https://max.cfw.sh/#
#define PH_CURRENT_MAX_ECO 16000
#define PH_CURRENT_MAX_NORMAL 28000
#define PH_CURRENT_MAX_SPORT 55000

// motor current limit for regen in mA - e-ABS motor braking enabled (NO battery charging)
// Energy dissipated in motor windings, not sent back to battery (D40 BMS blocks XT60 charging)
#define REGEN_CURRENT 15000

// maximum current for flux weakening in mA
#define FW_CURRENT_MAX 18000 //max id

// maximum battery currents in mA
#define BATTERYCURRENT_MAX 14500
#define REGEN_CURRENT_MAX 0  // Battery charging via regen DISABLED (XT60 doesn't support charging)

// battery voltage limits in mV
#define BATTERYVOLTAGE_MIN 33000
#define BATTERYVOLTAGE_MAX 42000


// motor spinning direction
#define REVERSE 1 //1 for original M365 motor

// settings for speed PLL (angle estimation)
#define SPEED_PLL 1 //1 for using PLL, 0 for angle extrapolation
#define P_FACTOR_PLL 10 //7 for original M365 motor
#define I_FACTOR_PLL 10 //7 for original M365 motor

#endif /* CONFIG_H_ */
