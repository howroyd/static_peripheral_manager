#include "uart_impl.h"

#include <iostream>
#include <array>
#include <span>
#include <numeric>

bool foo(UART_ID id)
{
    return false;
    /*
    std::array<std::byte, 15> out;
    std::iota<uint8_t*, uint8_t>(reinterpret_cast<uint8_t*>(out.begin()), reinterpret_cast<uint8_t*>(out.end()), 69);

    auto h_uart_scoped = UART_IMPL::UartInterface(id);
    return h_uart_scoped.send(std::span<const std::byte>{out});*/
}