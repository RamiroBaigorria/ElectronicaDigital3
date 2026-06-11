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


void cfgPIN();
void cfgTIMER0();
void cfgTIMER1();
void cfgADC();
void cfgDAC();
void cfgDMA();



int main(void) {

	cfgPIN();
	cfgTIMER0();
	cfgTIMER1();
	cfgADC();
	cfgDAC();
	cfgDMA();

	while(1){
		return 0 ;
	}
}

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

void cfgTIMER0(){
	TIM_TIMERCFG_T timCFG;
	timCFG.prescaleOpt = TIM_US;
	timCFG.prescaleValue = 1; //provisorio, cambiar si es necesario

	TIM_MATCHCFG_T matchCFG;
	matchCFG.channel = TIM_MATCH_1;
	matchCFG.intEn = ENABLE;  //o enable si uso START_NOW
	matchCFG.stopEn = DISABLE;
	matchCFG.resetEn = ENABLE;
	matchCFG.extOpt = TIM_NOTHING;
	matchCFG.matchValue = 125000; //para q interrumpa cada 10ms

	TIM_InitTimer(LPC_TIM0, &timCFG);

	NVIC_EnableIRQ(TIMER0_IRQn); //en caso de q usemos el START_NOW
	NVIC_ClearPendingIRQ(TIMER0_IRQn); //en caso de q usemos el START_NOW
	NVIC_SetPriority(TIMER0_IRQn, 1); //para cuando unamos el proyecto

	TIM_ConfigMatch(LPC_TIM0, &matchCFG);
}

void cfgTIMER1(void){

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

//usamos el adc para tomar muestras del valor detectado por el sensor infrarrojo en ese momento
//y poder calcular la distancia
//utilizamos el timer para iniciar la conversion del adc, asi q deshabilitamos la interrupciondel adc
void cfgADC(){
	ADC_Init(200000);
	ADC_PinConfig(ADC_CHANNEL_0);
	ADC_BurstDisable();
	ADC_StartCmd(ADC_START_NOW); //o ADC_START_NOW y entro a la interrupcion del timer
	ADC_ChannelEnable(ADC_CHANNEL_0);
	ADC_EdgeStartConfig(ADC_START_ON_RISING);
	ADC_IntDisable(ADC_INT_CH0); //o disable si uso el START_NOW

	//NVIC_EnableIRQ(ADC_IRQn);
	//NVIC_Clear_PendingIRQ(ADC_IRQn);
	//NVIC_SetPriority(); //para cuando unamos el proyecto

	ADC_PowerUp(); //capaz lo tenemos q poner en otro lado dsp
}

void cfgDAC(){
	DAC_CONVERTER_CFG_T dacCFG;
	dacCFG.doubleBuffer = DISABLE;
	dacCFG.dmaCounter = DISABLE;
	dacCFG.dmaRequest = DISABLE;

	DAC_Init(); //capaz lo tenemos q poner en otro lado dsp
	DAC_ConfigDAConverterControl(&dacCFG);
	DAC_SetBias(DAC_700uA);
}

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

	NVIC_EnableIRQ(DMA_IRQn);
	NVIC_ClearPendingIRQ(DMA_IRQn);
	NVIC_SetPriority(DMA_IRQn, 1); //para cuando unamos el proyecto
}

/*
 * =========================HANDLERs=========================
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

void testearDistancia(){
	return;
}
