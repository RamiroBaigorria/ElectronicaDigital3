//--------------------- EJERCICIO 1 ---------------------
// Utilizando interrupciones por SysTick y por eventos externos EINT, realizar un programa que permita habilitar y deshabilitar el temporizador por flanco ascendente en el pin P2.11
// El temporizador debe desbordar cada 10 ms utilizando un clock de cclk=62MHz.
// Por cada interrupción del SysTick, se debe mostrar por el puerto
// Detallar cálculos para obtener el valor de RELOAD y asegurar que la interrupción por SysTick sea de mayor prioridad que EINT.


#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>

// Variables globales
uint8_t values[8] = {10, 20, 30, 40, 50, 60, 70, 80}; // Ejemplo de valores
volatile uint8_t habilitado = 0;

// --- Prototipos de Funciones ---
void config_puerto(void);
void config_EINT(void);
void systick_init(void);

int main(void) {
	config_puerto(); // Configura P0.0-P0.7 como salida
	config_EINT();   // Configura P2.11 e interrupción externa
	systick_init();  // Configura SysTick (pero no lo arranca)

    while(1) {
        // El trabajo pesado se hace en las interrupciones
        __WFI(); // Sleep mode para ahorrar energía
    }
}

void config_puerto(void) {

    LPC_GPIO0->FIODIR |= 0xFF;	// Configuramos los primeros 8 pines del Puerto 0 como salida
    LPC_GPIO0->FIOCLR = 0xFF;	// Limpiamos el puerto al iniciar
}

void config_EINT(void)
{
    //Configuramos P2.11 como EINT3 (PINSEL4 bits 23:22 en 01)
    LPC_PINCON->PINSEL4 &= ~(3 << 22);	// Limpiamos bits 23 y 22
    LPC_PINCON->PINSEL4 |=  (1 << 22);	// Seteamos el bit 22 en 1 (Función EINT3)

    LPC_GPIOINT->IO2IntEnR |= (1 << 11);	//Habilitamos la interrupcion por flanco ascendente (Rising Edge) en P2.11

    //NVIC_SetPriority(EINT3_IRQn, 1);	//Configurar Prioridad en el NVIC (Prioridad 1)
    NVIC_IP[21] = (uint8_t) (p<<3);

    //NVIC_EnableIRQ(EINT3_IRQn);	//Habilitamos la interrupción en el NVIC
    NVIC -> ISER[0] |= (1<<21);
}

void systick_init(void) {

    SysTick->LOAD = 619999; //Cargamos el valor calculado para 10ms a 62MHz

    SysTick->VAL = 0;	//Limpiamos el contador actual

    NVIC_SetPriority(SysTick_IRQn, 0);	//Configurar Prioridad en el NVIC (Prioridad 1)

    // 4. Configuración de Control:
    // Bit 2: Clock del procesador (1)
    // Bit 1: Habilitar interrupción (1)
    // Bit 0: Enable (0) -> Arranca apagado
    SysTick->CTRL = (1 << 2) | (1 << 1);
}

void EINT3_IRQHandler(void) {	// Interrupción de EINT3 (P2.11)
    // Verificar si la interrupción fue por P2.11
    if (LPC_GPIOINT->IO2IntStatR & (1 << 11)) {

        if (habilitado == 0) {
            SysTick->CTRL |= (1 << 0); // Habilitar temporizador (Encendemos el bit ENABLE)
            habilitado = 1;
        } else {
            SysTick->CTRL &= ~(1 << 0); // Deshabilitar temporizador (Apagamos el bit ENABLE)
            habilitado = 0;
        }

        LPC_GPIOINT->IO2IntClr = (1 << 11); //Limpiamos la FLAG de interrupcion del GPIO
    }
}

// Interrupción de SysTick (Cada 10ms si está habilitado)
void SysTick_Handler(void) {
    uint16_t valor_parcial = 0;

    // Calcular promedio
    for(int i = 0; i < 8; i++) {
    	valor_parcial += values[i];
    }

    uint8_t promedio = (uint8_t)(valor_parcial >> 3); // Desplazar 3 bits a la derecha (>> 3) es lo mismo que dividir por 2^3 = 8

    // Mostrar por Puerto 0
    LPC_GPIO0->FIOCLR = 0xFF; // Limpiar puerto
    LPC_GPIO0->FIOSET = promedio; // Setear valor
}
