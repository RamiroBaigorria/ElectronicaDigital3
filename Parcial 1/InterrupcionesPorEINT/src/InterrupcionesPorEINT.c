//  Ejemplo de cómo usar Interrupciones Externas en el LPC1769. La idea principal es un LED que parpadea, y cuya velocidad cambia cada vez que presionas un botón conectado al pin P0.15, usando...
// ...una interrupción Externa Dedicada, en lugar de usar las interrupciones de los pines GPIO comunes

#ifdef __USE_CMSIS
#include "LPC17xx.h" // Contiene todas las direcciones de memoria que vimos en las tablas de registros (LPC_GPIO0, FIODIR, etc.).
#endif

#include <cr_section_macros.h>

// Los siguientes VOIDs le avisan al compilador que más abajo existen estas funciones para configurar los pines y las interrupciones.
void confGPIO(void); // Conf. de puertos
void confIntExt(void); // Conf. de interrupciones externas
void EINT0_IRQHandler(void); //Funci�n de interrupci�n EINT0
void retardo(uint32_t tiempo); //En el LPC1769, ciertos pines tienen una "línea directa" al procesador para interrupciones, y el pin P2.10 es el encargado de la EINT0.
uint8_t inte = 0; //// Una variable global que cuenta cuántas veces ocurrió cierta interrupción. Al ser global, puede ser vista tanto por el main como por el manejador de interrupciones (EINT3_IRQHandler)

int main(void) {
	uint32_t tiempo;
	confGPIO();
	confIntExt();

	//El bucle principal (Infinito)
	while(1){
		//Utilizando el operador modulo (%), determina si la variable "inte" es par o impar
		if(inte%2){
			tiempo = 1000000;
		}
		else {
			tiempo = 4000000;
		}

		LPC_GPIO0->FIOSET = (1<<22); //Pone un '1' en el bit 22 del Puerto 0 (Prende el LED de la posicion 22).
		retardo(tiempo); //Pausa el procesador ejecutando un ciclo vacío.
		LPC_GPIO0->FIOCLR = (1<<22); //Pone un '0' en el bit 22 (limpia o apaga el LED de la posicion 22).
		retardo(tiempo); //Pausa el procesador ejecutando un ciclo vacío.
	}
	return 0;
}

void retardo (uint32_t tiempo){
	for(uint32_t conta = 0; conta<tiempo;conta++){}
	return;
}

void confGPIO(void){
	LPC_GPIO0->FIODIR |= (1<<22); //Configura P0.22 como Salida (LED).
	return;
}
void confIntExt(void){
	LPC_PINCON->PINSEL4 |= (1<<20); //El pin físico P2.10 ya no será un simple GPIO, ahora funcionará como la entrada de interrupción EINT0. (El bit 20 de PINSEL4 controla la función del pin P2.10)
	LPC_SC->EXTINT      |= 1; //Limpia la bandera de interrupción de EINT0 escribiendo un 1. Es como "resetear" el sensor antes de empezar.
	LPC_SC->EXTMODE     |= 1; //Configura la interrupción por flanco (edge sensitive) en lugar de nivel.
	LPC_SC->EXTPOLAR    |= 1; //Configura que el flanco sea de subida (cuando el botón pasa de 0 a 1).
	NVIC_EnableIRQ(EINT0_IRQn); //Habilita en el núcleo ARM específicamente la interrupción EINT0.
	return;
}
void EINT0_IRQHandler(void)
{
	inte++; //inte ahora solo se modifica cuando ocurre la interrupción específica EINT0.
	LPC_SC->EXTINT |= 1;   //Limpia bandera de interrupci�n
	return;
}
