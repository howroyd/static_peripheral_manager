#pragma once

#include <cstddef>
#include <span>

#include "uart_impl.h"
#include "hw_iface.h"

namespace UART
{

class Iface
{
    using hw_iface_t = HardwarePeripheralWrapper<Impl, uart_n_hw_ports>;
    using hw_handle_t = hw_iface_t::handle_t;
    hw_handle_t h{};

public:
    Iface(UART_ID uart_id, UartConfig config = UartConfig{})
    {
        const auto idx = id_to_idx(uart_id);

        if (hw_iface_t::is_constructed(idx))
        {
            auto existing_handle = hw_iface_t::get_handle(idx);
            if (existing_handle->cfg == config)
                h = existing_handle;
            // else "already exists with different config"
        }
        else
            h = hw_iface_t::construct_instance(id_to_idx(uart_id), uart_id, config);
    }

    operator bool() const { return h ? true : false; }

    bool init()
    {
        return h->init();
    }
    bool deinit()
    {
        return h->deinit();
    }

    template <class T>
    bool send(std::span<const T> data)
    {
        return h->send(std::forward<std::span<const T>>(data));
    }

    template <class T>
    bool receive(std::span<T> data)
    {
        return h->send(std::forward<std::span<T>>(data));
    }

    UART_ID     id()     const { return h->id; }
    UartConfig  config() const { return h->cfg; }
    auto&       mutex()  const { return h->mutx; }

    [[nodiscard]] static auto is_constructed_all(void)
    {
        return hw_iface_t::is_constructed();
    }

protected:
    static constexpr std::size_t id_to_idx(UART_ID id) { return static_cast<std::size_t>(id); }
};

}