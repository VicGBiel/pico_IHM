#ifndef MAIN_SHARED_H
#define MAIN_SHARED_H

#include <stdint.h>
#include <stdbool.h>

// ─── Mensagens da FIFO entre cores ───────────────────────────────────────────
// Valores hexadecimais válidos (apenas dígitos 0-9 e A-F)
#define MSG_CORE1_READY   0xC10001u   // Core 1 sinaliza que está pronto
#define MSG_DADOS_NOVOS   0xDA7A00u   // Core 0 avisa que há dados novos

// ─── Tipo de último reset ─────────────────────────────────────────────────────
typedef enum {
    RESET_POWER_ON  = 0,
    RESET_WATCHDOG  = 1
} TipoReset_t;

// ─── Dados compartilhados entre Core 0 e Core 1 ──────────────────────────────
typedef struct {
    uint32_t contador;
    float    temperatura;
    int      umidade;
    bool     dados_validos;
} DadosSensor_t;

#endif // MAIN_SHARED_H