#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif
#include "lpc17xx_gpio.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_timer.h"

#define banco0_1 0x2007C000		//Primera mitad del primer banco (termina en 0x2007DFFF)
#define banco0_2 0x2007E000		//Segunda mitad del primer banco
#define banco1_1 0x20080000		//Primera mitad del segundo banco (termina en 0x20081FFF)
#define banco1_2 0x20082000		//Segunda mitad del segundo banco
#define TRANSFER_SIZE_ONDA 382
#define TRANSFER_SIZE (0x2000 / sizeof(uint32_t))	// 0x2000 == 8192 => 8192 / 4 bytes = 2048
													//calcula cuántos elementos del tipo uint32_t caben exactamente en un bloque de memoria de 8 Kilobytes
#define ceroOnda 512
#define maximoOnda 1023
#define minimoOnda 0
#define ADC_CLK_Value 200000	// 200000 ya que, si tengo una señal cuyo ancho de banda de banda es de 80KHz
									// para evitar errores, utilizo 200KHz ¿PORQUE? Por el teorema de muestreo:
									// Muestreo mas del doble del ancho de banda que tengo

volatile uint32_t prendido = 0;
volatile uint32_t cuentas = 0;
volatile uint32_t guardando = 0;
volatile uint32_t promedio = 0;
volatile uint32_t *punteroADC_Memory = (volatile uint32_t *) banco0_1;
volatile uint32_t *punteroPromedio = (volatile uint32_t *) banco0_2;
volatile uint32_t *ondaPorDAC = (volatile uint32_t *) banco1_1;

typedef enum //Se le pone nombre a cada ESTADO de mi maquina de estados
{
	Dato_ADC_Pointer = 0,
	Dato_ADC_DMA,
	Dato_FirstHalf_SecondHalf,
	Dato_ADC_DAC,
	ondaFinal,

} estado_transfer;
volatile estado_transfer ESTADO = Dato_ADC_Pointer;

void configTIMER(void);
void configEINT0(void);
void configADC(void);
void configDAC(void);
void configDMA(GPDMA_LLI_T *lliADC_Bank01, GPDMA_LLI_TlliBank11_DAC);
void promedioM2M(void);
void generarOnda(void);
void guardarDatosUsandoPointer(void);

int main(void){

	GPDMA_LLI_T lliADC_Bank01 = {0};
	lliADC_Bank01.srcAddr 	= (uint32_t) &(LPC_ADC->ADDR0) ;
	lliADC_Bank01.dstAddr	= (uint32_t) (banco0_1);
	lliADC_Bank01.nextLLI	= (uint32_t) &(lliADC_Bank01);
	lliADC_Bank01.control	= (TRANSFER_SIZE << 0 )	//TransferSize
									| (0 << 12)		//SrcBurstSize
									| (0 << 15)		//DstBurstSize
									| (2 << 18)		//SrcWidth
									| (2 << 21)		//DstWidth
									| (0 << 26)		//¿SrcIncrement?
									| (1 << 27);	//¿DstIncrement?

	GPDMA_LLI_T lliBank11_DAC  = {0};
	lliBank11_DAC.srcAddr 	= (uint32_t) (banco1_1) ;
	lliBank11_DAC.dstAddr	= (uint32_t) &(LPC_DAC->DACR);
	lliBank11_DAC.nextLLI	= (uint32_t) &(lliBank11_DAC);
	lliBank11_DAC.control	= (TRANSFER_SIZE_ONDA << 0)
									| (0 << 12)		//SrcBurstSize
									| (0 << 15)		//DstBurstSize
									| (2 << 18)		//SrcWidth
									| (2 << 21)		//DstWidth
									| (1 << 26)		//¿SrcIncrement?
									| (0 << 27);	//¿DstIncrement?

	configEINT0();
	configTIMER();
	configADC();
	configDAC();
	configDMA();

	while(1){
	}
}

configEINT0(){					//Interrupcion por flanco ascendente en el pin P2.10
	EXTI_CFG_T cfgEint;
	cfgEint.line = EXTI_EINT0;
	cfgEint.mode = EXTI_EDGE_SENSITIVE;
	cfgEint.polarity = EXTI_RISING_EDGE;

	EXTI_Init();
	EXTI_EnableIRQ(EINT0_IRQn);
	EXTI_ConfigEnable(&cfgEint);
	EXTI_PinConfig(EXTI_EINT0, EXTI_PULLDOWN);
	NVIC_SetPriority(EINT0_IRQn, 0);
}

configADC(){					//Configuracion util para el CASE "Dato_ADC_Pointer"
	if(ESTADO == Dato_ADC_Pointer){
		ADC_Init(ADC_CLK_Value);
		ADC_PinConfig(ADC_CHANNEL_0);
		ADC_PowerUp();
		ADC_ChannelEnable(ADC_CHANNEL_0);
		ADC_BurstEnable();
		ADC_IntEnable(ADC_INT_CH0);

		NVIC_EnableIRQ(ADC_IRQn);
		NVIC_SetPriority(ADC_IRQn, 1);
	}
	else if(ESTADO == Dato_ADC_DMA){
		ADC_IntDisable(ADC_INT_CH0);
		NVIC_DisableIRQ(ADC_IRQn);
	}
}

configDAC(){								//PCLK_DAC = 20MHz (por default), y 382Hz por las 382 muestas por segundo que necesito
	DAC_Init();
	DAC_CONVERTER_CFG_T cfgDAC;
	cfgDAC.doubleBuffer = DISABLE; 			//¿ENABLE?
	cfgDAC.dmaCounter = ENABLE;
	cfgDAC.dmaRequest = ENABLE;

	DAC_SetBias(DAC_700uA);
	DAC_ConfigDAConverterControl(&cfgDAC);
	DAC_SetDMATimeOut(52356); 				// timeout = 20MHz / 382Hz = 52356
}

configTIMER(){
	TIM_TIMERCFG_T cfgTIM0;
	cfgTIM0.prescaleOpt = TIM_US;
	cfgTIM0.prescaleValue = 1;

	TIM_MATCHCFG_T cfgMAT1;
	cfgMAT1.channel = TIM_MATCH_1;
	cfgMAT1.intEn = ENABLE;
	cfgMAT1.stopEn = DISABLE;
	cfgMAT1.resetEn = ENABLE;
	cfgMAT1.extOpt = TIM_NOTHING;
	cfgMAT1.matchValue = 1000000; //Matchea cada 1 segundo

	TIM_InitTimer(LPC_TIM0, &cfgTIM0);
	TIM_ConfigMatch(LPC_TIM0, &cfgMAT1);
	NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_SetPriority(TIMER0_IRQn, 1);

}



configDMA(){
	GPDMA_LLI_T lliADC_Bank01 = {0};
	lliADC_Bank01.srcAddr 	= (uint32_t) &(LPC_ADC->ADDR0) ;
	lliADC_Bank01.dstAddr	= (uint32_t) (banco0_1);
	lliADC_Bank01.nextLLI	= (uint32_t) &(lliADC_Bank01);
	lliADC_Bank01.control	= (TRANSFER_SIZE << 0 )	//TransferSize
									| (0 << 12)		//SrcBurstSize
									| (0 << 15)		//DstBurstSize
									| (2 << 18)		//SrcWidth
									| (2 << 21)		//DstWidth
									| (0 << 26)		//¿SrcIncrement?
									| (1 << 27);	//¿DstIncrement?

	GPDMA_LLI_T lliBank11_DAC  = {0};
	lliBank11_DAC.srcAddr 	= (uint32_t) (banco1_1) ;
	lliBank11_DAC.dstAddr	= (uint32_t) &(LPC_DAC->DACR);
	lliBank11_DAC.nextLLI	= (uint32_t) &(lliBank11_DAC);
	lliBank11_DAC.control	= (TRANSFER_SIZE_ONDA << 0)
									| (0 << 12)		//SrcBurstSize
									| (0 << 15)		//DstBurstSize
									| (2 << 18)		//SrcWidth
									| (2 << 21)		//DstWidth
									| (1 << 26)		//¿SrcIncrement?
									| (0 << 27);	//¿DstIncrement?

	GPDMA_Channel_CFG_T dmaADC_Bank01;
	dmaADC_Bank01.channelNum 	= GPDMA_CH_0;
	dmaADC_Bank01.transferSize 	= TRANSFER_SIZE;
	dmaADC_Bank01.type			= GPDMA_P2M;
	dmaADC_Bank01.srcMemAddr	= 0;
	dmaADC_Bank01.dstMemAddr	= (uint32_t) (banco0_1);
	dmaADC_Bank01.srcConn		= GPDMA_ADC;
	dmaADC_Bank01.dstConn		= DISABLE;
	dmaADC_Bank01.src.width		= GPDMA_WORD;
	dmaADC_Bank01.src.burst		= GPDMA_BSIZE_1;
	dmaADC_Bank01.src.increment = DISABLE;
	dmaADC_Bank01.dst.width		= GPDMA_WORD;
	dmaADC_Bank01.dst.burst		= GPDMA_BSIZE_1;
	dmaADC_Bank01.dst.increment	= ENABLE;
	dmaADC_Bank01.intTC			= ENABLE; //Ver que hago con la interrupcion
	dmaADC_Bank01.intErr		= ENABLE;
	dmaADC_Bank01.linkedList	= (uint32_t) &(lliADC_Bank01);

	GPDMA_Channel_CFG_T Bank01_Bank02;
	Bank01_Bank02.channelNum	= GPDMA_CH_7;
	Bank01_Bank02.transferSize	= TRANSFER_SIZE;
	Bank01_Bank02.type			= GPDMA_M2M;
	Bank01_Bank02.srcMemAddr	= (uint32_t) (banco0_1);
	Bank01_Bank02.dstMemAddr	= (uint32_t) (banco0_2);
	Bank01_Bank02.srcConn		= DISABLE;
	Bank01_Bank02.dstConn		= DISABLE;
	Bank01_Bank02.src.width		= GPDMA_WORD;
	Bank01_Bank02.src.burst		= GPDMA_BSIZE_1;
	Bank01_Bank02.src.increment	= ENABLE;
	Bank01_Bank02.dst.width		= GPDMA_WORD;
	Bank01_Bank02.dst.burst		= GPDMA_BSIZE_1;
	Bank01_Bank02.dst.increment	= ENABLE;
	Bank01_Bank02.intTC			= ENABLE; //Ver que hago con la interrupcion
	Bank01_Bank02.intErr		= ENABLE;
	Bank01_Bank02.linkedList	= 0;

	GPDMA_Channel_CFG_T Bank01_DAC;
	Bank01_DAC.channelNum	= GPDMA_CH_2;
	Bank01_DAC.transferSize	= TRANSFER_SIZE;
	Bank01_DAC.type			= GPDMA_M2P;
	Bank01_DAC.srcMemAddr	= (uint32_t) (banco0_1);
	Bank01_DAC.dstMemAddr	= 0;
	Bank01_DAC.srcConn		= 0;
	Bank01_DAC.dstConn		= GPDMA_DAC;
	Bank01_DAC.src.width	= GPDMA_WORD;
	Bank01_DAC.src.burst	= GPDMA_BSIZE_1;
	Bank01_DAC.src.increment= ENABLE;
	Bank01_DAC.dst.width	= GPDMA_WORD;
	Bank01_DAC.dst.burst	= GPDMA_BSIZE_1;
	Bank01_DAC.dst.increment= DISABLE;
	Bank01_DAC.intTC		= ENABLE; //Ver que hago con la interrupcion
	Bank01_DAC.intErr		= ENABLE;
	Bank01_DAC.linkedList	= 0;

	GPDMA_Channel_CFG_T Bank11_DAC;
	Bank11_DAC.channelNum	= GPDMA_CH_3;
	Bank11_DAC.transferSize = TRANSFER_SIZE_ONDA;
	Bank11_DAC.type			= GPDMA_M2P;
	Bank11_DAC.srcMemAddr	= (uint32_t) (banco1_1);
	Bank11_DAC.dstMemAddr	= 0;
	Bank11_DAC.srcConn		= 0;
	Bank11_DAC.dstConn		= GPDMA_DAC;
	Bank11_DAC.src.width	= GPDMA_WORD;
	Bank11_DAC.src.burst	= GPDMA_BSIZE_1;
	Bank11_DAC.src.increment= ENABLE;
	Bank11_DAC.dst.width	= GPDMA_WORD;
	Bank11_DAC.dst.burst	= GPDMA_BSIZE_1;
	Bank11_DAC.dst.increment= DISABLE;
	Bank11_DAC.intTC		= ENABLE; //Ver que hago con la interrupcion
	Bank11_DAC.intErr		= ENABLE;
	Bank11_DAC.linkedList	= (uint32_t) &(lliBank11_DAC);

	GPDMA_Init();
	GPDMA_SetupChannel(&dmaADC_Bank01);
	GPDMA_SetupChannel(&Bank01_Bank02);
	GPDMA_SetupChannel(&Bank01_DAC);
	GPDMA_SetupChannel(&Bank11_DAC);
	NVIC_EnableIRQ(DMA_IRQn);
	NVIC_SetPriority(DMA_IRQn, 2);

}


void EINT0_IRQHandler(void){

	if(prendido == 0){
		ESTADO = 0;
		prendido = 1;
	}
	else{
		ESTADO = (ESTADO+1)%5;
	}

	switch (ESTADO){

		case Dato_ADC_Pointer:
			GPDMA_ChannelGracefulStop(GPDMA_CH_3);
			TIM_Enable(LPC_TIM0);
			configADC();
			break;

		case Dato_ADC_DMA:
			TIM_ResetCounter(LPC_TIM0);
			configADC();
			break;

		case Dato_FirstHalf_SecondHalf:
			GPDMA_ChannelGracefulStop(GPDMA_CH_0);
			GPDMA_ChannelStart(GPDMA_CH_7);
			TIM_Disable(LPC_TIM0);
			TIM_ResetCounter(LPC_TIM0);
			ADC_BurstDisable();
			break;

		case Dato_ADC_DAC:
			GPDMA_ChannelGracefulStop(GPDMA_CH_7);
			GPDMA_ChannelStart(GPDMA_CH_2);
			break;

		case ondaFinal:
			GPDMA_ChannelGracefulStop(GPDMA_CH_2);
			generarOnda();
			GPDMA_ChannelStart(GPDMA_CH_3);
	}
	EXTI_ClearFlag(EXTI_EINT0);
}

void ADC_IRQHandler(void){
	if(guardando && ESTADO == 0){
		if(ADC_channelGetStatus(ADC_CHANNEL_0, ADC_DATA_DONE) == 1){
			guardarDatosUsandoPointer();
		}
	}
}

void TIMER0_IRQHandler(void){ //Un segundo guardando, Un segundo transfiriendo por DMA
	if(TIM_GetIntStatus(LPC_TIM0, TIM_MR1_INT)){
		if(guardando == 0){
			guardando = 1;
			if(ESTADO == Dato_ADC_DMA){
				GPDMA_ChannelStart(GPDMA_CH_0);
			}
		}
		else if(guardando == 1){
			guardando = 0;
			if(ESTADO == Dato_ADC_DMA){
				GPDMA_ChannelGracefulStop(GPDMA_CH_0);
			}
		}
	}
	TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
}

void DMA_IRQHandler(void){
	//CHANNEL 0
	if(GPDMA_IntGetStatus(GPDMA_INTTC,GPDMA_CH_0)){
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_0);
	}
	if(GPDMA_IntGetStatus(GPDMA_INTERR,GPDMA_CH_0)){
		GPDMA_ChannelStop(GPDMA_CH_0);
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_0);
	}

	//CHANNEL 7
	if(GPDMA_IntGetStatus(GPDMA_INTTC,GPDMA_CH_7)){
		GPDMA_ChannelStop(GPDMA_CH_7);	//STOP al canal ya que termino la transferencia
		promedioM2M();
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_7);
	}
	if(GPDMA_IntGetStatus(GPDMA_INTERR,GPDMA_CH_7)){
		GPDMA_ChannelStop(GPDMA_CH_7);
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_7);
	}

	//CHANNEL 2
	if(GPDMA_IntGetStatus(GPDMA_INTTC,GPDMA_CH_2)){
		GPDMA_ChannelStop(GPDMA_CH_2);	//STOP al canal ya que termino la transferencia
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_2);
	}
	if(GPDMA_IntGetStatus(GPDMA_INTERR,GPDMA_CH_2)){
		GPDMA_ChannelStop(GPDMA_CH_2);	//STOP al canal ya que termino la transferencia
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_2);
	}

	//CHANNEL 3
	if(GPDMA_IntGetStatus(GPDMA_INTTC,GPDMA_CH_3)){
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_3);
	}
	if(GPDMA_IntGetStatus(GPDMA_INTERR,GPDMA_CH_3)){
		GPDMA_ChannelStop(GPDMA_CH_3);
		GPDMA_ClearIntPending(GPDMA_CLR_INTTC,GPDMA_CH_3);
	}

}

guardarDatosUsandoPointer(){
	*punteroADC_Memory = ADC_ChannelGetData(ADC_CHANNEL_0);
	punteroADC_Memory++;

	if(punteroADC_Memory >= (volatile uint32_t *) banco0_2){
		punteroADC_Memory = (volatile uint32_t *) banco0_1;
	}
}

promedioM2M(){
	uint32_t suma = 0 ;
	uint32_t j = 0 ;

	for (j = 0; j < TRANSFER_SIZE; j++){
		suma += punteroPromedio[j];
	}

	promedio = suma / TRANSFER_SIZE;
}

generarOnda(){
	uint32_t i = 0;
	uint32_t idx = 0;
	uint32_t cuartoUnoDeOnda = 96;
	uint32_t cuartoDosDeOnda = 95;
	uint32_t cuartoTresDeOnda = 96;
	uint32_t cuartoCuatroDeOnda = 95;

	for(i = 0; i < cuartoUnoDeOnda; i++) {
		ondaPorDAC[idx++] = (ceroOnda + (i * ceroOnda / (cuartoUnoDeOnda - 1))) << 6;		//Primer cuadrante
	}

	for(i = 0; i < cuartoDosDeOnda; i++) {
		ondaPorDAC[idx++] = ( maximoOnda - (i * ceroOnda / cuartoDosDeOnda)) << 6;			//Segundo cuadrante
	}

	for(i = 0; i < cuartoTresDeOnda; i++) {
		ondaPorDAC[idx++] = ( ceroOnda - (i * ceroOnda / (cuartoTresDeOnda - 1))) << 6;		//Tercer cuadrante
	}

	for(i = 0; i < cuartoCuatroDeOnda; i++) {
		ondaPorDAC[idx++] = (minimoOnda + (i * ceroOnda / cuartoCuatroDeOnda)) << 6;		//Cuarto cuadrante
	}

}
