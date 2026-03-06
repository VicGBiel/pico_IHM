cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(picoB_ihm C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ─── libdvi LOCAL (copiada do exemplo BitDogLab) ──────────────────────────────
# A pasta libdvi deve estar dentro do projeto: pico_RX/libdvi/
add_subdirectory(libdvi)

# ─── Executável principal ─────────────────────────────────────────────────────
add_executable(picoB_ihm
    main.c
    core1_display.c
)

# ─── Bibliotecas ─────────────────────────────────────────────────────────────
target_link_libraries(picoB_ihm
    pico_stdlib
    pico_multicore
    pico_sync
    hardware_uart
    hardware_watchdog
    hardware_clocks
    hardware_vreg           # vreg_set_voltage para 252 MHz
    hardware_irq
    hardware_dma
    libdvi                  # biblioteca DVI local
)

# ─── Includes ────────────────────────────────────────────────────────────────
target_include_directories(picoB_ihm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}           # main_shared.h
    ${CMAKE_CURRENT_SOURCE_DIR}/libdvi    # dvi.h, dvi_serialiser.h, etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/include   # common_dvi_pin_configs.h
)

# Saída USB Serial para debug; UART stdio desativado (UART usada para dados)
pico_enable_stdio_usb(picoB_ihm 1)
pico_enable_stdio_uart(picoB_ihm 0)

# Gera .uf2, .bin, .hex
pico_add_extra_outputs(picoB_ihm)