# Known Issuses & Roadmap:
• Реализация Free List Coalescing (слияния блоков) и Lock-Free очередей на CAS-инструкциях в текущей версии учебного PoC оптимизирована под минималистичный embedded-интерфейс [1.1].
• В промышленной эксплуатации логика указателей prev/next и барьеры памяти (Memory Barriers) требуют адаптации под конкретную многопроцессорную когерентность кэшей (MESI)
## License
This project is licensed under the **MIT License** — feel free to use it in your own embedded or operating system kernels.
