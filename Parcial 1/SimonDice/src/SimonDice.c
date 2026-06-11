//Electronica Digital 3 - Simon Dice
//Baigorria, Ramiro
//Cravero, Lorenzo
//Gonzalez, Tobias

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include <stdint.h>

#define MAX_SECUENCIA 10
#define CANT_BOTONES  4
#define CANT_LEDS     4

#define TIEMPO_LED_ON   1000
#define TIEMPO_LED_OFF   300

#define BOTON_0    (1 << 0)   // P0.0
#define BOTON_1    (1 << 1)   // P0.1
#define BOTON_2    (1 << 2)   // P0.2
#define BOTON_3    (1 << 3)   // P0.3

#define MASK_BOTONES   (BOTON_0 | BOTON_1 | BOTON_2 | BOTON_3)

//-----------------------------------------------------------------------------------------

//Definición de un arreglo (array) constante(solo lectura). Es el "mapa" que le dice al programa exactamente qué bits debe tocar para encender cada LED físico.
const uint32_t led_mask[CANT_LEDS] = {	//uint32_t: El tipo de dato. Es un entero de 32 bits sin signo.
										//led_mask: Nombre que le damos al arreglo.
										//[CANT_LEDS]: Define el tamaño del arreglo. Como CANT_LEDS se definio antes como 4, el arreglo vale 4 (0,1,2,3)
										//Dentro de las llaves { } definimos qué valor tiene cada posición del arreglo
    (1 << 0),   // El LED 1 corresponde al pin P2.0. Cuando se usa este valor en FIOSET, se prenderá el primer LED.
    (1 << 1),   // El LED 2 corresponde al pin P2.1. Cuando se usa este valor en FIOSET, se prenderá el segundo LED.
    (1 << 2),   // El LED 3 corresponde al pin P2.2. Cuando se usa este valor en FIOSET, se prenderá el tercer LED.
    (1 << 3)    // El LED 4 corresponde al pin P2.3. Cuando se usa este valor en FIOSET, se prenderá el cuarto LED.
};

//-----------------------------------------------------------------------------------------

typedef enum //Se le pone nombre a cada ESTADO de mi maquina de estados
{
    ESTADO_IDLE = 0,
    ESTADO_INICIO,
    ESTADO_GENERAR_PASO,
    ESTADO_MOSTRAR_SECUENCIA,
    ESTADO_ESPERAR_JUGADOR,
    ESTADO_VALIDAR_JUGADA,
    ESTADO_RONDA_SUPERADA,
    ESTADO_GAME_OVER,
    ESTADO_VICTORIA
} estado_juego_t;

typedef enum
{
    SUB_ENCENDER_LED = 0,
    SUB_ESPERAR_LED_ON,
    SUB_APAGAR_LED,
    SUB_ESPERAR_LED_OFF
} subestado_mostrar_t;

//-----------------------------------------------------------------------------------------

void config_botones(void);
void leds_init(void);
void leds_apagar_todos(void);
void led_encender(uint8_t indice);
void led_apagar(uint8_t indice);
void led_mostrar(uint8_t indice);
uint8_t tiempo_cumplido(uint32_t referencia, uint32_t demora_ms);
uint8_t boton_sigue_presionado(uint8_t boton);
void demora_150ms(void);
void systick_init(void);

//-----------------------------------------------------------------------------------------

//El Estado Actual
volatile estado_juego_t estado_actual = ESTADO_IDLE;	//estado_actual: Es la variable que guía al switch gigante en el main
														//ESTADO_IDLE: Define que el juego arranca "en reposo", esperando que alguien presione una tecla para comenzar
//Secuencia y su Progreso
uint8_t secuencia[MAX_SECUENCIA] = {0}; //Es un arreglo (un "estante" con 10 espacios si MAX_SECUENCIA es 10) donde guardaremos números del 0 al 3. Cada número representa qué LED debe encenderse.
uint8_t longitud_secuencia = 0; //Indica cuántos pasos de ese estante estamos usando actualmente. En la primera ronda vale 1, en la segunda 2, y así hasta llegar al máximo

//Los Índices de Control
uint8_t indice_mostrar = 0; //Se usa cuando la placa te muestra las luces. Va de 0 hasta "longitud_secuencia"
uint8_t indice_jugador = 0; //Se usa cuando vos repetís la secuencia. Sirve para comparar tu presión actual con lo que hay guardado en la secuencia (Índice usado para validar la entrada del jugador)


//Comunicación con las Interrupciones (Volátiles)
volatile int8_t boton_presionado = -1; //Guarda cuál boton presiono el jugador (0, 1, 2 o 3). Empieza en -1 porque ese valor no corresponde a ningún botón real.
volatile uint8_t evento_boton = 0; //Es una "bandera" (flag). Se pone en 1 cuando hay una pulsación nueva. El main la mira, procesa el botón y la vuelve a poner en 0
volatile uint8_t evento_start = 0; //Bandera de inicio de juego. Sirve para sacar al juego del modo reposo (IDLE).

//Puntaje y Errores
uint8_t nivel = 0; //Almacena cuántas rondas se superaron (Puntaje o nivel actual)
uint8_t error_jugada = 0; // ¡ERROR! Si el jugador presiona el botón equivocado, esta variable se pone en 1 para activar el estado de GAME OVER.

//-----------------------------------------------------------------------------------------

//Contador universal de tiempo y la "Marca de Tiempo" de referencia
volatile uint32_t ticks_ms = 0; //Variable que cambia dentro de las interrupciones. Se incrementa en 1 cada vez que pasa un milisegundo dentro de la interrupción del SysTick.
uint32_t tiempo_referencia = 0; //Cronometro parcial. Es util para saber cuando prender y apagar un led mientras se esta ejecutando el juego

//Control de la submáquina de luces
subestado_mostrar_t subestado_mostrar = SUB_ENCENDER_LED; 	//subestado_mostrar: Controla en qué paso de la animación de luces estamos.
															//Mostrar una luz no es un solo paso; es una secuencia: Prender -> Esperar -> Apagar -> Esperar. Esta variable recuerda si estamos apenas encendiendo el LED o si ya estamos esperando para apagarlo

//Barrera de seguridad del usuario
volatile uint8_t habilitar_lectura_usuario = 0; //Cuando la placa está mostrando la secuencia de luces, esta variable vale 0. Si el usuario toca un botón en ese momento, la interrupción de GPIO va a ver que el permiso está en 0 e ignorará la pulsación

//variable que voy a usar para la animacion de victoria final
uint8_t paso_animacion = 0;
//-----------------------------------------------------------------------------------------

int main(void)
{
    leds_init();
    config_botones();
    systick_init();

    while (1)
    {
        switch (estado_actual)
        {
            case ESTADO_IDLE:
            {
                leds_apagar_todos();
                habilitar_lectura_usuario = 0;

                if (evento_start)
                {
                    evento_start = 0;
                    estado_actual = ESTADO_INICIO;
                }
                break;
            }

            case ESTADO_INICIO:
            {
                longitud_secuencia = 0;
                indice_mostrar = 0;
                indice_jugador = 0;
                boton_presionado = -1;
                evento_boton = 0;
                nivel = 0;
                error_jugada = 0;

                tiempo_referencia = 0;
                subestado_mostrar = SUB_ENCENDER_LED;
                habilitar_lectura_usuario = 0;

                leds_apagar_todos();

                estado_actual = ESTADO_GENERAR_PASO;
                break;
            }

            case ESTADO_GENERAR_PASO:
                        {

                            if (longitud_secuencia < MAX_SECUENCIA) {	//Verificamos si todavía hay espacio en la secuencia

                                secuencia[longitud_secuencia] = (ticks_ms % CANT_BOTONES);	//Generamos un nuevo paso aleatorio (0 a 3) usando el contador de milisegundos como fuente de azar

                                longitud_secuencia++;	//Incrementar la longitud para la próxima ronda

                                //Reiniciar variables para la fase de "Mostrar"
                                indice_mostrar = 0;
                                subestado_mostrar = SUB_ENCENDER_LED;

                                estado_actual = ESTADO_MOSTRAR_SECUENCIA;	//Cambiar al estado que reproduce las luces
                            }
                            else {
                                estado_actual = ESTADO_VICTORIA;	// Si llegamos al máximo definido, el jugador ganó el juego
                            }
                            break;
                        }
            case ESTADO_MOSTRAR_SECUENCIA:
            {
            	habilitar_lectura_usuario = 0; // Deshabilitamos los botones para que el usuario no interrumpa la animación

            	switch (subestado_mostrar)
				{
					case SUB_ENCENDER_LED: //Este CASE elige el LED correcto del arreglo "secuencia" y despues lo prende.
					{
						led_mostrar(secuencia[indice_mostrar]);	// Tomamos el LED que corresponde según el índice actual de la secuencia

						tiempo_referencia = ticks_ms;	// Guardamos el tiempo actual para saber cuándo apagarlo

						subestado_mostrar = SUB_ESPERAR_LED_ON;

						break;
					}

					case SUB_ESPERAR_LED_ON: //Este CASE mantiene el LED encendido el tiempo que definas (500ms en este caso).
					{
						if (tiempo_cumplido(tiempo_referencia, 500)) {	// ¿Ya pasaron 500ms (medio segundo)?
							subestado_mostrar = SUB_APAGAR_LED;
						}

						break;
					}

					case SUB_APAGAR_LED: //Este CASE apaga el led, para que se note la separación entre dos luces iguales seguidas
					{
						led_apagar(secuencia[indice_mostrar]);

						tiempo_referencia = ticks_ms;	// Guardamos el tiempo para el espacio en blanco entre LEDs

						subestado_mostrar = SUB_ESPERAR_LED_OFF;

						break;
					}

					case SUB_ESPERAR_LED_OFF: //Este CASE crea un breve "silencio" visual (300ms) antes de pasar al siguiente LED.
					{
						if (tiempo_cumplido(tiempo_referencia, 300)) {	// ¿Ya pasaron 300ms de silencio?

							indice_mostrar++; // Pasamos al siguiente LED de la secuencia

						    if (indice_mostrar >= longitud_secuencia) {	// ¿Ya mostramos todos los LEDs de la ronda actual?
						    	indice_jugador = 0; // Preparamos el índice para que el jugador empiece desde el primero
						    	estado_actual = ESTADO_ESPERAR_JUGADOR; // Cambiamos de estado principal
						    }
						    else {
						    	subestado_mostrar = SUB_ENCENDER_LED;	// Si faltan LEDs, volvemos a empezar el ciclo para el siguiente
						    }
						}
						break;
					}

					default:
					{
						subestado_mostrar = SUB_ENCENDER_LED;
						break;
					}
				}


                break;
            }

            case ESTADO_ESPERAR_JUGADOR: //Este CASE nos garantiza que el jugador no pueda ingresar datos mientras la placa todavía está mostrando la secuencia
            {
            	habilitar_lectura_usuario = 1;	// Habilitamos la lectura para que la interrupción acepte pulsaciones

             	if (evento_boton) { //¿Hubo una nueva pulsación detectada por la interrupción?

             		led_encender(boton_presionado);	//Feedback visual: Encendemos el LED que corresponde al botón apretado, donde 'boton_presionado' fue cargado por la interrupción EINT3

                    while (boton_sigue_presionado(boton_presionado)); // Antirrebote. Evitando que una pulsación larga cuente como varias jugadas

                    leds_apagar_todos();	// Cuando el antirrebote terminó, apagamos el LED

                    evento_boton = 0;	//Limpiamos la bandera del evento para la próxima vez

                    estado_actual = ESTADO_VALIDAR_JUGADA;	// Verificamos si el botón que tocó es el correcto
                }
             	break;
            }

            case ESTADO_VALIDAR_JUGADA:
            {

            	if (boton_presionado == secuencia[indice_jugador]) {	//Comparamos el botón que tocó el usuario con el que correspondía en la secuencia

            		indice_jugador++; // Si el usuario ACERTO, incrementamos el índice. Esto es lo que permite que el sistema sepa que el próximo botón que presiones debe compararse con el siguiente elemento del arreglo.


                    if (indice_jugador >= longitud_secuencia) {	// ¿Ya completó todos los pasos de la secuencia actual?
                    											// Si la secuencia era de 5 luces y vas por la 3ra, el else te manda de vuelta a ESTADO_ESPERAR_JUGADOR para que sigas presionando.

                    	estado_actual = ESTADO_RONDA_SUPERADA;	// El usuario repitió toda la secuencia correctamente
                    											// Si ya hiciste las 5, saltas a ESTADO_RONDA_SUPERADA para que el juego te agregue otra luz
                    }
                    else {

                    	estado_actual = ESTADO_ESPERAR_JUGADOR;	// Todavía le faltan botones para completar esta ronda
                    }
            	}
                else {	//El usuario se EQUIVOCO

                	error_jugada = 1;         // Marcamos que hubo un error
                	estado_actual = ESTADO_GAME_OVER; // Directo al final
                }
                break;
            }

            case ESTADO_RONDA_SUPERADA:
            {

            	if (tiempo_referencia == 0) {	//Usamos una bandera para saber si es la primera vez que entramos aquí

            		nivel++;                    // Aumentamos el nivel solo una vez

            		tiempo_referencia = ticks_ms; // Marcamos el inicio de Ronda Superada
            	}

            	if (tiempo_cumplido(tiempo_referencia, 800)) {	// Mientras no pasen 800ms, el 'break' hace que el main siga girando (Para darle una especie de "respiro" al jugador cuando acaba la ronda)

            		tiempo_referencia = 0; //Antes de irnos, reseteamos nuestra referencia para la próxima ronda

            		if (nivel >= MAX_SECUENCIA) {	//¿El jugador ya superó el nivel máximo permitido?

            			estado_actual = ESTADO_VICTORIA;	// El jugador ha completado todos los pasos posibles
            		}
            		else {	// Todavía hay más niveles, volvemos a generar un nuevo paso

            			estado_actual = ESTADO_GENERAR_PASO;
            		}
            	}
            	break;
            }

            case ESTADO_GAME_OVER: //Este CASE le da un feedback claro al usuario de que cometió un error y, lo más importante, limpia el sistema para que el juego pueda volver a empezar desde cero.
            {

            	if (error_jugada == 1) {
            		LPC_GPIO2->FIOSET = 0x0000000F; // Enciende P2.0 a P2.3. Encendemos todos los LEDs para indicar el error

            		tiempo_referencia = ticks_ms;	// Marcamos el inicio del Game Over

            		error_jugada = 2;                // Cambiamos el valor para no entrar más a este "if"
            	}

            	if (tiempo_cumplido(tiempo_referencia, 4000)) {	// Esperamos 4000ms (4 segundos) sin bloquear el main
            	    leds_apagar_todos();

            	    //Reiniciamos la lógica del juego
            	    longitud_secuencia = 0;
            	    nivel = 0;
            	    error_jugada = 0;
            	    evento_boton = 0;
            	    evento_start = 0;

            	    //Volvemos al inicio
            	    estado_actual = ESTADO_IDLE;
            	}
                break;
            }

            case ESTADO_VICTORIA:
            {

            	if (tiempo_referencia == 0) {	//Inicialización al entrar por primera vez

                	tiempo_referencia = ticks_ms;
                	paso_animacion = 0;	// Usaremos esto para contar en qué paso de la animacion final estamos
            	}

            	if (tiempo_cumplido(tiempo_referencia, 150)) { //velocidad de la animación de victoria

                	tiempo_referencia = ticks_ms;
                    leds_apagar_todos(); // Limpiamos antes de mostrar el siguiente paso

                    switch (paso_animacion)
                    	{
                        // --- Primera escalera ---
                        case 0: led_encender(0); break;
                        case 1: led_encender(1); break;
                        case 2: led_encender(2); break;
                        case 3: led_encender(3); break;

                        // --- Segunda escalera ---
						case 4: led_encender(0); break;
						case 5: led_encender(1); break;
						case 6: led_encender(2); break;
						case 7: led_encender(3); break;

                        // --- Triple Flash (Todos juntos) ---
                        case 8:  LPC_GPIO2->FIOSET = 0xF; break;
                        case 9:  leds_apagar_todos(); break;	//No es necesario apagarlo aca, ya que al inicio del IF lo hace pero bueno
                        case 10: LPC_GPIO2->FIOSET = 0xF; break;
                        case 11: leds_apagar_todos(); break;	//No es necesario apagarlo aca, ya que al inicio del IF lo hace pero bueno
                        case 12: LPC_GPIO2->FIOSET = 0xF; break;

                        // --- Fin de la animacion final ---
                        default:
                        	leds_apagar_todos();
                        	longitud_secuencia = 0;
                        	nivel = 0;
                        	paso_animacion = 0;
                        	tiempo_referencia = 0;
                        	evento_start = 0;
                        	estado_actual = ESTADO_IDLE;
                        	break;
                    	}

                    	paso_animacion++; // Avanzamos al siguiente paso de la animacion
                }
                break;
            }

            default:
            {
                estado_actual = ESTADO_IDLE;
                break;
            }
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------------------

uint8_t tiempo_cumplido(uint32_t referencia, uint32_t demora_ms)
{
    return ((ticks_ms - referencia) >= demora_ms);
}

//-----------------------------------------------------------------------------------------

void systick_init(void)
{
    SysTick->CTRL = 0; //Deshabilitar SysTick. Antes de cambiar la velocidad o la configuración de un temporizador, conviene apagarlo.

    SysTick->LOAD = (SystemCoreClock / 1000) - 1; //Cargar valor para 1 ms. Esto porque SystemCoreClock=100.000.000 => 100.000.000 / 1000 = Cantidad de pulsos de reloj en 1mS

    SysTick->VAL = 0; //Reiniciar contador actual. Nos asegura que el temporizador empiece a contar desde el principio (LOAD) en cuanto lo encendamos

    SysTick->CTRL = (1 << 0) | (1 << 1) | (1 << 2); //Habilitar SysTick: bit 0 = ENABLE(enciende el motor del temporizador) --- bit 1 = TICKINT(Habilita la Interrupción) --- bit 2 = CLKSOURCE (usa el clock del procesador)
}

void SysTick_Handler(void)
{
    ticks_ms++;
}

//-----------------------------------------------------------------------------------------


void EINT3_IRQHandler(void)
{
    uint32_t estado_p0;

    estado_p0 = LPC_GPIOINT->IO0IntStatF; //Leemos qué pines del Puerto 0 dispararon la interrupción por flanco de bajada (Falling edge)

    /* Limpiar flags */
    LPC_GPIOINT->IO0IntClr = estado_p0; //Limpiamos los flags inmediatamente para permitir futuras interrupciones

    if (estado_p0 & ((1 << 0)|(1 << 1)|(1 << 2)|(1 << 3))) { //Cualquier boton empieza el juego
            evento_start = 1;
        }

    if (habilitar_lectura_usuario == 1) {

            if (estado_p0 & (1 << 0)) {
                boton_presionado = 0;
                evento_boton = 1;
            }
            else if (estado_p0 & (1 << 1)) {
                boton_presionado = 1;
                evento_boton = 1;
            }
            else if (estado_p0 & (1 << 2)) {
                boton_presionado = 2;
                evento_boton = 1;
            }
            else if (estado_p0 & (1 << 3)) {
                boton_presionado = 3;
                evento_boton = 1;
            }
    }
}

//-----------------------------------------------------------------------------------------


void config_botones(void)	//Preparamos los pines P0.0 a P0.3 para que no solo detecten cuando presionas un botón, sino que también sean capaces de "despertar" al procesador mediante una interrupción
{
    LPC_PINCON->PINSEL0 &= ~0x000000FF; //Movemos un 1111 1111 a los primeros 4 pines del puerto 0 que se manejan con 2 bits cada uno(P0.0 al P0.15)
    									//Al usar &= ~ esos 1111 1111 se convierten en ceros, es decir, estamos diciendo que los pines funcionan como GPIO

    LPC_PINCON->PINMODE0 &= ~0x000000FF; //Movemos un 1111 1111 a los primeros 4 pines del puerto 0 que se manejan con 2 bits cada uno(P0.0 al P0.15)
    									 //Al usar &= ~ esos 1111 1111 se convierten en ceros, es decir, estamos activando las pull-up de los pines

    //#define MASK_BOTONES   (BOTON_0 | BOTON_1 | BOTON_2 | BOTON_3) donde cada boton vale 1 (definido al inicio)

    LPC_GPIO0->FIODIR &= ~MASK_BOTONES; // Usando &= ~ Limpiamos los bits de cada boton a 0 (entrada). Ahora el microcontrolador sabe que debe "escuchar" lo que viene desde afuera en esos pines.

    LPC_GPIOINT->IO0IntEnF |= MASK_BOTONES; //Habilitamos la interrupción por flanco descendente. Como usamos Pull-ups, el estado normal es "alto" (1). Al presionar, el voltaje cae a "bajo" (0). Ese salto es el que genera la interrupción.
    										//El registro IO0IntEnF controla las interrupciones de todo el Puerto 0 (32 pines). Si usaramos un igual simple (=), estariamos borrando las interrupciones de los otros 28 pines para solo activar 4 botones
    LPC_GPIOINT->IO0IntEnR &= ~MASK_BOTONES; //Deshabilitamos el flanco de subida para que no interrumpa al soltar el botón (solo nos interesa cuando se pulsa).

    LPC_GPIOINT->IO0IntClr = MASK_BOTONES; //Escribe un 1 en los bits de limpieza para borrar cualquier interrupción vieja que haya quedado "colgada" durante el arranque

    NVIC_EnableIRQ(EINT3_IRQn); //Esta línea le da permiso al grupo de interrupciones de GPIO (llamado EINT3) para que detengan al procesador y ejecuten el código de respuesta
    							//NVIC_EnableIRQ(...) --> Función estándar de la librería CMSIS que escribe un 1 en un registro especial del núcleo ARM llamado ISER (Interrupt Set-Enable Register).
    							//EINT3_IRQn --> Al pasarle EINT3_IRQn, le estás diciendo al NVIC: "Habilitá la línea de comunicación número 21 (número para las interrupciones de GPIO (Puerto 0 y Puerto 2))" .
}

void leds_apagar_todos(void) // Esta función es la encargada de "limpiar" el estado de los LEDs en un solo paso, en lugar de apagar uno por uno
{
	//Accedemos al registro de dirección del Puerto 2, usando LPC_GPIO2
    LPC_GPIO2->FIOCLR = ( // FIOCLR --> registro específico para poner pines en 0 (0V).
    	// Lo de abajo es igual a poner: (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) donde el operador | (OR): Une todos esos "unos" en un solo valor:
        (1 << 0) | //Ponemos un 1 en la posición del pin P2.0. Apagando el led
        (1 << 1) | //Ponemos un 1 en la posición del pin P2.1. Apagando el led
        (1 << 2) | //Ponemos un 1 en la posición del pin P2.2. Apagando el led
        (1 << 3)   //Ponemos un 1 en la posición del pin P2.3. Apagando el led
    );
}

void led_mostrar(uint8_t indice) //  Esta función se encarga de encender un solo LED específico basándose en un número de índice (0, 1, 2 o 3).
{
    leds_apagar_todos(); //Antes de mostrar el nuevo paso de la secuencia, debemos asegurarnos de que no haya otros LEDs encendidos de pasos anteriores (Apaga los 4 LEDS)

    if (indice < CANT_LEDS) //Es un salvavida por si se produce un posible error de logica (Ej: el microcontrolador quiere prender el led 5 cuando solo hay 4 leds disponibles)
    {
        LPC_GPIO2->FIOSET = led_mask[indice];	//led_mask[indice]: Aquí accedemos al arreglo global que definimos al inicio (Si indice es 0, busca led_mask[0], que vale (1 << 0).)
        										//el registro FIOSET pone en nivel alto (3.3V) únicamente los bits que reciben un 1.
        										//Por lo tanto, al asignar el valor de la máscara, el pin correspondiente al índice se activa instantáneamente.
    }
}

void leds_init(void) //Funcion util para configurar los pines fisicos del LPC1769 como salidas digitales (por eso les pasamos un "uno" negado, es decir, un "cero")
{
	//Accedemos al registro que controla la función de los pines del Puerto 2
    LPC_PINCON->PINSEL4 &= ~( 	//   &= ~ --> es una operación de "limpieza" (AND con el complemento). Se asegura de poner esos bits en 00(binario)
        (3 << 0) |	//El número 3(decimal) es 11(binario). Al desplazarlo 0 posiciones, apuntamos a los dos bits que controlan el pin P2.0.
        (3 << 2) |	//El número 3(decimal) es 11(binario). Al desplazarlo 2 posiciones, apuntamos a los dos bits que controlan el pin P2.1.
        (3 << 4) |	//El número 3(decimal) es 11(binario). Al desplazarlo 4 posiciones, apuntamos a los dos bits que controlan el pin P2.2.
        (3 << 6)	//El número 3(decimal) es 11(binario). Al desplazarlo 6 posiciones, apuntamos a los dos bits que controlan el pin P2.3.
    );

    //Accedemos al registro de dirección del Puerto 2. Aca ponemos nuestros 4 pines de LEDs como salida.
    LPC_GPIO2->FIODIR |= ( //    |= --> Esta es una operación OR. Se usa para poner bits en 1 sin modificar los demás bits que ya estaban configurados en el registro.
        (1 << 0) |	//Ponemos un 1 en la posicion 0 (En el registro FIODIR, un 1 significa SALIDA)
        (1 << 1) |	//Ponemos un 1 en la posicion 1
        (1 << 2) |	//Ponemos un 1 en la posicion 2
        (1 << 3)	//Ponemos un 1 en la posicion 3
    );

    leds_apagar_todos();
}

//Funcion que se encarga de encender el led que le pida
void led_encender(uint8_t indice) //uint8_t indice --> Es el "nombre" o número del LED que queremos prender (0, 1, 2 o 3).
{
    if (indice >= CANT_LEDS)	//Es un salvavida por si se produce un posible error de logica (Ej: el microcontrolador quiere prender el led 5 cuando solo hay 4 leds disponibles)
        return;

    LPC_GPIO2->FIOSET = led_mask[indice];	//led_mask[indice]: Aquí accedemos al arreglo global que definimos al inicio (Si indice es 0, busca led_mask[0], que vale (1 << 0) y prendemos el led)
}

void led_apagar(uint8_t indice)	//uint8_t indice --> Es el "nombre" o número del LED que queremos prender (0, 1, 2 o 3).
{
    if (indice >= CANT_LEDS)	//Es un salvavida por si se produce un posible error de logica (Ej: el microcontrolador quiere prender el led 5 cuando solo hay 4 leds disponibles)
        return;

    LPC_GPIO2->FIOCLR = led_mask[indice];	//led_mask[indice]: Aquí accedemos al arreglo global que definimos al inicio (Si indice es 0, busca led_mask[0], que vale (1 << 0) y apagamos el led)
}

uint8_t boton_sigue_presionado(uint8_t boton) 	//uint8_t boton: Recibe un número del 0 al 3 (el índice del botón que queremos chequear).
												//uint8_t: Devuelve un 1 (Verdadero) si el botón sigue apretado, o un 0 (Falso) si fue soltado.
{
    switch (boton) //El switch nos permite ejecutar una lógica diferente dependiendo de qué botón estemos consultando. Si le pasamos un 0, el programa saltará directamente a la lógica que lee el pin P0.0.
    {
        case 0:
            return ((LPC_GPIO0->FIOPIN & BOTON_0) == 0);	//LPC_GPIO0->FIOPIN: si un pin tiene 3.3V, lee un 1; si tiene 0V, lee un 0.
            												//& BOTON_0 : Al hacer un AND con la máscara del botón 0 (0000...0001), el resultado será 0 en todas las posiciones excepto, tal vez, en la posición del botón que nos interesa.
            												//== 0 (Lógica Negativa): Ya que los botones estan configurados con PULL-UP
            												//Si el resultado de la máscara es 0, significa que el botón está tocando tierra, es decir, sigue presionado. Por eso la comparación devuelve un 1 (Verdadero) hacia afuera de la función
        case 1:
            return ((LPC_GPIO0->FIOPIN & BOTON_1) == 0);

        case 2:
            return ((LPC_GPIO0->FIOPIN & BOTON_2) == 0);

        case 3:
            return ((LPC_GPIO0->FIOPIN & BOTON_3) == 0);

        default:
            return 0; //Si por algún error le preguntamos a la función por un "Botón 5", el default se asegura de devolver un 0 para no trabar el programa con valores basura.
    }
}
