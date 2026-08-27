/**
 * @file NetworkParser.hpp
 * @brief Класс для парсинга системных файлов сети Linux (/proc/net/tcp)
 * @author Sergei Zoabch
 * 
 */

#ifndef NETWORK_PARSER_HPP
#define NETWORK_PARSER_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdint>

/**
 * @struct ConnectionInfo
 * @brief Структура, хранящая информацию об одном сетевом подключении.
 */

 struct ConnectionInfo {
    std::string localIP; /**< Локальный IP-адрес (например, "127.0.0.1") */
    uint16_t localPort;  /**< Локальный порт (например, 8080) */
    std::string remoteIp; /**< Удалённый IP-адрес */
    uint16_t remotePort;  /**< Удалённый порт */
    std::string state;    /**< Статус соединения (LISTEN, ESTABLISHED и т.д.) */
 };

 /**
  * @class NetworkParser
  * @brief Отвечает за чтение и декодирование сетевых таблиц ядра Linux.
  */
 class NetworkParser{
    private:
        /**
         * @brief Преобразуем HEX строк статуса в текст
         * @param stateHex Строка статуса из фала 
         * @return Текстовое описние статуса
         */
        std::string convertState(const std::string& stateHex) const {
            if (stateHex == "01"){ return "ESTABLISHED";}
            if (stateHex == "0A"){ return "LISTEN";}
            //Можно увеличить для полного списка
            return "UNKNOWN (" + stateHex + ")";
        }
    /**
     * @brief Декодирует HEX-строку в IP-адрес и порт.
     * @param hexStr Исходная строка из /proc/net/tcp.
     * @param ip Ссылка для записи полученного IP.
     * @param port Ссылка для записи полученного порта.
     * @return true если парсинг успешен, false в противном случае.
     */

     bool parseAddress(const std::string& hexStr, std::string& ip, uint16_t& port) const {
        size_t colonPos = hexStr.find(':');
        if (colonPos == std::string::npos || colonPos != 8){
            return false;
        }

        // Вырезаем HeX значения IP и Порта
        std::string ipHex = hexStr.substr(0, colonPos);
        std::string portHex = hexStr.substr(colonPos + 1);

        // Парсим порт (из HEX в uint16_t)
        std::stringstream ssPort;
        ssPort << std::hex << portHex;
        unsigned int p;
        ssPort >> p;
        port = static_cast<uint16_t>(p);

        // Парсим iP (учитывая Little - Endian, читаем по два символа с конца)
        std::stringstream ssIp;
        unsigned int bytes[4];
        for(int i = 0; i < 4; ++i){
            std::stringstream byteStream;
            byteStream << std::hex << ipHex.substr((3 - i) * 2, 2);
            byteStream >> bytes[i];
        }
        ssIp << bytes[0] << "." << bytes[1] << "." << bytes[2] << "." << bytes[3];
        ip = ssIp.str();

        return true;
     } 
    public:
        NetworkParser() = default;

        /**
         * @brief Читает файл /proc/net/tcp и возвращвет список активных подключений.
         * @return Вектор структур ConnectionInfo 
         */

         std::vector<ConnectionInfo> parseTcpConnections(){
            std::vector<ConnectionInfo> connections;
            std::ifstream file("/proc/net/tcp");

            if (!file.is_open()){
                throw std::runtime_error("failed to open /proc/net/tcp");
            }
            std::string line;
            // Пропускаем первую строчку-заголовок файла 
            std::getline(file, line);

            //читаем файл построчно 
            while (std::getline(file, line)){
                std::stringstream ss(line);
                std::string sl, local, remote, st;

                ss >> sl >> local >> remote >> st;
                if (local.empty() || remote.empty() || st.empty()){
                    continue;
                }

                ConnectionInfo conn;
                if(parseAddress(local, conn.localIP, conn.localPort)&&
                   parseAddress(remote, conn.remoteIp, conn.remotePort)){
                    conn.state = convertState(st);
                    connections.push_back(conn);
                   }
            }
            return connections;
         }
 };

#endif
