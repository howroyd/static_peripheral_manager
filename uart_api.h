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

static constexpr std::size_t uart_n_hw_ports = 2;

static_assert( (uart_n_hw_ports - 1) < static_cast<std::size_t>(UART_ID::UART_INVALID) );

[[nodiscard]] static constexpr bool is_valid_id(UART_ID id)
{
    switch (id)
    {
    case UART_ID::UART0:
    case UART_ID::UART1:
        return true;
    case UART_ID::UART_INVALID:
        break;
    };
    return false;
}

[[nodiscard]] static constexpr const char* id_str(UART_ID id)
{
    switch (id)
    {
    case UART_ID::UART0:
        return "UART0";
    case UART_ID::UART1:
        return "UART1";
    case UART_ID::UART_INVALID:
        break;
    };
    return "UART_INVALID";
}

struct UartConfig
{
    int     baud = 115200;
    int     bits = 8;
    bool    parity = false;
    int     stop_bits = 1;
    bool    flow_control = false;

    friend constexpr bool operator<=>(const UartConfig&, const UartConfig&) = default;
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
    using std::cout;

    cout << "api_uart_send to UART" << static_cast<int>(id) << "\n";
    for (uint16_t idx = 0 ; idx < len ; ++idx)
    {
        cout << static_cast<int>(buf[idx]) << '\t';
        //buf[idx] = 0;
        std::this_thread::sleep_for(1ms);
    }
    cout << "\n";
    return true;
}

inline bool api_uart_receive(UART_ID id, uint8_t* buf, uint16_t len)
{
    std::cout << "api_uart_receive from UART" << static_cast<int>(id) << "\n";
    std::iota(buf, buf + len, 16);
    return true;
}