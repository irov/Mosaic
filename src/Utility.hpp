#pragma once

#include "Context.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <type_traits>

namespace Mosaic
{
    namespace Detail
    {
        enum class ValueContextAction : uint8_t
        {
            None,
            Copy,
            Paste
        };

        void setFlag(Response & response, unsigned bit, bool value = true) noexcept;
        [[nodiscard]] bool modifierMatches(const Modifiers & actual, const Modifiers & expected) noexcept;
        [[nodiscard]] bool contextMenuPointerHit(const Context * ui, const Context::Node & node, const PointerState & pointer) noexcept;
        [[nodiscard]] bool contextMenuOpen(const Context * ui, Id owner) noexcept;
        void openContextMenu(Context * ui, Response & response, size_t node, const Vec2 & position);
        [[nodiscard]] Response contextMenuItem(Context * ui, StringView label, bool enabled, const SourceLocation & location);
        [[nodiscard]] ValueContextAction valueContextMenu(Context * ui, Id owner, StringView currentValue, bool canCopy, bool canPaste, const SourceLocation & location);

        template<class T> [[nodiscard]] bool toString(T value, String * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            char buffer[64] = {};
            auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);

            if(result.ec != std::errc{})
            {
                return false;
            }

            String output(buffer, result.ptr);

            *_out = output;

            return true;
        }

        template<class T> requires std::is_floating_point_v<T>
        [[nodiscard]] bool formatFloating(T value, int precision, StringView format, String * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            std::chars_format numberFormat = std::chars_format::fixed;
            int resolvedPrecision = std::clamp(precision, 0, 8);
            size_t tokenBegin = StringView::npos;
            size_t tokenEnd = StringView::npos;

            if(format.empty() == false)
            {
                for(size_t index = 0; index < format.size(); ++index)
                {
                    if(format[index] != '%')
                    {
                        continue;
                    }

                    if(index + 1 < format.size() && format[index + 1] == '%')
                    {
                        ++index;
                        continue;
                    }

                    tokenBegin = index;
                    for(size_t end = index + 1; end < format.size(); ++end)
                    {
                        if(format[end] == '.')
                        {
                            int parsedPrecision = 0;
                            bool hasPrecision = false;
                            for(size_t digit = end + 1; digit < format.size() && std::isdigit(static_cast<unsigned char>(format[digit])); ++digit)
                            {
                                parsedPrecision = parsedPrecision * 10 + (format[digit] - '0');
                                hasPrecision = true;
                            }

                            if(hasPrecision == true)
                            {
                                resolvedPrecision = std::clamp(parsedPrecision, 0, 8);
                            }
                        }

                        char conversion = format[end];
                        bool conversionFound = false;
                        switch(conversion)
                        {
                        case 'f':
                        case 'F':
                        case 'e':
                        case 'E':
                        case 'g':
                        case 'G':
                        {
                            conversionFound = true;
                            break;
                        }
                        default:
                        {
                            break;
                        }
                        }

                        if(conversionFound == true)
                        {
                            tokenEnd = end + 1;

                            if(conversion == 'e')
                            {
                                numberFormat = std::chars_format::scientific;
                            }
                            else if(conversion == 'E')
                            {
                                numberFormat = std::chars_format::scientific;
                            }
                            else if(conversion == 'g')
                            {
                                numberFormat = std::chars_format::general;
                            }
                            else if(conversion == 'G')
                            {
                                numberFormat = std::chars_format::general;
                            }

                            break;
                        }
                    }
                    break;
                }

                if(tokenBegin == StringView::npos)
                {
                    String output(format);

                    *_out = output;

                    return true;
                }

                if(tokenEnd == StringView::npos)
                {
                    String output(format);

                    *_out = output;

                    return true;
                }
            }

            char buffer[64] = {};
            auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, numberFormat, resolvedPrecision);

            if(result.ec != std::errc{})
            {
                return false;
            }

            String number(buffer, result.ptr);

            if(format.empty() == true)
            {

                *_out = number;

                return true;
            }

            String output;
            output.append(format.substr(0, tokenBegin));
            output += number;
            output.append(format.substr(tokenEnd));
            for(size_t index = 0; index + 1 < output.size();)
            {
                if(output[index] == '%' && output[index + 1] == '%')
                {
                    output.erase(index, 1);
                }
                else
                {
                    ++index;
                }
            }

            *_out = output;

            return true;
        }

        template<class T> requires std::is_integral_v<T>
        [[nodiscard]] bool formatIntegral(T value, StringView format, String * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(format.empty() == true)
            {
                bool successful = Detail::toString(value, _out);

                return successful;
            }

            size_t tokenBegin = StringView::npos;
            size_t tokenEnd = StringView::npos;
            char conversion = 0;
            for(size_t index = 0; index < format.size(); ++index)
            {
                if(format[index] != '%')
                {
                    continue;
                }

                if(index + 1 < format.size() && format[index + 1] == '%')
                {
                    ++index;
                    continue;
                }

                tokenBegin = index;
                for(size_t end = index + 1; end < format.size(); ++end)
                {
                    char candidate = format[end];
                    bool conversionFound = false;
                    switch(candidate)
                    {
                    case 'd':
                    case 'i':
                    case 'u':
                    case 'x':
                    case 'X':
                    {
                        conversionFound = true;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                    }

                    if(conversionFound == true)
                    {
                        conversion = candidate;
                        tokenEnd = end + 1;
                        break;
                    }
                }
                break;
            }

            if(tokenBegin == StringView::npos)
            {
                String output(format);

                *_out = output;

                return true;
            }

            if(tokenEnd == StringView::npos)
            {
                String output(format);

                *_out = output;

                return true;
            }

            String number;

            if(conversion == 'x' || conversion == 'X')
            {
                using Unsigned = std::make_unsigned_t<T>;
                char buffer[2 + sizeof(T) * 2] = {};
                auto result = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<Unsigned>(value), 16);

                if(result.ec != std::errc{})
                {
                    return false;
                }

                number.assign(buffer, result.ptr);

                if(conversion == 'X')
                {
                    for(char & character : number)
                    {
                        if(character >= 'a' && character <= 'f')
                        {
                            character -= 'a' - 'A';
                        }
                    }
                }

                size_t width = 0;
                for(size_t index = tokenBegin + 1; index + 1 < tokenEnd; ++index)
                {
                    if(format[index] >= '0' && format[index] <= '9')
                    {
                        width = width * 10 + static_cast<size_t>(format[index] - '0');
                    }
                }

                if(width > number.size())
                {
                    number.insert(0, width - number.size(), '0');
                }
            }
            else
            {
                if(Detail::toString(value, &number) == false)
                {
                    return false;
                }
            }

            String output;
            output.append(format.substr(0, tokenBegin));
            output += number;
            output.append(format.substr(tokenEnd));
            for(size_t index = 0; index + 1 < output.size();)
            {
                if(output[index] == '%' && output[index + 1] == '%')
                {
                    output.erase(index, 1);
                }
                else
                {
                    ++index;
                }
            }

            *_out = output;

            return true;
        }
    } // namespace Detail
} // namespace Mosaic
