#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif
#include <cr_section_macros.h>

void EINT3_IRQHandler(void){ //Crear el Handler específico (EINT3 maneja Puertos 0 y 2)

	LPC_GPIOINT->IO0IntClr = (1 << 0); //Limpiar la bandera de interrupción (¡Fundamental hacerlo apenas entrás!)(Va abajo de cualquier cambio que se haga en la func

	if(LPC_GPIO0->FIOPIN & (1 << 22)){ //Ejecutar la acción (Cambiar de estado el LED)
			LPC_GPIO0->FIOCLR = (1 << 22); // Si estaba prendido, lo apaga
		} else {
			LPC_GPIO0->FIOSET = (1 << 22); // Si estaba apagado, lo prende
		}
	LPC_SC -> EXTINT |= (1 << 3);

}
int main(void) { //Configurar pines (Entradas/Salidas)

	LPC_GPIO0->FIODIR |= (1 << 22); // P0.22 como salida (LED)

    LPC_PINCON->PINSEL4 &= ~(3 << 22);	// Limpiamos bits 23 y 22
    LPC_PINCON->PINSEL4 |=  (1 << 22);	// Seteamos el bit 22 en 1 (Función EINT3)

    LPC_SC->EXTMODE |= (1 << 3); // EINT3 se dispara por flanco
    LPC_SC->EXTPOLAR &= ~(1 << 3); // EINT3 por flanco de bajada


	NVIC_EnableIRQ(EINT3_IRQn); //Encender el canal de interrupción en el NVIC
	NVIC_SetPriority(EINT3_IRQn, 0); // Le damos la mayor prioridad

		while(1){ // El programa se queda acá sin hacer nada hasta que apretás el botón
		}

	return 0;
}
