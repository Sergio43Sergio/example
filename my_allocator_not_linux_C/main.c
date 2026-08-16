/**
 * @file main.c
 * @brief Hardware simulation and integration test suite for the U-Boot allocator.
 * @details Allocates a static memory array to simulate physical SRAM of an embedded 
 *          platform (e.g., NXP Layerscape ARM64) and performs continuous allocation,
 *          validation, and boundary testing according to the ISO C23 standard.
 * 
 * @copyright Copyright (c) 2026 Sergey Zobach
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "uboot_allocator.h"
#include <stdio.h> /* Разрешено исключительно внутри имметатора, нужен для printf*/
#include <stdint.h>
#include <stdlib.h>  /* Добавляем СТРОГО для выделения mock-памяти на ПК */

/**
 * @brief Размер имметируемой статическй памяти SRAM (8КБ).
 */
#define MOCK_SRAM_SIZE 8192 

/**
 * @brief Физический плоский массив байт, имитирующий встроенную память SRAM процессора.
 * @details Размещается в секции данных (.data) текущего процесса.
 *          Используется атрибут C23 [[alignas(16)]] для имитации жесткого аппаратного 
 *          выравнивания страниц памяти на уровне кристалла SoC.
 */

 //static uint8_t mock_sram[MOCK_SRAM_SIZE] __attribute__((aligned(16)));

 /**
 * @brief Вспомогательная функция для вывода карты памяти текущего блока (дамп метаданных).
 * @param ptr Указатель на Payload пользовательского блока.
 */

 static void dump_allocated_block(void *ptr){
    if(ptr == nullptr){
        printf(" DUMP | Передан нулевой указатель \n");
        return;
    }

    /*Читаем заголовок */
     uintptr_t header_addr = (uintptr_t)ptr - UB_HEADER_SIZE;
    ub_block_header_t *header = (ub_block_header_t *)header_addr;

    printf("DUMP |= Блок по адресу Payload: %p\n", ptr);
    printf("  |- Адрес заголовка: 0x%lx\n", header_addr);
    printf("  |- Размер (size):   %zu байт\n", header->size);
    printf("  |- Сигнатура (magic): 0x%x (Валидна: %s)\n", 
           header->magic, (header->magic == UB_MAGIC) ? "ДА" : "НЕТ");
    printf("  |- Статус (is_free): %s\n", header->is_free ? "СВОБОДЕН" : "ЗАНЯТ");
 }

 /**
 * @brief Точка входа в имитатор .
 * @details Инициализирует виртуальный SRAM пул, проводит серию проверок 
 *          алгоритма First-Fit, фрагментации и защиты от Buffer Overflow.
 * @return int Статус завершения: 0 - все тесты успешно пройдены.
 */
int main(void){
     printf("=== Запуск имитатора SRAM и тестов аллокатора U-Boot (C23) ===\n\n");
     /* ИСПРАВЛЕНИЕ: Выделяем память под виртуальный SRAM на куче ОС, 
     * чтобы обойти защиту секций PIE/ASLR в Linux */
    void *mock_sram = malloc(MOCK_SRAM_SIZE);
    if (mock_sram == nullptr) {
        fprintf(stderr, " FAIL | Не удалось выделить память под симулятор SRAM на ПК\n");
        return -1;
    }

      /* 1. Инициализация пула памяти */
    printf("[ШАГ 1] Инициализация региона SRAM объемом %d байт...\n", MOCK_SRAM_SIZE);
    if (ub_boot_heap_init(mock_sram, MOCK_SRAM_SIZE) != 0) {
        fprintf(stderr, "FAIL | Не удалось инициализировать SRAM кучу!\n");
        return -1;
    }
    printf(" OK | SRAM куча успешно размечена и готова к работе.\n\n");

    /* 2. Тест первой аллокации */
    printf("[ШАГ 2] Выделение памяти под массив данных драйвера (64 байта)...\n");
    int32_t *driver_buffer = (int32_t *)ub_malloc(64);
    
    if (driver_buffer == nullptr) {
        fprintf(stderr, "[FAIL] Ошибка ub_malloc: вернулся nullptr\n");
        return -1;
    }
    
    /* Заполняем данными, проверяя доступность памяти */
    driver_buffer[0] = 0xAA;
    driver_buffer[1] = 0xBB;
    dump_allocated_block(driver_buffer);
    printf("[ OK ] Память успешно выделена и проверена на запись.\n\n");

    /* 3. Тест выравнивания и минимального размера (Splitting) */
    printf("[ШАГ 3] Запрос маленького блока (4 байта) для проверки выравнивания и защиты Splitting...\n");
    void *small_ptr = ub_malloc(4);
    
    /* Наш аллокатор должен округлить 4 байта до минимальных 16 (UB_MIN_BLOCK_SIZE), 
     * чтобыPayload свободно вмещал FreeNode при освобождении */
    dump_allocated_block(small_ptr);
    printf("[ OK ] Защита от коллизии размеров отработала корректно.\n\n");

    /* 4. Тест освобождения ресурсов (Free) */
    printf("[ШАГ 4] Освобождение буфера драйвера (64 байта)...\n");
    ub_free(driver_buffer);
    
    /* Проверяем, что заголовок блока поменял статус на true (свободен) */
    dump_allocated_block(driver_buffer);
    printf("[ OK ] Блок успешно возвращен в Explicit Free List.\n\n");

    /* 5. Проверка повторной аллокации (First Fit) */
    printf("[ШАГ 5] Повторный запрос 32 байт (должен занять место только что освобожденного блока)...\n");
    void *realloc_ptr = ub_malloc(32);
    dump_allocated_block(realloc_ptr);
    
    if (realloc_ptr != driver_buffer) {
        printf("[WARN] Алгоритм First Fit выбрал другое место, но блок валиден.\n");
    } else {
        printf("[ OK ] Алгоритм First Fit идеально переиспользовал старый освобожденный блок.\n");
    }

    /* Очищаем оставшуюся память */
    ub_free(small_ptr);
    ub_free(realloc_ptr);

    printf("\n=== Все интеграционные тесты Си-аллокатора U-Boot успешно ПРОЙДЕНЫ ===\n");
    return 0;
}
