/*
 * Ohmímetro com Reconhecimento de Código de Cores
 * Por: Leonardo Rodrigues Luz
 *
 * Modificado a partir do código original de Wilton Lacerda Silva
 *
 * Funcionalidades:
 * 1. Mede resistência usando divisor de tensão
 * 2. Identifica o valor comercial mais próximo (série E24 5%)
 * 3. Exibe o código de cores no display OLED
 * 4. Mostra as faixas coloridas em um resistor estilizado
 *
 *  CEPEDI - TIC 37
 *  EMBARCATECH
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/np_led.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28
#define Botao_A 5
#define MATRIX_LED_PIN 7
#define botaoB 6

#include "pico/bootrom.h"
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}

// Valores da série E24 com tolerância 5%
const int e24_series[] = {
    100, 110, 120, 130, 150, 160, 180, 200, 220, 240, 270, 300,
    330, 360, 390, 430, 470, 510, 560, 620, 680, 750, 820, 910};

const uint8_t resistor_colors[12][3] = {
    {0, 0, 0},    // Preto
    {37, 18, 0},  // Marrom
    {63, 0, 0},   // Vermelho
    {63, 17, 0},  // Laranja
    {63, 63, 0},  // Amarelo
    {0, 63, 0},   // Verde
    {0, 0, 63},   // Azul
    {32, 0, 32},  // Violeta
    {32, 32, 32}, // Cinza
    {63, 63, 63}, // Branco
    {53, 43, 13}, // Dourado
    {48, 48, 48}  // Prateado
};

int R_conhecido = 10000;   // Resistor de 10k ohm
float ADC_VREF = 3.31;     // Tensão de referência do ADC
int ADC_RESOLUTION = 4095; // Resolução do ADC (12 bits)

// Estrutura para armazenar as faixas do resistor
typedef struct
{
  int digit1;
  int digit2;
  int multiplier;
} ResistorBands;

// Função para encontrar o valor E24 mais próximo
int find_closest_e24(float measured)
{
  int decade = (int)pow(10, (int)log10(measured));
  float normalized = measured / decade;

  int closest = e24_series[0];
  float min_diff = fabs(normalized - (e24_series[0] / 100.0));

  for (int i = 1; i < 24; i++)
  {
    float diff = fabs(normalized - (e24_series[i] / 100.0));
    if (diff < min_diff)
    {
      min_diff = diff;
      closest = e24_series[i];
    }
  }

  // Ajustar para a década correta
  int result = closest * (decade / 100);
  return result;
}

// Função para determinar as faixas do resistor
ResistorBands determine_bands(int value)
{
  ResistorBands bands;

  if (value < 10)
  {
    bands.digit1 = 0;
    bands.digit2 = value;
    bands.multiplier = 0;
  }
  else if (value < 100)
  {
    bands.digit1 = value / 10;
    bands.digit2 = value % 10;
    bands.multiplier = 1;
  }
  else
  {
    int decade = (int)pow(10, (int)log10(value) - 1);
    bands.digit1 = (value / decade) / 10;
    bands.digit2 = (value / decade) % 10;
    bands.multiplier = (int)log10(value) - 1;
  }

  return bands;
}

/*
 * Função para exibir as faixas do resistor como texto
 */
void display_resistor_bands(ssd1306_t ssd, ResistorBands bands)
{
  // Nomes das cores correspondentes aos dígitos
  const char *color_names[] = {
      "Preto", "Marrom", "Vermelho", "Laranja", "Amarelo",
      "Verde", "Azul", "Violeta", "Cinza", "Branco",
      "Dourado", "Prateado"};

  // Exibe as faixas como texto
  char band1_str[20], band2_str[20], band3_str[20];

  // Primeira faixa
  snprintf(band1_str, sizeof(band1_str), "1a: %s", color_names[bands.digit1]);
  ssd1306_draw_string(&ssd, band1_str, 5, 30);

  // Segunda faixa
  snprintf(band2_str, sizeof(band2_str), "2a: %s", color_names[bands.digit2]);
  ssd1306_draw_string(&ssd, band2_str, 5, 40);

  // Multiplicador
  snprintf(band3_str, sizeof(band3_str), "Mult: %s", color_names[bands.multiplier]);
  ssd1306_draw_string(&ssd, band3_str, 5, 50);
}

// Função para mostrar cores nos LEDs Neopixel
void show_bands_on_leds(ResistorBands bands)
{
  npSetLED(getIndex(3, 2), resistor_colors[bands.digit1][0],
           resistor_colors[bands.digit1][1],
           resistor_colors[bands.digit1][2]);

  npSetLED(getIndex(2, 2), resistor_colors[bands.digit2][0],
           resistor_colors[bands.digit2][1],
           resistor_colors[bands.digit2][2]);

  npSetLED(getIndex(1, 2), resistor_colors[bands.multiplier][0],
           resistor_colors[bands.multiplier][1],
           resistor_colors[bands.multiplier][2]);

  npWrite();
}

int main()
{
  stdio_init_all();

  // Inicializa LEDs Neopixel
  npInit(MATRIX_LED_PIN);

  // Configura botão B para modo BOOTSEL
  gpio_init(botaoB);
  gpio_set_dir(botaoB, GPIO_IN);
  gpio_pull_up(botaoB);
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

  // Configura botão A
  gpio_init(Botao_A);
  gpio_set_dir(Botao_A, GPIO_IN);
  gpio_pull_up(Botao_A);

  // Inicializa I2C e display OLED
  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  ssd1306_t ssd;
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
  ssd1306_config(&ssd);
  ssd1306_send_data(&ssd);

  // Limpa o display
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

  // Inicializa ADC
  adc_init();
  adc_gpio_init(ADC_PIN);

  char resistance_str[20];
  // char e24_str[20];

  while (true)
  {
    adc_select_input(2); // Seleciona o ADC para o pino 28

    // Faz a média de 500 leituras para maior precisão
    float soma = 0.0f;
    for (int i = 0; i < 500; i++)
    {
      soma += adc_read();
      sleep_ms(1);
    }
    float media = soma / 500.0f;

    // Calcula a resistência desconhecida
    float R_x = (R_conhecido * media) / (ADC_RESOLUTION - media);

    // Encontra o valor comercial mais próximo
    int e24_value = find_closest_e24(R_x);

    // Determina as faixas do resistor
    ResistorBands bands = determine_bands(e24_value);

    // Prepara strings para exibição
    if (R_x >= 1000)
    {
      sprintf(resistance_str, "%.1fk", R_x / 1000);
    }
    else
    {
      sprintf(resistance_str, "%.0f", R_x);
    }
    // sprintf(e24_str, "E24: %d", e24_value);

    // Atualiza o display
    ssd1306_fill(&ssd, false);

    // Cabeçalho
    ssd1306_draw_string(&ssd, "Ohmimetro E24", 15, 0);
    ssd1306_line(&ssd, 0, 10, 127, 10, true);

    // Valores medidos
    ssd1306_draw_string(&ssd, "Medido:", 5, 15);
    ssd1306_draw_string(&ssd, resistance_str, 60, 15);

    // ssd1306_draw_string(&ssd, "Padrao:", 5, 25);
    // ssd1306_draw_string(&ssd, e24_str, 60, 25);

    // Exibe as faixas como texto
    display_resistor_bands(ssd, bands);

    // Atualiza LEDs Neopixel (opcional - mantém as cores físicas)
    show_bands_on_leds(bands);

    ssd1306_send_data(&ssd);
    sleep_ms(500);
  }
}