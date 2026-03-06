#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "main_shared.h"
#include "hardware/vreg.h"
#include "hardware/structs/bus_ctrl.h"

extern void desenhar_tela(const DadosSensor_t *d, bool sinal_ok);

// ─── Configurações UART ───────────────────────────────────────────────────────
#define UART_ID         uart0
#define BAUD_RATE       115200
#define UART_RX_PIN     1           // GP1 → RX

// ─── Configurações Watchdog ───────────────────────────────────────────────────
#define WDT_TIMEOUT_MS  3000

// ─── Protocolo de pacote UART ─────────────────────────────────────────────────
#define PKT_PREFIX      "$PKT,"
#define PKT_MAX_LEN     64

// ─── Dados compartilhados protegidos por mutex ────────────────────────────────
static DadosSensor_t dados_sensor = {0};
static mutex_t       mutex_dados;

// ─── Heartbeat: bit 0 = Core 0 vivo | bit 1 = Core 1 vivo ────────────────────
static volatile bool vivo_core0 = false;
static volatile bool vivo_core1 = false;

// ─── Tipo de reset detectado na inicialização ─────────────────────────────────
static TipoReset_t tipo_reset = RESET_POWER_ON;

// ─── Declarações externas (core1_display.c) ───────────────────────────────────
extern void core1_entry(void);
extern void core1_set_reset_info(TipoReset_t tipo);

// ─── Protótipos locais ────────────────────────────────────────────────────────
static TipoReset_t diagnosticar_reset(void);
static bool        parsear_pacote(const char *linha, DadosSensor_t *out);
static void        core0_loop(void);

// ─────────────────────────────────────────────────────────────────────────────
int main(void) {

    vreg_set_voltage(VREG_VOLTAGE_1_20); 
    sleep_ms(10);
    set_sys_clock_khz(252000, true);

    stdio_init_all();

    hw_set_bits(&bus_ctrl_hw->priority, BUSCTRL_BUS_PRIORITY_PROC1_BITS);

    // ── 1. Detecta causa do último reset ──────────────────────────────────────
    tipo_reset = diagnosticar_reset();

    // ── 2. Inicializa mutex ───────────────────────────────────────────────────
    mutex_init(&mutex_dados);

    // ── 3. Inicializa UART0 no GP1 ───────────────────────────────────────────
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);

    // ── 4. Repassa o tipo de reset para o Core 1 ──────────────────────────────
    core1_set_reset_info(tipo_reset);

    // ── 5. Lança o Core 1 ────────────────────────────────────────────────────
    multicore_launch_core1(core1_entry);

    // ── 6. Aguarda Core 1 sinalizar que está pronto ───────────────────────────
    uint32_t sinal = multicore_fifo_pop_blocking();
    if (sinal != MSG_CORE1_READY) {
        // Core 1 não respondeu corretamente — deixa WDT reiniciar
        while (true) { tight_loop_contents(); }
    }

    // ── 7. Habilita o Watchdog ────────────────────────────────────────────────
    watchdog_enable(WDT_TIMEOUT_MS, true);

    // ── 8. Loop principal do Core 0 ───────────────────────────────────────────
    core0_loop();

    return 0;
}

static void core0_loop(void) {
    char linha[PKT_MAX_LEN];
    int  idx = 0;
    
    bool uart_ok = false;
    uint32_t ultimo_dado_ms = to_ms_since_boot(get_absolute_time());
    uint32_t ultimo_desenho_ms = 0; // Variável para controlar os FPS
    DadosSensor_t dados_display = {0};

    while (true) {
        bool deve_redesenhar = false;

        // Lê bytes da UART até '\n'
        if (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);

            if (c == '\n') {
                linha[idx] = '\0';
                idx = 0;

                DadosSensor_t novo = {0};
                if (parsear_pacote(linha, &novo)) {
                    dados_display = novo;
                    uart_ok = true;
                    ultimo_dado_ms = to_ms_since_boot(get_absolute_time());
                    deve_redesenhar = true;
                }
            } else if (idx < PKT_MAX_LEN - 1) {
                linha[idx++] = c;
            } else {
                idx = 0; 
            }
        }

        // Verifica o timeout do sinal (3 segundos)
        uint32_t agora = to_ms_since_boot(get_absolute_time());
        if (uart_ok && (agora - ultimo_dado_ms) > 3000) {
            uart_ok = false;
            deve_redesenhar = true;
        }

        // REDESENHA O ECRÃ NO MÁXIMO A 10 FPS (a cada 100ms)
        // Isto liberta 90% do barramento de memória para o vídeo DVI não bloquear!
        if (deve_redesenhar && (agora - ultimo_desenho_ms > 100)) {
            desenhar_tela(&dados_display, uart_ok);
            ultimo_desenho_ms = agora;
        }

        // Seta heartbeat do Core 0 e avalia o Watchdog
        vivo_core0 = true;
        if (vivo_core0 && vivo_core1) {
            watchdog_update();
            vivo_core0 = false;
            vivo_core1 = false;
        }

        
        // Pequena pausa para o processador respirar
        sleep_us(100); 
    }
}

// ─── Detecta se o último reset foi causado pelo Watchdog ─────────────────────
static TipoReset_t diagnosticar_reset(void) {
    if (watchdog_caused_reboot()) {
        return RESET_WATCHDOG;
    }
    return RESET_POWER_ON;
}

// ─── Parseia linha no formato "$PKT,00042,27.35,65" ──────────────────────────
static bool parsear_pacote(const char *linha, DadosSensor_t *out) {
    if (strncmp(linha, PKT_PREFIX, strlen(PKT_PREFIX)) != 0) {
        return false;
    }

    const char *ptr = linha + strlen(PKT_PREFIX);
    uint32_t contador;
    float    temp;
    int      umidade;

    int campos = sscanf(ptr, "%lu,%f,%d",
                        (unsigned long *)&contador,
                        &temp,
                        &umidade);

    if (campos != 3) return false;

    out->contador    = contador;
    out->temperatura = temp;
    out->umidade     = umidade;
    return true;
}

void core0_get_dados(DadosSensor_t *dst) {
    mutex_enter_blocking(&mutex_dados);
    *dst = dados_sensor;
    dados_sensor.dados_validos = false;
    mutex_exit(&mutex_dados);
}

void core1_set_heartbeat(void) {
    vivo_core1 = true;
}