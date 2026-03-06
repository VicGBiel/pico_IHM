cmake_minimum_required(VERSION 3.13)

# Importa o SDK do Pico (a variável PICO_SDK_PATH deve estar definida no ambiente)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(picoA_transmissor C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Inicializa o SDK
pico_sdk_init()

# Define o executável com os arquivos fonte
add_executable(picoA_transmissor
    main.c
)

# Linka as bibliotecas necessárias
target_link_libraries(picoA_transmissor
    pico_stdlib          # stdio, sleep, gpio
    hardware_uart        # UART
)

# Habilita saída via USB Serial (útil para debug) e desabilita UART padrão do stdio
pico_enable_stdio_usb(picoA_transmissor 1)
pico_enable_stdio_uart(picoA_transmissor 0)

# Gera os arquivos .uf2, .bin, .hex para gravação
pico_add_extra_outputs(picoA_transmissor)