#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "lib/font.h"
#include "lib/ssd1306.h"
#include "lib/icons.h"
#include "thermostat.pio.h"

// Definições dos pinos GPIO
#define BTN_A                 5    // Push button A
#define BTN_B                 6    // Push button B

#define NUM_PIXELS            25   // Número de pixels da matriz de LEDs
#define MATRIX_PIN            7    // Pino da matriz de LEDs

#define BUZZER_A_PIN          10   // Buzzer A
#define BUZZER_B_PIN          21   // Buzzer B

#define LED_PIN_RED           13   // LED Red
#define LED_PIN_BLUE          12   // LED Blue
#define LED_PIN_GREEN         11   // LED Green

#define I2C_SDA               14   // SSD1306: SDA
#define I2C_SCL               15   // SSD1306: SCL

#define BTN_J                 22   // Botão do Joystick
#define JOYSTICK_X            26   // Pino do Joystick X

// Parâmetros de temperatura (escala de simulação)
#define TEMP_MIN              0.0f   // Temperatura mínima (°C)
#define TEMP_MAX              50.0f  // Temperatura máxima (°C)
#define HYST_MIN              0.1f   // Histerese mínima (°C) 
#define HYST_MAX              3.0f   // Histerese máxima (°C)

// Parâmetros do PWM
#define PWM_WRAP              4096   // Valor de wrap do PWM

// Define os pinos do display OLED e o endereço I2C
#define I2C_PORT              i2c1   // Porta I2C
#define endereco              0x3C   // Endereço I2C do display SSD1306

ssd1306_t ssd; // Estrutura para o display SSD1306

// Variáveis globais para controle de temperatura
static volatile float current_temp = 25.0f; // Temperatura atual (°C)
static volatile float setpoint = 25.0f;     // Temperatura desejada (°C)
static volatile float hysteresis = 0.1f;     // Histerese (°C)
char buffer[32]; // Buffer para armazenar valores de temperatura como string

// Variáveis para controle de estado e tempo
static volatile uint8_t state = 0; // Estado atual do termostato
static volatile uint32_t last_time = 0; // Armazena o tempo da última interrupção 

// Variáveis para controle de cor e ícone exibido na matriz de LEDs
double red = 255.0, green = 255.0 , blue = 255.0; // Variáveis para controle de cor
int icon = 0; //Armazena o número atualmente exibido
double* icons[6] = {icon_zero, icon_one, icon_two, icon_three, icon_four, icon_five}; //Ponteiros para os desenhos dos números

// Protótipos das funções
void get_state(char* buffer); // Função para obter o estado atual do termostato

void setup_GPIOs(); // Função para configurar os pinos GPIO

void draw_screen(); // Função para desenhar a tela do display

void turn_off_leds(uint gpio, bool turn_off_all); // Função para desligar os LEDs (Todos ou todos menos um específico)

void pwm_setup_gpio(uint gpio, uint freq); // Função para configurar o PWM

uint32_t matrix_rgb(double r, double g, double b); // Função para definir a intensidade de cores do LED

void desenho_pio(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b); // Função para acionar a matriz de LEDs

static void gpio_irq_handler(uint gpio, uint32_t events); // Função de callback para tratamento de interrupções

int main() {
    stdio_init_all(); // Habilita a comunicação serial

    setup_GPIOs(); // Configura os pinos GPIO
    
    // Inicializa o ADC para leitura do Joystick X e configura o pino
    adc_init(); 
    adc_gpio_init(JOYSTICK_X); 

    //Configurações da PIO
    PIO pio = pio0; 
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, MATRIX_PIN);
    
    // Inicializa o I2C para o display SSD1306
    i2c_init(I2C_PORT, 400 * 1000); // 400 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Inicializa o display SSD1306 (função da biblioteca)
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); 
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    
    // Configuração dos botões para interrupção
    gpio_set_irq_enabled_with_callback(BTN_A, GPIO_IRQ_EDGE_RISE, 1, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_B, GPIO_IRQ_EDGE_RISE, 1, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BTN_J, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);

    // Loop principal
    while (true) {
        // Leitura do ADC para obter a temperatura simulada 
        adc_select_input(0); 
        uint16_t adc_value = adc_read();

        uint32_t current_time = to_ms_since_boot(get_absolute_time()); // Obtém o tempo atual

        static uint32_t last_beep_time = 0; // Armazena o tempo do último beep

        // Controle de estados do termostato
        switch (state) {
            case 0:
                // Altera cor e ícone de acordo com a temperatura
                red = 0.0; green = 255.0; blue = 0.0;
                icon = 0;
                
                // Controle de temperatura com histerese (aquecimento e resfriamento)
                if (current_temp < setpoint - hysteresis) { // Se a temperatura atual for menor que o setpoint - histerese envia um sinal para o buzzer e muda o estado para 1 (aquecimento)
                    turn_off_leds(LED_PIN_RED, false);
                    pwm_setup_gpio(BUZZER_A_PIN, 600);
                    sleep_ms(300);
                    pwm_setup_gpio(BUZZER_A_PIN, 0);
                    state = 1;
                } else if (current_temp > setpoint + hysteresis) { // Se a temperatura atual for maior que o setpoint + histerese envia um sinal para o buzzer e muda o estado para 2 (resfriamento)
                    turn_off_leds(LED_PIN_BLUE, false);
                    pwm_setup_gpio(BUZZER_A_PIN, 600);
                    sleep_ms(300);
                    pwm_setup_gpio(BUZZER_A_PIN, 0);
                    state = 2;
                }
                break;
            case 1:
                // Altera cor e ícone de acordo com o aquecimento
                red = 255.0; green = 0.0; blue = 0.0;
                icon = 4;
                if (current_temp < setpoint - hysteresis) { // Checa se a temperatura atual é menor que o setpoint - histerese se sim, aumenta a temperatura e emite um beep
                    current_temp += 0.1f;
                    if (current_time - last_beep_time > 1000) {
                        pwm_setup_gpio(BUZZER_B_PIN, 250);
                        gpio_put(LED_PIN_RED, 1);
                        last_beep_time = current_time;
                    }
                } else { // Se não, muda o estado para 0
                    state = 0;
                    gpio_put(LED_PIN_RED, 0);
                    pwm_setup_gpio(BUZZER_B_PIN, 0);
                    turn_off_leds(0, true);
                }
                break;
            case 2:
                // Altera cor e ícone de acordo com o resfriamento
                red = 0.0; green = 190.0; blue = 255.0;
                icon = 3;
                if (current_temp > setpoint + hysteresis) { // Checa se a temperatura atual é maior que o setpoint + histerese se sim, diminui a temperatura e emite um beep
                    current_temp -= 0.1f;
                    if (current_time - last_beep_time > 1000) {
                        pwm_setup_gpio(BUZZER_B_PIN, 500);
                        gpio_put(LED_PIN_BLUE, 1);
                        last_beep_time = current_time;
                    }
                } else { // Se não, muda o estado para 0
                    state = 0;
                    gpio_put(LED_PIN_BLUE, 0);
                    pwm_setup_gpio(BUZZER_B_PIN, 0);
                    turn_off_leds(0, true);
                }
                break;
            case 3:
                // Altera cor e ícone de acordo com a mudança de temperatura
                red = 255.0; green = 255.0; blue = 0.0;
                icon = 1;
                current_temp = TEMP_MIN + ((float)adc_value / 4095.0f) * (TEMP_MAX - TEMP_MIN); // Altera a temperatura de acordo com o valor do ADC
                break;
            case 4:
                // Altera cor e ícone de acordo com a mudança de setpoint
                red = 255.0; green = 255.0; blue = 0.0;
                icon = 2;
                setpoint = TEMP_MIN + ((float)adc_value / 4095.0f) * (TEMP_MAX - TEMP_MIN); // Altera o setpoint de acordo com o valor do ADC
                break;
            case 5:
                // Altera cor e ícone de acordo com a mudança de histerese
                red = 255.0; green = 255.0; blue = 0.0;
                icon = 5;
                hysteresis = HYST_MIN + ((float)adc_value / 4095.0f) * (HYST_MAX - HYST_MIN); // Altera a histerese de acordo com o valor do ADC
                break;
            default:
                state = 0;
                break;
        }

        // Desenha a tela e envia os dados para o display
        draw_screen();
        ssd1306_send_data(&ssd); 

        // Desativa os buzzers e LEDs
        pwm_setup_gpio(BUZZER_A_PIN, 0);
        pwm_setup_gpio(BUZZER_B_PIN, 0);
        turn_off_leds(0, true);

        desenho_pio(icons[icon], 0, pio, sm, red, green, blue); // Desenha o ícone na matriz de LEDs

        sleep_ms(100); // Delay do loop principal
    }
    
    return 0;
}

// Função para desenhar a tela do display
void draw_screen() {
    if (state >= 3 && state <= 5) { // Se o estado for de mudança de temperatura, setpoint ou histerese desenha a tela de configuração
        ssd1306_fill(&ssd, false); 
        ssd1306_rect(&ssd, 0, 0, 128, 64, true, false);
        ssd1306_rect(&ssd, 0, 0, 128, 14, true, false);
        ssd1306_rect(&ssd, 13, 0, 128, 25, true, false);
        ssd1306_draw_string(&ssd, "Thermostat°C", 12, 4);
        if (state == 3) {
            ssd1306_draw_string(&ssd, "Temperature:", 19, 16);
            sprintf(buffer, "%.1f", current_temp);
        } else if (state == 4) {
            ssd1306_draw_string(&ssd, "Setpoint:", 30, 16); 
            sprintf(buffer, "%.1f", setpoint);
        } else if (state == 5) {
            ssd1306_draw_string(&ssd, "Hysteresis:", 22, 16); 
            sprintf(buffer, "%.1f", hysteresis);
        }
        ssd1306_draw_string(&ssd, buffer, 55, 26);
        ssd1306_draw_string(&ssd, "Status:", 37, 41); 
        get_state(buffer);
        ssd1306_draw_string(&ssd, buffer, 5, 50);
    } else { // Se não, desenha a tela padrão
        ssd1306_fill(&ssd, false); 
        ssd1306_rect(&ssd, 0, 0, 128, 64, true, false); 
        ssd1306_rect(&ssd, 0, 0, 128, 14, true, false); 
        ssd1306_rect(&ssd, 13, 0, 50, 25, true, false); 
        ssd1306_rect(&ssd, 13, 49, 100, 25, true, false); 
        ssd1306_rect(&ssd, 13, 99, 128, 25, true, false); 
        ssd1306_draw_string(&ssd, "Thermostat°C", 12, 4); 
        ssd1306_draw_string(&ssd, "Temp:", 7, 16); 
        sprintf(buffer, "%.1f", current_temp); 
        ssd1306_draw_string(&ssd, buffer, 9, 26); 
        ssd1306_draw_string(&ssd, "Set:", 61, 16); 
        sprintf(buffer, "%.1f", setpoint); 
        ssd1306_draw_string(&ssd, buffer, 59, 26); 
        ssd1306_draw_string(&ssd, "H:", 108, 16); 
        sprintf(buffer, "%.1f", hysteresis); 
        ssd1306_draw_string(&ssd, buffer, 103, 26); 
        ssd1306_draw_string(&ssd, "Status:", 37, 41); 
        get_state(buffer);
        ssd1306_draw_string(&ssd, buffer, 5, 50);
    }
}

// Função para obter o estado atual do termostato
void get_state(char* buffer) {
    switch (state) {
        case 0:
            sprintf(buffer, "Stable");
            break;
        case 1:
            sprintf(buffer, "Heating");
            break;
        case 2:
            sprintf(buffer, "Cooling");
            break;
        case 3:
            sprintf(buffer, "Changing Temp");
            break;
        case 4:
            sprintf(buffer, "Changing Set");
            break;
        case 5:
            sprintf(buffer, "Changing Hyster");
            break;
        default:
            sprintf(buffer, "Unknown");
            break;
    }
}

// Função para configurar os pinos GPIO
void setup_GPIOs() {
    // Inicializa os pinos dos botões como entrada com pull-up 
    gpio_init(BTN_A);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_pull_up(BTN_A);
    
    gpio_init(BTN_B);
    gpio_set_dir(BTN_B, GPIO_IN);
    gpio_pull_up(BTN_B);
    
    gpio_init(BTN_J);
    gpio_set_dir(BTN_J, GPIO_IN);
    gpio_pull_up(BTN_J);
    
    // Inicializa os pinos do LED RGB como saída
    gpio_init(LED_PIN_RED);   
    gpio_set_dir(LED_PIN_RED, GPIO_OUT);
    gpio_init(LED_PIN_BLUE);  
    gpio_set_dir(LED_PIN_BLUE, GPIO_OUT);
    gpio_init(LED_PIN_GREEN); 
    gpio_set_dir(LED_PIN_GREEN, GPIO_OUT);
    
    // Inicializa os pinos dos buzzers como saída
    gpio_init(BUZZER_A_PIN);  
    gpio_set_dir(BUZZER_A_PIN, GPIO_OUT);
    gpio_init(BUZZER_B_PIN);  
    gpio_set_dir(BUZZER_B_PIN, GPIO_OUT);
}

// Função para desligar os LEDs (Todos ou todos menos um específico)
void turn_off_leds(uint gpio, bool turn_off_all) {
    if (turn_off_all) {
        gpio_put(LED_PIN_RED, 0);
        gpio_put(LED_PIN_BLUE, 0);
        gpio_put(LED_PIN_GREEN, 0);
    } else {
        gpio_put(LED_PIN_RED, gpio == LED_PIN_RED ? 1 : 0);
        gpio_put(LED_PIN_BLUE, gpio == LED_PIN_BLUE ? 1 : 0);
        gpio_put(LED_PIN_GREEN, gpio == LED_PIN_GREEN ? 1 : 0);
    }
}

// Função para configurar o PWM
void pwm_setup_gpio(uint gpio, uint freq) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);  // Define o pino como saída PWM
    uint slice_num = pwm_gpio_to_slice_num(gpio);  // Obtém o slice do PWM

    if (freq == 0) {
        pwm_set_enabled(slice_num, false);  // Desabilita o PWM
        gpio_put(gpio, 0);  // Desliga o pino
    } else {
        uint32_t clock_div = 4; // Define o divisor do clock
        uint32_t wrap = (clock_get_hz(clk_sys) / (clock_div * freq)) - 1; // Calcula o valor de wrap

        // Configurações do PWM (clock_div, wrap e duty cycle) e habilita o PWM
        pwm_set_clkdiv(slice_num, clock_div); 
        pwm_set_wrap(slice_num, wrap);  
        pwm_set_gpio_level(gpio, wrap / 5);  
        pwm_set_enabled(slice_num, true);  
    }
}

// Rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(double r, double g, double b) {
    unsigned char R, G, B;
    R = r * red;
    G = g * green;
    B = b * blue;
    return (G << 24) | (R << 16) | (B << 8);
}

// Rotina para acionar a matrix de leds - ws2812b
void desenho_pio(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b) {
    for (int16_t i = 0; i < NUM_PIXELS; i++) {
        valor_led = matrix_rgb(desenho[24-i], desenho[24-i], desenho[24-i]);
        pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Função de callback para tratamento de interrupções
static void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual
    if (current_time - last_time > 500000) { // Debounce de 500 ms
        last_time = current_time;
        if (gpio == BTN_A) { // Se o botão A for pressionado, muda o estado para 3 (mudança de temperatura)
            if (state == 3) {
                state = 0;
            } else if (state == 0) {
                state = 3;
            }
        } else if (gpio == BTN_B) { // Se o botão B for pressionado, muda o estado para 4 (mudança de setpoint)
            if (state == 4) {
                state = 0;
            } else if (state == 0) {
                state = 4;
            }
        } else if (gpio == BTN_J) { // Se o botão do Joystick for pressionado, muda o estado para 5 (mudança de histerese)
            if (state == 5) {
                state = 0;
            } else if (state == 0) {
                state = 5;
            }
        }
    }
}