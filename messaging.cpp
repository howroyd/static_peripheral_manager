#include <cstdint>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <functional>

// TODO endianness, tidy word size

namespace Message
{

template <std::size_t MaxWordCount>
struct message_data
{
    using word_t = std::int_fast8_t;

    std::size_t word_count{};
    std::array<word_t, MaxWordCount> data{};

    template <class... Args>
    constexpr message_data(Args... args)
        : word_count{sizeof...(Args)},
        data{static_cast<word_t>(args)...}
    {
        static_assert(sizeof...(Args) <= MaxWordCount, "Overflow"); // TODO maths of type sizes
    }

    template <std::size_t N>
    constexpr message_data(const std::array<word_t, N>& src)
        : word_count{N},
        data{src}
    {
        static_assert(N <= MaxWordCount, "Overflow"); // TODO maths of type sizes
    }
};

namespace match
{

template <bool b>
struct always
{
    template <class T>
    [[nodiscard]] static constexpr bool test([[maybe_unused]] T val)
    {
        return std::conditional_t<b, std::true_type, std::false_type>();
    }
};

template <auto Value>
struct equal_to
{
    template <class T>
    [[nodiscard]] static constexpr bool test(T val)
    {
        return std::equal_to{}(val, Value);
    }
};

template <auto... Values>
struct in
{
    template <class T>
    [[nodiscard]] static constexpr bool test(T val)
    {
        return (equal_to<Values>(val) || ...);
    }
};

template <auto Value>
struct less_than
{
    template <class T>
    [[nodiscard]] static constexpr bool test(T val)
    {
        return std::less{}(val, Value);
    }
};

}

// TODO bit of a hack whist awaiting compiler support?
// https://stackoverflow.com/questions/54278201/what-is-c20s-string-literal-operator-template
struct Tag
{
    constexpr Tag(const char*) {}
    friend auto operator<=>(const Tag&, const Tag&) = default;
};

template <
    Tag NameType,
    std::size_t Msb,
    std::size_t Lsb,
    class T = std::byte,
    T DefaultValue = T{},
    class MatchRequirement = match::always<true>>
class field
{
    T value;

public:
    using FieldId = field<NameType, Msb, Lsb, T>;
    using value_type = T;

    constexpr field(T val = DefaultValue)
        : value{val}{}

    template <class DataStore>
    constexpr void insert(const DataStore& data) const
    {
        // TODO
        // check bounds
        // bit shifting
        // apply masks
        // recombining bits into a T
        (void) data;
    }

    template <class DataStore>
    [[nodiscard]] static constexpr T extract(const DataStore& data)
    {
        // TODO
        // check bounds
        // bit shifting
        // apply masks
        // recombining bits into a T
        (void) data;
        return T{};
    }

    [[nodiscard]] constexpr bool is_valid()
    {
        if (not MatchRequirement::test(value))
            std::cerr << "Invalid " << (int)Msb << "\n";
        return MatchRequirement::test(value);
    }

    template <T NewDefault, class NewMatcher = match::always<true>>
    using extend_field = field<NameType, Msb, Lsb, T, NewDefault, NewMatcher>;

    template <T NewDefaultValue>
    using with_defualt = extend_field<NewDefaultValue>;

    template <T NewValue>
    using with_required = extend_field<NewValue, match::equal_to<NewValue>>;

    template <T... PotentialValues>
    using with_in = extend_field<DefaultValue, match::in<PotentialValues...>>;

    template <T Value>
    using with_less_than = extend_field<DefaultValue, match::less_than<Value>>;

    template <class NewRequiredMatcher>
    using with_match = extend_field<DefaultValue, NewRequiredMatcher>;

    // TODO
    template <T NewValue> using equal_to = with_required<NewValue>;
    template <T Value>    using less_than = with_less_than<Value>;
};

template <class, class = void>
constexpr bool is_field_type = false;

template <class T>
constexpr bool is_field_type<
    T,
    std::void_t<class T::FieldId>
    > = true;

template <
    Tag NameType,
    std::size_t MaxWordCount,
    class... Fields>
struct message : public message_data<MaxWordCount>
{
    constexpr message()
        : message_data<MaxWordCount>()
    {
        (set(Fields{}), ...);
    }

    template <class T,
        class... Args,
        std::enable_if_t<std::is_integral_v<T>, int> = 0>
    constexpr message(T v, Args... args)
        : message_data<MaxWordCount>{v, args...}
    {}

    template <class ArgField,
        class... ArgFields,
        std::enable_if_t<is_field_type<ArgField>, int> = 0>
    constexpr message(ArgField arg_field, ArgFields... arg_fields)
        : message_data<MaxWordCount>{}
    {
        // TODO Set defaults... this mimics the default ctor
        (set(Fields{}), ...);

        set(arg_field);

        // TODO set remainder
        if (sizeof...(arg_fields)) (set(arg_fields), ...);
    }

    template <class FieldType>
    constexpr void set(FieldType field)
    {
        static_assert(has_field<FieldType>());
        //check_field_is_contained<FieldType>(); // TODO what does this do?
        field.insert(this->data);
    }

    template <class FieldType>
    constexpr FieldType get()
    {
        static_assert(has_field<FieldType>());
        //check_field_is_contained<FieldType>(); // TODO what does this do?
        return FieldType::extract(this->data);
    }

    template <class Field>
    [[nodiscard]] static constexpr bool has_field()
    {
        return (std::is_same_v<class Field::FieldId, class Fields::FieldId> || ...);
    }

    [[nodiscard]] constexpr bool is_valid()
    {
        return (get<Fields>().is_valid() && ...);
    }
};


}


namespace Device::I2cRegisters::Filter
{

using cmd    = Message::field<"cmd",    47, 40, std::uint8_t>;
using init   = Message::field<"init",   39, 25, std::int16_t>;
using enable = Message::field<"enable", 24, 24, bool>;
using kp     = Message::field<"kp",     23, 16, std::uint8_t>;
using ki     = Message::field<"ki",     15, 8,  std::uint8_t>;
using kd     = Message::field<"kd",     7,  0,  std::uint8_t>;

using _msg_base_t = Message::message<
        "filter_config",
        6, // n words for packet,
        cmd::equal_to<0x69>,
        init::less_than<16383>, // only 15 bit (signed)
        enable,
        kp,
        ki,
        kd>;

struct config_message
    : _msg_base_t
{
    template <class... Args>
    constexpr config_message(Args... args)
        : _msg_base_t{args...}
    {}

};



}



int main()
{
    using std::cout;
    using std::clog;
    using std::cerr;

    using namespace Device::I2cRegisters::Filter;

    [[maybe_unused]] std::array<std::uint32_t, 6> packet;
/*
    config_message from_pkt(packet);
    if (from_pkt.is_valid())
    {
        cout << "from_pkt is valid\n";
    }
    if (from_pkt.get<enable>())
    {
        cout << "from_pkt filter is enabled\n";
    }
*/

    auto out_pkt = config_message{
        init{0},
        enable{true},
        kp{1},
        ki{2},
        kd{3}
    };
    if (out_pkt.is_valid())
    {
        cout << "out_pkt is valid\n";
    }
    else cerr << "out_pkt is invalid\n";
    out_pkt.set(enable{false});

    cout << "\n--END--\n\n";

    return EXIT_SUCCESS;
}