Esto hice del adc /*
 *
 *
 * PRUEBA DE MODULO: PWM A MOTORES USANDO TIMER1
 *
 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "lpc17xx_timer.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"

#include <cr_section_macros.h>

void Timer1_cfg(void);
void Pin_cfg(void);


int main(void) {

Pin_cfg();
Timer1_cfg();


    return 0 ;
}

void Pin_cfg(void){
	PINSEL_CFG_T pinsel_cfg = {
			port: PORT_0,
			pin: PIN_0,
			func: PINSEL_FUNC_00,
			mode: PINSEL_TRISTATE,
			openDrain: DISABLE
	};

	PINSEL_ConfigPin(&pinsel_cfg);
	GPIO_SetDir(PORT_0,1<<0,GPIO_OUTPUT);
	GPIO_SetPinState(PORT_0,1<<0,SET);
}

void Timer1_cfg(void){

	TIM_TIMERCFG_T timer1cfg = {prescaleOpt: TIM_US, prescaleValue: 100};  // DEFINIR SI VAMOS A USAR PRESCALER
	TIM_MATCHCFG_T match0cfg;
	TIM_MATCHCFG_T match1cfg;

	match0cfg.channel = 0;			// USO MAT1.0 para T
	match0cfg.intEn = DISABLE;
	match0cfg.stopEn = DISABLE;
	match0cfg.resetEn = ENABLE;
	match0cfg.extOpt = 0;
	match0cfg.matchValue = 100;

	match1cfg.channel = 1;			// USO MAT1.1 para el duty cycle
	match1cfg.intEn = ENABLE;
	match1cfg.stopEn = DISABLE;
	match1cfg.resetEn = ENABLE;
	match1cfg.extOpt = 0;
	match1cfg.matchValue = 50;
	//ES EN ESTE CASO QUE MATCHVALUE DEBE VALES LO QUE SE INGRESE DEL UART

	NVIC_EnableIRQ(TIMER1_IRQn);

	TIM_InitTimer(LPC_TIM1, &timer1cfg);
	TIM_ConfigMatch(LPC_TIM1, &match0cfg);
	TIM_ConfigMatch(LPC_TIM1, &match1cfg);
	TIM_Enable(LPC_TIM1);
}

void TIMER1_IRQHandler(void){
	if(TIM_GetIntStatus(LPC_TIM1, TIM_MR1_INT) == SET){
		GPIO_ClearPins(PORT_0,1<<0);
		TIM_ClearIntPending(LPC_TIM1, TIM_MR1_INT);
	}
}
