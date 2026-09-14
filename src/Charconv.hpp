#pragma once

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <system_error>
#include <type_traits>

// Number conversion without <charconv>. Apple's libc++ gates the floating point
// overloads of std::to_chars and std::from_chars behind availability annotations
// (macOS 13.3 and macOS 26.0), which puts them out of reach for hosts targeting an
// older deployment target. These helpers reproduce the std::to_chars/std::from_chars
// contract on top of snprintf/strtod and a hand written integer path, so the same
// code works on every toolchain.

namespace Mosaic
{
    namespace Detail
    {
        inline constexpr size_t CharsBufferCapacity = 512;

        enum class NumberFormat : uint8_t
        {
            Fixed,
            Scientific,
            General
        };

        struct ToCharsResult
        {
            char * ptr = nullptr;
            std::errc ec = std::errc{};
        };

        struct FromCharsResult
        {
            const char * ptr = nullptr;
            std::errc ec = std::errc{};
        };

        [[nodiscard]] inline int digitValue(char symbol, int base) noexcept
        {
            int value = -1;

            if(symbol >= '0' && symbol <= '9')
            {
                value = symbol - '0';
            }
            else if(symbol >= 'a' && symbol <= 'z')
            {
                value = symbol - 'a' + 10;
            }
            else if(symbol >= 'A' && symbol <= 'Z')
            {
                value = symbol - 'A' + 10;
            }

            if(value < 0 || value >= base)
            {
                return -1;
            }

            return value;
        }

        // Matches std::to_chars for integers: no plus sign, lowercase digits above 9.
        template<class T>
        [[nodiscard]] inline ToCharsResult toChars(char * first, char * last, T value, int base) noexcept
        {
            static_assert(std::is_integral_v<T>, "toChars requires an integral value");

            using Unsigned = std::make_unsigned_t<T>;

            bool negative = false;
            Unsigned magnitude = static_cast<Unsigned>(value);

            if constexpr(std::is_signed_v<T>)
            {
                if(value < T{})
                {
                    negative = true;
                    magnitude = static_cast<Unsigned>(Unsigned{} - magnitude);
                }
            }

            char digits[sizeof(Unsigned) * 8 + 1];
            size_t digitCount = 0;

            do
            {
                Unsigned digit = magnitude % static_cast<Unsigned>(base);
                magnitude /= static_cast<Unsigned>(base);

                digits[digitCount++] = static_cast<char>(digit < 10 ? '0' + digit : 'a' + (digit - 10));
            } while(magnitude != Unsigned{});

            size_t size = digitCount + (negative == true ? 1u : 0u);

            if(first + size > last)
            {
                return {last, std::errc::value_too_large};
            }

            char * cursor = first;

            if(negative == true)
            {
                *cursor++ = '-';
            }

            while(digitCount != 0)
            {
                *cursor++ = digits[--digitCount];
            }

            return {cursor, std::errc{}};
        }

        // Matches std::from_chars for integers: rejects leading whitespace and '+'.
        template<class T>
        [[nodiscard]] inline FromCharsResult fromChars(const char * first, const char * last, T & value, int base) noexcept
        {
            static_assert(std::is_integral_v<T>, "fromChars requires an integral value");

            using Unsigned = std::make_unsigned_t<T>;

            const char * cursor = first;

            bool negative = false;

            if constexpr(std::is_signed_v<T>)
            {
                if(cursor != last && *cursor == '-')
                {
                    negative = true;
                    ++cursor;
                }
            }

            const char * digitsBegin = cursor;

            Unsigned accumulator = Unsigned{};
            bool overflow = false;

            Unsigned limit = negative == true
                ? static_cast<Unsigned>(static_cast<Unsigned>(std::numeric_limits<T>::max()) + Unsigned{1})
                : static_cast<Unsigned>(std::numeric_limits<T>::max());

            while(cursor != last)
            {
                int digit = digitValue(*cursor, base);

                if(digit < 0)
                {
                    break;
                }

                if(overflow == false)
                {
                    Unsigned next = accumulator * static_cast<Unsigned>(base) + static_cast<Unsigned>(digit);

                    if(accumulator > (limit - static_cast<Unsigned>(digit)) / static_cast<Unsigned>(base))
                    {
                        overflow = true;
                    }
                    else
                    {
                        accumulator = next;
                    }
                }

                ++cursor;
            }

            if(cursor == digitsBegin)
            {
                return {first, std::errc::invalid_argument};
            }

            if(overflow == true)
            {
                return {cursor, std::errc::result_out_of_range};
            }

            if constexpr(std::is_signed_v<T>)
            {
                value = negative == true
                    ? static_cast<T>(Unsigned{} - accumulator)
                    : static_cast<T>(accumulator);
            }
            else
            {
                value = static_cast<T>(accumulator);
            }

            return {cursor, std::errc{}};
        }

        template<class T>
        [[nodiscard]] inline T parseFloating(const char * text, char ** end) noexcept
        {
            if constexpr(std::is_same_v<T, float>)
            {
                return std::strtof(text, end);
            }
            else if constexpr(std::is_same_v<T, long double>)
            {
                return std::strtold(text, end);
            }
            else
            {
                return static_cast<T>(std::strtod(text, end));
            }
        }

        [[nodiscard]] inline ToCharsResult writeChars(char * first, char * last, const char * text, size_t size) noexcept
        {
            if(first + size > last)
            {
                return {last, std::errc::value_too_large};
            }

            std::memcpy(first, text, size);

            return {first + size, std::errc{}};
        }

        // Matches std::to_chars(first, last, value, format, precision).
        template<class T>
        [[nodiscard]] inline ToCharsResult toCharsFloat(char * first, char * last, T value, NumberFormat format, int precision) noexcept
        {
            static_assert(std::is_floating_point_v<T>, "toCharsFloat requires a floating point value");

            char conversion = 'f';

            if(format == NumberFormat::Scientific)
            {
                conversion = 'e';
            }
            else if(format == NumberFormat::General)
            {
                conversion = 'g';
            }

            char specifier[16] = {};
            std::snprintf(specifier, sizeof(specifier), "%%.%d%c", precision, conversion);

            char buffer[CharsBufferCapacity] = {};
            int written = std::snprintf(buffer, sizeof(buffer), specifier, static_cast<double>(value));

            if(written < 0)
            {
                return {last, std::errc::invalid_argument};
            }

            if(static_cast<size_t>(written) >= sizeof(buffer))
            {
                return {last, std::errc::value_too_large};
            }

            auto returnedValue = writeChars(first, last, buffer, static_cast<size_t>(written));

            return returnedValue;
        }

        // Matches std::to_chars(first, last, value): the shortest representation that
        // parses back to the same value, in fixed or scientific style, whichever is shorter.
        template<class T>
        [[nodiscard]] inline ToCharsResult toCharsFloat(char * first, char * last, T value) noexcept
        {
            static_assert(std::is_floating_point_v<T>, "toCharsFloat requires a floating point value");

            int maximumPrecision = std::numeric_limits<T>::max_digits10;

            char fixed[CharsBufferCapacity] = {};
            char scientific[CharsBufferCapacity] = {};

            size_t fixedSize = 0;
            size_t scientificSize = 0;

            // Precision means digits after the point for %f, so a magnitude below one
            // needs max_digits10 plus its leading zeros before it can round-trip.
            int fixedPrecisionLimit = maximumPrecision;

            if(value != T{} && std::isfinite(static_cast<double>(value)) == true)
            {
                int exponent = static_cast<int>(std::floor(std::log10(std::fabs(static_cast<double>(value)))));

                if(exponent < 0)
                {
                    fixedPrecisionLimit = maximumPrecision - exponent;
                }

                if(fixedPrecisionLimit > 400)
                {
                    fixedPrecisionLimit = 400;
                }
            }

            // The shortest round-tripping form of each style is the one with the
            // lowest precision that still parses back to the same value.
            for(int precision = 0; precision <= fixedPrecisionLimit; ++precision)
            {
                int written = std::snprintf(fixed, sizeof(fixed), "%.*f", precision, static_cast<double>(value));

                if(written < 0 || static_cast<size_t>(written) >= sizeof(fixed))
                {
                    break;
                }

                if(parseFloating<T>(fixed, nullptr) == value)
                {
                    fixedSize = static_cast<size_t>(written);

                    break;
                }
            }

            for(int precision = 0; precision <= maximumPrecision; ++precision)
            {
                int written = std::snprintf(scientific, sizeof(scientific), "%.*e", precision, static_cast<double>(value));

                if(written < 0 || static_cast<size_t>(written) >= sizeof(scientific))
                {
                    break;
                }

                if(parseFloating<T>(scientific, nullptr) == value)
                {
                    scientificSize = static_cast<size_t>(written);

                    break;
                }
            }

            if(fixedSize != 0 && (scientificSize == 0 || fixedSize <= scientificSize))
            {
                auto returnedValue = writeChars(first, last, fixed, fixedSize);

                return returnedValue;
            }

            if(scientificSize != 0)
            {
                auto returnedValue = writeChars(first, last, scientific, scientificSize);

                return returnedValue;
            }

            // Not finite: printf spells these out, and neither style round-trips.
            int written = std::snprintf(fixed, sizeof(fixed), "%f", static_cast<double>(value));

            if(written < 0 || static_cast<size_t>(written) >= sizeof(fixed))
            {
                return {last, std::errc::value_too_large};
            }

            auto returnedValue = writeChars(first, last, fixed, static_cast<size_t>(written));

            return returnedValue;
        }

        // Matches std::from_chars(first, last, value, chars_format::general): rejects
        // leading whitespace, a leading '+' and hexadecimal forms.
        template<class T>
        [[nodiscard]] inline FromCharsResult fromCharsFloat(const char * first, const char * last, T & value) noexcept
        {
            static_assert(std::is_floating_point_v<T>, "fromCharsFloat requires a floating point value");

            if(first == last)
            {
                return {first, std::errc::invalid_argument};
            }

            if(*first == '+' || std::isspace(static_cast<unsigned char>(*first)) != 0)
            {
                return {first, std::errc::invalid_argument};
            }

            size_t size = static_cast<size_t>(last - first);

            if(size >= CharsBufferCapacity)
            {
                return {first, std::errc::value_too_large};
            }

            char buffer[CharsBufferCapacity] = {};
            std::memcpy(buffer, first, size);

            // general format has no hexadecimal form, so "0x1p3" parses as a plain "0"
            // and stops at the 'x', the same way std::from_chars does
            size_t signOffset = (buffer[0] == '-') ? 1u : 0u;

            if(buffer[signOffset] == '0' && (buffer[signOffset + 1] == 'x' || buffer[signOffset + 1] == 'X'))
            {
                buffer[signOffset + 1] = '\0';
            }

            errno = 0;

            char * parsedEnd = nullptr;
            T parsed = parseFloating<T>(buffer, &parsedEnd);

            if(parsedEnd == buffer)
            {
                return {first, std::errc::invalid_argument};
            }

            const char * consumed = first + (parsedEnd - buffer);

            if(errno == ERANGE && (parsed == T{} || std::isinf(static_cast<double>(parsed)) == true))
            {
                return {consumed, std::errc::result_out_of_range};
            }

            value = parsed;

            return {consumed, std::errc{}};
        }

        // One entry point for both kinds of number, mirroring std::to_chars: integers
        // are written in base ten, floating point in its shortest round-tripping form.
        template<class T>
        [[nodiscard]] inline ToCharsResult toChars(char * first, char * last, T value) noexcept
        {
            if constexpr(std::is_floating_point_v<T>)
            {
                return toCharsFloat(first, last, value);
            }
            else
            {
                return toChars(first, last, value, 10);
            }
        }

        template<class T>
        [[nodiscard]] inline ToCharsResult toChars(char * first, char * last, T value, NumberFormat format, int precision) noexcept
        {
            auto returnedValue = toCharsFloat(first, last, value, format, precision);

            return returnedValue;
        }

        // Mirrors std::from_chars: base ten for integers, the general format for
        // floating point.
        template<class T>
        [[nodiscard]] inline FromCharsResult fromChars(const char * first, const char * last, T & value) noexcept
        {
            if constexpr(std::is_floating_point_v<T>)
            {
                return fromCharsFloat(first, last, value);
            }
            else
            {
                return fromChars(first, last, value, 10);
            }
        }
    }
}
