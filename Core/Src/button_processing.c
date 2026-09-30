/*
 * m365
 *
 * Copyright (c) 2021 Francois Deslandes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "button_processing.h"
#include "main.h"
#include "config.h"


volatile uint32_t main_loop_counter;
static uint8_t power_button_state = 0;

uint8_t buttonState() {
    static const uint32_t DEBOUNCE_MILLIS = 20 ;
    bool buttonstate = HAL_GPIO_ReadPin( PWR_BTN_GPIO_Port, PWR_BTN_Pin ) == GPIO_PIN_SET ;
    uint32_t buttonstate_ts = HAL_GetTick() ;

    uint32_t now = HAL_GetTick() ;
    if( now - buttonstate_ts > DEBOUNCE_MILLIS )
    {
        if( buttonstate != (HAL_GPIO_ReadPin( PWR_BTN_GPIO_Port, PWR_BTN_Pin ) == GPIO_PIN_SET))
        {
            buttonstate = !buttonstate ;
            buttonstate_ts = now ;
        }
    }
    return buttonstate ;
}

eButtonEvent getButtonEvent()
{
    static const uint32_t MULTI_GAP_MILLIS_MAX 	= 350;
    static const uint32_t SINGLE_PRESS_MILLIS_MAX 	= 700;
    static const uint32_t LONG_PRESS_MILLIS_MAX 	= 4000;

    static uint32_t button_down_ts = 0 ;
    static uint32_t button_up_ts = 0 ;
    static uint8_t click_count = 0 ;
    static bool button_down = false ;

    eButtonEvent button_event = NO_PRESS ;
    uint32_t now = HAL_GetTick() ;
    uint8_t cur_state = buttonState();

    if( button_down != cur_state ) {
        button_down = cur_state ;
        if( button_down ) {
            button_down_ts = now ;
        } else {
            button_up_ts = now ;
            uint32_t press_duration = now - button_down_ts;
            if (press_duration < SINGLE_PRESS_MILLIS_MAX) {
                click_count++;
                // Instant trigger as soon as the 5th rapid click is released
                if (click_count >= 5) {
                    click_count = 0;
                    return FIVE_PRESS;
                }
            }
        }
    }

    if (!button_down && click_count > 0 && (now - button_up_ts > MULTI_GAP_MILLIS_MAX)) {
    	if (click_count == 1) {
    		button_event = SINGLE_PRESS;
    	} else if (click_count == 2) {
    		button_event = DOUBLE_PRESS;
    	} else if (click_count == 3) {
    		button_event = TRIPLE_PRESS;
    	} else if (click_count >= 5) {
    		button_event = FIVE_PRESS;
    	}
    	click_count = 0;
	} else if (button_down && now - button_down_ts >= SINGLE_PRESS_MILLIS_MAX && now - button_down_ts <= LONG_PRESS_MILLIS_MAX) {
		click_count = 0;
		button_event = LONG_PRESS ;
	} else if (button_down && now - button_down_ts > LONG_PRESS_MILLIS_MAX) {
		click_count = 0;
		button_event = VERY_LONG_PRESS ;
	}

    return button_event ;
}

void checkButton(MotorParams_t *MP,MotorState_t *MS) {
	/* Infinite loop */
	  if(MS->shutdown>85&&(MS->mode>>4)) power_control(DEV_PWR_OFF);
	  if(main_loop_counter > 25){
			switch( getButtonEvent() ){
				  case NO_PRESS : break ;
				  case SINGLE_PRESS : {
					  MS->light = !MS->light;
					 // commands_printf("SINGLE_PRESS");
				  } break ;
				  case VERY_LONG_PRESS :   {
					  MS->mode &= ~(1 << 4); //clear "off" (bit 4)
					  MS->shutdown=0;
					  autodetect();
					 // commands_printf("LONG_PRESS");
				  } break ;
				  case LONG_PRESS :		{
					  MS->mode |= (1 << 4); //set "off" (bit 4)

					  if(MS->shutdown==0){
						  MS->shutdown=1;
						  MS->beep = 1;
					  }

				  } break ;

				  case DOUBLE_PRESS : {
					 // commands_printf("DOUBLE_PRESS");
					  MS->mode=MS->mode+2;
					  if(MS->mode>4)MS->mode=0;
					  set_mode(MP,MS);
				  } break ;

				  case FIVE_PRESS : {
					  // Toggle Parking / Lock Mode
					  MS->parking_locked = !MS->parking_locked;
					  MS->beep = 1; // Acoustic confirmation
				  } break ;

				  default: break ;
			 }
		}
		main_loop_counter++;



}

void PWR_init() {
	/* Check button pressed state at startup */
	power_button_state = buttonState();

    /* Power ON board temporarily, ultimate decision to keep hardware ON or OFF is made later */
	power_control(DEV_PWR_ON);

	}

void power_control(uint8_t pwr)
{
	if(pwr == DEV_PWR_ON) {
		/* Turn the PowerON line high to keep the board powered on even when
		 * the power button is released
		 */
		HAL_GPIO_WritePin(TPS_ENA_GPIO_Port, TPS_ENA_Pin, GPIO_PIN_SET);
	} else if(pwr == DEV_PWR_OFF) {

		//motors_free(0, NULL);
		//sleep_x_ticks(2000);
		//stop_motors();


		while(HAL_GPIO_ReadPin(PWR_BTN_GPIO_Port, PWR_BTN_Pin));
		HAL_GPIO_WritePin(TPS_ENA_GPIO_Port, TPS_ENA_Pin, GPIO_PIN_RESET);
		while(1);
	} else if(pwr == DEV_PWR_RESTART) {

		//motors_free(0, NULL);
		//sleep_x_ticks(2000);
		//stop_motors();

		/* Restart the system */
		NVIC_SystemReset();
	}
}

void set_mode(MotorParams_t *MP, MotorState_t *MS){

	switch( MS->mode & 0x07){ //look only on the lowest 3 bits
		case eco :{
			MP->phase_current_limit=PH_CURRENT_MAX_ECO/CAL_I;
			MP->speed_limit=SPEEDLIMIT_ECO;

			} break ;
		case normal :{
			MP->phase_current_limit=PH_CURRENT_MAX_NORMAL/CAL_I;
			MP->speed_limit=SPEEDLIMIT_NORMAL;

			} break ;
		case sport :{
			MP->phase_current_limit=PH_CURRENT_MAX_SPORT/CAL_I;
			MP->speed_limit=SPEEDLIMIT_SPORT;

			} break ;

	}
	calculate_tic_limits();
}


