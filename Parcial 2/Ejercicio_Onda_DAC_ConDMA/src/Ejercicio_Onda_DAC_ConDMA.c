/*
 * Forma de onda: 				Escalera de 10 pisos
 * Amplitud maxima: 			Aprox. 3,3 [V]
 * Duracion de cada escalon: 	0,1 [mS] = 100 [us] (tiempo que tarda el contador del DAC en llegar a cero --> tiempo = timeOutDAC / PCLK = 2500/25000000 = 0.0001 [S] = 0.1 [mS]
 * Peroiodo de la señal(T): 	10 escalones x 0.1 [ms] = 1 [mS]
 * Frecuencia de la señal(f): 	f = 1/T = 1/0.001[S] = 1000 [Hz]1 [kHz]
 */

#include "LPC17xx.h"
#include "LPC17xx_dac.h"
#include "LPC17xx_gpdma.h"

#define N 10 					//Cantidad de elementos del Array


volatile uint16_t valores[N];	//Inicializo array en el que voy a poner los 10 elementos
GPDMA_LLI_T lli;				//Hacemos a la "lli" global para que el DMA siempre pueda acceder

void configDAC(void);
void configDMA(GPDMA_LLI_T* lli);

int main(void) {

	for(int i=0; i<N; i++){
		valores[i] = ((1023/(N-1))*i) << 6; //DACR[15:6], por eso lo muevo 6 lugares
	}

	GPDMA_LLI_T lli;
	lli.srcAddr = (uint32_t) &valores;
	lli.dstAddr = (uint32_t) &(LPC_DAC->DACR);
	lli.nextLLI = (uint32_t) &lli;
	lli.control = (N << 0)		//TransferSize
				| (0 << 12)		//Source Burst Size
				| (0 << 15)		//Destinatio Burst Size
				| (1 << 18)		//Halfword Source
				| (1 << 21)		//Halfword Destination
				| (1 << 26)		//Source increment
				| (0 << 27)		//Destination NO increment
				| (0 << 31);	//Terminal Count DISABLED

	configDMA(&lli);
	configDAC();

    while(1) {
    }
    return 0 ;
}

void configDAC(void){
	DAC_Init();
	DAC_CONVERTER_CFG_T dacDMA;
	dacDMA.doubleBuffer = DISABLE;
	dacDMA.dmaCounter = ENABLE;
	dacDMA.dmaRequest = ENABLE;

	DAC_SetBias(DAC_350uA);
	DAC_SetDMATimeOut(2500);	// Periodo entre Request´s=> 0.1ms = 0,0001 segundos
	    						// timeOut = periodo (en Segundos) / PCLK (en Hz)-> timeOut = 0,0001 / 25.000.000 = 2500
								// Configuracion del DAC para que pida datos al DMA cada 0.1 ms
	DAC_UpdateValue(0);
	DAC_ConfigDAConverterControl(&dacDMA);
}

void configDMA(GPDMA_LLI_T* lli){
	GPDMA_Endpoint_T dsrc = {GPDMA_HALFWORD, GPDMA_BSIZE_1, ENABLE};
	GPDMA_Endpoint_T ddrc = {GPDMA_HALFWORD, GPDMA_BSIZE_1, DISABLE};

	GPDMA_Channel_CFG_T dmaCfg;
	dmaCfg.channelNum = GPDMA_CH_0;		//Selecciono Canal 0 del GPDMA para hacer la transferencia
	dmaCfg.transferSize = N ;			//Tamaño de transferencia de 10
	dmaCfg.type = GPDMA_M2P;			//Transferencia Memory to Pheriperal
	dmaCfg.srcMemAddr = (uint32_t) &valores;		//¿Source? Array llamado Valores (Memory)
	dmaCfg.dstMemAddr = (uint32_t) &(LPC_DAC->DACR);	//¿Destination? Registro DACR del DAC que contiene el valor digital que se convertira a analogico
	dmaCfg.srcConn = DISABLE;			//DISABLE ya que el SRC es MEMORY
	dmaCfg.dstConn = GPDMA_DAC;
	dmaCfg.src = dsrc;					//GPDMA_Endpoint_T declarado anteriormente para el SRC
	dmaCfg.dst = ddrc;					//GPDMA_Endpoint_T declarado anteriormente para el DST
	dmaCfg.intTC = DISABLE;				//Interrupcion por "terminar de contar" deshabilitada
	dmaCfg.intErr = DISABLE;			//Interrupcion por error deshabilitada
	dmaCfg.linkedList = (uint32_t) &lli;			//lli circular

	GPDMA_Init();
	GPDMA_SetupChannel(&dmaCfg);	//Cargo el "formulario" completado anteriormente
	GPDMA_ChannelStart(GPDMA_CH_0);

}
