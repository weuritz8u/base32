#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace base32 {

namespace detail {

inline constexpr char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

constexpr int decode_char(char c) noexcept {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';

    if (c >= '2' && c <= '7')
        return c - '2' + 26;

    // Optional: lowercase akzeptieren
    if (c >= 'a' && c <= 'z')
        return c - 'a';

    return -1;
}

} // namespace detail

/**
 * Encodes binary data using RFC 4648 Base32.
 *
 * Padding '=' is added so that the output length is a multiple of 8.
 */
inline std::string encode(std::span<const std::byte> data) {
    std::string result;

    if (data.empty())
        return result;

    result.reserve(((data.size() + 4) / 5) * 8);

    std::uint32_t buffer = 0;
    int bits = 0;

    for (const std::byte byte : data) {
        buffer = (buffer << 8) |
                 std::to_integer<std::uint8_t>(byte);

        bits += 8;

        while (bits >= 5) {
            bits -= 5;

            const auto index =
                static_cast<std::uint8_t>((buffer >> bits) & 0x1F);

            result += detail::alphabet[index];
        }
    }

    if (bits > 0) {
        const auto index =
            static_cast<std::uint8_t>((buffer << (5 - bits)) & 0x1F);

        result += detail::alphabet[index];
    }

    while (result.size() % 8 != 0)
        result += '=';

    return result;
}

/**
 * Convenience overload for strings.
 */
inline std::string encode(std::string_view input) {
    const auto* ptr =
        reinterpret_cast<const std::byte*>(input.data());

    return encode({
        ptr,
        input.size()
    });
}

/**
 * Decodes RFC 4648 Base32.
 *
 * Returns std::nullopt for invalid Base32 input.
 * Both uppercase and lowercase letters are accepted.
 */
inline std::optional<std::vector<std::byte>>
decode(std::string_view input) {
    if (input.empty())
        return std::vector<std::byte>{};

    // Base32 output must have a length divisible by 8
    if (input.size() % 8 != 0)
        return std::nullopt;

    std::vector<std::byte> result;
    result.reserve((input.size() / 8) * 5);

    std::uint32_t buffer = 0;
    int bits = 0;
    bool padding_started = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];

        if (c == '=') {
            padding_started = true;
            continue;
        }

        // Characters after padding are invalid
        if (padding_started)
            return std::nullopt;

        const int value = detail::decode_char(c);

        if (value < 0)
            return std::nullopt;

        buffer = (buffer << 5) |
                 static_cast<std::uint32_t>(value);

        bits += 5;

        if (bits >= 8) {
            bits -= 8;

            const auto byte =
                static_cast<std::uint8_t>((buffer >> bits) & 0xFF);

            result.push_back(
                static_cast<std::byte>(byte)
            );
        }
    }

    // Verify that unused trailing bits are zero.
    if (bits > 0) {
        const auto mask =
            static_cast<std::uint32_t>((1u << bits) - 1u);

        if ((buffer & mask) != 0)
            return std::nullopt;
    }

    // Validate padding according to RFC 4648.
    const auto padding =
        input.size() - input.find_last_not_of('=') - 1;

    if (padding > 6)
        return std::nullopt;

    if (input.find('=') != std::string_view::npos) {
        const std::size_t expected_padding =
            (8 - ((input.size() - padding) % 8)) % 8;

        if (padding != expected_padding)
            return std::nullopt;
    }

    return result;
}

/**
 * Convenience function returning a string.
 */
inline std::optional<std::string>
decode_string(std::string_view input) {
    auto decoded = decode(input);

    if (!decoded)
        return std::nullopt;

    std::string result;
    result.reserve(decoded->size());

    for (const auto byte : *decoded) {
        result.push_back(
            static_cast<char>(std::to_integer<unsigned char>(byte))
        );
    }

    return result;
}

} // namespace base32
