/*
 * Forma de onda: 				Escalera de 10 pisos
 * Amplitud maxima: 			Aprox. 3,3 [V]
 * Duracion de cada escalon: 	100 [uS]
 * Peroiodo de la señal(T): 	10 escalones x 100uS = 1000 uS = 1 mS = 0.001 S
 * Frecuencia de la señal(f): 	1/T = 1 / 0.001 = 1000 Hz = 1 KHz
 */

#include "LPC17xx.h"
#include "LPC17xx_dac.h"
#include "LPC17xx_timer.h"

uint16_t values[10] = {0,1,2,3,4,5,6,7,8,9};

void configDAC(void);
void configTIMER(void);
void TIMER0_IRQHandler(void)


int main(void) {

	configDAC();
	configTIMER();
    while(1) {
    }
    return 0 ;
}

void configDAC(void) {
	DAC_Init(); 			//Inicializa el periférico DAC, configura el pin P0.26 automáticamente como salida analógica
	DAC_CONVERTER_CFG_T dacDMA = {DISABLE, DISABLE, DISABLE}; //Esto es innecesario, ya que no estamos usando el DMA, por lo tanto, por default se pondra en cero
	DAC_SetBias(DAC_350uA);	//Bajo desempeño, poco consumo
	DAC_UpdateValue(0); 	//Establece la salida inicial en 0[V]. El DAC tiene una resolución de 10 bits (0 a 1023)
	DAC_ConfigDAConverterControl(&dacDMA);
}

void configTIMER(void){
	TIM_TIMERCFG_T prescalerConfig;
	prescalerConfig.prescaleOpt = TIM_US; 	//TIM_US: Modo "alto nivel". Le digo al driver: "Quiero que el Timer incremente cada X microsegundos"
	prescalerConfig.prescaleValue = 10; 	//El Timer incrementa cada 10 microsegundos (uS)

	TIM_MATCHCFG_T match0Config;
	match0Config.channel = TIM_MATCH_0;
	match0Config.intEn = ENABLE;		//La interrupcion por MATCHx esta habilitada
	match0Config.stopEn = DISABLE; 		//Para que la onda sea continua (es decir, que el Timer no se detenga en la primer interrupcion) tiene que estar deshabilitado
	match0Config.resetEn = ENABLE; 		//Al llegar a 9, el contador vuelve a 0
	match0Config.extOpt = TIM_NOTHING;	//Al saltar la interrupcion por MATCHx, no pasa nada con el estado del pin
    match0Config.matchValue = 9; 		//El Timer cuenta desde 0 hasta 9, por lo tanto, Tiempo de interrupción: 10 cuentas x 10uS = 100uS

    TIM_InitTimer  (LPC_TIM0, &prescalerConfig);
    TIM_ConfigMatch(LPC_TIM0, &match0Config);
    NVIC_EnableIRQ(TIMER0_IRQn);
    TIM_Enable(LPC_TIM0);
}

void TIMER0_IRQHandler(void){
	static uint8_t onda = 0;
	if(TIM_GetIntStatus(LPC_TIM0,TIM_MR0_INT)){ 	//TIM_GetIntStatus(LPC_TIM_TypeDef, TIM_INT)
		TIM_ClearIntPending(LPC_TIM0,TIM_MR0_INT);	//Limpia la bandera de interrupcion, para permitir la siguiente
		onda = (onda + 1) % 10;						//Incrementa el índice del arreglo values de 0 a 9 de forma cíclica.
		DAC_UpdateValue((1023/9))* values[onda];	//Calcula el valor digital para el DAC.
													//Si values[onda] es 9, el DAC saca 113 x 9 = 1017 (casi el máximo de 1023, equivalente a aprox. 3.28[V] si Vref = 3.3[V]
	}
}
