/**
 * Utilizando un TIMER y un pin de CAPTURE
 * 1- demodular una señal PWM que ingresa por dicho pin (calcular el ciclo de trabajo y el periodo)
 * 2- sacar una tensión continua proporcional y guardar en buffer circular de 10 elementos
 * 3- sacar promedio de 10 datos a través del DAC (rango dinámico 0-2V y actualizar cada 0,5s)
 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "LPC17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_timer.h"
//#include "lpc17xx_gpdma.h"
//#include "lpc17xx_gpio.h"

#define ELEMENTOS 10

volatile uint32_t PROMEDIO = 0;
volatile uint32_t PWM = 0;
volatile uint32_t tension_PWM = 0;

void configADC(void);
void configDAC(void);
void configTIMER(void);

int main(){
	while(1){

	}
}


