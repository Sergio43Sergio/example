/**
 * @file main.cpp
 * @brief Демонстрационный файл для тестирования кастомного аллокатора C++20.
 * @details Демонстрирует бесшовную интеграцию кастомного аллокатора HeapAllocator 
 *          со стандартным контейнером std::vector из библиотеки STL.
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


#include "modern_allocator.hpp"
#include <vector>
#include <iostream>

/**
 * @brief Точка входа в программу
 * @details Инициализирует STL-контейнер с кастомным аллокатором, выполняет
 *          наполнение данными для вызовов системных вызовов mmap() и 
 *          автоматически освобождает ресурсы при выходе из области видимости.
 * @return int Статус завершения программы (0 - успешное выполнение).
 */

 int main(){
    std::cout << "--- Запуск теста кастомного C++20 аллокатора ---" << std::endl;

    /**
     * @brief Стандартный вектор STL, смапленный на нашу кастомную кучу.
     * @details Вторым параметром шаблона передаётся наш HeapAllocator. Вектор будет 
     *          вызывать наш метод allocate() вместо стандартного malloc().
     */
    std::vector<int, os::memory::HeapAllocator<int>> my_vector;

    /**
     * @brief Наполнение вектора данными.
     * @details Цикл провоцирует внутренний рост вектора (rellocation). 
     *          Наш аллокатор будет автоматически запрашивать новые страницы памяти  
     *          у ядра Linux через mmap() и производить их выравнивание и расщепление (Splitting).
     */
    for (int i = 1; i <= 10; ++i){
        my_vector.push_back(i * 10);
    }

    std::cout << "Элементы вектора: ";

    /**
     * @brief Вывод элементов на экране с использованием range-based for (C++11/17).
     * 
     */

     for (int val : my_vector){
        std::cout << val << " ";
     }
     /**
      * @note В этой точке my_vector выходит из области идимости. Автоматически 
      *       вызывается деструктор вектора, который внутри себя вызовет метод
      *       deallocate() нашего аллокатора, возращая всю память в список свободных блоков.
      */
     return 0;
 }