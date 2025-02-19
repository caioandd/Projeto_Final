#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/irq.h" 
#include "hardware/pwm.h" 
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "pio_matrix.pio.h"

#define btn_a 5
#define btn_b 6
#define led_g 11
#define led_b 12
#define led_r 13
#define WRAP_PERIOD 24999 // Valor do WRAP
#define PWM_DIVISER 100.0 // Valor do divisor de clock do PWM 
#define pausa 3000
#define NUM_LEDS 25 //Numero de LEDs
#define MATRIZ_PIN 7 //GPIO

volatile bool estado = false;
volatile int opc;
uint16_t step = 500;
int pos_prensa = 0;
uint pos_atual = 0;
int numero = pausa/1000;

void init_gpios();
void prensa();

int main()
{
    stdio_init_all();
    init_gpios();

    while (true) {
        prensa();
        sleep_ms(50);
    }
}

void init_gpios(){
    gpio_init(btn_a);
    gpio_init(btn_b);
    gpio_init(led_b);
    gpio_init(led_g);
    gpio_init(led_r);

    gpio_set_dir(btn_a, GPIO_IN);
    gpio_set_dir(btn_b, GPIO_IN);
    gpio_set_dir(led_b, GPIO_OUT);
    gpio_set_dir(led_g, GPIO_OUT);
    gpio_set_dir(led_r, GPIO_OUT);

    gpio_pull_up(btn_a);
    gpio_pull_up(btn_b);
}

void prensa(){
   // Enquanto os botões estiverem pressionados
    while (gpio_get(btn_a) == 0 && gpio_get(btn_b) == 0) {
        // Aumenta o brilho progressivamente
        while (pos_prensa < 25500 && gpio_get(btn_a) == 0 && gpio_get(btn_b) == 0) {
            pwm_set_gpio_level(led_b, pos_prensa);
            printf("Avanço de prensa: %d\n", pos_prensa);
            sleep_ms(100);
            pos_prensa += step;
        }
        // Se os botões foram soltos, interrompe imediatamente
        if (gpio_get(btn_a) != 0 || gpio_get(btn_b) != 0) {
            printf("Botões soltos, parando...\n");
            while (pos_prensa >=0) // Retorna a prensa
            {
                pwm_set_gpio_level(led_b, pos_prensa);
                printf("Recuo de prensa: %d\n", pos_prensa);
                sleep_ms(100);
                pos_prensa -= step;
            }
        }

        estado = true; // Ativa estado para reduzir brilho
        sleep_ms(pausa); //utilizar get time

        while (pos_prensa >=0 && estado && gpio_get(btn_a) == 0 && gpio_get(btn_b) == 0 && numero==0) {
            pwm_set_gpio_level(led_b, pos_prensa);
            printf("Recuo de prensa: %d\n", pos_prensa);
            sleep_ms(100);
            pos_prensa -= step;
        }

        // Se os botões foram soltos, interrompe imediatamente
        if (gpio_get(btn_a) != 0 || gpio_get(btn_b) != 0) {
            printf("Botões soltos, parando...\n");
            return;
        }
        sleep_ms(2000);
        estado = false; // Reset para o próximo ciclo
    }
}

