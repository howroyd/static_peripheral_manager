#pragma once

#include <cstdint>
#include <numeric>
#include <iostream>
#include <thread>
#include <chrono>

enum UART_ID
{
    UART0, UART1, UART_INVALID
};

inline bool api_uart_init()
{
    return true;
}

inline bool api_uart_deinit()
{
    return true;
}

inline bool api_uart_send(UART_ID id, const uint8_t* buf, uint16_t len)
{
    using namespace std::chrono_literals;
    
    std::cout << "api_uart_send to UART" << static_cast<int>(id) << "\n";
    for (uint16_t idx = 0 ; idx < len ; ++idx)
    {
        std::cout << static_cast<int>(buf[idx]) << '\t';
        //buf[idx] = 0;
        std::this_thread::sleep_for(1ms);
    }
    std::cout << "\n";
    return true;
}

inline bool api_uart_receive(UART_ID id, uint8_t* buf, uint16_t len)
{
    std::cout << "api_uart_receive from UART" << static_cast<int>(id) << "\n";
    std::iota(buf, buf + len, 16);
    return true;
}