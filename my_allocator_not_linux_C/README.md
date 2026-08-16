# Autonomous C23 SRAM Heap Allocator for U-Boot

A minimalist, high-performance, bare-metal memory allocator implemented in **Strict ISO C23** tailored specifically for early boot stages (such as SPL or Pre-Relocation phase) in embedded bootloaders like **U-Boot**. 

Designed to manage tight internal static RAM (SRAM) regions before main DRAM initialization on 64-bit architectures (e.g., NXP Layerscape ARM64).

## Design Constraints & Implementation Details

- **Zero External Dependencies:** Built completely independent of the standard host `libc` (`malloc`, `free`, `memset` are not invoked), complying with strict SPL boundaries.
- **Explicit Free List Topology:** Free blocks track metadata utilizing a doubly-linked list (`ub_free_node_t`) mapped *directly inside the unallocated payload area*, achieving **Zero-memory overhead**.
- **Bitwise Address Alignment:** Natural alignment on 64-bit bounds is strictly enforced via bitmasking (`(raw_start + 7UL) & ~7UL`). Structures leverage C23 `_Alignas(8)` rules to prevent hardware `Alignment Fault` exceptions.
- **Collision & Overrun Defense:** Enforces a rigid minimal block constraint (`UB_MIN_BLOCK_SIZE = 16 bytes`) to prevent overlapping list updates from clobbering adjacent block headers. Integrated `UB_MAGIC` canaries provide robust buffer overflow mitigation.
- **Hardware Lockup Catch:** Upon detecting header corruption, the allocator intentionally triggers an infinite halt loop (`while(true)`), freezing execution state to allow reliable post-mortem JTAG/J-Link register inspection.

## Repository Structure

- `uboot_allocator.h` — Clean public C23 interface, Javadoc Doxygen specifications.
- `uboot_allocator.c` — Main allocation logic, pointer metrics, pure integer arithmetic via `uintptr_t`.
- `main.c` — Host test bench providing runtime SRAM array simulation and full feature verification.
- `CMakeLists.txt` — Build architecture compiling under pure C23 profiles with severe warning criteria (`-Wall -Wextra -Werror`).

## Compilation & Test Run

Ensure your compiler supports the ISO C23 standard (GCC 13+ or Clang 15+).

```bash
# 1. Generate build tree (Defaults to Debug with GDB symbols)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 2. Compile targets
cmake --build build

# 3. Execute regression tests via CTest automation
ctest --test-dir build --output-on-failure

# 4. Fire up the manual demonstration binary
./build/uboot_allocator_demo
```

## License
Licensed under the permissive **MIT License** — custom tailored for standalone integration into system bootloaders or embedded OS kernels.
