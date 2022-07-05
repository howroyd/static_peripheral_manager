#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <span>
#include <type_traits>
#include <iostream>
#include <bitset>

#include "uart_api.h"

bool foo(UART_ID id = UART0);

// Configs
struct uart_global_cfg
{
    static constexpr std::size_t n_uarts = 2;
};

struct UartConfig
{
    int     baud = 115200;
    int     bits = 8;
    bool    parity = false;
    int     stop_bits = 1;
    bool    flow_control = false;

    friend constexpr bool operator <=>(const UartConfig&, const UartConfig&) = default;
};


// Forward decls
namespace UART_IMPL {
template <class T>
static bool impl_send(UART_ID id, std::span<const T> data);

template <class T>
static bool impl_receive(UART_ID id, std::span<T> data);
}

// Impl Interface
class UartImpl
{
    UART_ID id;
    UartConfig cfg;
    mutable std::mutex mutx;

public:
    UartImpl(UART_ID uart_id, UartConfig config) noexcept
        : id{uart_id}, cfg{config}
    {}


    template <class T>
    bool send(std::span<const T> data)
    {
        std::scoped_lock lock(mutx);
        return UART_IMPL::impl_send(id, std::forward<std::span<const T>>(data));
    }

    template <class T>
    bool receive(std::span<T> data)
    {
        std::scoped_lock lock(mutx);
        return UART_IMPL::impl_receive(id, std::forward<std::span<T>>(data));
    }

    auto& get_mutex() const
    {
        return mutx;
    }

    UartConfig  config() const { return cfg; }
    UART_ID     ID()     const { return id; }
};


// Backend
namespace UART_IMPL {

template <class Impl, std::size_t N_INSTANCES>
class HardwarePeripheralWrapper
{
    using weak_handle_t = std::weak_ptr<Impl>;

public:
    using handle_t = std::shared_ptr<Impl>;

    template <class... Args>
    static handle_t construct_instance(std::size_t idx, Args&&... args)
    {
        if (not instance_handles[idx].expired())
            return nullptr;

        const auto p = p_instance_from_idx(idx);
        if (not p)
            return nullptr;

        const auto ret = handle_t{ new (p) Impl(std::forward<Args>(args)...),
                                    [](Impl* _p){std::destroy_at(_p);} };
        instance_handles[idx] = ret;
        return ret;
    }

    static handle_t get_handle(std::size_t idx)
    {
        return handle_t{get_weak_handle(idx)};
    }
    static auto& get_mutex(std::size_t idx)
    {
        return get_handle(idx)->get_mutex();
    }
    static auto get_mutex()
    {
        std::array<std::mutex*, N_INSTANCES> ret{};
        for (std::size_t i = 0 ; i < N_INSTANCES ; ++i)
            ret[i] = &(get_mutex(i));
        return ret;
    }

    static bool is_constructed(std::size_t idx)
    {
        if (not (idx < N_INSTANCES))
            return false;
        return not get_weak_handle(idx).expired();
    }

    static auto is_constructed(void)
    {
        std::bitset<N_INSTANCES> ret;
        for (std::size_t i = 0 ; i < N_INSTANCES ; ++i)
            ret[i] = is_constructed(i);
        return ret;
    }

private:
    static constexpr std::byte* p_instance_from_idx(std::size_t idx)
    {
        if (not (idx < N_INSTANCES))
            return nullptr;

        const auto offset = idx * sizeof(Impl);
        return instance_memory + offset;
    }

    static constexpr weak_handle_t get_weak_handle(std::size_t idx)
    {
        return instance_handles.at(idx);
    }

    static std::byte instance_memory[N_INSTANCES * sizeof(Impl)] alignas(Impl);
    static std::array<weak_handle_t, N_INSTANCES> instance_handles;
    //static std::array<std::mutex, N_INSTANCES> instance_mutexes;
};

template <class Impl, std::size_t N_INSTANCES>
inline std::byte HardwarePeripheralWrapper<Impl, N_INSTANCES>::instance_memory[N_INSTANCES * sizeof(Impl)]{};
template <class Impl, std::size_t N_INSTANCES>
inline std::array<typename HardwarePeripheralWrapper<Impl, N_INSTANCES>::weak_handle_t, N_INSTANCES> HardwarePeripheralWrapper<Impl, N_INSTANCES>::instance_handles{};
//template <class Impl, std::size_t N_INSTANCES>
//inline std::array<std::mutex, N_INSTANCES> HardwarePeripheralWrapper<Impl, N_INSTANCES>::instance_mutexes{};


class UartInterface
{
    using impl_t = HardwarePeripheralWrapper<UartImpl, uart_global_cfg::n_uarts>;
    static impl_t impl;
    impl_t::handle_t my_handle;

public:
    UartInterface(UART_ID id, UartConfig cfg = UartConfig{})
        : my_handle{get_uart_handle(id, cfg)}
    {}

    static impl_t::handle_t get_uart_handle(UART_ID id, UartConfig cfg = UartConfig{})
    {
        const std::size_t idx = static_cast<std::size_t>(id);

        if (not (idx < uart_global_cfg::n_uarts))
            return nullptr;

        if (impl.is_constructed(idx))
        {
            const auto h = impl.get_handle(idx);
            return cfg == h->config() ? h : nullptr;
        }

        return impl.construct_instance(idx, id, cfg);
    }

    operator bool() const
    {
        return my_handle ? true : false;
    }

    template <class T>
    bool send(std::span<const T> data)
    {
        if (not my_handle) return false;
        return my_handle->send(std::forward<std::span<const T>>(data));
    }

    template <class T>
    bool receive(std::span<T> data)
    {
        if (not my_handle) return false;
        return my_handle->send(std::forward<std::span<T>>(data));
    }

    UartConfig  config() const { return my_handle ? my_handle->config() : UartConfig{}; }
    UART_ID     ID()     const { return my_handle ? my_handle->ID() : UART_INVALID; }

    static bool is_constructed(std::size_t idx)
    {
        return impl.is_constructed(idx);
    }

    static auto is_constructed()
    {
        return impl.is_constructed();
    }

    static auto& get_mutex(std::size_t idx)
    {
        return impl.get_mutex(idx);
    }
    static auto get_mutex()
    {
        return impl.get_mutex();
    }
};

inline HardwarePeripheralWrapper<UartImpl, uart_global_cfg::n_uarts> UartInterface::impl;
}


namespace UART_IMPL {
template <class T>
inline static bool impl_send(UART_ID id, std::span<const T> data)
{
    const uint16_t len = data.size_bytes() <= UINT16_MAX ? static_cast<uint16_t>(data.size_bytes()) : 0;

    if (not len)
        return false;

    //std::scoped_lock lock(mutex_api[static_cast<std::size_t>(id)]);
    return api_uart_send(id, reinterpret_cast<const uint8_t*>(data.data()), len);
}

template <class T>
inline static bool impl_receive(UART_ID id, std::span<T> data)
{
    const uint16_t len = data.size_bytes() <= UINT16_MAX ? static_cast<uint16_t>(data.size_bytes()) : 0;

    if (not len)
        return false;

    //std::scoped_lock lock(mutex_api[static_cast<std::size_t>(id)]);
    return api_uart_receive(id, reinterpret_cast<uint8_t*>(data.data()), len);
}
}