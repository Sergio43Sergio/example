/**
 * @file main.cpp
 * @brief Ситсемный демон для мониторинга сетевых подключений в изолированном chroot
 * @author Sergei Zobach 
 * @date 08/2026
 */

 #include <iostream>
 #include <unistd.h>
 #include <sys/stat.h>
 #include <syslog.h>
 #include <fcntl.h>
 #include <cstdlib>
 #include <atomic>
 #include <thread>
 #include <chrono>
 #include <csignal>
 #include "NetworkParser.hpp"

 /**
  * @class NetworkDaemon
  * @brief Класс отвечающий за инициализацию и жизненный цикл фонового процесса.
  * 
  */
 class NetworkDaemon {
    private:
        //Атомарный флаг для безопасного управления циклом демона из обработчика сигналов
        static inline std::atomic<bool> m_isRunning{true};

    /**
     * @brief Асинхронно-безопасный обработчик системных сигналов Linux (SIGTERM, SIGINT).
     * @note Внутри вызываются только lock-free атомарные операции. Никаких syslog/printf!
     * @param signal Номер полученного сигнала.
     */
        static void signalHandler(int signal){
            if (signal ==SIGTERM || signal == SIGINT ){
                // syslog(LOG_INFO, "Termination signal received. Stopping deamon gracefully...");
                m_isRunning.store(false, std::memory_order_relaxed);
            }
        }

        /**
         * @brief настраиваем перехват сигналов через современную структуру sigaction 
         */
        void setupSignals(){
            struct sigaction sa{}; 
            sa.sa_handler = NetworkDaemon::signalHandler;
            ::sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            //Регистрируем обработчик для корректного завершения
            ::sigaction(SIGTERM, &sa, nullptr);
            ::sigaction(SIGINT, &sa, nullptr);
        }

        /**
         * @brief Выполняем двойной forck 
         */
        void daemonize(){
            pid_t pid = ::fork();
            if (pid < 0){
                std::cerr << "First fork failed. " << std::endl;
                std::exit(EXIT_FAILURE);
            }
            if (pid > 0) {
                std::exit(EXIT_SUCCESS); // завершение родительского процесса
            }
            //Создаём новую сессию, отвязываемся от управляющего терминала
            if (::setsid() < 0){
                std::exit(EXIT_FAILURE);
            }
            //Настраиваем безопасный POSIX сигналы сразу после создания сессии
            setupSignals();

            pid=::fork();
            if(pid < 0){
                std::exit(EXIT_FAILURE);
            }
            if(pid > 0){
                std::exit(EXIT_SUCCESS);
            }

            // Сбрасываем маску прав создания файлов
            ::umask(0);

            // Меняем рабочую директорию на корневую
            if(::chdir("/") < 0){
                std::exit(EXIT_FAILURE);
            }

            //Закрываем стандартные дескрипторы ввода-вывода (0, 1, 2)
            ::close(STDIN_FILENO);
            ::close(STDOUT_FILENO);
            ::close(STDERR_FILENO);

            // Перенаправляем потоки в /dev/null для предатвращения ошибок ввода-вывода
            [[maybe_unused]] int fd_in = ::open("/dev/null", O_RDONLY); // stdin -> fd 0
            [[maybe_unused]] int fd_out = ::open("/dev/null", O_RDWR); // stdout -> fd 1
            [[maybe_unused]] int fd_err = ::open("/dev/null", O_RDWR); // stderr -> fd 2

        }

    public:
        /**
         * @brief Конструктор. Инициализирует подключение к системному журналу syslog.
         */
        NetworkDaemon(){
            ::openlog("net_monitor", LOG_PID, LOG_DAEMON);
        }
        /**
         * @brief Деструктор. Корректно закрывает syslog при уничтожении объекта.
         */
        ~NetworkDaemon(){
            syslog(LOG_INFO, "Daemon stopped. Cleaning up resources");
            ::closelog();
        }
        /**
         * @brief метод запуска фонового процесса 
         */
        void run(){
            syslog(LOG_INFO, "Initializing daemonization process...");
            daemonize();
            syslog(LOG_INFO, "Network Daemon successfully detached and running in background.");

            using namespace std::chrono_literals;
            NetworkParser parser;
            
            //основной рабочий цикл фоновой программы 
            while (m_isRunning.load(std::memory_order_relaxed)){
                try{
                    auto connections = parser.parseTcpConnections();
                    syslog(LOG_INFO, "--- Active Network Connections --- ");
                    for(const auto& conn : connections){
                        //логируем каждое подключение
                        syslog(LOG_INFO, "Proto: TCP | Local: %s:%d | Remote: %s:%d | State: %s",
                               conn.localIP.c_str(), conn.localPort,
                               conn.remoteIp.c_str(), conn.remotePort,
                                conn.state.c_str());
                    }
                } catch (const std::exception& e){
                    syslog(LOG_ERR, "Parser error: %s", e.what());
                }
                //syslog(LOG_INFO, "Daemon pulse: Checking network connections (stub)...");

                //записываем на 10 секунде
                std::this_thread::sleep_for(10s);
            }
            // Логируем выход из цикла уже в основном потоке
            syslog(LOG_INFO, "Exited main loop safety. Preparing for destruction. ");

        } //run()

 };

 /**
  * @brief главная точка входа в программу.
  * @return Код заврешающей программы
  */

  int main(){
    try {
        NetworkDaemon daemon;
        daemon.run();
    } catch (const std::exception& e){
        ::openlog("net_monitor_err", LOG_PID, LOG_DAEMON);
        syslog(LOG_ERR, "Fatal exception caught in main: %s", e.what());
        ::closelog();
        return EXIT_FAILURE;
    }
     return EXIT_SUCCESS;
  }
