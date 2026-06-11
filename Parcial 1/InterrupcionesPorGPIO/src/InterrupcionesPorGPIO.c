//  Ejemplo de cómo usar Interrupciones Externas en el LPC1769. La idea principal es un LED que parpadea, y cuya velocidad cambia cada vez que presionas un botón conectado al pin P0.15, usando...
// ...una interrupción en lugar de estar preguntando todo el tiempo si se presionó el botón (polling)

#ifdef __USE_CMSIS
#include "LPC17xx.h" // Contiene todas las direcciones de memoria que vimos en las tablas de registros (LPC_GPIO0, FIODIR, etc.).
#endif

#include <cr_section_macros.h>

// Los siguientes VOIDs le avisan al compilador que más abajo existen estas funciones para configurar los pines y las interrupciones.
void confGPIO(void); // Conf. de puertos
void confIntGPIO(void); // Conf. de interrupciones por GPIO
void EINT3_IRQHandler(void)
void retardo(uint32_t tiempo);
uint8_t inte = 0; // Una variable global que cuenta cuántas veces ocurrió cierta interrupción. Al ser global, puede ser vista tanto por el main como por el manejador de interrupciones (EINT3_IRQHandler)

int main(void) {
	uint32_t tiempo;

	//Llama a las funciones de configuración antes de entrar al bucle infinito.
	confGPIO();
	confIntGPIO();

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
	for(uint32_t conta = 0; conta<tiempo;conta++){

	}
	return;
}
void confGPIO(void){
	LPC_GPIO0->FIODIR |= (1<<22); //Configura P0.22 como Salida (LED).
	LPC_GPIO0->FIODIR &=~ (1<<15); //Configura P0.15 como Entrada (Botón).
	return;
}
void confIntGPIO(void){
	LPC_GPIOINT->IO0IntEnR |= (1<<15); //Habilita la interrupción por flanco de subida (Rising Edge) en el pin P0.15. La interrupción saltará cuando el botón pase de 0V a 3.3V
	LPC_GPIOINT->IO0IntClr |= (1<<15); //Limpia cualquier basura o bandera vieja en ese pin para empezar de cero.
	NVIC_EnableIRQ(EINT3_IRQn);        // Habilita de interrupciones externas. (Le dice al controlador de interrupciones del núcleo ARM (NVIC) que deje pasar las interrupciones del grupo "EINT3" (donde están todos los GPIO).)
	return;
}

//Esta funcion no se llama en el código, sino que el procesador salta aquí automáticamente cuando el pin P0.15 cambia de estado.
void EINT3_IRQHandler(void)
{
	if(LPC_GPIOINT->IO0IntStatR & (1<<15)){ //Como muchos pines comparten esta interrupción, el chip pregunta: "¿Fuiste vos, P0.15, el que causó esto?"
		inte++; //Incrementa el contador que luego usa el main para decidir la velocidad.
		LPC_GPIOINT->IO0IntClr |= (1<<15); //limpia la bandera de interrupción
	}
	return;
}
