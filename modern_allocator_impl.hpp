/**
 * @file modern_allocator_impl.hpp
 * @brief Файл реализации внутренних алгоритмов управления памятью кастомного аллокатора C++20.
 * @details Содержит низкоуровневые механизмы выделения, освобождения, расщепления и работы со списками.
 * 
 * @copyright Copyright (c) 2026 Sergey Z
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

#include <iostram>

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
        
        
     }

}

#endif //OS_MEMORY_MODERN_ALLOCATOR_IMPL_HPP

