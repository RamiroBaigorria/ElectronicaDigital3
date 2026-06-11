#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif
#include <stdlib.h> // Para usar la función abs()

/*Ejercicio 1 del parcial de ED3 del año 2026
1)Emitir la "seq" por el pin P0.15 usando SysTick
2)Ante cada interrupcion por flaco ascendente en P2.15, debe alternar la duracion de los elementos de la secuencia entre 5[mS] y 10[mS]. Considerar 5[mS] como valor inicial
3)Ante una interrupcion EINT1, por flanco de bajada, debe detener la secuencia deshabilitando SysTick, y dejar el pin P0.15 en estado logico bajo

LAS CORRECCIONES ESTAN HECHAS CON COMENTARIOS AL LADO DE CADA LINEA
*/

uint32_t nTicks[2]={0x927BF,completar}, cTicks[2]={0x00,0x07}; //Completar: 0x124F7F

uint8_t seq[15] = {0,0,0,0,1,0,1,0,1,0,1,0,0,0,0};


void EINT3_IRQHandler(void){

	static uint8_t index = 1;						// static uint8_t index = 0;

	if(LPC_GPIOINT->IntStatus &= (1<<2)){ 			// if(LPC_GPIOINT->IOIntStatus & (1<<2)){

		if(LPC_GPIOINT->IO2IntStatF &=(1<<15)){		// if(LPC_GPIOINT->IO2IntStatR & (1<<13)) {

			cfgSysTick(nTicks[index], cTicks[0]);	// cfgSysTick(nTicks[index], cTicks[1]);

			index=(index+1)%1;						// index = (index+1) % 2 ;

			//no limpia int							// LPC_GPIOINT->IO2IntClr |= (1<<13)
		}
	}
}

void EINT1_IRQHandler(void){

	cfgSysTick(nTicks[0],cTicks[0]);
	LPC_GPIO2->FIOSET |= (1<<15);					// LPC_GPIO0->FIOCLR |= (1<<15);
	LPC_GPIOINT->IO0IntClr |= (1<<15);				// LPC_SC->EXTINT |= (1<<1);
}

void SysTick_Handler(void){
	static uint8_t index=0;							// static uint8_t i=0;

	if(seq[i]){
		LPC_GPIO0->FIOCLR=(1<<15);					//LPC_GPIO0->FIOSET = (1<<15);
	}

	if(!seq[i]){
		LPC_GPIO0->FIOSET=(1<<15);					// LPC_GPIO0->FIOCLR=(1<<15);
	}

	index=(index+1)%15;								// i=(i+1)%15;
}

void cfgSysTick(uint32_t nTicks, uint8_t cTicks){	// void cfgSysTick(uint32_t nTicks, uint32_t cTicks)

	//Reinicio										// SysTick->CTRL = 0;
	SysTick->LOAD = nTicks;
	SysTick->VAL = 0x01;							// SysTick->VAL = 0;
	SysTick->CTRL = cTicks;
}


//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------


//Ejercicio 2 del parcial de ED3 del año 2026



// Estructuras de datos globales del enunciado
uint16_t perfil[2][10] = {
    {10, 30, 60, 85, 100, 100, 70, 40, 20, 0},   // Fila 0: Ciclos de trabajo (%)
    {300, 450, 600, 600, 1200, 900, 600, 450, 300, 0} // Fila 1: Tiempos (segundos)
};

// Variables de control de estado
volatile uint32_t ms_counter = 0;
volatile uint32_t segundos_transcurridos = 0;
volatile uint8_t flag_10s = 0;
volatile uint8_t paso_actual = 0;
volatile uint8_t sistema_estable = 0;
volatile uint8_t duty_actual = 0;

void conf_timer0_pwm(uint8_t duty) {
    LPC_SC->PCONP |= (1 << 1);       // Alimentar Timer 0
    LPC_TIM0->TCR = 0x02;            // Resetear el contador (pone TC y PR en 0)
    LPC_TIM0->PR = 0;                // Prescaler en 0
    LPC_TIM0->MR0 = 24999;           // Período de 1ms @ 25MHz (25000 tics)


    LPC_TIM0->MR1 = ((LPC_TIM0->MR0) * duty) / 100;	// CORRECCIÓN 1: El cálculo del ciclo de trabajo debe basarse en MR0 (25000)

    LPC_TIM0->MCR = (1 << 0) | (1 << 1) | (1 << 3); // CORRECCIÓN 2: Int en MR0, Reset en MR0, Int en MR1

    LPC_TIM0->TCR |= (1 << 0);						// CORRECCIÓN 3: Encender el Timer correctamente (Bit 0 = 1, Bit 1 = 0)
}

int main(void) {
	/*
	 * CODIGO que no se pide en el ejercicio pero se da por hecho
	 */

    // Variables locales para verificar estabilidad térmica
    uint16_t temp_actual = 0;
    uint16_t temp_anterior = 0;
    uint8_t primera_medicion = 1;

    conf_timer0_pwm(duty_actual);
    NVIC_EnableIRQ(TIMER0_IRQn);

    // ====================================================================
    // i) y ii) Bucle de Medición y Espera de Estabilización (Cada 10s)
    // ====================================================================
    while (!sistema_estable) {
        if (flag_10s) {
            flag_10s = 0; // Bajar bandera de software

            // Acondicionamiento de señal del enunciado: T(°C) = Valor_Convertido / 4
            // Leemos el canal del ADC (ej. Canal 0 de ADDR0)
            uint16_t adc_raw = (LPC_ADC->ADDR0 >> 4) & 0xFFF;
            temp_actual = adc_raw / 4;

            if (primera_medicion == 1) {
                temp_anterior = temp_actual;
                primera_medicion = 0;
            } else {
                // El enunciado dice: "estable una vez que la diferencia entre mediciones es menor a 10 °C."
                if (abs(temp_actual - temp_anterior) < 10) {
                    sistema_estable = 1; // Rompe el lazo de estabilización
                }

                temp_anterior = temp_actual;
            }
        }
    }

    // ====================================================================
    // iii) Configuración de PWM dinámico y ejecución del Perfil Térmico
    // ====================================================================
    segundos_transcurridos = 0; // Reiniciar contador de tiempo de perfiles

    while (paso_actual < 10) {
        // Si llegamos al final del perfil marcado por un tiempo de 0 segundos
        if (perfil[1][paso_actual] == 0) {
            break;
        }

        // Actualizar el Duty Cycle del Timer con el valor del perfil
        duty_actual = perfil[0][paso_actual];
        LPC_TIM0->MR1 = (25000 * duty_actual) / 100;

        // Esperar en bajo consumo a que la ISR del timer dictamine que se cumplió el tiempo
        while (segundos_transcurridos < perfil[1][paso_actual]) {
            __WFI(); // Wait For Interrupt (Ahorro de energía)
        }

        // Pasar al siguiente tramo del perfil térmico
        segundos_transcurridos = 0;
        paso_actual++;
    }

    // Fin del proceso normal: Apagar todo
    LPC_TIM0->TCR = 0; // Detener Timer
    LPC_GPIO0->FIOCLR = (1 << 0); // Apagar horno

    while(1);
}

// ====================================================================
// b) ISR de EINT0 - Interrupción de Falla Grave (Corte de Energía)
// ====================================================================
void EINT0_IRQHandler(void) {
    LPC_TIM0->TCR = 0;            // Deshabilitar el temporizador de inmediato
    LPC_GPIO0->FIOCLR = (1 << 0); // Apagar de forma mandatoria la carga (Resistencia)
    LPC_GPIO0->FIOSET = (1 << 1); // Encender la alarma sonora (Falla crítica)

    LPC_SC->EXTINT = (1 << 0);    // Limpiar bandera de interrupción física

    while(1);                     // Bloquear el sistema por seguridad
}

// ====================================================================
// c) ISR de TIMER0 - Control de PWM por software y Cronómetro
// ====================================================================
void TIMER0_IRQHandler(void) {
    // Caso 1: Match 0 -> Fin del ciclo de 1ms (Inicio de período)
    if (LPC_TIM0->IR & (1 << 0)) {
        LPC_TIM0->IR = (1 << 0); // Limpiar bandera de interrupción

        if (duty_actual > 0) {
            LPC_GPIO0->FIOSET = (1 << 0); // Encender la resistencia
        }

        // Contadores de tiempo reales
        ms_counter++;
        if (ms_counter >= 1000) {
            ms_counter = 0;
            segundos_transcurridos++; // Cuenta los segundos del paso térmico actual

            // Cada 10 segundos levanta el flag para el chequeo del ADC en el Main
            static uint8_t cuenta_10s = 0;
            cuenta_10s++;
            if (cuenta_10s >= 10) {
                cuenta_10s = 0;
                flag_10s = 1;
            }
        }
    }

    // Caso 2: Match 1 -> Tiempo de encendido (Duty Cycle cumplido)
    if (LPC_TIM0->IR & (1 << 1)) {
        LPC_TIM0->IR = (1 << 1); // Limpiar bandera de interrupción

        if (duty_actual < 100) {
            LPC_GPIO0->FIOCLR = (1 << 0); // Apagar la resistencia por el resto del ms
        }
    }
}


//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

/*                                               Ejercicio 3 del parcial de ED3 del año 2026
 *
 * 	Explique el funcionamiento Nested Vectored Interrupt Controller (NVIC) ante la llegada de una interrupecion. Suponga la llegada de una interrupción cuyo ID es 18
 * 	correspondiente a External Interrupt 0 (EINT0) describa lo que sucede, considerando que la ISR se encuentra en la posicion de memoria 0x0800030C
 *
 */

/*
 * 	Cuando un periferico solicita una interrupción, por ejenmplo la interrupcion del EINT0 (Cuyo ID es 18), el periferico genera la solicitud y el NVIC (Nested Vectored
 * Interrupt Controller) del Cortex-M3 verifica que este habilitado y observa su prioridad. Si corresponde atenderla, el Cortex-M3 guarda automaticamente en la pila el contexto
 * actual, protegiendo el flujo de programa principal sin necesidad de hacerlo por software. Posteriormente, el NVIC lee la direccion de la funcion ISR correspondiente desde
 * la Tabla de Vectores de Interrupcion de la memoria flash y la carga en la PC. El procesador pasa a modo "Handler Controller" y, utilizando el puntero principal de la
 * pila (MSP), marca la interrupcion como "Activa". La rutina de servicio se ejecuta y, al finalizar, el hardware restaura automaticamente el contexto previamente guardado,
 * continuando la ejecucion normal del programa. El NVIC permite ademas el anidamiento de interrupciones segun prioridades.
 * 	La formula matematica para calcular el desplazamiento entre dos posiciones de memoria es:
 * 	offSetFinal = OffSetInicial + (deltaID * 4 bytes)
 *
 */
