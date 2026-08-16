/**
 * @file uboot_allocator.h
 * @brief Custom high-performance SRAM heap allocator for U-Boot bootloader.
 * @details Implements an explicit free-list allocator tailored for early boot 
 *          stages (SPL/Pre-Relocation) on ARM64 architectures (e.g., Layerscape).
 *          Designed and complied according to the ISO C23 standard specifications.
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

#ifndef UBOOT_ALLOCATOR_H
#define UBOOT_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Побитовое выравнивание размера памяти вверх до кратности 8 байт (машинное слово ARM64).
 * @details Маска ~7UL даёт число -8 на уровне бит, зануляя 3 младших разряда (Natural Alignment).
 *          Используется суффикс UL для явного указания типа unsigned long в арифметике адресов.
 */
 #define UB_ALIGN(size) (((size) + 7UL) & ~7UL)

 /**
  * @brief Магическач сигнатура защиты от переполнения буфера (Buffer Overflow) в SRAM куче.
  */
 #define UB_MAGIC 0x5542414CU /* "UBAL" - U-Boot Allocator */
 /* Объявление структуры для использования внутри самого заголовка */
 struct ub_block_header;
 /**
  * @struct ub_block_header_t
  * @brief Данные каждого непрерывного блока памяти в куче U-Boot.
  * @details Сортируем поля жёстко по убыванию их размера (Natural Alignment)
  *          Это исключит появление пустых байтов для выравнивания байтов Padding-a от компилятора.
  *          Используем стандартный атрибут C23 [[alignas(8)]] вместо расширений GNU.
  */
typedef struct ub_block_header {
    size_t size;                        /**< Размер полезной памяти (Payload) в байтах */
    struct ub_block_header *next_all;   /**< Указатель на физически следующий блок в адресном пространстве. */
    uint32_t magic;                     /**< Сигнатура целостности метаданных для обнаружения переполнения (Buffer Overflow)*/
    bool is_free;                       /**< Флаг состояния блоков (встроено в ядро стандарта C23). */
} __attribute__((aligned(8))) ub_block_header_t;

/**
 * @struct ub_free_node_t 
 * @brief Описываем узлы явного свободного списка свбодных блоков (Explicit Free List).
 * @details Мапим кастомный указатель поверх пустующей пользовательской памяти (Payload).
 *          Это позволяет экономить драгоцнную SRAM раннего загрузчика (Zero-memory overhead).
 */
 typedef struct {
    ub_block_header_t *next_free; /**< Указатель на заголовок следующего свободного блока в куче. */
    ub_block_header_t *prev_free; /**< Указатель на заголовок предыдущео свободного блока в куче. */
 } __attribute__((aligned(8))) ub_free_node_t;

 /**
  * @brief Константа размера служебного загловка, рассчитанная с учётом выравнивания.
  */
 #define UB_HEADER_SIZE UB_ALIGN(sizeof(ub_block_header_t))

 /**
  * @brief Минимальный полезный размер блока, способный физически вместить указатели списка FreeNode.
  */
 #define UB_MIN_BLOCK_SIZE UB_ALIGN(sizeof(ub_free_node_t))
 /**
  * @brief Инициализация пула памяти аллокатора в SRAM.
  * @details Принимает сырой физический регион памяти и размечает на нём стартовую структуру кучи.
  *          В стандарте С23 используется атрибут [[nodiscard]], запрещвющий игнорировать статус инициализации.
  * @param sram_start Физический стартовый адрес выделенного под кучу региона памяти SRAM.
  * @param sram_size Общий объём выделенного региона памяти в байтах.
  * @return int Статус операций: 0 - успешная разметка, -1 - некорркные параметры или маленький размер.
  */
  [[nodiscard]] int ub_boot_heap_init(void *sram_start, size_t sram_size);
  /**
   * @brief Выделение выровненного блока памяти из кастомной кучи U-Boot.
   * @details Ищет блок по стратегии First Fit. При необходимости производит разделение (Splitting).
   *          Атрибут С23 [[nodiscard]] гарантирует, что адрес не будет потерян, предотвращая утечки памяти.
   * @param size Запрашиваемый размер памяти в байтах.
   * @return void* Указатель на чистый, выровненный пользовательский буфер (Payload), или nullptr при нехватке памяти.
   */
  [[nodiscard]] void *ub_malloc(size_t size);
  /**
   * @brief Возврат памяти в пул свободных ресурсов аллокатора.
   * @details Выполняет валидацию канарейки 'magic'. Помечает блок как свободный и его в Explicit Free List.
   *          Используется спецификатор С23 'nullptr' вместо старого Си-макроса 'NULL'.
   * @param ptr Указатель на ранее выделенный полезный буфер (Payload), который необходимо освободить.
   */
  void ub_free(void *ptr);
#endif /* UBOOT_ALLOCATOR_H */
