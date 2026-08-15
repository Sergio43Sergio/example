/**
 * @file modern_allocator_impl.hpp
 * @brief Файл реализации внутренних алгоритмов управления памятью кастомного аллокатора C++20.
 * @details Содержит низкоуровневые механизмы выделения, освобождения, расщепления и работы со списками.
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

#ifndef OS_MEMORY_MODERN_ALLOCATOR_IMPL_HPP
#define OS_MEMORY_MODERN_ALLOCATOR_IMPL_HPP

// говорим плагину в VSCode? что этот файл является частью modern_allocator.hpp
// @verbatim
// clang-format off
#ifndef __INTELLISENSE__
#include "modern_allocator.hpp"
#endif
// clang-format on 
// @endverbation
#include "modern_allocator.hpp" 

#include <iostream>

namespace os::memory {

    /**
     * @brief Запрос страниц виртуальной памяти у ядра Linux через системный вызов mmap().
     * @param size Минимальный необходимый размер полезной памяти без учёта заголовка.
     * @return BlockHeader* Указатель на сформированный заголовок на выделенных страницах ядра, или nullptr при ошибке. 
     */

    inline BlockHeader* request_space_from_kernel(size_t size) noexcept {
        // 1. Считаем суммарный размер: полезные данные пользователя + наш служебный заголовок 
        size_t total_needed = size + HEADER_SIZE;
        //2. Расчитываем, сколько полных аппаратных страниц по 4096 байт нужно затребовать
        size_t pages_count = (total_needed + PAGE_SIZE - 1 ) / PAGE_SIZE;
        size_t mmap_size = pages_count * PAGE_SIZE;
        // 3. Выполняем реальный системный вызов у ядру Linux для выделения анонимной памяти в RAM
        void* ptr = ::mmap(nullptr, mmap_size, PORT_READ | PORT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        // 4. Если ядро ОС отказало в памяти, тогда возыращвем nullptr
        if (ptr == MAP_FAILED){
            return nullptr;
        }
        // 5. Инициализируем структуру трафарет метаданных прямо на полученных сырых байтах
        auto* header = static_cast<BlockHeader*>(ptr);
        header->magic = BLOCK_MAGIC;
        header->size = mmap_size - HEADER_SIZE;
        header->is_free = true;
        header->next_all = nullptr;

        return header;
    
    }   

    /**
     * @brief Вставка свободного блока в начало явного двухсвязного списка свободных элементов
     * @param block Указатель на заголовок освобождаемого блока
     */
     inline void add_to_free_list(BlockHeader* block) noexcept {
        if (!block) return;

        auto& state = get_heap_state();

        // 1. Побайтово сдвигаем вперед на размер заголовка, что бы попасть в пустующую пользовательскую область (payload)
        auto* payload_ptr = reinterpret_cast<std::byte*>(block) + HEADER_SIZE;
        // 2. Временно накладываем структуру узла списка FreeNode поверх этой пустой памяти (Zero-memory overhead)
        auto* block_free = reinterpret_cast<FreeNode*>(payload_ptr);
        // 3. Связываем указатели: текущий блок становится новой головой списка
        block_free->next_free = state.free_list_head;
        block_free->prev_free = nullptr;
        // 4.Если в списке уже были элементы, корректируем обратную ссылку у старой головы
        if (state.free_list_head){
            auto* next_payload = reinterpret_cast<std::byte*>(state.free_list_head) + HEADER_SIZE;
            auto* head_free = reinterpret_cast<FreeNode*>(next_payload);
            head_free->prev_free = block;
        }
        state.free_list_head = block;
     }

     /**
      * @brief Удаление блока из двухсвязанного списка свободных элементов при его выделении.
      * @param block Указатель на заголовок занимаемого блока.
      */
     inline void remove_from_free_list(BlockHeader* block) noexcept {
        if (!block) return;

        auto& state = get_heap_state();
        auto* payload_ptr = reinterpret_cast<std::byte*>(block) + HEADER_SIZE;
        auto* block_free = reinterpret_cast<FreeNode*>(payload_ptr);

        //1. Если удаляемый блок является текущей головой списка
        if (block == state.free_list_head){
            state.free_list_head = block_free->next_free;
        }
        // 2. Перевешиваем указатели у следующего элемента в списке (если он есть )
        if (block_free->next_free){
            auto* next_payload = reinterpret_cast<std::byte*>(block_free->next_free) + HEADER_SIZE;
            auto* next_free_info = reinterpret_cast<FreeNode*>(next_payload);
            next_free_info->prev_free = block_free->prev_free;
        }
        //3. Перевешиваем указатели у предыдущего элемента в списке (если он есть)
        if (block_free->prev_free){
            auto* prev_payload = reinterpret_cast<std::byte*>(block_free->prev_free) + HEADER_SIZE;
            auto* prev_free_info = reinterpret_cast<FreeNode*>(prev_payload);
            prev_free_info->next_free = block_free->next_free;
        }

     }

     /**
      * @brief Внутреняя потокобезопасная функция выделения сырой выровненной памяти.
      * @param size Запрашиваемый размер памяти в байтах.
      * @return void* Указатель на выделенный пользовательский буфер (Payload), или nullptr при ошибке.
      */
     inline void* internal_malloc(size_t size){
        auto& state = get_heap_state();
        // 1. Защита критической секции от гонок данных с помощью RAII - блокировки мьтекса (Thread-Safety)
        std::lock_guard<std::mutex> lock(state.mutex);
        // 2. Аппаратно выравниваем размер вверх по сетке кратных степеней двойки (-8) через маску ~7
        size_t aligned_size = align_up<std::byte>(size);
        BlockHeader* curr = state.free_list_head;
        BlockHeader* found_block = nullptr;
        // 3. Линейный поиск первого подходящего по размеру свободного блока (First Fit)
        while (curr != nullptr){
            if (curr->is_free && curr->size >= aligned_size){
                found_block =curr;
                break;
            }
            auto* payload_ptr = reinterpret_cast<std::byte*>(curr) + HEADER_SIZE;
            auto* free_info = reinterpret_cast<FreeNode*>(payload_ptr);
            curr = free_info->next_free;
        }
        //4. Если в куче нет подходящего блока - запрашиваем новую страницу у ядра Linux
        if(!found_block){
            found_block = request_space_from_kernel(aligned_size);
            if (!found_block) return nullptr;
        } else {
            //Если блок взять из существующих резервов, удаляем его из списка свободных элементов
            remove_from_free_list(found_block);
        }
        // 5. Алгоритм расщепления блока (Splitting). Если кусок слишклм большой, отрезаем лишнее
        if (found_block->size >= aligned_size + HEADER_SIZE + MIN_BLOCK_SIZE){
            auto* raw_base = reinterpret_cast<std::byte*>(found_block);
            //Вычисляем адрес начала нового отрезанного заголовка
            auto* next_block = reinterpret_cast<BlockHeader*>(raw_base + HEADER_SIZE + aligned_size);
            //Инициализируем новый отрезанный блок как свободный
            next_block->magic = BLOCK_MAGIC;
            next_block->size = found_block-> size - aligned_size - HEADER_SIZE;
            next_block->is_free = true;
            next_block->next_all = found_block->next_all;

            //Корректируем размеры исходного выделяемого блока
            found_block->size = aligned_size;
            found_block->next_all = next_block;
            //Возвращаем отрезанную линиюю памяти обратно в спискок свободных элементов 
            add_to_free_list(next_block);
        }
        //6. Марккируем блок как занятый и возвращаем адрес, сдвинутый ровно за служебный заголовок (Payload)
        found_block->is_free = false;
        return reinterpret_cast<void*>(reinterpret_cast<std::byte*>(found_block) + HEADER_SIZE);

     }

     /**
      * @brief Внутреняя потокобезопасная функция освобождения блока памяти.
      * @param ptr Указатель на пользовательский буфер (Payload), который необходимо освободить.
      */
     inline void internal_free(void* ptr) noexcept {
        if (!ptr) return;

        auto& state = get_heap_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        //1. Делаем шаг назад на размер HEADER_SIZE, чтобы получить доступ к загаловку
        auto* header = reinterpret_cast<BlockHeader*>(static_cast<std::byte*>(ptr) - HEADER_SIZE);
        // 2. Аппаратная валидация канарейки. Если magic затерт - произошло переполнение буфера (Buffer Overflow)
        if (header->magic != BLOCK_MAGIC){
            std::cerr << "[CRITICAL ERROR] Heap corruption detected (invalid magic)! \n";
            std::abort(); // Аварийно роняем процесс, предотвращая эксплуотацию уязвимости памяти       
        }
    //Защита от повторного освобождения (Double Free)
    if(header->is_free) return;

    //  Маркируруем  блок как свободный и возвращаем его структуру в пул свободных ресурсов
    header->is_free = true;
    add_to_free_list(header);
    }

} // namespace os::memory

#endif //OS_MEMORY_MODERN_ALLOCATOR_IMPL_HPP

