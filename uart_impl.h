#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <span>
#include <type_traits>
#include <iostream>

#include "uart_api.h"

bool foo(UART_ID id = UART0);

// Configs
struct uart_global_cfg
{
    static constexpr std::size_t n_uarts = 2;
};

struct UartConfig
{
    int baud = 115200;
    int bits = 8;
    bool parity = false;
    int stop_bits = 1;
    bool flow_control = false;

    bool operator==(const UartConfig& other) const
    {
        return (other.baud == baud) 
            && (other.bits == bits)
            && (other.parity == parity)
            && (other.stop_bits == stop_bits)
            && (other.flow_control == flow_control);
    }
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

public:
    constexpr UartImpl(UART_ID uart_id, UartConfig config) noexcept 
        : id{uart_id}, cfg{config}
    {}
    

    template <class T>
    bool send(std::span<const T> data)
    {
        return UART_IMPL::impl_send(id, std::forward<std::span<const T>>(data));
    }

    template <class T>
    bool receive(std::span<T> data)
    {
        return UART_IMPL::impl_receive(id, std::forward<std::span<T>>(data));
    }

    UartConfig  config() const { return cfg; }
    UART_ID     ID()     const { return id; }
};


// Backend
namespace UART_IMPL {

inline std::byte global_instances[uart_global_cfg::n_uarts * sizeof(UartImpl)] alignas(UartImpl);
inline std::array<std::weak_ptr<UartImpl>, uart_global_cfg::n_uarts> global_handles;
inline std::mutex mutex_api[uart_global_cfg::n_uarts];

inline constexpr std::byte* pointer_from_idx(std::size_t idx)
{
    if (not (idx < uart_global_cfg::n_uarts))
        return nullptr;
    const auto offset = idx * sizeof(UartImpl);
    return global_instances + offset;
}

inline auto& get_mutexs()
{
    return mutex_api;
}

template <class T, std::size_t N>
inline auto is_constructed(const std::array<T,N>& arr)
{
    std::array<bool, N> ret;
    for (std::size_t i = 0 ; i < N ; ++i)
    {
        ret[i] = not arr[i].expired();
    }
    return ret;
}

template <class T, class... Args>
inline static std::shared_ptr<T> construct_instance(std::byte* where, Args... args)
{
    return { new (where) T(args...), [](T* p){std::destroy_at(p);} };
}
}


inline static std::shared_ptr<UartImpl> get_uart_handle(UART_ID id, UartConfig cfg = UartConfig{})
{
    using namespace UART_IMPL;
    using std::cout;
    
    const std::size_t idx = static_cast<std::size_t>(id);

    // Invalid UART
    if (not (idx < global_handles.size()))
        return nullptr;

    auto& handle = global_handles[idx];

    // Not already constructed
    if (handle.expired())
    {
        const auto ptr = construct_instance<UartImpl>(pointer_from_idx(idx), id, cfg);
        handle = ptr;

        return ptr;
    }

    // Get copy of existing
    auto ptr = std::shared_ptr(handle);

    // Existing is incompatible
    if (cfg != ptr->config())
        return nullptr;

    return ptr;
}

namespace UART_IMPL {
template <class T>
inline static bool impl_send(UART_ID id, std::span<const T> data)
{
    const uint16_t len = data.size_bytes() <= UINT16_MAX ? static_cast<uint16_t>(data.size_bytes()) : 0;
    
    if (not len)
        return false;
    
    std::scoped_lock lock(mutex_api[static_cast<std::size_t>(id)]);
    return api_uart_send(id, reinterpret_cast<const uint8_t*>(data.data()), len);
}

template <class T>
inline static bool impl_receive(UART_ID id, std::span<T> data)
{
    const uint16_t len = data.size_bytes() <= UINT16_MAX ? static_cast<uint16_t>(data.size_bytes()) : 0;
    
    if (not len)
        return false;
    
    std::scoped_lock lock(mutex_api[static_cast<std::size_t>(id)]);
    return api_uart_receive(id, reinterpret_cast<uint8_t*>(data.data()), len);
}
}