#include "lpc17xx_adc.h"
#include "lpc17xx_dac.h"
#include "lpc17xx_gpdma.h"

#define BUFFER_SIZE 100

volatile uint16_t buffer1[BUFFER_SIZE];
volatile uint16_t buffer2[BUFFER_SIZE];

void cpmfogADC(void) {
	ADC_Init(10000);
	ADC_PinConfig(ADC_CHANNEL_4);
	ADC_ChannelEnable(ADC_CHANNEL_4);
	ADC_BurstEnable();
}

void configDAC(void){
	DAC_Init();
	DAC_CONVERTER_CFG_T dacdm = {ENABLE, ENABLE, ENABLE};
	DAC_SetBias(DAC_350uA);
	DAC_UpdateValue(0);
}

void configDMA(void){
	GPDMA_Endpoint_T dsrc = {GPDMA_HALFWORD, GPDMA_BSIZE_1, DISABLE};
	GPDMA_Endpoint_T ddst = {GPDMA_HALFWORD, GPDMA_BSIZE_1, ENABLE};

	GPDMA_Init();
	GPDMA_Channel_CFG_T cfdma = {0};
	cfdma.ChannelNum = GPDMA_CH_0;
	cfdma.transferSize = 100;
	cfdma.type = GPDMA_BSIZE_1;
	cfdma.srcMemAddr = &LPC_ADC->ADGDR;
	cfdma.dstMemAddr = &buffer1;
	cfdma.srcConn = GPDMA_ADC;
	cfdma.src = &dsrc;
	cfdma.dst = &ddst;
	cfdma.intTc = ENABLE;
	cfdma.intErr = DISABLE;
	cfdma.linkedList = &lli2;

	GPDMA_SetupChannel(&cfdma);
	GPDMA_ChannelStart(GPDMA_CH_0);
}

int main(void){
	GPDMA_LLI_T lli1{&LPC_ADC->ADGDR, &buffer1, &lli2, 100 | 1<<18 | 1<<21 | 1<<27| 1<<31};
	GPDMA_LLI_T lli2{&LPC_ADC->ADGDR, &buffer2, &lli1, 100 | 1<<18 | 1<<21 | 1<<27| 1<<31};

	configADC();
	configDAC();
	configDMA();

	while(1){
	}

	return;
}
