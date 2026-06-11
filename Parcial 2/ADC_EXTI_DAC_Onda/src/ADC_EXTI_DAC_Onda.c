/**
 * El ADC esta convirtiendo señal de entrada 16kHz y guarda en primer mitad de SRAM_BANK0 usando DMA
 * Cuando viene EXTI, el ADC debe completar el ciclo de conversión que estaba realizando y parar, luego saco muestras de ADC por el DAC con DMA (SRAM_BANK0_PRIMER_MITAD -> DAC)
 * Cuando viene EXTI de nuevo, saco por el DAC la forma de onda guardada en SRAM_BANK0_SEGUNDA_MITAD (crear forma de onda y guardarla) de forma tal q tenga periodo 614us, al terminar vuelvo a convertir con el ADC.
 * Se alterna así entre los dos estados del sistema con cada interrupción externa.
 *  CCLK = 80 Mhz
 *
 * FORMA_DE_ONDA:
 * - 8k muestras y debe salir con periodo 614us
 * - Separo la señal en 2 (4k subida pos, 4k subida neg)
 *      - debe subir de 512 a 1023 en 4k pasos → aumento en 1 cada 8 pasos
 *      - luego repito pero desde 0 hasta 512
 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include "LPC17xx.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_gpdma.h"
#include "lpc17xx_exti.h"
//#include "lpc17xx_gpio.h"

//-------BANCOS-------
#define banco0_1 0x2007C000
#define banco0_2 0x2007E000

//-------FRECUENCIAS-------
//#define frecuenciaCCLK 80000000		//80MHz
//#define frecuenciaPCLK 20000000		//PCLK = CCLK / 4 = 80000000 / 4 = 20000000
//#define frecuenciaADC 16384			//16KHz
#define frecuenciaADCDoble 32768		//32KHz Muestreamos al doble de la las muestras por segundo
//#define periodoSeñal = 61[uS]			//Tdeseado = 1 / Fdeseado = 1 / 16384 = 61[uS]
//#define periodoSeñalDoble = 30.5[uS]	//Tdeseado = 1 / FdeseadoDoble = 1 / 32768 = 30.5[uS]
#define triggerM2P 609					//trigger = (Tdeseado * PCLK) - 1 = (30.5[uS] * 20[MHz]) - 1 = 609

//-------ONDA-------
//#define periodoONDA 614 				//614[uS]
//#define mitadONDA 307
#define triggerONDA 20					//trigger = (Tdeseado * PCLK) - 1 = (1[uS] * 20[MHz]) - 1 = 20
#define pasoONDA 1,6					//pasoONDA = 1023 / 614 = 1.66 [tamañoDePaso] => valorPasoONDA = 3.3 / 1023 = 0.003 [mV] cada paso
//#define ceroONDA 512
//#define maximoONDA 1023
//#define minimoONDA 0

//-------OTROS-------
#define TRANSFERSIZE 4095
//#define muestrasONDA 8000
//#define muestrasONDASubida 4000
//#define muestrasONDABajada 4000

//volatile uint32_t onda = 0;
volatile uint32_t *ondaDAC = (volatile uint32_t *) banco0_2;	//Array de datos en los que guardare la ONDA


void configADC(void);
void configDAC(void);
void configEINT(void);
void configDMA(GPDMA_LLI_T *lliADC_Banco01, GPDMA_LLI_T *lliBanco01_DAC, GPDMA_LLI_T *lliOnda_DAC);
void crearSeñal(void);

typedef enum{
	ADC_2_MEM = 0,
	ADC_2_DAC,
	MEM_2_OndaDAC,

} ESTADO_T;
ESTADO_T estado = ADC_2_MEM;

int main(){

	GPDMA_LLI_T lliADC_Banco01 = {0};
	lliADC_Banco01.srcAddr = (uint32_t) &(LPC_ADC->ADDR0);
	lliADC_Banco01.dstAddr = (uint32_t) banco0_1;
	lliADC_Banco01.nextLLI = (uint32_t) (&lliADC_Banco01);
	lliADC_Banco01.control = (TRANSFERSIZE << 0)
								| (0 << 12)		//SrcBurstSize
								| (0 << 15)		//DstBurstSize
								| (1 << 18)		//SrcWidth
								| (1 << 21)		//DstWidth
								| (0 << 26)		//¿SrcIncrement?
								| (1 << 27);	//¿DstIncrement?;

	GPDMA_LLI_T lliBanco01_DAC = {0};
	lliBanco01_DAC.srcAddr = (uint32_t) banco0_1;
	lliBanco01_DAC.dstAddr = (uint32_t) &(LPC_DAC->DACR);;
	lliBanco01_DAC.nextLLI = (uint32_t) (&lliBanco01_DAC);
	lliBanco01_DAC.control = (TRANSFERSIZE << 0)
								| (0 << 12)		//SrcBurstSize
								| (0 << 15)		//DstBurstSize
								| (1 << 18)		//SrcWidth
								| (1 << 21)		//DstWidth
								| (1 << 26)		//¿SrcIncrement?
								| (0 << 27);	//¿DstIncrement?;

	GPDMA_LLI_T lliOnda_DAC = {0};
	lliOnda_DAC.srcAddr = (uint32_t) banco0_2;
	lliOnda_DAC.dstAddr = (uint32_t) &(LPC_DAC->DACR);
	lliOnda_DAC.nextLLI = (uint32_t) (&lliOnda_DAC);
	lliOnda_DAC.control = (periodoONDA << 0)
							| (0 << 12)		//SrcBurstSize
							| (0 << 15)		//DstBurstSize
							| (1 << 18)		//SrcWidth
							| (1 << 21)		//DstWidth
							| (1 << 26)		//¿SrcIncrement?
							| (0 << 27);	//¿DstIncrement?;



	configADC();
	configDAC();
	configDMA(&lliADC_Banco01, &lliBanco01_DAC, &lliOnda_DAC);
	configEINT();

	while(1){

		switch(estado){
			case ADC_2_MEM:
				ADC_PowerUp();
				GPDMA_ChannelGracefulStop(GPDMA_CH_0);
				GPDMA_ChannelGracefulStop(GPDMA_CH_2);
				GPDMA_ChannelStart(GPDMA_CH_1);
				break;

			case ADC_2_DAC:
				GPDMA_ChannelGracefulStop(GPDMA_CH_1);
				GPDMA_ChannelGracefulStop(GPDMA_CH_0);
				ADC_PowerDown();
				DAC_SetDMATimeOut(triggerM2P);			//Explicado en el #define
				GPDMA_ChannelStart(GPDMA_CH_2);
				break;

			case MEM_2_OndaDAC:
				GPDMA_ChannelGracefulStop(GPDMA_CH_1);
				GPDMA_ChannelGracefulStop(GPDMA_CH_2);
				DAC_SetDMATimeOut(triggerONDA);
				GPDMA_ChannelStart(GPDMA_CH_0);
				break;

			default:
				break;
		}

		return 0;
	}
}

configADC(){
	ADC_Init(frecuenciaADCDoble);			//frecuenciaADCDoble = 32768 Hz
	ADC_PinConfig(ADC_CHANNEL_0);
	ADC_StartCmd(ADC_START_CONTINUOUS);		//Me pone el START en 000, por lo tanto el BURST va a poder funcionar
	ADC_ChannelEnable(ADC_CHANNEL_0);
	ADC_BurstEnable();
	ADC_PowerDown();
}

configDAC(){
	DAC_Init();
	DAC_CONVERTER_CFG_T configDAC;
	configDAC.doubleBuffer = DISABLE;
	configDAC.dmaCounter = ENABLE;
	configDAC.dmaRequest =	ENABLE;
	DAC_SetBias(DAC_700uA);

	DAC_ConfigDAConverterControl(&configDAC);

}

configEINT(){
	EXTI_CFG_T confEXTI;
	confEXTI.line = EXTI_EINT0;
	confEXTI.mode = EXTI_EDGE_SENSITIVE;
	confEXTI.polarity = EXTI_FALLING_EDGE;		//FALLING ya que el pin esta con PULLUP

	EXTI_Init();
	EXTI_PinConfig(EXTI_EINT0, EXTI_PULLUP);
	EXTI_ConfigEnable(&confEXTI);
	NVIC_EnableIRQ(EINT0_IRQn);
	NVIC_SetPriority(EINT0_IRQn, 0);

}

configDMA(GPDMA_LLI_T *lliADC_Banco01, GPDMA_LLI_T *lliBanco01_DAC, GPDMA_LLI_T *lliOnda_DAC){

	GPDMA_Channel_CFG_T configADC_Banco01;
	configADC_Banco01.channelNum = GPDMA_CH_1;
	configADC_Banco01.transferSize = TRANSFERSIZE;
	configADC_Banco01.type = GPDMA_P2M;
	configADC_Banco01.srcMemAddr = 0;
	configADC_Banco01.dstMemAddr = (uint32_t) (banco0_1);
	configADC_Banco01.srcConn = GPDMA_ADC;
	configADC_Banco01.dstConn = 0;
	configADC_Banco01.src.width = GPDMA_HALFWORD;
	configADC_Banco01.src.burst = GPDMA_BSIZE_1;
	configADC_Banco01.src.increment = DISABLE;
	configADC_Banco01.dst.width = GPDMA_HALFWORD;
	configADC_Banco01.dst.burst = GPDMA_BSIZE_1;
	configADC_Banco01.dst.increment = ENABLE;
	configADC_Banco01.intTC = DISABLE;
	configADC_Banco01.intErr = ENABLE;
	configADC_Banco01.linkedList = &lliADC_Banco01;

	GPDMA_Channel_CFG_T configBanco01_DAC;
	configBanco01_DAC.channelNum = GPDMA_CH_2;
	configBanco01_DAC.transferSize = TRANSFERSIZE;
	configBanco01_DAC.type = GPDMA_M2P;
	configBanco01_DAC.srcMemAddr = (uint32_t) (banco0_1);
	configBanco01_DAC.dstMemAddr = 0;
	configBanco01_DAC.srcConn = 0;
	configBanco01_DAC.dstConn = GPDMA_DAC;
	configBanco01_DAC.src.width = GPDMA_HALFWORD;
	configBanco01_DAC.src.burst = GPDMA_BSIZE_1;
	configBanco01_DAC.src.increment = ENABLE;
	configBanco01_DAC.dst.width = GPDMA_HALFWORD;
	configBanco01_DAC.dst.burst = GPDMA_BSIZE_1;
	configBanco01_DAC.dst.increment = DISABLE;
	configBanco01_DAC.intTC = DISABLE;
	configBanco01_DAC.intErr = ENABLE;
	configBanco01_DAC.linkedList = &lliBanco01_DAC;

	GPDMA_Channel_CFG_T configOndaDAC;
	configOndaDAC.channelNum = GPDMA_CH_0;
	configOndaDAC.transferSize = 614;
	configOndaDAC.type = GPDMA_M2P;
	configOndaDAC.srcMemAddr = (uint32_t) (banco0_2);
	configOndaDAC.dstMemAddr = 0;
	configOndaDAC.srcConn = 0;
	configOndaDAC.dstConn = GPDMA_DAC;
	configOndaDAC.src.width = GPDMA_HALFWORD;
	configOndaDAC.src.burst = GPDMA_BSIZE_1;
	configOndaDAC.src.increment = ENABLE;
	configOndaDAC.dst.width = GPDMA_HALFWORD;
	configOndaDAC.dst.burst = GPDMA_BSIZE_1;
	configOndaDAC.dst.increment = DISABLE;
	configOndaDAC.intTC = DISABLE;
	configOndaDAC.intErr = ENABLE;
	configOndaDAC.linkedList = &lliOnda_DAC;



	GPDMA_Init();
	GPDMA_SetupChannel(&configADC_Banco01);
	GPDMA_SetupChannel(&configBanco01_DAC);
	GPDMA_SetupChannel(&configOndaDAC);
	NVIC_EnableIRQ(DMA_IRQn);
	NVIC_SetPriority(DMA_IRQn, 2);
}

void DMA_IRQHandler(void){
	if(GPDMA_IntGetStatus(GPDMA_INTERR, GPDMA_CH_0) == 1){
		GPDMA_ChannelStop(GPDMA_CH_0);
		GPDMA_ClearIntPending(GPDMA_CLR_INTERR, GPDMA_CH_0);
	}

	if(GPDMA_IntGetStatus(GPDMA_INTERR, GPDMA_CH_1) == 1){
			GPDMA_ChannelStop(GPDMA_CH_1);
			GPDMA_ClearIntPending(GPDMA_CLR_INTERR, GPDMA_CH_1);
		}

	if(GPDMA_IntGetStatus(GPDMA_INTERR, GPDMA_CH_2) == 1){
			GPDMA_ChannelStop(GPDMA_CH_2);
			GPDMA_ClearIntPending(GPDMA_CLR_INTERR, GPDMA_CH_2);
		}
}

void EINT0_IRQHandler(void){
	if(EXTI_GetFlag(EXTI_EINT0) ==1){

		EXTI_ClearFlag(EXTI_EINT0);
		switch (state){
			case ADC_2_MEM:
				state = ADC_2_DAC;
				break;

			case ADC_2_DAC:
				state = MEM_2_OndaDAC;
				break;

			case MEM_2_OndaDAC:
				state = ADC_2_DAC;
				break;

			default:
				break;
		}


	}
}


crearSeñal(void){

	uint32_t i = 0;
	uint32_t idx = 0;
	uint32_t periodoONDA = 614; 				//614[uS]
	uint32_t mitadONDA = 307;
	uint32_t ceroONDA = 512;
	uint32_t maximoONDA = 1023;
	uint32_t minimoONDA = 0;
	uint32_t ondaRELATIVA = (i - mitadONDA);

	for(i = 0; i < mitadONDA; i++){														//0 a 306
			ondaDAC[idx++] = (ceroONDA + ((i * (ceroONDA - 1)) / (mitadONDA - 1))) << 6;
		}
	for(i = mitadONDA; i < periodoONDA; i++){											//307 a 613
			ondaDAC[idx++] = ((ondaRELATIVA * (ceroONDA - 1)) / (mitadONDA - 1)) << 6;
		}
}
