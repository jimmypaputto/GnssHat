/*
 * Jimmy Paputto 2021
 */

#ifndef JIMMY_PAPUTTO_UTILS_HPP_
#define JIMMY_PAPUTTO_UTILS_HPP_

#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>


bool try3times(std::function<bool()> configureFunction);

void setGpio(const char* chipname, const uint32_t line_num, int value);

static void printVector(const std::string& name, std::span<const uint8_t> data)
{
    printf("[DEBUG] %s: [", name.c_str());
    for (size_t i = 0; i < data.size(); ++i) {
        printf("0x%02X", data[i]);
        if (i < data.size() - 1) {
            printf(", ");
        }
    }
    printf("] (size: %zu)\r\n", data.size());
}

template<typename E, E beginVal, E endVal>
constexpr uint8_t countEnum()
{
    uint8_t size = 0;
    for (
        uint8_t i = static_cast<uint8_t>(beginVal);
        i <= static_cast<uint8_t>(endVal);
        i++
    )
    {
        size++;
    }
    return size;
}

template<typename E>
constexpr std::underlying_type_t<E> to_underlying(E e) noexcept
{
    return static_cast<std::underlying_type_t<E>>(e);
}

constexpr bool isLittleEndian()
{
    return std::endian::native == std::endian::little;
}

/// Read a value of type T from a little-endian byte buffer at given offset.
/// Uses std::memcpy to avoid strict aliasing and unaligned access UB.
template <typename T>
T readLE(std::span<const uint8_t> data, size_t offset)
{
    T result;
    std::memcpy(&result, data.data() + offset, sizeof(T));
    return result;
}

/// Overload: read from raw byte pointer.
template <typename T>
T readLE(const uint8_t* data)
{
    T result;
    std::memcpy(&result, data, sizeof(T));
    return result;
}

/// Append a value of type T as little-endian bytes to a vector.
template <typename T>
void appendLE(T value, std::vector<uint8_t>& out)
{
    const size_t offset = out.size();
    out.resize(offset + sizeof(T));
    std::memcpy(out.data() + offset, &value, sizeof(T));
}

template <typename T>
void serialize(T toSerialize, std::vector<uint8_t>& bufor, const uint32_t offset)
{
    if (bufor.size() <= offset + sizeof(T) - 1)
    {
        return;
    }
    std::memcpy(bufor.data() + offset, &toSerialize, sizeof(T));
}

template <typename integer>
std::vector<uint8_t> serializeInt2LittleEndian(integer value)
{
    if (isLittleEndian())
    {
        std::vector<uint8_t> result(sizeof(integer));
        std::memcpy(result.data(), &value, sizeof(integer));
        return result;
    }
    else
    {
        std::vector<uint8_t> result(sizeof(integer));
        for (size_t i = 0; i < sizeof(integer); ++i)
        {
            result[i] = (value >> (8 * i)) & 0xFF;
        }
        return result;
    }
}

template <typename T>
std::vector<T> operator + (const std::vector<T>& vector1, const std::vector<T>& vector2)
{
    std::vector<T> output;
    output.insert(output.end(), vector1.begin(), vector1.end());
    output.insert(output.end(), vector2.begin(), vector2.end());
    return output;
}

template <typename T>
std::vector<T> operator += (std::vector<T>& vector1, const std::vector<T>& vector2)
{
    vector1.insert(vector1.end(), vector2.begin(), vector2.end());
    return vector1;
}

bool getBit(uint8_t val, uint8_t pos);
void setBit(uint32_t& val, uint8_t pos, bool bit);

#endif  // JIMMY_PAPUTTO_UTILS_HPP_
