cmake_minimum_required(VERSION 3.16)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/stm32_gcc.cmake)

project(openstint-transponder C ASM)

set(PROJECT_SOURCES
    main.c
)

set(CMAKE_INCLUDE_CURRENT_DIR TRUE)

stm32_fetch_cmsis(C0)
stm32_fetch_hal(C0)

find_package(CMSIS COMPONENTS STM32C0 REQUIRED)
find_package(HAL COMPONENTS STM32C0 LL_RCC LL_UTILS LL_GPIO LL_TIM LL_SPI REQUIRED)

add_executable(transponder ${PROJECT_SOURCES})
target_link_libraries(transponder
    HAL::STM32::C0::LL_RCC
    HAL::STM32::C0::LL_UTILS
    HAL::STM32::C0::LL_GPIO
    HAL::STM32::C0::LL_TIM
    HAL::STM32::C0::LL_SPI
    CMSIS::STM32::C011F4
    STM32::NoSys
)
stm32_generate_hex_file(transponder)
stm32_print_size_of_target(transponder)
