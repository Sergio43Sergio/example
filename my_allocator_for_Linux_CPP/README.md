# Modern C++20 Low-Level Heap Allocator

A custom, high-performance, thread-safe memory allocator implemented in **Modern C++20** that bypasses standard `malloc` and manages virtual memory pages directly via the Linux `mmap` system call. Fully compliant with the ISO C++ `Allocator` concept, making it seamlessly compatible with any STL containers (`std::vector`, `std::list`, `std::map`, etc.).

## Key Architectural Features

- **Direct Kernel Interaction:** Allocates raw memory pages directly from the Linux kernel using `::mmap()` with `MAP_ANONYMOUS` flags.
- **Explicit Free List:** Manages unallocated memory blocks using a doubly-linked list (`FreeNode`) embedded directly inside the unused payload area (**Zero-memory overhead**).
- **First-Fit & Splitting Strategy:** Fast linear search for the first available block that satisfies the request. If the chosen block is significantly larger than requested, the allocator slices it, creating a new free block to prevent **internal memory fragmentation**.
- **Hardware-Level Alignment:** Compile-time `constexpr` bitwise alignment masking `& ~(alignment - 1)` (equivalent to `-8`) ensures **Natural Alignment** on 64-bit architectures (ARM64/x86_64), preventing hardware `Alignment Fault` exceptions.
- **Exploit & Overrun Protection:** Every block is guarded by a structured `BlockHeader` metadata with a 32-bit `BLOCK_MAGIC` canary signature. Any buffer overflow or double-free instantly triggers a secure `std::abort()`.
- **Thread-Safety & STL Compatibility:** Protected by a centralized `std::mutex` using RAII `std::lock_guard` patterns, allowing safe usage across multi-threaded STL workloads.
- **Zero-Cost Abstractions:** Leverage `std::byte`, C++20 `concepts`, `constexpr` evaluation, and `= default` trivial type optimization to compile into raw, optimized assembly with zero runtime overhead.

## Project Structure

- `modern_allocator.hpp` — Public interface, Doxygen Javadoc documentation, C++20 Allocator template wrapper.
- `modern_allocator_impl.hpp` — Low-level implementation, bitwise pointer arithmetic, mmap subsystem.
- `main.cpp` — Integration test suite deploying `std::vector` over the custom heap.
- `CMakeLists.txt` — Modern CMake script supporting multi-configuration builds (`Debug` with GDB symbols / `Release` with `-O3` optimizations) and integrated CTest automation.

## Build & Run Instructions

The project requires a compiler that supports C++20 (GCC 11+ or Clang 13+) and CMake 3.14+.

```bash
# 1. Configure the project (Defaults to 'Debug' with GDB symbols)
cmake -B build

# 2. Build the binaries
cmake --build build

# 3. Run the automated integration test suite via CTest
ctest --test-dir build --output-on-failure

# 4. Run the demo binary manually
./build/allocator_demo
```
### Expected Test & Demo Output

When you run **CTest**, you should see the following automated integration report:

```text
Internal ctest changing into directory: /path/to/project/build
Test project /path/to/project/build
    Start 1: RunAllocatorIntegrationTest
1/1 Test #1: RunAllocatorIntegrationTest ......   Passed    0.00 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.00 sec
```

When running the **demo binary** manually, it invokes our custom allocator to feed the `std::vector`, performs page allocation, structural block splitting, and secure RAII deallocation at the scope exit:

```text
\$ ./build/allocator_demo
--- Запуск теста кастомного C++20 аллокатора ---
Элементы вектора: 10 20 30 40 50 60 70 80 90 100 

Выход из main(). Вектор уничтожается, вызывая deallocate()...
```
## License
This project is licensed under the **MIT License** — feel free to use it in your own embedded or operating system kernels.
