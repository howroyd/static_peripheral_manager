#include <iostream>
#include <array>
#include <span>
#include <numeric>
#include <thread>
#include <chrono>

#include "uart_impl.h"

template <class T, class Cast=T>
void print_span(const std::span<T> data, const char delimiter = '\0')
{
    for (const auto& x : data) std::cout << static_cast<Cast>(x) << delimiter;
    std::cout << "\n";
}

template <class T, class Cast=T>
void print_span_tsv(const std::span<T> data)
{
    return print_span<T, Cast>(std::forward<const std::span<T>>(data), '\t');
}

void print_is_constructed()
{
    using std::cout;
    auto is_constructed = UART_IMPL::UartInterface::is_constructed();
    for (std::size_t i = 0 ; i < is_constructed.size() ; ++i)
        cout << "UART" << i << " is " << (is_constructed[i] ? "" : "not ") << "constructed\n";
}

int main()
{
    using std::cout;
    using std::clog;
    using std::cerr;

    std::array<char, 10> countup, countdown;

    std::iota(countup.begin(), countup.end(), 1);
    std::iota(countdown.rbegin(), countdown.rend(), 1);

    cout << "\nInitial data:\n";
    print_span_tsv<char, int>(countup);
    print_span_tsv<char, int>(countdown);
    print_is_constructed();

    cout << "\nSend data to persistent instance:\n";
    auto h_uart0 = UART_IMPL::UartInterface(UART0);
    auto h_uart0_clone = UART_IMPL::UartInterface(UART0, {57600});
    if (h_uart0_clone)
    {
        cerr << "ERROR: got existing instance thats using different config\n";
        return EXIT_FAILURE;
    }
    h_uart0.send(std::span<const char>(countup));
    h_uart0.send(std::span<const char>(countdown));
    print_is_constructed();
    {
        cout << "\nSend data to rvalue instance:\n";
        UART_IMPL::UartInterface(UART1).send(std::span<const char>(countup));
        UART_IMPL::UartInterface(UART1).send(std::span<const char>(countdown));
    }
    {
        cout << "\nSend data to scoped instance:\n";
        auto h_uart1 = UART_IMPL::UartInterface(UART1);
        h_uart1.send(std::span<const char>(countup));
        h_uart1.send(std::span<const char>(countdown));
    }
    {
        cout << "\nSend data from multiple translation units:\n";
        auto h_uart1 = UART_IMPL::UartInterface(UART1);
        h_uart1.send(std::span<const char>(countup));
        foo(UART1);
        print_is_constructed();
    }

    std::mutex mutex_cout;

    auto thread_fn = [&mutex_cout](UART_ID uart_id, bool forwards = true, std::size_t n_iterations = 5)
    {
        using namespace std::chrono_literals;

        const auto this_id = std::this_thread::get_id();
        auto h_uart = UART_IMPL::UartInterface(uart_id);
        auto p_mutexs_uart_cout = h_uart.get_mutex();

        std::array<char, 10> data;
        std::iota(data.begin(), data.end(), 1);

        {
            std::scoped_lock lock(mutex_cout, *p_mutexs_uart_cout[0], *p_mutexs_uart_cout[1]);
            clog << "Thread" << this_id << " starting\n";
        }

        for (std::size_t iter = 0 ; iter < n_iterations ; ++iter)
        {
            h_uart.send(std::span<const char>{data});
            if (forwards) std::rotate(data.begin(), data.begin() + 1, data.end());
            else std::rotate(data.rbegin(), data.rbegin() + 1, data.rend());

            std::this_thread::sleep_for(1ms);
        }

        {
            std::scoped_lock lock(mutex_cout, *p_mutexs_uart_cout[0], *p_mutexs_uart_cout[1]);
            clog << "Thread" << this_id << " finished\n";
        }
    };

    cout << "\nSend data from multiple threads:\n";
    std::thread threads[2] = {std::thread(thread_fn, UART1), std::thread(thread_fn, UART1, false)};

    for (auto& thread : threads) thread.join();

    cout << "\n--END--\n\n";

    return EXIT_SUCCESS;
}