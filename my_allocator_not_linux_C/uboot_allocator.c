/**
 * @file uboot_allocator.c
 * @brief Implementation of the low-level SRAM heap allocator for U-Boot.
 * @details Fully autonomous C23 implementation using strict bitwise arithmetic
 *          on `uintptr_t` integers. Contains First-Fit search, block splitting,
 *          and explicit free-list mechanics without invoking host libc libraries.
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

/**
 * @brief глобальный указатель на корень списка свободныз блоков в SRAM.
 * @details Так как в U-Boot нет операционной системы. состояние кучи хранится в виде
 *          глобальной переменной внктри секции данных (.data) самого загрузчика.
 */
static ub_block_header_t *g_ub_free_list_head = nullptr;

int ub_boot_heap_init(void *sram_start, size_t sram_size){
    // 1. Базовая проверка входящих аргументов 
    if (sram_start == nullptr || sram_size <= UB_HEADER_SIZE){
        return -1;
    }

    // 2. Вырвыниваем стратовый адресс вверх, если он не был выровнен аппаратно 
    uintptr_t raw_start = (uintptr_t)sram_start;
    uintptr_t aligned_start = (raw_start + 7UL) & ~7UL;
    // Корректируем доступный размер с учётом сдвига выравнивания
    size_t alignment_loss = aligned_start - raw_start;
    if (sram_size <= (UB_HEADER_SIZE + alignment_loss)){
        return -1;
    }
    size_t available_size = sram_size - alignment_loss;
    //--- Железное исправление: обнуляем сырую память под заголовок перед разметкой ---
    uint8_t *header_bytes = (uint8_t *)aligned_start;
    for (size_t i = 0; i < UB_HEADER_SIZE; ++i){
        header_bytes[i] = 0;
    }
    // 3. Размечаем первый монолитный мастер-блок в начале SRAM
    ub_block_header_t *master_block = (ub_block_header_t *)aligned_start;
    master_block->magic = UB_MAGIC;
    master_block->size = available_size - UB_HEADER_SIZE;
    master_block->is_free = true;
    master_block->next_all = nullptr;
    // 4. Инициализация FreeNode
    uintptr_t payload_addr = aligned_start + UB_HEADER_SIZE;
    ub_free_node_t *free_node = (ub_free_node_t *)payload_addr;
    free_node->next_free = nullptr;
    free_node->prev_free = nullptr; 
    // Инициализируем список свободных ресурсов
    g_ub_free_list_head = master_block;

    return 0;
}

/**
 * @brief Внутренняя функция вставки блока в начало списка свободных элементов.
 * @param block Указатель на заголовок освобождаемого блока.
 */
static void add_to_free_list(ub_block_header_t *block){
    if (block == nullptr) return;

    uintptr_t payload_addr = (uintptr_t)block + UB_HEADER_SIZE;
    ub_free_node_t *block_free = (ub_free_node_t *)payload_addr;

    block_free->next_free = g_ub_free_list_head;
    block_free->prev_free = nullptr;

    if (g_ub_free_list_head != nullptr){
        uintptr_t next_payload = (uintptr_t)g_ub_free_list_head + UB_HEADER_SIZE;
        ub_free_node_t *head_free = (ub_free_node_t *)next_payload;
        head_free->prev_free = block;
    }
    g_ub_free_list_head = block;
}

/**
 * @brief Внутренняя функция исключения блока из списка свободных элементов.
 * @param block Указатель на заголовок занимаемого блока.
 */

 static void remove_from_free_list(ub_block_header_t *block){
    if(block == nullptr) return;

    uintptr_t payload_addr = (uintptr_t)block + UB_HEADER_SIZE;
    ub_free_node_t *block_free = (ub_free_node_t *)payload_addr;

    if (block == g_ub_free_list_head){
        g_ub_free_list_head = block_free->next_free;
    }

    if (block_free->next_free != nullptr){
        uintptr_t next_payload = (uintptr_t)block_free->next_free + UB_HEADER_SIZE;
        ub_free_node_t *next_free_info = (ub_free_node_t *)next_payload;
        next_free_info->prev_free = block_free->prev_free;
    }

    if (block_free->prev_free != nullptr){
        uintptr_t prev_payload = (uintptr_t)block_free->next_free + UB_HEADER_SIZE;
        ub_free_node_t *prev_free_info = (ub_free_node_t *)prev_payload;
        prev_free_info->next_free = block_free->next_free;
    } 
 }

 void *ub_malloc(size_t size){
    if(size == 0) return nullptr;

    //1. Выравнивание запрашиваемый размер до шага машинного слова ARM64 (8 байт )
    size_t aligned_size = UB_ALIGN(size);
    //2. Системная защита от коллизии памяти: Payload не должен быть меньше 16 байт
    if (aligned_size < UB_MIN_BLOCK_SIZE){
        aligned_size = UB_MIN_BLOCK_SIZE;
    }

    ub_block_header_t *curr = g_ub_free_list_head;
    ub_block_header_t *found_block = nullptr;

    //3.Линейный поиск подходящего пустого региона (First Fit)
    while (curr != nullptr){
        if (curr->is_free && curr->size >= aligned_size){
            found_block = curr;
            break;
        }
        uintptr_t payload_addr = (uintptr_t)curr + UB_HEADER_SIZE;
        ub_free_node_t *free_info = (ub_free_node_t *)payload_addr;
        curr = free_info->next_free;
    }

    //Ошибка: В SRAM не осталось непрерывного региона нужного объёма
    if (found_block == nullptr){
        return nullptr;
    }
    //Исключаем найденный кусок из глобального пула свободных ресурсов
    remove_from_free_list(found_block);
    // 4. Алгоритм разделения (Splitting): отрезаем лишний кусок, если места с запасом
    if (found_block->size >= aligned_size + UB_HEADER_SIZE + UB_MIN_BLOCK_SIZE){
        uintptr_t base_addr = (uintptr_t)found_block;
        uintptr_t next_addr = base_addr + UB_HEADER_SIZE + aligned_size;

        ub_block_header_t *next_block = (ub_block_header_t *)next_addr;

        next_block->magic = UB_MAGIC;
        next_block->size = found_block->size - aligned_size - UB_HEADER_SIZE;
        next_block->is_free = true;
        next_block->next_all = found_block->next_all;

        found_block->size = aligned_size;
        found_block->next_all = next_block;

        // Возвращаем отрезанный остаток обратно в список свободных блоков
        add_to_free_list(next_block);
    }

    found_block->is_free = false;
    //Возвращаем чистый физический адрес Payload, находящийся строго за заголовком
    return (void *)((uintptr_t)found_block + UB_HEADER_SIZE);
 }

 void ub_free(void *ptr){
    if (ptr == nullptr) return;

    //1. Арифметика uintptr_t: делаем шаг назад, что бы считать метаданные заголовка
    uintptr_t header_addr = (uintptr_t)ptr - UB_HEADER_SIZE;
    ub_block_header_t *header = (ub_block_header_t *)header_addr;

    //2. Валидация канарейки на Buffer Overflow
    // Поскольку в SPL U-Boot нет функции abort(), при разрушении magic мы уходим в вечный цикл (hang)
    if(header->magic != UB_MAGIC){
        while (true){
            // Аппаратная петля фиксации краша памяти для отладки JTAG/J-Link
        }
    }

    if (header->is_free){
        return; //Защита от Double Free
    }

    // 3. Маркируем блоки как свободный и возвращаем в пул Explicit free list
    header->is_free = true;
    add_to_free_list(header);
 }
