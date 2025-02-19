#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "pico/bootrom.h"

// Definição de portas e pinos
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO_DISPLAY 0x3C
#define JOYSTICK_X_PIN 26
#define JOYSTICK_Y_PIN 27
#define JOYSTICK_PB 22
#define BOTAO_A 5
#define LED_AZUL_PIN 12
#define LED_VERMELHO_PIN 13

// Variáveis para controle do estado dos LEDs
volatile bool led_azul_on = true;
volatile bool led_vermelho_on = true;
volatile uint32_t last_interrupt_time = 0;

// Configuração do PWM para os LEDs
void setup_pwm_for_led(uint gpio_pin, uint channel) {
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio_pin);
    pwm_set_wrap(slice_num, 255);
    pwm_set_chan_level(slice_num, channel, 0);
    pwm_set_enabled(slice_num, true);
}

// Função de interrupção para o Botão A (alternando LEDs)
void button_a_isr(uint gpio, uint32_t events) {
    uint32_t interrupt_time = to_ms_since_boot(get_absolute_time());
    if (interrupt_time - last_interrupt_time > 200) { // Debounce de 200ms
        led_azul_on = !led_azul_on;
        led_vermelho_on = !led_vermelho_on;
        last_interrupt_time = interrupt_time;
    }
}

int main() {
    stdio_init_all();

    // Configuração do Botão A com interrupção
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &button_a_isr);

    // Configuração do botão do joystick
    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB);

    // Configuração do PWM para LEDs
    setup_pwm_for_led(LED_AZUL_PIN, PWM_CHAN_A);
    setup_pwm_for_led(LED_VERMELHO_PIN, PWM_CHAN_B);

    // Inicialização do I2C e do display OLED
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    ssd1306_t ssd;
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_DISPLAY, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicialização do ADC para leitura do joystick
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);

    uint16_t adc_value_x, adc_value_y;
    char str_x[5], str_y[5];
    bool cor = true;

    while (true) {
        // Leitura dos valores do joystick
        adc_select_input(0);
        adc_value_x = adc_read();
        adc_select_input(1);
        adc_value_y = adc_read();
        sprintf(str_x, "%d", adc_value_x);
        sprintf(str_y, "%d", adc_value_y);

        // Ajuste do brilho dos LEDs com base no joystick
        uint8_t brilho_azul = (uint8_t)((abs((int)adc_value_y - 2048)) * 255 / 2048);
        uint8_t brilho_vermelho = (uint8_t)((abs((int)adc_value_x - 2048)) * 255 / 2048);

        pwm_set_chan_level(pwm_gpio_to_slice_num(LED_AZUL_PIN), PWM_CHAN_A, led_azul_on ? brilho_azul : 0);
        pwm_set_chan_level(pwm_gpio_to_slice_num(LED_VERMELHO_PIN), PWM_CHAN_B, led_vermelho_on ? brilho_vermelho : 0);

        // Atualização do display com informações
        ssd1306_fill(&ssd, !cor);
        ssd1306_draw_string(&ssd, "Joystick X:", 10, 20);
        ssd1306_draw_string(&ssd, str_x, 80, 20);
        ssd1306_draw_string(&ssd, "Joystick Y:", 10, 40);
        ssd1306_draw_string(&ssd, str_y, 80, 40);
        ssd1306_send_data(&ssd);

        sleep_ms(100);
    }

    return 0;
}
