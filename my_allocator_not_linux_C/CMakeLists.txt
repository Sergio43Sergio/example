# =============================================================================
# @file CMakeLists.txt
# @brief Скрипт сборки автономного SRAM-аллокатора U-Boot на стандарте C23.
# @copyright Copyright (c) 2026 Sergey (MIT License)
# =============================================================================

cmake_minimum_required(VERSION 3.21...3.30)

# 1. Объявление проекта и используемого языка (строго чистый Си)
project(UBootAllocator
    VERSION 1.0.0
    DESCRIPTION "Autonomous C23 SRAM heap allocator custom-tailored for U-Boot bootloader"
    LANGUAGES C
)

# 2. Настройка конфигурации отладки GDB по умолчанию
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' for GDB symbols as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# 3. Настройка глобального стандарта ISO C23
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # Требуем строгий ISO C23 без compiler-specific GNU костылей

# 4. Создание статической библиотеки аллокатора
# Это позволяет обособить uboot_allocator.c от тестового стенда
add_library(uboot_allocator STATIC uboot_allocator.c uboot_allocator.h)

# Настраиваем папки инклудов для библиотеки
target_include_directories(uboot_allocator PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# Включаем жесткие, бескомпромиссные флаги компиляции U-Boot/ядра (Warning as Error)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(uboot_allocator PRIVATE
        -Wall -Wextra -Werror -Wpedantic
        -Wshadow -Wstrict-prototypes -Wmissing-prototypes
    )
endif()

# 5. Сборка демонстрационного исполняемого файла (Имитатора SRAM)
add_executable(uboot_allocator_demo main.c) # main.c

# Линкуем наш тестовый стенд с библиотекой аллокатора
target_link_libraries(uboot_allocator_demo PRIVATE uboot_allocator)

# =============================================================================
# 6. БЛОК АВТОМАТИЧЕСКОГО ТЕСТИРОВАНИЯ (CTest)
# =============================================================================
enable_testing()

# Создаем изолированный бинарник для прогона автотестов
add_executable(uboot_allocator_tests main.c)
target_link_libraries(uboot_allocator_tests PRIVATE uboot_allocator)

# Регистрируем тест в автоматизированной системе CTest
add_test(NAME RunUBootAllocatorTests COMMAND uboot_allocator_tests)
