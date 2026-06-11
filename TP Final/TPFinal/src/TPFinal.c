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
#include "lpc17xx_uart.h"

// Definición de alias para los pines físicos del microcontrolador
#define PIN_MOTOR_IZQ_A    (1 << 0)   // Pin P1.0
#define PIN_MOTOR_IZQ_B    (1 << 1)   // Pin P1.1
#define PIN_MOTOR_DER_A    (1 << 2)   // Pin P1.2
#define PIN_MOTOR_DER_B    (1 << 3)   // Pin P1.3

#define TRANSFERSIZE 10

#define LIMITE_OBSTACULO   1850  	// Equivale a 20 cm medidos en el laboratorio (REALIZAR LA MEDICION REAL NOSOTROS MISMOS, ESTE ES UN VALOR COMUN pero NO SIEMPRE CIERTO)
								 	// Poner el auto frente a una pared a la distancia exacta a la que queremos que doble (Ejemplo: 20 cm).
									// Medimos con el ADC qué valor entrega el sensor a esa distancia. (Ejemplo: El ADC nos devuelve 1850)
									// Directamente definimos la constante LIMITE_OBSTACULO en base a ese número analógico

uint32_t variableUsadaParaDAC = 0; 	// La usaremos mas adelante en algun momento (NO SE USA TODAVIA)
uint32_t adc_buffer[10]; 			// Buffer en RAM para almacenar las distancias

void configPIN(void);
void configTIMER0(void);
void configTIMER1(void);
void configADC(void);
void configDAC(void);
void configDMA(void);
void configNVIC(void);
void configUART(void);

int main(void) {

	configPIN();
	configTIMER0();
	configTIMER1();
	configADC();
	configDAC();
	configDMA();
	configNVIC();
	configUART();

	while(1){
		return 0 ;
	}
}

/* ==========================================================
 *                         OTROS
 * ==========================================================
 */

void configPIN(void){
	PINSEL_CFG_T cfgPIN0;// Configuracion del pin P0.0 como GPIO
		cfgPIN0.port 		= PORT_0;
		cfgPIN0.pin  		= PIN_0;
		cfgPIN0.func 		= PINSEL_FUNC_00;
		cfgPIN0.mode 		= PINSEL_TRISTATE;
		cfgPIN0.openDrain 	= DISABLE;

	PINSEL_CFG_T cfgPIN2;// Configuracion del pin P0.2 como TXD0
		cfgPIN2.port 		= PORT_0;
		cfgPIN2.mode 		= PINSEL_TRISTATE;
		cfgPIN2.openDrain	= DISABLE;
		cfgPIN2.pin 		= PIN_2;			// Configurar P0.2 como TXD0
		cfgPIN2.func 		= PINSEL_FUNC_01;	// Función 01 selecciona UART0 TX


	PINSEL_CFG_T cfgPIN3;// Configuracion del pin P0.3 como RXD0
		cfgPIN3.port 		= PORT_0;
		cfgPIN3.mode 		= PINSEL_TRISTATE;
		cfgPIN3.openDrain	= DISABLE;
		cfgPIN3.pin 		= PIN_3;			// Configurar P0.3 como RXD0
		cfgPIN3.func 		= PINSEL_FUNC_01;	// Función 01 selecciona UART0 TX


	PINSEL_ConfigPin(&cfgPIN2);
	PINSEL_ConfigPin(&cfgPIN3);
	PINSEL_ConfigPin(&cfgPIN0);

	GPIO_SetDir(PORT_1, PIN_MOTOR_IZQ_A | PIN_MOTOR_IZQ_B | PIN_MOTOR_DER_A | PIN_MOTOR_DER_B, GPIO_OUTPUT);	// Configurar los 4 pines de control de motores como salida
	GPIO_ClearPins(PORT_1, PIN_MOTOR_IZQ_A | PIN_MOTOR_IZQ_B | PIN_MOTOR_DER_A | PIN_MOTOR_DER_B);				// Estado inicial seguro: Todo apagado (Auto frenado)

	GPIO_SetDir(PORT_0, 1<<0, GPIO_OUTPUT);
	GPIO_SetPinState(PORT_0, 1<<0, SET);
	GPIO_ClearPins(PORT_0, 0x400000);
}

void configNVIC(void){
	//Prioridades PROVISORIAS

	NVIC_EnableIRQ(TIMER0_IRQn);
	NVIC_ClearPendingIRQ(TIMER0_IRQn); 	//PROVISORIO: en caso de que usemos el START_NOW
	NVIC_SetPriority(TIMER0_IRQn, 0);

	NVIC_EnableIRQ(TIMER1_IRQn);
	NVIC_ClearPendingIRQ(TIMER1_IRQn); 	//PROVISORIO: en caso de que usemos el START_NOW
	NVIC_SetPriority(TIMER1_IRQn, 1);

	NVIC_EnableIRQ(ADC_IRQn);
	NVIC_ClearPendingIRQ(ADC_IRQn);
	NVIC_SetPriority(ADC_IRQn, 2);

	NVIC_EnableIRQ(DMA_IRQn);
	NVIC_ClearPendingIRQ(DMA_IRQn);
	NVIC_SetPriority(DMA_IRQn, 3);


}

void configUART(void){ //Estos drivers son del fabricante, no de Trujillo (No se si el tiene)

	    UART_CFG_T cfgUART;
	    cfgUART.baudRate = 9600;             	// Velocidad estándar de fábrica de los módulos Bluetooth
	    cfgUART.parity   = UART_PARITY_NONE;   	// Sin bit de paridad
	    cfgUART.dataBits = UART_DBITS_8;     	// 8 bits de datos
	    cfgUART.stopBits = UART_STOPBIT_1;     	// 1 bit de parada

	    UART_ConfigStructInit(&cfgUART);	// Verificar bien esta funcion de carga de formulario, porque me figura error

	    UART_PinConfig(UART_TX0_P0_2);		// Es lo mismo que el "PINSEL_CFG_T cfgPIN2" del "void cfgPIN(void)"
	    UART_PinConfig(UART_RX0_P0_3);		// Es lo mismo que el "PINSEL_CFG_T cfgPIN3" del "void cfgPIN(void)"

	    UART_Init(LPC_UART0, &cfgUART);		// Inicializar físicamente el periférico UART0

	    UART_TxCmd(LPC_UART0, ENABLE);		// Habilitar la transmisión y recepción (Línea obligatoria de CMSIS)

	    UART_IntConfig(LPC_UART0, UART_INTCFG_RBR, ENABLE); // Habilitar interrupción específica por RECEPCIÓN (RBR) en el periférico
	    													// Esto hace que la UART avise cuando "llegó un dato"
}

void testearDistancia(){
	return;
}

/* ==========================================================
 *                         TIMERs
 * ==========================================================
 */

void configTIMER0(void){
	TIM_TIMERCFG_T cfgTIM0;
	cfgTIM0.prescaleOpt	 = TIM_US;
	cfgTIM0.prescaleValue = 1; 			//PROVISORIO: cambiar si es necesario

	TIM_MATCHCFG_T cfgMATCH;
	cfgMATCH.channel	= TIM_MATCH_1;
	cfgMATCH.intEn 		= ENABLE;
	cfgMATCH.stopEn 	= DISABLE;
	cfgMATCH.resetEn 	= ENABLE;
	cfgMATCH.extOpt 	= TIM_NOTHING;
	cfgMATCH.matchValue = 100000; 		//Interrupcion cada 100ms
  //cfgMATCH.matchValue = 125000; 	no me parece correcto, con el prescaler en 1 , 125mil tics son 125[mS]


	TIM_InitTimer(LPC_TIM0, &cfgTIM0);
	TIM_ConfigMatch(LPC_TIM0, &cfgMATCH);

	//TIM_Enable(LPC_TIM0);				//¿Hace falta?

}

void configTIMER1(void){

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
	cfgMATCH1.resetEn 		= DISABLE;		// Va en DISABLE ya que el RESET lo tenemos que manejar con el MATCH0
	cfgMATCH1.extOpt 		= 0;
	cfgMATCH1.matchValue 	= 50;			//PROVISORIO: Ya que este matchValue debe valer lo que se ingrese por el UART

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

void configADC(void){
	ADC_Init(200000);							//frecuenciaMaximaPosible = 200 [kHz]
	ADC_PinConfig(ADC_CHANNEL_0);
	ADC_ChannelEnable(ADC_CHANNEL_0);
	ADC_BurstDisable();
  //ADC_StartCmd(ADC_START_ON_MAT01);			//Puse el ADC_PowerUp() directamente en el HANDLER del TIMER0 y nos evitamos el ADC_StartCmd(...)
  //ADC_EdgeStartConfig(ADC_START_ON_RISING);
	ADC_IntDisable(ADC_INT_CH0);
  //ADC_PowerUp(); 								//Lo puse directamente en el HANDLER del TIMER0 y nos evitamos el ADC_StartCmd(...)

}

void configDAC(void){

	DAC_Init();
	DAC_CONVERTER_CFG_T cfgDAC;
	cfgDAC.doubleBuffer = DISABLE;
	cfgDAC.dmaCounter = DISABLE;
	cfgDAC.dmaRequest = DISABLE;

	DAC_SetBias(DAC_700uA);
	DAC_ConfigDAConverterControl(&cfgDAC);
	DAC_UpdateValue(variableUsadaParaDAC);			//Aca va a ir la variable que vamos a determinar en otro lugar, que va a salir por el DAC

}

/* ==========================================================
 *                          DMA
 * ==========================================================
 */

//DMA: Utilizamos el DMA para pasar las muestras del ADC al DAC

void configDMA(){

	GPDMA_Channel_CFG_T dmaCFG;

		dmaCFG.channelNum = GPDMA_CH_0;
		dmaCFG.transferSize = 1; 			//Porque solo necesitamos transferir la muestra del adc (no guadrar los ultimos 10 valores p.ej)
		dmaCFG.type = GPDMA_P2P; 			//Porque transferimos del ADC al DAC
		dmaCFG.srcMemAddr = 0; 				//No usamos lugares de memoria en este channel del DMA
		dmaCFG.dstMemAddr = 0;
		dmaCFG.srcConn = GPDMA_ADC;
		dmaCFG.dstConn = GPDMA_DAC;
		dmaCFG.src.width = GPDMA_HALFWORD;	//Porque el ADC usa solo 12bits, poner palabra completa no es eficiente
		dmaCFG.src.burst = GPDMA_BSIZE_1;	//Porque solo hacemos 1 transferencia
		dmaCFG.src.increment = DISABLE;
		dmaCFG.dst.width = GPDMA_HALFWORD;	//Porque el DAC usa solo 10bits, poner palabra completa no es eficiente
		dmaCFG.dst.burst = GPDMA_BSIZE_1;
		dmaCFG.dst.increment = DISABLE;
		dmaCFG.intTC = ENABLE; 				//PROVISORIO: Capaz no lo usamos
		dmaCFG.intErr = ENABLE; 			//PROVISORIO: Por si salta algun error
		dmaCFG.linkedList = 0;

	GPDMA_Channel_CFG_T cfgDMA;
		cfgDMA.channelNum = GPDMA_CH_1;
		cfgDMA.transferSize = TRANSFERSIZE;                  	// Almacenar 10 muestras consecutivas
		cfgDMA.type = GPDMA_P2M;                   	// De Periférico (ADC) a Memoria (RAM)
		cfgDMA.srcMemAddr = 0;
		cfgDMA.dstMemAddr = (uint32_t) adc_buffer;  // Destino: nuestro vector en RAM
		cfgDMA.srcConn = GPDMA_ADC;
		cfgDMA.dstConn = 0;
		cfgDMA.src.width = GPDMA_HALFWORD;
		cfgDMA.src.burst = GPDMA_BSIZE_1;
		cfgDMA.src.increment = DISABLE;            	// El ADC es un registro fijo
		cfgDMA.dst.width = GPDMA_HALFWORD;
		cfgDMA.dst.burst = GPDMA_BSIZE_1;
		cfgDMA.dst.increment = ENABLE;             	// Incrementar para llenar el vector
		cfgDMA.intTC = ENABLE;                    	// Interrumpir cuando el buffer esté lleno
		cfgDMA.linkedList = 0;

	GPDMA_Init();
	GPDMA_SetupChannel(&dmaCFG);
	GPDMA_SetupChannel(&cfgDMA);

}

/* ==========================================================
 *                        HANDLERs
 * ==========================================================
 */

void TIMER0_IRQHandler(void){

	if(TIM_GetIntStatus(LPC_TIM0, TIM_MR1_INT) == 1){
			ADC_PowerUp();
			TIM_ClearIntPending(LPC_TIM0, TIM_MR1_INT);
		}
}

void ADC_IRQHandler(void){

	if(ADC_ChannelGetStatus(ADC_CHANNEL_0, ADC_DATA_DONE) == 1){

		adcValue = ADC_ChannelGetData(ADC_CHANNEL_0);
		GPDMA_ChannelStart(GPDMA_CH_1);
	}

}

void TIMER1_IRQHandler(void){
	if(TIM_GetIntStatus(LPC_TIM1, TIM_MR1_INT) == 1){
		GPIO_ClearPins(PORT_0, 1<<0);

		/*
		 * LOGICA DEL DUTY CYCLE
		 */


		TIM_ClearIntPending(LPC_TIM1, TIM_MR1_INT);
	}
}

void DMA_IRQHandler(void){

	//-------------CHANNEL 0-------------//

	if(GPDMA_IntGetStatus(GPDMA_INTTC, GPDMA_CH_0)){
		GPIO_ClearPins(PORT_0, 0x400000); 		//no hubo error
		testearDistancia(); 					//comparo la distancia entre el objeto y el sensor y decido que hacer

		GPDMA_ClearIntPending(GPDMA_CLR_INTTC, GPDMA_CH_0);
	}

	if(GPDMA_IntGetStatus(GPDMA_INTERR, GPDMA_CH_0)){
		GPIO_SetPins(PORT_0, 0x400000); //hubo error
		//frenar los motores, ver como hacer

		GPDMA_ClearIntPending(GPDMA_CLR_INTERR, GPDMA_CH_0);
	}

	//-------------CHANNEL 1-------------//

	if (GPDMA_IntGetStatus(GPDMA_INTTC, GPDMA_CH_1)) {

			// Calcular promedio de las 10 muestras para evitar ruidos (Moving Average)
	        static uint32_t i = 0;
	        uint32_t suma = 0;
	        uint32_t promedio_distancia = 0;

	        for(int i=0; i<TRANSFERSIZE; i++) {
	            suma += adc_buffer[i];
	        }
	        promedio_distancia = suma / TRANSFERSIZE;	//Moving Average

	        	// Rango límite: El objeto está muy cerca
				if (promedio_distancia > LIMITE_OBSTACULO) {
					// Frenar un motor y activar el otro para pivotear (girar)
					GPIO_SetPins  (PORT_1, PIN_MOTOR_IZQ_A); // Giro horario
					GPIO_ClearPins(PORT_1, PIN_MOTOR_IZQ_B);
					GPIO_ClearPins(PORT_1, PIN_MOTOR_DER_A); // Frena motor derecho para girar
					GPIO_ClearPins(PORT_1, PIN_MOTOR_DER_B);
				} else {
					// Seguir marchando hacia adelante
					GPIO_SetPins  (PORT_1, PIN_MOTOR_IZQ_A); // Giro horario
					GPIO_ClearPins(PORT_1, PIN_MOTOR_IZQ_B);
					GPIO_SetPins  (PORT_1, PIN_MOTOR_DER_A); // Giro horario
					GPIO_ClearPins(PORT_, PIN_MOTOR_DER_B);
				}

	        // Limpiar bandera y volver a encender el canal para la próxima ráfaga de mediciones
			GPDMA_ClearIntPending(GPDMA_CLR_INTTC, GPDMA_CH_1);
	        GPDMA_ChannelStart(GPDMA_CH_1);
	}
}

