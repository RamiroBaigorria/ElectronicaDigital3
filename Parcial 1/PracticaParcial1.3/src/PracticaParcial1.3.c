#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

volatile uint32_t habil=0;
volatile uint32_t estado=0;
volatile uint32_t abajo=0;
volatile uint32_t cuenta=0;

void configGPIO(void);
void configEINT2(void);
void configSysTick(void);
void SysTick_Handler(void);
void EINT2_IRQHandler(void);

int main(void){
    configEINT2();
    configGPIO();
    configSysTick();
    while (1){

    }
}

void configGPIO(void){
    LPC_PINCON->PINSEL4 &= ~(3<<8); // como gpio
    LPC_GPIO2->FIODIR |= (1<<4); // como salida
    LPC_GPIO2->FIOSET |= (1<<4); // en 1

    LPC_GPIO2->FIOMASK |= (0xFF<<0); // enmascaro los demas
    LPC_GPIO2->FIOMASK &= ~(1<<4); //
}

void configEINT2(void){
    LPC_PINCON->PINSEL4 &= ~(3<<24); //limpio
    LPC_PINCON->PINSEL4 |= (1<<24); // como eint2
    LPC_PINCON->PINMODE4 &= ~(3<<24); // pull up

    LPC_SC->EXTINT |= (1<<2); // limpi flag
    LPC_SC->EXTMODE |= (1<<2); // por flanco
    LPC_SC->EXTPOLAR &= ~(1<<2); // por flanco de bajada

    NVIC->IP[EINT2_IRQn] = (1<<3);
    NVIC->ISER[0] |= (1 << EINT2_IRQn); // habilito int
}

void configSysTick(void){
    SysTick->CTRL = 0;
    SysTick->LOAD = 599999;
    SysTick->VAL = 0;
    SysTick->CTRL |= (1<<2);
    SysTick->CTRL |= (1<<1);
    SysTick->CTRL &= ~(1<<0);
    SCB->SHP[11] = (1<<4);
}

void EINT2_IRQHandler(void){
    if (LPC_SC->EXTINT & (1<<2)){
        if (!(habil)){
            habil=1; // habilito
            estado = 0;
            cuenta = 0;
            LPC_GPIO2->FIOCLR |= (1<<4); // limpio para que empiece abajo
            SysTick->CTRL |= (1<<0); // enable
        }else {
            SysTick->CTRL &= ~(1<<0); // disable
            habil=0; // deshabilito
            LPC_GPIO2->FIOSET |= (1<<4);
        }
        LPC_SC->EXTINT |= (1<<2); // limpio flag
    }
}

void SysTick_Handler(void) {
    cuenta++;

    switch (estado) {

        case 0: // ETAPA 1: 40 mseg en bajo
            if (cuenta >= 4) {
                LPC_GPIO2->FIOSET = (1 << 4); // Sube para empezar oscilación
                estado = 1;
                cuenta = 0;
            }
            break;

        case 1: // ETAPA 2: 4 ciclos de 20ms (10ms alto, 10ms bajo)
            // Cambia el estado del pin cada 10ms (1 tick)
            if (LPC_GPIO2->FIOPIN & (1 << 4)) {
                LPC_GPIO2->FIOCLR = (1 << 4);
            } else {
                LPC_GPIO2->FIOSET = (1 << 4);
            }

            // Según la imagen son 4 pulsos. Un pulso completo son 2 ticks (subida/bajada).
            // 4 pulsos x 2 ticks/pulso = 8 ticks.
            if (cuenta >= 8) {
                LPC_GPIO2->FIOCLR = (1 << 4); // Baja para etapa final
                estado = 2;
                cuenta = 0;
            }
            break;

        case 2: // ETAPA 3: 40 mseg en bajo
            if (cuenta >= 4) {
                LPC_GPIO2->FIOSET = (1 << 4); // Sube y finaliza
                SysTick->CTRL &= ~(1 << 0);   // Detener timer
                habil = 0;
            }
            break;
    }
}
//    if (estado==0){
//        LPC_GPIO2->FIOCLR |= (1<<4); // limpio
//        cuenta++; // ya pasaron 10ms abajo porque interrumpio -> +1
//        if (cuenta==4){ // 4 x 10 ms
//            estado=1; // paso a la oscilacion
//            cuenta=0; //reseteo
//            abajo=0; // reseteo por las dudas
//        }
//    }
//    if (estado==1){
//        if (abajo==0){
//            LPC_GPIO2->FIOSET |= (1<<4); //en 1
//            cuenta++;
//            abajo=1; // paso al bajo
//        }
//        else if (abajo==1){
//            LPC_GPIO2->FIOCLR |= (1<<4); // lo bajo
//            abajo=0; // limpio para que entre a subir la proxima iteracion
//        }
//        if (cuenta==4){ // llego a 4 ciclos arriba
//            estado=2; // paso a la tercera parte
//            cuenta=0; // reseteo
//        }
//    }
//    else if (estado==2){
//        LPC_GPIO2->FIOCLR |= (1<<4); // limpio
//        cuenta++;
//        if (cuenta>4){ // en 1 recien baja, va a estar un ciclo arriba antes de entrar, entonces debo contar hasta 5
//            estado=0; // reseteo todo para empezar de nuevo
//            cuenta=0;
//            abajo=0;
//            SysTick->CTRL &= ~(1<<0);
//            habil=0;
//        }
//    }
}


//---------------------------------------------------------------------------------------------------------------------
/*
2)
systick handler
cuenta++;
tiempo++;
if (tiempo==lim){
    if (!on){
        LPC_GPIO1->FIOSET |= (1<<18);
        on=1;
        tiempo=0;
    }
    else {
        LPC_GPIO1->FIOCLR |= (1<<18);
        on=0;
        tiempo=0;
    }
}

eint1 handler
if (flanco==0){
    t_prev = ((SysTick->LOAD + 1) * cuenta) + ((SysTick->LOAD + 1) - SysTick->VAL)
    flanco=1;
}
else {
    now = ((SysTick->LOAD + 1) * cuenta) + ((SysTick->LOAD + 1) - SysTick->VAL);
    periodo = now - t_prev;
    //y las demas cosas que hay
}

systick config
LOAD = 999999; 10ms
CTRL |= ((1<<0) | (1<<1) | (1<<2));
SCB->SHP[11] = (1<<4);

eint1 config
NO ES GPIOINT
LPC_SC->EXTINT |= (1<<1);
LPC_SC->EXTMODE |= (1<<1);
LPC_SC->EXTPOLAR |= (1<<1);
NVIC->IP[EINT1_IRQn] = (2<<3);
ENTI1 EN VEZ DE EINT3 EN E=NVIC ENABLE

main
while (1){
    if (periodo<((SysTick->LOAD +1) * 10)){
            lim=5;
        }
    else if (periodo>=((SysTick->LOAD +1) * 10) && periodo<=((SysTick->LOAD +1) * 100)){
        lim= (periodo / (SysTick->LOAD +1))/2
    }
    else {
        lim= 50;
    }
}

*/
//------------------------------------------------------------------------------------------------------

/*
3)

los registros involucrados son el ISER (set enable) y el ICER (clear enable)
son los que habilitan y deshabilitan interrupciones de NVIC. para que una interrumpcion
se ejecute, debe estar habilitada tanto por el propio periferico en su configuracion
como por el nvic, usando estos registros o las funciones propias de CMSIS:
NVIC_EnableIRQ(x);
NVIC_DisableIRQ(x);

