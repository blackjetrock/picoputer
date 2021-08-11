cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(picoputer1)

pico_sdk_init()

add_executable(picoputer1
picoputer1.c
picoputer1.c
stub.c
p.c
arithmetic.c
server.c
fparithmetic.c
s_ldexp.c
s_ldexpf.c
s_scalbn.c
s_scalbnf.c
w_sqrt.c
w_remainder.c
)


pico_generate_pio_header(picoputer1 ${CMAKE_CURRENT_LIST_DIR}/picoputer.pio)

pico_enable_stdio_usb(picoputer1 1)

pico_enable_stdio_uart(picoputer1 1)
pico_add_extra_outputs(picoputer1)

target_link_libraries(picoputer1 pico_stdlib pico_sd_card hardware_pio hardware_spi)
