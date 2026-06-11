#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"

void cfgPIN(void);
void cfgTIMER0(void);
void cfgTIMER1(void);
void cfgADC(void);
void cfgDAC(void);
void cfgDMA(void);
void cfgNVIC(void);

int main(void) {

	cfgPIN();
	cfgTIMER0();
	cfgTIMER1();
	cfgADC();
	cfgDAC();
	cfgDMA();
	cfgNVIC();

	while(1){
		return 0 ;
	}
}



/* ==========================================================
 *                         OTROS
 * ==========================================================
 */

void cfgPIN(void){
	PINSEL_CFG_T cfgPINSEL;
		cfgPINSEL.port 		= PORT_0;
		cfgPINSEL.pin  		= PIN_0;
		cfgPINSEL.func 		= PINSEL_FUNC_00;
		cfgPINSEL.mode 		= PINSEL_TRISTATE;
		cfgPINSEL.openDrain = DISABLE;

	PINSEL_ConfigPin(&cfgPINSEL);

	GPIO_SetDir(PORT_0, 1<<0, GPIO_OUTPUT);
	GPIO_SetPinState(PORT_0, 1<<0, SET);
	GPIO_ClearPins(PORT_0, 0x400000);
}

void cfgNVIC(void){
	//Prioridades PROVISORIAS

	NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_ClearPendingIRQ(TIMER0_IRQn); 	//PROVISORIO: en caso de que usemos el START_NOW
	NVIC_SetPriority(TIMER0_IRQn, 0);

	NVIC_EnableIRQ(TIMER1_IRQn);
	NVIC_ClearPendingIRQ(TIMER0_IRQn); 	//PROVISORIO: en caso de que usemos el START_NOW
	NVIC_SetPriority(TIMER0_IRQn, 1);

	NVIC_EnableIRQ(ADC_IRQn);
	NVIC_Clear_PendingIRQ(ADC_IRQn);
	NVIC_SetPriority(ADC_IRQn, 2);

	NVIC_EnableIRQ(DMA_IRQn);
	NVIC_ClearPendingIRQ(DMA_IRQn);
	NVIC_SetPriority(DMA_IRQn, 3);


}

void testearDistancia(){
	return;
}

/* ==========================================================
 *                         TIMERs
 * ==========================================================
 */

void cfgTIMER0(void){
	TIM_TIMERCFG_T cfgTIM0;
	cfgTIM0.prescaleOpt	 = TIM_US;
	cfgTIM0.prescaleValue = 1; 			//PROVISORIO: cambiar si es necesario

	TIM_MATCHCFG_T cfgMATCH;
	cfgMATCH.channel	= TIM_MATCH_1;
	cfgMATCH.intEn 		= ENABLE;
	cfgMATCH.stopEn 	= DISABLE;
	cfgMATCH.resetEn 	= ENABLE;
	cfgMATCH.extOpt 	= TIM_NOTHING;
	cfgMATCH.matchValue = 125000; 		//Interrupcion cada 10ms


	TIM_InitTimer(LPC_TIM0, &cfgTIM0);
	TIM_ConfigMatch(LPC_TIM0, &cfgMATCH);

	//TIM_Enable(LPC_TIM0);				//¿Hace falta?

}

void cfgTIMER1(void){

	TIM_TIMERCFG_T cfgTIM1;
	cfgTIM1.prescaleOpt 	= TIM_US;
	cfgTIM1.prescaleValue 	= 100;


	TIM_MATCHCFG_T cfgMATCH0;
	cfgMATCH0.channel 		= 0;			// USO MAT1.0 para periodo completo (util para reiniciar)
	cfgMATCH0.intEn 		= DISABLE;
	cfgMATCH0.stopEn 		= DISABLE;
	cfgMATCH0.resetEn 		= ENABLE;
	cfgMATCH0.extOpt 		= 0;
	cfgMATCH0.matchValue 	= 100;

	TIM_MATCHCFG_T cfgMATCH1;
	cfgMATCH1.channel 		= 1;			// USO MAT1.1 para el duty cycle
	cfgMATCH1.intEn 		= ENABLE;
	cfgMATCH1.stopEn 		= DISABLE;
	cfgMATCH1.resetEn 		= ENABLE;
	cfgMATCH1.extOpt 		= 0;
	cfgMATCH1.matchValue 	= 50;		//PROVISORIO: Ya que este matchValue debe valer lo que se ingrese por el UART

	TIM_InitTimer(LPC_TIM1, &cfgTIM1);
	TIM_ConfigMatch(LPC_TIM1, &cfgMATCH0);
	TIM_ConfigMatch(LPC_TIM1, &cfgMATCH1);

	//TIM_Enable(LPC_TIM1);				//¿Hace falta?
}

/* ==========================================================
 *                       CONVERSORES
 * ==========================================================
 */

//ADC: Tomar muestras del valor detectado por el sensor infrarrojo en "x" momento y poder calcular la distancia (Sin usar modo BURST)

void cfgADC(void){
	ADC_Init(200000);							//frecuenciaMaximaPosible = 200 [kHz]
	ADC_PinConfig(ADC_CHANNEL_0);
	ADC_ChannelEnable(ADC_CHANNEL_0);
	ADC_BurstDisable();
	ADC_StartCmd(ADC_START_NOW); 				//o ADC_START_NOW y entro a la interrupcion del timer
	ADC_EdgeStartConfig(ADC_START_ON_RISING);
	ADC_IntDisable(ADC_INT_CH0); 				//o disable si uso el START_NOW

	ADC_PowerUp(); //capaz lo tenemos q poner en otro lado dsp
}

void cfgDAC(void){
	DAC_CONVERTER_CFG_T dacCFG;
	dacCFG.doubleBuffer = DISABLE;
	dacCFG.dmaCounter = DISABLE;
	dacCFG.dmaRequest = DISABLE;

	DAC_Init(); //capaz lo tenemos q poner en otro lado dsp
	DAC_ConfigDAConverterControl(&dacCFG);
	DAC_SetBias(DAC_700uA);
}

/* ==========================================================
 *                          DMA
 * ==========================================================
 */

void cfgDMA(){
	GPDMA_Endpoint_T scrCFG;
	scrCFG.width = GPDMA_HALFWORD; //xq el adc usa solo 12bits y el dac 10bists, poner palabra completa no es eficiente
	scrCFG.burst = GPDMA_BSIZE_1; // xq solo hacemos 1 transferencia
	scrCFG.increment = DISABLE;

	GPDMA_Endpoint_T dstCFG;
	dstCFG.width = GPDMA_HALFWORD;
	dstCFG.burst = GPDMA_BSIZE_1;
	dstCFG.increment = DISABLE;

	GPDMA_Channel_CFG_T dmaCFG;
	dmaCFG.channelNum = GPDMA_CH_0;
	dmaCFG.transferSize = 1; //xq solo necesitamos transferir la muestra del adc (no guadrar los ultimos 10 valores p.ej)
	dmaCFG.type = GPDMA_P2P; //xq transferimos del adc al dac
	dmaCFG.srcMemAddr = 0; //no usamos lugares de memeoria (por ahora)
	dmaCFG.dstMemAddr = 0;
	dmaCFG.srcConn = GPDMA_ADC;
	dmaCFG.dstConn = GPDMA_DAC;
	dmaCFG.src = scrCFG;
	dmaCFG.dst = dstCFG;
	dmaCFG.intTC = ENABLE; //provisorio, capaz no lo usamos
	dmaCFG.intErr = ENABLE; //por si salta algun error
	dmaCFG.linkedList = 0;

	GPDMA_Init();
	GPDMA_SetupChannel(&dmaCFG);

}

/* ==========================================================
 *                        HANDLERs
 * ==========================================================
 */

void TIMER0_IRQHandler(void){
	ADC_StartCmd(ADC_START_NOW);
	if(ADC_ChannelGetStatus(ADC_CHANNEL_0, ADC_DATA_DONE)){
		adcValue = ADC_ChannelGetData(ADC_CHANNEL_0);
	}

	//DAC_UpdateValue((adcValue << 2)); //provisorio, dsp lo tenemos q hacer bien
	GPDMA_ChannelStart(GPDMA_CH_0);

	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
}

void TIMER1_IRQHandler(void){
	if(TIM_GetIntStatus(LPC_TIM1, TIM_MR1_INT) == SET){
		GPIO_ClearPins(PORT_0,1<<0);
		TIM_ClearIntPending(LPC_TIM1, TIM_MR1_INT);
	}
}

void DMA_IRQHandler(void){
	if(GPDMA_IntGetStatus(GPDMA_INTTC, GPDMA_CH_0)){
		GPIO_ClearPins(PORT_0, 0x400000); //no hubo error
		testearDistancia(); //comparo la distancia entre el objeto y el sensor y decido que hacer

		GPDMA_ClearIntPending(GPDMA_CLR_INTTC, GPDMA_CH_0);
	}

	if(GPDMA_IntGetStatus(GPDMA_INTERR, GPDMA_CH_0)){
		GPIO_SetPins(PORT_0, 0x400000); //hubo error
		//frenar los motores, ver como hacer

		GPDMA_ClearIntPending(GPDMA_CLR_INTERR, GPDMA_CH_0);
	}
}

