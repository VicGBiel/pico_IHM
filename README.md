# Sistema de Telemetria DVI com UART e Watchdog — RP2040

Esse projeto implementa um sistema de telemetria embarcado de dois nós utilizando o microcontrolador RP2040 (Raspberry Pi Pico), com interface de vídeo DVI em tempo real, comunicação UART assíncrona e resiliência via Watchdog Timer.

---

## Visão Geral

| Nó     | Função                                      | Clock     |
|--------|---------------------------------------------|-----------|
| Pico A | Transmissor UART — simula leituras de sensor | 125 MHz   |
| Pico B | Receptor multicore + renderização DVI        | 252 MHz   |

O Pico B exibe em monitor HDMI (via placa periférica BitDogLab) os dados de temperatura, umidade e contador de pacotes recebidos. O sistema se recupera automaticamente de travamentos por meio de Watchdog Timer com heartbeat bicore.

---

## Hardware Necessário

- 2x Raspberry Pi Pico (RP2040)
- Placa periférica HDMI BitDogLab (conector HDMI fêmea com resistores TMDS integrados)
- Monitor com entrada HDMI
- Cabo jumper para a conexão UART entre os Picos
- 2x cabo USB para alimentação e gravação

### Conexões

```
Pico A  GP0 (TX)  ──────────────  GP1 (RX)  Pico B
Pico A  GND       ──────────────  GND       Pico B
```

A placa HDMI encaixa diretamente nos pinos do Pico B. Os pinos DVI utilizados são:

```
GP16  — TMDS D0
GP17  — TMDS D1
GP18  — TMDS D2
GP19  — TMDS CLK
```

Configuração correspondente em `common_dvi_pin_configs.h`: `picodvi_dvi_cfg`.

---

## Estrutura do Repositório

```
.
├── picoA/
│   ├── CMakeLists.txt
│   ├── main.c                  # Transmissor UART
│   └── pico_sdk_import.cmake
│
├── picoB/
│   ├── CMakeLists.txt
│   ├── main.c                  # Core 0: UART, mutex, Watchdog
│   ├── core1_display.c         # Core 1: DVI, framebuffer, renderização
│   ├── main_shared.h           # Tipos e constantes compartilhados
│   ├── pico_sdk_import.cmake
│   └── libdvi/                 # PicoDVI — biblioteca local (copiar do exemplo BitDogLab)
│       ├── CMakeLists.txt
│       ├── asm_suppress.h      # Correção para SDK 2.2.0 no Windows
│       ├── dvi.c / dvi.h
│       ├── dvi_serialiser.c / .h / .pio
│       ├── dvi_timing.c / .h
│       ├── tmds_encode.c / .h / .S / .pio
│       └── ...
│
└── docs/
    └── relatorio.docx
```

---

## Protocolo UART

O Pico A envia pacotes ASCII a cada 1 segundo no seguinte formato:

```
$PKT,<contador>,<temperatura>,<umidade>\n
```

Exemplo:

```
$PKT,00042,27.35,65
```

| Campo       | Tipo    | Descrição                       |
|-------------|---------|----------------------------------|
| contador    | uint32  | Número sequencial do pacote      |
| temperatura | float   | Temperatura em graus Celsius     |
| umidade     | int     | Umidade relativa em percentual   |

---

## Arquitetura do Firmware (Pico B)

O firmware do Pico B utiliza os dois cores do RP2040 com responsabilidades estritamente separadas, eliminando condição de corrida sobre o framebuffer.

```
Core 0                                Core 1
──────────────────────────────────    ──────────────────────────────────
Lê bytes da UART                      Inicializa DVI (640x480p@60Hz)
Parseia pacotes $PKT                  Aguarda MSG_CORE1_READY enviado
Salva dados em DadosSensor_t          via FIFO para o Core 0
  (protegida por mutex)
Envia MSG_DADOS_NOVOS via FIFO        Loop principal:
Sinaliza vivo_core0 = true              Checa FIFO por MSG_DADOS_NOVOS
                                        Copia dados (mutex)
watchdog_update() chamado               fb_clear()
somente se vivo_core0 && vivo_core1     desenhar_tela_interna()
                                        push_scanlines()
                                        Sinaliza vivo_core1 = true
```

O framebuffer (`uint8_t framebuf[240][320]`, RGB332, 8 bpp) é acessado exclusivamente pelo Core 1. O pixel doubling vertical (`framebuf[y/2]`) mapeia os 240 pixels lógicos nos 480 pixels físicos do sinal DVI.

---

## Mecanismo de Watchdog

- Timeout configurado: **3000 ms**
- `watchdog_update()` é chamado pelo Core 0 somente quando `vivo_core0 == true && vivo_core1 == true`
- Após a chamada, ambos os flags são resetados para `false`
- Se qualquer core travar, o timeout expira e o sistema reinicia automaticamente
- Na próxima inicialização, `watchdog_caused_reboot()` detecta a causa e a interface exibe o diagnóstico

---

## Interface DVI

Resolução lógica do framebuffer: **320x240 pixels** (8 bpp RGB332)  
Resolução de saída: **640x480p @ 60 Hz** (pixel doubling 2x horizontal e vertical)

Layout da tela:

```
+--------------------------------------------------+
|  TELEMETRIA DVI - BitDogLab                      |
+--------------------------------------------------+
|  [ POWER ON - inicializacao normal             ] |
+--------------------------------------------------+
|  TEMPERATURA:   27.35 C                          |
|  UMIDADE:       65 %                             |
|  PACOTE Nro:    00042                            |
+--------------------------------------------------+
|  SINAL UART:   [ OK ]                            |
+--------------------------------------------------+
|  Embarcatech | WDT: ATIVO | 640x480p             |
+--------------------------------------------------+
```

O campo de status UART muda para `[ SEM SINAL ]` com fundo vermelho após 3 segundos sem receber pacotes. O campo de reset alterna entre verde (Power On) e vermelho (Watchdog Reset) conforme a causa do último reboot.

---

## Dependências e Compilação

### Requisitos

- [Pico SDK 2.x](https://github.com/raspberrypi/pico-sdk)
- CMake >= 3.13
- Arm GNU Toolchain (arm-none-eabi-gcc)
- Ninja
- PicoDVI — copiar a pasta `libdvi` do repositório [hlab (BitDogLab)](https://github.com/jrfo-hwit/hlab) para dentro de `picoB/libdvi/`

### Compilação

**Pico A:**

```bash
cd picoA
mkdir build && cd build
cmake .. -G "Ninja"
ninja
```

**Pico B:**

```bash
cd picoB
mkdir build && cd build
cmake -DPICO_COPY_TO_RAM=1 .. -G "Ninja"
ninja
```

> A flag `-DPICO_COPY_TO_RAM=1` é obrigatória para o Pico B. Garante que o código DVI execute da RAM, atendendo aos requisitos de latência do pipeline TMDS.

### Nota para Windows (SDK 2.2.0)

O arquivo `pico.h` do SDK 2.2.0 contém macros CMake que o preprocessador do assembler GNU não reconhece ao compilar `tmds_encode.S`. A correção está em `picoB/libdvi/asm_suppress.h`, incluído automaticamente via `-include` no `CMakeLists.txt` da libdvi.

---

## Gravação

Após a compilação, grave o `.uf2` gerado em cada Pico segurando o botão BOOTSEL ao conectar o USB:

```
picoA/build/picoA.uf2   →  Pico A
picoB/build/picoB_ihm.uf2  →  Pico B
```
