#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

// ─── Configurações UART ───────────────────────────────────────────────────────
#define UART_ID       uart0
#define BAUD_RATE     115200
#define UART_TX_PIN   0        // GP0 → TX

// ─── Simulação de sensores ────────────────────────────────────────────────────
#define TEMP_MIN      20.0f    // Temperatura mínima simulada (°C)
#define TEMP_MAX      35.0f    // Temperatura máxima simulada (°C)
#define SEND_INTERVAL 1000     // Intervalo de envio em milissegundos

// ─── Protótipos ───────────────────────────────────────────────────────────────
static float simular_temperatura(void);
static int   simular_umidade(void);
static void  uart_send_dados(uint32_t contador, float temp, int umidade);

// ─────────────────────────────────────────────────────────────────────────────
int main(void) {

    // Inicializa o clock padrão e o stdio (opcional, útil para debug via USB)
    stdio_init_all();

    // ── Configura UART0 ──────────────────────────────────────────────────────
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);

    // Formato: 8 bits de dados, sem paridade, 1 stop bit (8N1)
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);

    // Aguarda 1 segundo para estabilizar antes de começar a transmitir
    sleep_ms(1000);

    // ── Loop principal ───────────────────────────────────────────────────────
    uint32_t contador = 0;

    while (true) {
        contador++;

        float temp    = simular_temperatura();
        int   umidade = simular_umidade();

        // Envia os dados formatados via UART
        uart_send_dados(contador, temp, umidade);

        // Aguarda o intervalo antes do próximo envio
        sleep_ms(SEND_INTERVAL);
    }

    return 0; // Nunca alcançado
}

// ─── Gera uma temperatura simulada entre TEMP_MIN e TEMP_MAX ─────────────────
static float simular_temperatura(void) {
    // Usa o contador de ciclos do RP2040 como semente pseudo-aleatória
    uint32_t seed = time_us_32();
    float variacao = (float)(seed % 1500) / 100.0f; // 0.0 a 14.99
    return TEMP_MIN + variacao;
}

// ─── Gera uma umidade simulada entre 40% e 90% ───────────────────────────────
static int simular_umidade(void) {
    uint32_t seed = time_us_32() ^ 0xDEADBEEF;
    return 40 + (int)(seed % 51); // 40 a 90
}

// ─── Formata e envia os dados via UART ───────────────────────────────────────
// Protocolo de mensagem:
//   $PKT,<contador>,<temperatura>,<umidade>\n
// Exemplo:
//   $PKT,00042,27.35,65\n
//
// O prefixo "$PKT" permite que o receptor identifique e valide o pacote.
static void uart_send_dados(uint32_t contador, float temp, int umidade) {
    char buffer[64];

    // Monta a string do pacote
    int len = snprintf(buffer, sizeof(buffer),
                       "$PKT,%05lu,%.2f,%d\n",
                       (unsigned long)contador,
                       (double)temp,
                       umidade);

    // Envia byte a byte via UART0
    for (int i = 0; i < len; i++) {
        uart_putc(UART_ID, buffer[i]);
    }
}