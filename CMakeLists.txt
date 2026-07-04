cmake_minimum_required(VERSION 3.20)

# Cross-compilation setup — must come before project()
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

# Bare-metal: skip the hosted-environment link test during configure
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(inverter_firmware C ASM)
 
# ── Compiler flags ─────────────────────────────────────────────────
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb")
set(CMAKE_C_FLAGS   "${CPU_FLAGS} -Wall -ffunction-sections -fdata-sections -O0 -g")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS} -g")

# ── Linker flags ───────────────────────────────────────────────────
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/STM32F103XB_FLASH.ld")
set(CMAKE_EXE_LINKER_FLAGS
    "${CPU_FLAGS} \
     -T${LINKER_SCRIPT} \
     -Wl,--gc-sections \
     -Wl,-Map=${CMAKE_PROJECT_NAME}.map \
     --specs=nosys.specs")

# ── Source files ───────────────────────────────────────────────────
add_executable(${CMAKE_PROJECT_NAME}.elf
    src/main.c
    src/gpio_driver.c
    src/clock_config.c
    src/adc_driver.c
    src/pwm_driver.c
    src/system_stm32f1xx.c
    startup/startup_stm32f103xb.s
)

# ── Include directories ────────────────────────────────────────────
# include/        — project headers (hw_config.h, driver headers)
# include/CMSIS/  — ARM core headers (core_cm3.h, etc.)
# include/Device/ — STM32F1xx register definitions (stm32f103xb.h, etc.)
target_include_directories(${CMAKE_PROJECT_NAME}.elf PRIVATE
    include
    include/CMSIS
    include/Device
)

# ── Preprocessor defines ───────────────────────────────────────────
# STM32F103xB: tells CMSIS which device-specific header to activate
target_compile_definitions(${CMAKE_PROJECT_NAME}.elf PRIVATE
    STM32F103xB
)

# ── Post-build: generate .bin and print memory usage ──────────────
find_program(OBJCOPY arm-none-eabi-objcopy)
find_program(SIZE    arm-none-eabi-size)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${OBJCOPY} -O binary
            ${CMAKE_PROJECT_NAME}.elf
            ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${SIZE} ${CMAKE_PROJECT_NAME}.elf
    COMMENT "Generating .bin and printing Flash/RAM usage"
)
