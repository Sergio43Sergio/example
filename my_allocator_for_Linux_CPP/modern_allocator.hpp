/**
 * @file modern_allocator.hpp
 * @brief Модуль кастомного системного аллокатора памяти на стандарте C++20.
 * @details Реализует структуру Explicit Free List поверх страниц виртуальной памяти Linux (mmap).
 *          Полностью совместим со стандартными контейнерами STL (std::vector, std::list и др.).
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


#ifndef MODERN_ALLOCATE_HPP
#define MODERN_ALLOCATE_HPP

#include <cstddef>
#include <cstdint>
#include <new>
#include <concepts>
#include <string_view>
#include <mutex>
#include <sys/mman.h>
#include <unistd.h>

/**
 * @namespace os::memory
 * @brief Подсистема управления низкоуровневой динамической памятью.
 */
 namespace os::memory {
    /**
    * @brief Побитовое выравнивание размера памяти вверх до кратности выравнивания типа Т
    * @details Вычисляется полностью на этапе компиляции (Compile-time). Гарантирует минимальное выравнивание в 8 байт.
    * @tparam T Тип данных, по которому определяется требуемое выравниевание (alignof).
    * @param size Исходный размер в байтах, который необходимо выровнять.
    * @param size_t Выровненный размер, кратный требованиям процессора к адресации.
    */

    template<typename T>
    [[nodiscard]] constexpr size_t align_up(size_t size) noexcept {
        constexpr size_t alignment = alignof(T) > 8 ? alignof(T) : 8;
        return (size + (alignment - 1)) & ~(alignment - 1);
    }
    /**
    * @brief Магическая сигнатура (канарейка) для валидации целостности заголовка блоков кучи 
     */
     constexpr uint32_t BLOCK_MAGIC = 0x414C4C4F; // "ALLO"

     /**
     * @brief Базовый размер страницы виртуальной памяти в Linux (4 КБ)
     */
     constexpr size_t PAGE_SIZE = 4096;

     /**
     * @struct BlockHeader
     * @brief Методанные каждого непрерывного блока памяти в куче
     * @details Находится в памяти непосредственно перед пользовательским буфером данных (Payload)
     */
     struct BlockHeader {
        size_t size{0}; /**< 8 байт | Размер полезной (пользовательской) памяти в блоке в байтах. */
        BlockHeader* next_all{nullptr}; /**< 8 байт | Указатель на следующий физически расположенный в памяти блок. */       
        uint32_t magic{BLOCK_MAGIC}; /**< 4 байта | Сигнатура для переполнения буфера (Buffer Overflow). */
        bool is_free{true}; /**< 1 байт | Флаг состояния блока: true - свободен , false - занят */
     };
     /**
     * @struct FreeNode
     * @brief Узлы явного двухсвязного списка свободных блоков (Explicit Free List)
     * @details Накладывается (reinterpret_cast) поверх пользовательской памяти блока, когда он свободен
     *          Не занимает дополнительного места в памяти (Zero - memory overherad)
     */
     struct FreeNode {
        BlockHeader* next_free{nullptr}; /**< Указатель на следующую свободный блок в куче.  */
        BlockHeader* prev_free{nullptr}; /**< Указатель на предыдущий свободный блок в куче. */
     };
     /**
      * @brief Размер заголовка методанных, выровненный по границам процессора 
      */
      constexpr size_t HEADER_SIZE = align_up<BlockHeader>(sizeof(BlockHeader));
      /**
      * @brief Минимальный полезный размер блока, способный вместить указатели двусвязанного списка свободных элементов 
      */
      constexpr size_t MIN_BLOCK_SIZE = align_up<FreeNode>(sizeof(FreeNode));
      /**
       * @struct HeapState
       * @brief Инкапсуляция глобального состояния подсистемы динамической памяти 
       */
       struct HeapState {
          BlockHeader* free_list_head{nullptr}; /**< Голова (первый элемент) явного списка свободных блоков  */
          std::mutex mutex;                     /**< Мьютекс для обеспесения потокобезопасности при работе с STL */
        };
       /**
        * @brief Получение глобального синглота состояния кучи.
        * @return HeapState& ссылка на единственный экземпляр состояния памяти 
        */ 
        [[nodiscard]] inline HeapState& get_heap_state() noexcept {
            static HeapState state;
            return state;
        }

        /**
         * @brief Внутреняя низкоуровневая функция выделения сырой выровненной памяти.
         * @param size Запрашиваемый размер памяти в байтах
         * @return void* Указатель на выделенный пользовательский буфер, который необходимо освободить.
         */
         [[nodiscard]] void* internal_malloc(size_t size);
        
        /**
         * @brief Внутреняя низкоуровневая функция освобождения блоков памяти 
         * @param ptr Указатель на пользовательский буфер, который необходимо освободить.
         */
         void internal_free(void *ptr) noexcept;

        /**
        * @brief Вставка свободного блока в начало глобального двухсвязанного списка свободных элементов
        * @param block Указатель на заголовок свободного блока 
        */
        void add_to_free_list(BlockHeader* block) noexcept;
        /**
         * @brief Удаление блока из списка свободных элементов при его выделении пользователю.
         * @param block Указатель на заголовок занимаемого блока.
         */
         void remove_from_free_list(BlockHeader* block) noexcept;
        /**
         * @brief системный запрос новых страниц виртуальной памяти у ядра ОС Linux.
         * @details Использует вызов ::mmap с флагом MAP_ANONYMOUS
         * @param size Минимально необходимый размер полезной памяти в байтах 
         * @return BlockHeader* Указатель на заголовок нового блока памяти, создание на страницах ядра
         */
         [[nodiscard]] BlockHeader* request_space_from_kernel(size_t size) noexcept;
 
         /**
          * @class HeapAllocator
          * @brief Шаблонный класс алакотора, полностью удаветворяющий требованиям концепта Allocator
          * @details Позволяет использовать кастомную кучу mmap в любых контейнерах STL (std::vector, std::list и т.д. )
          * @tparam T Тип элементов, для которых выделяют память 
          */
          template <typename T>
          class HeapAllocator {
            public:
               using value_type = T; /**< Обязательный тип данных для совместимости с шаблоном STL. */
               /**
                * @brief Конструктор по умолчанию. Вычисляется на этапе компиляции (constexpr) Без Overhead 
                */
                constexpr HeapAllocator() noexcept = default;
               /**
                * @brief шаблонный конструктор копирования для преобразования типов аллокатора.
                * @tparam U Тип элементов другого аллокатора
                */
                template <typename U>
                constexpr HeapAllocator(const HeapAllocator<U>&) noexcept {}

                /**
                 * @brief Выделяет память под n объектов типа T.
                 * @details Вычисляет суммарный объём байт и обращается к internal_mallock. При нехватке выбрасывается исключение
                 * @param n Количество жлементов для аллокатора
                 * @return T* Выровненный указатель на массив объектов
                 * @throws std::bad_alloc Если операционная система отказала в выделении памяти 
                 */

                 [[nodiscard]] T* allocate(size_t n){
                    if (n == 0) return nullptr;

                    size_t total_bytes = n * sizeof(T);
                    void* raw_ptr = internal_malloc(total_bytes);

                    if (!raw_ptr) {
                        throw std::bad_alloc();
                    }

                    return static_cast<T*>(raw_ptr);
                 }

                 /**
                  * @brief Освобождает ранее выделенную память 
                  * @param p Указатель на первый элемент освобождённого массива
                  * @param n Количество элементов (не используется в кастомной логике free)
                  */
                  void deallocate(T* p, size_t /*n*/) noexcept {
                    internal_free(p);
                  }
                  /**
                   * @brief Оператор сравнения аллокаторов
                   * @details Возвращает true, так как память, выделенная одним экземпляром этого аллокатора,
                   *          может быть безопасно освобождена другим экземплятором
                   * @return true Всегда истинно для бестейтовых (stateless) глобальных аллокаторов.
                   */
                   bool operator==( const HeapAllocator&) const noexcept { return true;} 

            };
 }          // namespace os::memory
 //Вклюяаем файл реализации шаблонов и внутренних функций
 #include "modern_allocator_impl.hpp"

#endif // OS_MEMORY_MODERN_ALLOCATOR_HPP
