// License: public domain/CC0
#include <regex>
#include <string>
#include <cstdio>
#include <utility>
#include <climits>
#include <cinttypes>

bool parseNumber(const char* str, uint64_t& result, int radix = 0)
{
    errno = 0;
    char* end;
    result = strtoull(str, &end, radix);
    if(!result && end == str)
        return false;
    if(result == ULLONG_MAX && errno)
        return false;
    if(*end)
        return false;
    return true;
}

template<typename T, size_t Radix>
struct Number
{
    Number(T& value) : value(value) { }
    Number(const Number&) = delete;
    Number(Number&&) = delete;
private:
    T& value;

    template<typename>
    friend struct Parser;
};

template<typename T>
using Hex = Number<T, 16>;

template<typename T>
struct Parser;

template<>
struct Parser<uint64_t>
{
    void parse(const std::string& str, uint64_t& value)
    {
        if(!parseNumber(str.c_str(), value))
            throw std::invalid_argument("Failed to parse '" + str + "' to uint64_t");
    }
};

template<>
struct Parser<uint32_t>
{
    void parse(const std::string& str, uint32_t& value)
    {
        uint64_t pvalue = 0;
        if (!parseNumber(str.c_str(), pvalue))
            throw std::invalid_argument("Failed to parse '" + str + "' to uint32_t");
        if (pvalue > std::numeric_limits<uint32_t>::max())
            throw std::invalid_argument("Value '" + str + "' too big for uint32_t");
        value = pvalue & 0xFFFFFFFF;
    }
};

template<>
struct Parser<std::string>
{
    void parse(const std::string& str, std::string& value)
    {
        value = str;
    }
};

template<typename T, size_t Radix>
struct Parser<Number<T, Radix>>
{
    void parse(const std::string& str, Number<T, Radix>& value)
    {
        if(!parseNumber(str.c_str(), value.value, Radix))
            throw std::invalid_argument("Failed to parse '" + str + "' to Number");
    }
};

namespace detail
{
    template<typename... Ts>
    void discard(Ts...) { }

    template<typename T>
    bool extract(const std::string& str, T& value)
    {
        Parser<T>().parse(str, value);
        return true;
    }

    template<typename... Ts, size_t... I>
    void extract_smatch_helper(const std::smatch& m, std::tuple<Ts&...> args, std::index_sequence<I...>)
    {
        discard(extract(m[I + 1].str(), std::get<I>(args))...);
    }

    template<typename... Ts>
    void extract_smatch(const std::smatch& m, Ts&... args)
    {
        if(sizeof...(args) + 1 != m.size())
            throw std::invalid_argument("Amount of capture groups not equal to amount of template parameters");
        extract_smatch_helper(m, std::forward_as_tuple(args...), std::index_sequence_for<Ts...>{});
    }
}

template<typename... Ts>
bool match(const std::regex& regex, const std::string& str, Ts&... args)
{
    std::smatch m;
    if(!std::regex_search(str, m, regex))
        return false;
    detail::extract_smatch(m, args...);
    return true;
}