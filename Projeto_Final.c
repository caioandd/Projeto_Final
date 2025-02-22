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

//============BOTOES============
#define btn_a 5 // Botao A
#define btn_b 6 // Botao B
#define btn_j 22 // Botao de joystick
#define DEBOUNCE_DELAY 500 // Debounce para acionamento de botões
volatile int ultima_interrup = 0; // Para armazenar o último tempo de interrupção
//============SAIDAS============
#define led_g 11 // LED verde
#define led_b 12 // LED azul
#define led_r 13 // LED vermelho
#define buzzer 21 // Buzzer
//============ADC============
#define VRX_PIN 27 // GPIO DE EIXO Y 
#define VRY_PIN 26 // GPIO DE EIXO X
float ledx; // ADC para PWM
float ledy; // ADC para PWM
//============PWM============
#define WRAP_PERIOD 24999 // Valor do WRAP
#define PWM_DIVISER 100.0 // Valor do divisor de clock do PWM 
#define pausa 3000 // Tempo de prensagem 
#define pausa_f 5000 // Pausa de final curso
uint16_t step = 500; // Passo de movimento automático
uint16_t stepm = 100; // Passo de movimento manual
int pos_prensa = 0; // Posição de prensa
//============MATRIZ============
#define NUM_LEDS 25 //Numero de LEDs
#define MATRIZ_PIN 7 //GPIO
//============DISPLAY============
#define I2C_PORT i2c1
#define I2C_SDA 14 // GPIO SDA DO DISPLAY
#define I2C_SCL 15 // GPIO SCL DO DISPLAY
#define endereco 0x3C // ENDEREÇO DO DISPLAY
ssd1306_t ssd; // Inicializa a estrutura do display
char str[10]; // Armazena a string da posição da prensa
char str1[10]; // Armazena a string da temperatura
char str2[10]; // Armazena a string de ciclos completos
int cont; // Contador de ciclos completos (CC)
//============STATUS============
volatile bool estado = false; // Status do modo automático
volatile bool sensorFC = false; // Status de sensor de fim de curso
volatile bool manual = false; // Status de modo manual
volatile int opc; // Etapa do modo automático

// Funções primárias
void init_gpios();
void init_display();
void info_display();
uint32_t matrix_rgb(double r, double g, double b);
void padrao(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b);
static void gpio_irq_handler(uint gpio, uint32_t events);

int main()
{   
    stdio_init_all();
    adc_init();
    init_gpios();
    init_display();

    adc_set_temp_sensor_enabled(true);
    adc_select_input(4); // Canal do sensor de temperatura

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

    gpio_set_irq_enabled_with_callback(btn_a, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler); // Interrupção para botão A
    gpio_set_irq_enabled_with_callback(btn_b, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler); // Interrupção para botão B
    gpio_set_irq_enabled_with_callback(btn_j, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler); // Interrupção para botão do joystick

    r=0;
    g=0;
    b=0; 
    cont = 0; // Contador de ciclos completos (CC) = 0

    while (true) { 
        info_display();
        while (manual){ //============MODO MANUAL============
            adc_select_input(0);
            uint16_t vry_value = adc_read(); // Lê o valor do eixo X, de 0 a 4095.
            ledy = vry_value*25000/4095; // Converte sinal para level de PWN. Valores de -25k até 25k
            info_display();
            if (ledy>13000 && pos_prensa<25000){ // Avanço manual de prensa
                pwm_set_gpio_level(led_b, pos_prensa);
                sleep_ms(100);
                pos_prensa += stepm;
                printf("Avanço de prensa: %d\n", pos_prensa);
            }
            if (ledy<11000 && pos_prensa>0){ // Recuo manual de prensa
                pwm_set_gpio_level(led_b, pos_prensa);
                sleep_ms(100);
                pos_prensa -= stepm;
                printf("Avanço de prensa: %d\n", pos_prensa);
            }       
        }
        if (opc==0 && estado){ //============MODO AUTOMÁTICO============
            while (estado && pos_prensa<25000) { // Avanço automático de prensa
                pwm_set_gpio_level(led_b, pos_prensa);
                printf("Avanço de prensa: %d\n", pos_prensa);
                sleep_ms(100);
                pos_prensa += step;
                info_display();
            }
            printf("Avanço de prensa: %d\n", pos_prensa);
            opc=1;
        }
        if (opc==1 && estado && sensorFC){ // Contagem de prensagem
            for (uint numero = pausa/1000; numero>0; numero--){
                r=0.2;
                padrao(nums[numero], valor_led, pio, sm, r, g, b);
                gpio_put(buzzer, true);
                sleep_ms(1000);
                gpio_put(buzzer, false);
            }
            r=0; 
            padrao(nums[1], valor_led, pio, sm, r, g, b);
            opc=2;
            sensorFC = !sensorFC;
            cont++;
        }
        if (opc==2 && estado){ // Recuo automático de prensa
            while (estado && pos_prensa>0) {
                pwm_set_gpio_level(led_b, pos_prensa);
                printf("Avanço de prensa: %d\n", pos_prensa);
                sleep_ms(100);
                pos_prensa -= step;
                info_display(); 
            }
            printf("Avanço de prensa: %d\n", pos_prensa);
            pwm_set_gpio_level(led_b, 0);
            opc=3;
        }
        if (opc==3 && estado){ // Contagem para reinício
            for (uint numero = pausa_f/1000; numero>0; numero--){
                r=0.2;
                padrao(nums[numero], valor_led, pio, sm, r, g, b);
                gpio_put(buzzer, true);
                sleep_ms(1000);
                gpio_put(buzzer, false);
            }
            r=0;
            padrao(nums[1], valor_led, pio, sm, r, g, b);
            opc=0;
        }
        if (!estado && pos_prensa>0){ // Recuo imediato
            printf("Desligando prensa...\n");
            while (pos_prensa >500){ // Recuo automático de prensa
                pwm_set_gpio_level(led_b, pos_prensa);
                printf("Recuo de prensa: %d\n", pos_prensa);
                sleep_ms(100);
                pos_prensa -= step;
                sprintf(str, "%d", pos_prensa); // Converte o inteiro para string
                info_display(); 
            }
            pos_prensa=0;
            printf("Recuo de prensa: %d\n", pos_prensa);
            pwm_set_gpio_level(led_b, pos_prensa);
            opc=0;
        }
    }  
}
//============INICIALIZAÇÃO DE GPIOS============
void init_gpios(){
    gpio_init(btn_a);
    gpio_init(btn_b);
    gpio_init(btn_j);
    gpio_init(led_b);
    gpio_init(led_g);
    gpio_init(led_r);
    gpio_init(buzzer);
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    gpio_set_dir(btn_a, GPIO_IN);
    gpio_set_dir(btn_b, GPIO_IN);
    gpio_set_dir(btn_j, GPIO_IN);
    gpio_set_dir(led_b, GPIO_OUT);
    gpio_set_dir(led_g, GPIO_OUT);
    gpio_set_dir(led_r, GPIO_OUT);
    gpio_set_dir(buzzer, GPIO_OUT);

    gpio_pull_up(btn_a);
    gpio_pull_up(btn_b);
    gpio_pull_up(btn_j);
}
//============CONVERSAO DE CORES DE LEDS============
uint32_t matrix_rgb(double r, double g, double b){
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G<<24) | (R << 16) | (B << 8);
}
//============DESENHO DE MATRIZ============
void padrao(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b){
    for (int16_t i = 0; i < NUM_LEDS; i++){
        int led_matrix_location = PHYSICAL_LEDS_MAPPER[i];
        valor_led = matrix_rgb(r*desenho[led_matrix_location], g*desenho[led_matrix_location], b*desenho[led_matrix_location]);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}
//============INTERRUPÇÃO DE BOTÕES============
static void gpio_irq_handler(uint gpio, uint32_t events){
    
    uint32_t tempo_interrup = to_ms_since_boot(get_absolute_time()); // Obtém o tempo atual
    if (tempo_interrup - ultima_interrup > DEBOUNCE_DELAY) { // Verifica o tempo de debounce
        ultima_interrup = tempo_interrup; // Atualiza o tempo da última interrupção
        if (gpio_get(btn_a)==0 && !manual){
            estado = !estado;
            printf("Modo automático está %s\n", estado ? "ligado." : "desligado.");
        }   
        if (gpio_get(btn_b)==0){
            sensorFC = !sensorFC;
        }
        if (gpio_get(btn_j)==0 && !estado){
            estado = false;
            manual = !manual;
            printf("Modo manual está %s\n", manual ? "ligado." : "desligado.");
            gpio_put(led_r, manual);
        }
    }
}
void init_display(){
    i2c_init(I2C_PORT, 400 * 1000); // Inicialização do I2C utilizando 400Khz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Define GPIO como I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Define GPIO como I2C
    gpio_pull_up(I2C_SDA); // Define GPIO SDA como pullup
    gpio_pull_up(I2C_SCL); // Define GPIO SCL como pullup
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_fill(&ssd, false); // Limpa o display   
    ssd1306_send_data(&ssd); // Atualiza o display
}
void info_display(){
    /*uint16_t raw = adc_read();  // Lê o valor bruto do ADC (0 a 4095)
    float voltage = raw * 3.3f / (1 << 12);  // Converte para tensão (3.3V referência, 12 bits)
    float temperatura = 27 - (voltage - 0.706f) / 0.001721f;  // Converte para °C
    sprintf(str1, "%.2f", temperatura);*/
    sprintf(str, "%d", pos_prensa); // Converte o inteiro para string
    sprintf(str2, "%d", cont); // Converte o inteiro para string
    ssd1306_fill(&ssd, false); // Limpa o display
    ssd1306_draw_string(&ssd, "PRENSA: ", 8, 30); // Desenha uma string   
    ssd1306_draw_string(&ssd, str, 74, 30); // Desenha valor da prensa
    ssd1306_draw_string(&ssd, "TEMP: " , 8, 40); // Desenha uma string
    ssd1306_draw_string(&ssd, "99.99 C", 56, 40); // Desenha valor da temperatura
    //ssd1306_draw_string(&ssd, str1, 56, 40); // Desenha valor da temperatura
    ssd1306_draw_string(&ssd, "CC: " , 8, 50); // Desenha uma string
    ssd1306_draw_string(&ssd, str2 , 40, 50); // Desenha uma string
    if (manual)
    {
        ssd1306_draw_string(&ssd, "MANUAL ON", 8, 10); // Desenha uma string
    }else{
        ssd1306_draw_string(&ssd, "MANUAL OFF", 8, 10); // Desenha uma string
    }
    if (estado)
    {
        ssd1306_draw_string(&ssd, "AUTO ON" , 8, 20); // Desenha uma string
    }else{
        ssd1306_draw_string(&ssd, "AUTO OFF" , 8, 20); // Desenha uma string
    }
    ssd1306_send_data(&ssd); // Atualiza o display
    
}