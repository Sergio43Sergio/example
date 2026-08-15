#======================================================================
# @file CMakeLists.txt
# @brief Скрипт сборки проекта кастомного аллокатора на стандарте C++20
# @copyright Copyright (c) 2026 Sergei Zobach (MIT License)
#======================================================================

cmake_minimum_required(VERSION 3.14...3.28)

# 1. Объявляем проекты и используемых языков
project(ModernAllocator
        VERSION 1.0.0
        DESCRIPTION "Custom high-performance C++20 explicit free-list allocator using Linux mmap"
        LANGUAGES CXX 
)
#----------------------------------------------------------------
# 2. Настройка конфигурации отладки (GDB) о умолчанию
#----------------------------------------------------------------
# Если при запуске CMake не указал тип сборки, принудительно ставим Debug
 if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to 'Debug' as none was specified.")
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
        #Позволяет VS Code и другим IDE видеть доступные режимы
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
 endif()
# Настройка глобального стандарта С++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) #Отключаем compailer-specific расширения (GNU), требуем чистый ISO C++

#  Создание интерфейсной библиотеки (так как наш аллокатор шаблонный и полностью в .hpp)
add_library(allocator_headers INTERFACE)

# Привязываем заголовочные файлы к таргету библиотеки
target_include_directories(allocator_headers INTERFACE
     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

# 4. Настройка сборки демонстрационного бинарного файла
add_executable(allocator_demo main.cpp)
# Линкуем демо-приложение с нашей интерфейсной библиотекой 
target_link_libraries(allocator_demo PRIVATE allocator_headers)
# Включаем жёсткие флаги компилятора для обеспечения безопасности кода (как в ядре или OpenBMC)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
   target_compile_options(allocator_demo PRIVATE
            -Wall -Wextra -Werror -Wpedantic
            -Wshadow -Wnon-virtual-dtor -Wold-style-cast
            )
endif()

#==============================================================
# 5. Блок автоматического тестирования (CTest)
#==============================================================
enable_testing()

# Создаём тестовый исполняемый файл 
add_executable(allocator_tests main.cpp) # В нашем случае main.cpp уже выполняет проверку контейнера
target_link_libraries(allocator_tests PRIVATE allocator_headers)

# регистрируем тест в системе CTest
add_test(NAME RunAllocatorIntegrationTest COMMAND allocator_tests)

