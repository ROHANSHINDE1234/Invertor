cmake_minimum_required(VERSION 3.20) # Tells CMake "don't try to process this file unless you're at least this version"
set(CMAKE_SYSTEM_NAME Generic) # This tells Cmake that the machine on which are building is not the same machine we are writing on another one which doesnot have a OS
set(CMAKE_SYSTEM_PROCESSOR arm) # This tells that it is a arm processor
set(CMAKE_C_COMPILER arm-none-eabi-gcc) # This handles the .C files
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc) # This handles the .S files
set(CMAKE_CXX_COMPILER arm-none-eabi-g++) # This handles the .CPP files
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY) # This tells CMake to try to compile a static library when testing the compiler, which is useful for cross-compiling
project(inverter_firmware C ASM) # This fires cmake and tells it to use C and ASM files
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb") # This tells the compiler to use the cortex-m3 architecture and thumb instruction set
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -ffunction-sections -fdata-sections -O0 -g") # This tells the compiler to use the CPU_FLAGS and also to enable all warnings, put each function and data in its own section, optimize for debugging, and include debug information
set(CMAKE_ASM_FLAGS "${CPU_FLAGS} -g") # This tells the assembler to use the CPU_FLAGS and include debug information
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/STM32F103XB_FLASH.ld") # This tells the linker to use the linker script located at the specified path
set(CMAKE_EXE_LINKER_FLAGS
    "${CPU_FLAGS} -T${LINKER_SCRIPT} -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map --specs=nosys.specs") # This tells the linker to use the CPU_FLAGS and the linker script, and to garbage collect unused sections, create a map file, and use the nosys specifications


# This tells CMake to create an executable target with the specified name and source files
add_executable(${CMAKE_PROJECT_NAME}.elf
    src/main.c
    src/gpio_driver.c
    src/clock_config.c
    src/pwm_driver.c
    src/system_stm32f1xx.c
    startup/startup_stm32f103xb.s
)

# This tells CMake to include the specified directories when compiling the target
target_include_directories(${CMAKE_PROJECT_NAME}.elf PRIVATE
    include
    include/CMSIS
    include/Device
)

# This tells CMake to define the specified preprocessor macro when compiling the target
target_compile_definitions(${CMAKE_PROJECT_NAME}.elf PRIVATE
    STM32F103xB
)

# This tells CMake to link the specified libraries when linking the target
find_program(OBJCOPY arm-none-eabi-objcopy)
find_program(SIZE arm-none-eabi-size)

# This tells CMake to add a custom command that runs after the target is built, which generates a binary file from the ELF file and prints the memory usage
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${OBJCOPY} -O binary
        ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${SIZE} ${CMAKE_PROJECT_NAME}.elf
    COMMENT "Generating .bin and printing memory usage"
)
