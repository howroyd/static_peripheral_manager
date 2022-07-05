#pragma once

#include <iostream>

#include <mutex>
#include <span>

#include "uart_api.h"

bool foo(UART_ID id = UART0);

namespace UART
{

class Impl
{
    bool initialised = false;

public:
    UART_ID id;
    UartConfig cfg;
    mutable std::mutex mutx{};

    Impl() = delete;
    Impl(UART_ID uart_id, UartConfig uart_config)
        : id{uart_id}, cfg{uart_config}
    {
        initialised = init();
    }
    ~Impl()
    {
        if (initialised)
            initialised = deinit();
    }

    bool init()
    {
        std::scoped_lock lock(mutx);
        return api_uart_init();
    }
    bool deinit()
    {
        std::scoped_lock lock(mutx);
        return api_uart_deinit();
    }
    
    template <class T>
    bool send(std::span<const T> data)
    {
        const uint16_t len = data.size_bytes() <= UINT16_MAX ? static_cast<uint16_t>(data.size_bytes()) : 0;

        if (not len)
            return false;

        std::scoped_lock lock(mutx);
        return api_uart_send(id, reinterpret_cast<const uint8_t*>(data.data()), len);
    }

    template <class T>
    bool receive(std::span<T> data)
    {
        const uint16_t len = data.size_bytes() <= UINT16_MAX ? static_cast<uint16_t>(data.size_bytes()) : 0;

        if (not len)
            return false;

        std::scoped_lock lock(mutx);
        return api_uart_receive(id, reinterpret_cast<uint8_t*>(data.data()), len);
    }
};

}