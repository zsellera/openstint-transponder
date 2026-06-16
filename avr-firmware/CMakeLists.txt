cmake_minimum_required(VERSION 3.20)

# ── Hardware revision ─────────────────────────────────────────────────────────
# v1: attiny1616, LED on PORTA pin 5
# v2: attiny816,  LED on PORTB pin 4  (default)
set(HW_REV "v2" CACHE STRING "Hardware revision (v1 or v2)")
set_property(CACHE HW_REV PROPERTY STRINGS v1 v2)

if(HW_REV STREQUAL "v1")
    set(_DEFAULT_MCU      attiny1616)
    set(_DEFAULT_LED_PORT PORTA)
    set(_DEFAULT_LED_PIN  5)
else()
    set(_DEFAULT_MCU      attiny816)
    set(_DEFAULT_LED_PORT PORTB)
    set(_DEFAULT_LED_PIN  4)
endif()

# Force-update derived vars when HW_REV changes; preserve user overrides otherwise
if(NOT _HW_REV_LAST STREQUAL HW_REV)
    set(MCU      "${_DEFAULT_MCU}"      CACHE STRING "Target AVR MCU"        FORCE)
    set(LED_PORT "${_DEFAULT_LED_PORT}" CACHE STRING "LED port (PORTA/PORTB)" FORCE)
    set(LED_PIN  "${_DEFAULT_LED_PIN}"  CACHE STRING "LED pin number (0-7)"  FORCE)
    set(_HW_REV_LAST "${HW_REV}" CACHE INTERNAL "")
endif()
set(F_CPU      "10000000UL"     CACHE STRING "CPU frequency")
set(PROGRAMMER "atmelice_updi" CACHE STRING "avrdude programmer ID")

# ── Toolchain ─────────────────────────────────────────────────────────────────
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR avr)
# Prevent CMake from link-testing the cross compiler
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(AVR_GCC     avr-gcc     REQUIRED)
find_program(AVR_OBJCOPY avr-objcopy REQUIRED)
find_program(AVR_SIZE    avr-size    REQUIRED)
find_program(AVRDUDE     avrdude)

set(CMAKE_C_COMPILER   "${AVR_GCC}")
set(CMAKE_ASM_COMPILER "${AVR_GCC}")

project(transponder C)

# ── ATtiny DFP ────────────────────────────────────────────────────────────────
set(ATTINY_DFP_VERSION "2.1.484")
set(ATTINY_DFP_URL
    "http://packs.download.atmel.com/Atmel.ATtiny_DFP.${ATTINY_DFP_VERSION}.atpack")

# Search: explicit cache → sibling directory (any version) → download
if(NOT ATTINY_DFP OR NOT EXISTS "${ATTINY_DFP}/include")
    set(_dfp_exact "${CMAKE_CURRENT_SOURCE_DIR}/../Atmel.ATtiny_DFP.${ATTINY_DFP_VERSION}")
    if(EXISTS "${_dfp_exact}/include")
        set(ATTINY_DFP "${_dfp_exact}" CACHE PATH "ATtiny DFP root" FORCE)
    else()
        file(GLOB _dfp_candidates LIST_DIRECTORIES true
             "${CMAKE_CURRENT_SOURCE_DIR}/../Atmel.ATtiny_DFP.*")
        foreach(_c IN LISTS _dfp_candidates)
            if(EXISTS "${_c}/include")
                set(ATTINY_DFP "${_c}" CACHE PATH "ATtiny DFP root" FORCE)
                break()
            endif()
        endforeach()
    endif()
endif()

if(NOT ATTINY_DFP OR NOT EXISTS "${ATTINY_DFP}/include")
    message(STATUS "ATtiny DFP not found — downloading from Atmel...")
    set(_atpack  "${CMAKE_BINARY_DIR}/Atmel.ATtiny_DFP.${ATTINY_DFP_VERSION}.atpack")
    set(_dfp_dir "${CMAKE_CURRENT_SOURCE_DIR}/../Atmel.ATtiny_DFP.${ATTINY_DFP_VERSION}")
    file(DOWNLOAD "${ATTINY_DFP_URL}" "${_atpack}"
         SHOW_PROGRESS
         STATUS _dl_status)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        list(GET _dl_status 1 _dl_msg)
        message(FATAL_ERROR "ATtiny DFP download failed (${_dl_code}): ${_dl_msg}\n"
                            "Download manually from ${ATTINY_DFP_URL} and set "
                            "-DATTINY_DFP=<path>")
    endif()
    file(ARCHIVE_EXTRACT INPUT "${_atpack}" DESTINATION "${_dfp_dir}")
    set(ATTINY_DFP "${_dfp_dir}" CACHE PATH "ATtiny DFP root" FORCE)
    message(STATUS "ATtiny DFP extracted to ${ATTINY_DFP}")
endif()

message(STATUS "HW_REV   : ${HW_REV}")
message(STATUS "MCU      : ${MCU}")
message(STATUS "LED_PORT : ${LED_PORT}  pin ${LED_PIN}")
message(STATUS "F_CPU    : ${F_CPU}")
message(STATUS "DFP      : ${ATTINY_DFP}")

# ── Firmware target ───────────────────────────────────────────────────────────
set(_avr_mcu_flags
    -mmcu=${MCU}
    -B "${ATTINY_DFP}/gcc/dev/${MCU}"
    -I "${ATTINY_DFP}/include"
)

add_executable(main.elf main.c)

target_compile_options(main.elf PRIVATE
    ${_avr_mcu_flags}
    -O2
    -fstack-usage
    -DF_CPU=${F_CPU}
    -DLED_PORT=${LED_PORT}
    -DLED_PIN_bm=PIN${LED_PIN}_bm
)
target_link_options(main.elf PRIVATE ${_avr_mcu_flags})

# .hex + size report
add_custom_command(TARGET main.elf POST_BUILD
    COMMAND "${AVR_OBJCOPY}" -O ihex -R .eeprom
            "$<TARGET_FILE:main.elf>" "${CMAKE_BINARY_DIR}/main.hex"
    COMMAND "${AVR_SIZE}" --mcu=${MCU} --format=avr "$<TARGET_FILE:main.elf>"
    COMMENT "Generating main.hex"
    VERBATIM
)

# ── Convenience targets ───────────────────────────────────────────────────────
add_custom_target(flash
    COMMAND "${AVRDUDE}" -c ${PROGRAMMER} -p ${MCU}
            -U "flash:w:${CMAKE_BINARY_DIR}/main.hex"
            -U fuse1:w:0x02:m
    DEPENDS main.elf
    COMMENT "Flashing ${MCU} and writing fuses via UPDI"
)
