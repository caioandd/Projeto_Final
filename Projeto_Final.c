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
#include "inc/matrix.h"

#define btn_a 5
#define btn_b 6
#define led_g 11
#define led_b 12
#define led_r 13
#define WRAP_PERIOD 24999 // Valor do WRAP
#define PWM_DIVISER 100.0 // Valor do divisor de clock do PWM 
#define pausa 3000
#define pausa_f 5000
#define NUM_LEDS 25 //Numero de LEDs
#define MATRIZ_PIN 7 //GPIO
#define DEBOUNCE_DELAY 500

volatile bool estado = false;
volatile bool sensorFC = false;
volatile int opc;
uint16_t step = 500;
int pos_prensa = 0;
uint pos_atual = 0;
volatile int ultima_interrup = 0; // Para armazenar o último tempo de interrupção

void init_gpios();
uint32_t matrix_rgb(double r, double g, double b);
void padrao(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b);
static void gpio_irq_handler(uint gpio, uint32_t events);

int main()
{   
    stdio_init_all();
    init_gpios();

    PIO pio = pio1; 
    bool ok;
    uint16_t i;
    uint32_t valor_led;
    double r, g, b;

    ok = set_sys_clock_khz(128000, false);

    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, MATRIZ_PIN);

    gpio_set_function(led_b, GPIO_FUNC_PWM); // GPIO como PWM
    uint slice = pwm_gpio_to_slice_num(led_b); // Slice da GPIO
    pwm_set_clkdiv(slice, PWM_DIVISER); // Divisor de clock do PWM
    pwm_set_wrap(slice, WRAP_PERIOD); // WRAP
    pwm_set_enabled(slice, true); // Define estado do PWN no LED_B

    gpio_set_irq_enabled_with_callback(btn_a, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(btn_b, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
    r=0;
    g=0;
    b=0;
    while (true) {
 // Enquanto os botões estiverem pressionados
        if (opc==0 && estado){
            while (estado && pos_prensa<25000) {
                pwm_set_gpio_level(led_b, pos_prensa);
                printf("Avanço de prensa: %d\n", pos_prensa);
                sleep_ms(100);
                pos_prensa += step;
            }
            printf("Avanço de prensa: %d\n", pos_prensa);
            opc=1;
        }
        if (opc==1 && estado && sensorFC){
            for (uint numero = pausa/1000; numero>0; numero--){
                r=0.2;
                padrao(nums[numero], valor_led, pio, sm, r, g, b);
                sleep_ms(1000);
            }
            r=0; 
            padrao(nums[1], valor_led, pio, sm, r, g, b);
            opc=2;
            sensorFC = !sensorFC;
            }
        if (opc==2 && estado)
        {
            while (estado && pos_prensa>0) {
                pwm_set_gpio_level(led_b, pos_prensa);
                printf("Avanço de prensa: %d\n", pos_prensa);
                sleep_ms(100);
                pos_prensa -= step;
            }
            printf("Avanço de prensa: %d\n", pos_prensa);
            pwm_set_gpio_level(led_b, 0);
            opc=3;
        }
        if (opc==3 && estado){
            for (uint numero = pausa_f/1000; numero>0; numero--){
                r=0.2;
                padrao(nums[numero], valor_led, pio, sm, r, g, b);
                sleep_ms(1000);
            }
        r=0;
        padrao(nums[1], valor_led, pio, sm, r, g, b);
        opc=0;
        }

        if (!estado && pos_prensa>0){
            printf("Desligando prensa...\n");
            while (pos_prensa >0) // Retorna a prensa
            {
                pwm_set_gpio_level(led_b, pos_prensa);
                printf("Recuo de prensa: %d\n", pos_prensa);
                sleep_ms(100);
                pos_prensa -= step;
            }
            printf("Recuo de prensa: %d\n", pos_prensa);
            pwm_set_gpio_level(led_b, 0);
            opc=0;
        }  
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

uint32_t matrix_rgb(double r, double g, double b){
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G<<24) | (R << 16) | (B << 8);
}

void padrao(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b){
    for (int16_t i = 0; i < NUM_LEDS; i++){
        int led_matrix_location = PHYSICAL_LEDS_MAPPER[i];
        valor_led = matrix_rgb(r*desenho[led_matrix_location], g*desenho[led_matrix_location], b*desenho[led_matrix_location]);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

static void gpio_irq_handler(uint gpio, uint32_t events){

uint32_t tempo_interrup = to_ms_since_boot(get_absolute_time()); // Obtém o tempo atual
    if (tempo_interrup - ultima_interrup > DEBOUNCE_DELAY) { // Verifica o tempo de debounce
    ultima_interrup = tempo_interrup; // Atualiza o tempo da última interrupção
        if (gpio_get(btn_a)==0){
            estado = !estado;
        }   
        if (gpio_get(btn_b)==0){
            sensorFC = !sensorFC;
        }   
    }
}

