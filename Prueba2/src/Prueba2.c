#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>

//Definiciones de hardware según el esquema anterior
#define LED_ROJO  (1 << 22)   // P0.22 (Empate)
#define LED_VERDE (1 << 25)   // P3.25 (Gana J1)
#define LED_AZUL  (1 << 26)   // P3.26 (Gana J2)

// Matriz de resultados: 0=Empate, 1=Gana J1, 2=Gana J2
const int TABLA_RESULTADOS[3][3] = {
    {0, 2, 1}, // J1 eligió Piedra
    {1, 0, 2}, // J1 eligió Papel
    {2, 1, 0}  // J1 eligió Tijera
};

// Función para obtener el índice (0, 1 o 2) basado en los pines de entrada
int obtenerEleccion(uint32_t puerto, int offset) {
    if (puerto & (1 << (offset + 0))) return 0; // Piedra
    if (puerto & (1 << (offset + 1))) return 1; // Papel
    if (puerto & (1 << (offset + 2))) return 2; // Tijera
    return -1; 	// Ninguna tecla presionada
}

int main(void) {
    // Configuración básica de GPIOs
    LPC_GPIO0->FIODIR |= LED_ROJO;
    LPC_GPIO3->FIODIR |= (LED_VERDE | LED_AZUL);
    LPC_GPIO0->FIODIR &= ~(0x3F); // P0.0 al P0.5 como entradas

    while(1) {
        uint32_t entradas = LPC_GPIO0->FIOPIN;

        // Obtenemos elecciones: J1 usa P0.0-0.2, J2 usa P0.3-0.5 (offset 3)
        int el_J1 = obtenerEleccion(entradas, 0);
        int el_J2 = obtenerEleccion(entradas, 3);

        // Apagar todos los LEDs (Lógica inversa: SET apaga si están a VCC)
        LPC_GPIO0->FIOSET = LED_ROJO;
        LPC_GPIO3->FIOSET = LED_VERDE | LED_AZUL;

        if (el_J1 != -1 && el_J2 != -1) {
            // ¡Aquí ocurre la magia de la matriz!
            int resultado = TABLA_RESULTADOS[el_J1][el_J2];

            if (resultado == 0)      LPC_GPIO0->FIOCLR = LED_ROJO;
            else if (resultado == 1) LPC_GPIO3->FIOCLR = LED_VERDE;
            else                     LPC_GPIO3->FIOCLR = LED_AZUL;
        }
    }
}
