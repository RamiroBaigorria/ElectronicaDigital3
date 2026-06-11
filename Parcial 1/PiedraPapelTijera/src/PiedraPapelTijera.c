/*
Entradas:
Px.0 Piedra
Px.1 Papel
Px.2 Tijera

Salidas:
Px.4 Empate
Px.5 Gana el jugador
Px.6 Gana el microcontrolador

Logica:
1. El microcontrolador debe permanecer en espera mientras los tres bits menos
significativos del puerto se encuentren en el estado lógico 0 0 0 (ningún botón presionado).
2. Cuando el estado del puerto cambie a 001 (1), 010 (2) o 100 (4), se considerará que el
usuario ha realizado una jugada válida.
3. A partir de ese momento el programa debe determinar la jugada del usuario, generar la
jugada del microcontrolador, determinar el resultado del juego y encender el LED
correspondiente.
4. Luego el sistema puede volver al estado de espera para aceptar una nueva jugada.

0 = Piedra
1 = Papel
2 = Tijera

Resultado:
El resultado del juego debe calcularse utilizando una matriz resultado[jugador][cpu], donde
1 indica que gana el jugador, 0 indica empate y -1 indica que gana el microcontrolador.
int resultado[3][3] = {
 { 0, -1, 1},
 { 1, 0, -1},
 {-1, 1, 0}
};

Salida por consola:
Además de indicar el resultado mediante LEDs, el programa deberá utilizar semihosting
para imprimir información en la consola del entorno de desarrollo.
En cada jugada válida el programa deberá mostrar:
- la jugada ingresada por el usuario
- la jugada generada por el microcontrolador
- el resultado de la ronda
No es necesario implementar un sistema de puntaje ni almacenar resultados previos. El
programa solo debe informar el resultado de la jugada actual.

 */

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif
#include <cr_section_macros.h>
#include <stdlib.h> //Para que la maquina elija aleatoriamente
#include <stdio.h>  //Para el semihosting

void retardo(){
	for(int i = 0 ; i < 120000 ; i++);
}

int main(void){

	//Entradas P0.0 ,P0.1,P0.2 (PULSADORES)
	LPC_GPIO0 -> FIODIR &= ~((1 << 0) | (1 << 1) | (1 << 2));

	//Salidas P0.4 ,P0.5 ,P0.6 (LEDS)
	LPC_GPIO0 -> FIODIR |= ((1 << 4) | (1 << 5) | (1 << 6));

	int resultados[3][3] = {   //Tabla de resultados
			 { 0, -1, 1},
			 { 1, 0, -1},
			 {-1, 1, 0}
			};

	int mapa[8] = { -1, 0, 1, -1, 2, -1, -1, -1 };

	char *nombres[] = {"Piedra", "Papel", "Tijera"};

	while(1){
		while((LPC_GPIO0 -> FIOPIN & (0b111)) == 0); // Me quedo en espera hasta que se presione algun boton
		uint32_t lectura = LPC_GPIO0 -> FIOPIN;      //Hacemos una lectura del puerto

		//Guardamos la jugada que se hizo
		int mascara = (lectura & (0b111)); //Mascara que nos devuelve un numero en binario entre 0-7
		int jugada = mapa[mascara];        //obtenemos el indice de la jugada

		if(jugada == -1){
			//Jugada invalida -> no hacemos nada
		}
		else{
			LPC_GPIO0 -> FIOCLR = (1 << 4) | (1 << 5) | (1 << 6);
			int cpu = rand() % 3;
			int res = resultados[jugada][cpu];

			if(res == 0){
				LPC_GPIO0 -> FIOSET = (1 << 4); //Prendemos led de empate
			}
			else if(res == 1){
				LPC_GPIO0 -> FIOSET = (1 << 5); //Prendemos led de jugador
			}
			else{
				LPC_GPIO0 -> FIOSET = (1 << 6); //Prendemos led de cpu
			}
			printf("Jugador: %s  // CPU: %s // Resultado: %d\n", nombres[jugada], nombres[cpu], res);
			retardo();
		}
	}
	return 0;
}
