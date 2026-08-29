#include "Context.hpp"
#include "Utility.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cctype>
#include <limits>
#include <type_traits>

namespace Mosaic
{
    namespace Detail
    {
        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseNumericText(StringView text, T * const _out, NumericBase base) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(text.empty() == true)
            {
                return false;
            }

            if(base == NumericBase::Hexadecimal)
            {
                if(text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
                {
                    text.remove_prefix(2);
                }

                if(text.empty() == true)
                {
                    return false;
                }

                using Unsigned = std::make_unsigned_t<T>;
                Unsigned parsed{};
                auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, 16);

                if(result.ec != std::errc{})
                {
                    return false;
                }

                if(result.ptr != text.data() + text.size())
                {
                    return false;
                }

                //////////////////////////////////////////////////////////////////////////
                if constexpr(std::is_signed_v<T>)
                {
                    *_out = std::bit_cast<T>(parsed);
                }
                else
                {
                    *_out = parsed;
                }

                return true;
            }

            T parsed{};
            auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);

            if(result.ec != std::errc{})
            {
                return false;
            }

            if(result.ptr != text.data() + text.size())
            {
                return false;
            }

            *_out = parsed;

            return true;
        }

        template<class T> requires std::is_floating_point_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseNumericText(StringView text, T * const _out, NumericBase) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(text.empty() == true)
            {
                return false;
            }

            T parsed{};
            auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, std::chars_format::general);

            if(result.ec != std::errc{})
            {
                return false;
            }

            if(result.ptr != text.data() + text.size())
            {
                return false;
            }

            if(std::isfinite(parsed) == false)
            {
                return false;
            }

            *_out = parsed;

            return true;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool numericText(T value, int, bool, NumericBase base, uint8_t minimumDigits, String * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(base == NumericBase::Decimal)
            {
                bool successful = Detail::toString(value, _out);

                return successful;
            }

            using Unsigned = std::make_unsigned_t<T>;
            Unsigned unsignedValue = std::is_signed_v<T> ? std::bit_cast<Unsigned>(value) : value;
            Array<char, sizeof(T) * 2 + 1> buffer{};
            auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), unsignedValue, 16);

            if(result.ec != std::errc{})
            {
                return false;
            }

            size_t digitCount = static_cast<size_t>(result.ptr - buffer.data());
            size_t padding = minimumDigits > digitCount ? static_cast<size_t>(minimumDigits) - digitCount : 0;
            String output(padding, '0');
            output.append(buffer.data(), digitCount);
            for(char & character : output)
            {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }

            *_out = output;

            return true;
        }

        template<class T> requires std::is_floating_point_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool numericText(T value, int precision, bool scientific, NumericBase, uint8_t, String * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(scientific == false)
            {
                bool successful = Detail::formatFloating(value, precision, {}, _out);

                return successful;
            }

            String format = "%.";
            String precisionText;
            if(Detail::toString(std::clamp(precision, 0, 8), &precisionText) == false)
            {
                return false;
            }

            format += precisionText;
            format += 'e';
            bool successful = Detail::formatFloating(value, precision, format, _out);

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        template<class T> requires std::is_arithmetic_v<T>
        void storeNumericValue(NumericState & state, T value) noexcept
        {
            if constexpr(std::is_floating_point_v<T>)
            {
                state.lastFloatingValue = static_cast<long double>(value);
                state.lastValueKind = NumericState::ValueKind::Floating;
            }
            else if constexpr(std::is_signed_v<T>)
            {
                state.lastSignedValue = static_cast<int64_t>(value);
                state.lastValueKind = NumericState::ValueKind::Signed;
            }
            else
            {
                state.lastUnsignedValue = static_cast<uint64_t>(value);
                state.lastValueKind = NumericState::ValueKind::Unsigned;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        template<class T> requires std::is_arithmetic_v<T>
        [[nodiscard]] bool numericValueMatches(const NumericState & state, T value) noexcept
        {
            if constexpr(std::is_floating_point_v<T>)
            {
                return state.lastValueKind == NumericState::ValueKind::Floating && state.lastFloatingValue == static_cast<long double>(value);
            }
            else if constexpr(std::is_signed_v<T>)
            {
                return state.lastValueKind == NumericState::ValueKind::Signed && state.lastSignedValue == static_cast<int64_t>(value);
            }
            else
            {
                return state.lastValueKind == NumericState::ValueKind::Unsigned && state.lastUnsignedValue == static_cast<uint64_t>(value);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        template<class T> requires std::is_arithmetic_v<T>
        [[nodiscard]] bool numericStepNegative(T value) noexcept
        {
            if constexpr(std::is_unsigned_v<T>)
            {
                return false;
            }

            bool result = value < T{};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void fillLastNode(Context * ui, Id id)
        {
            for(auto node = ui->nodes.rbegin(); node != ui->nodes.rend(); ++node)
            {
                if(node->id != id)
                {
                    continue;
                }

                node->layout.width = SizeRule::Fill;

                return;
            }
        }

        template<class T> requires std::is_arithmetic_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response numericInput(Context * ui, StringView label, T * value, T step, T fastStep, const NumericInputOptions & options, const SourceLocation & location)
        {
            auto numericScope = Mosaic::scope(ui, {}, location);
            ui->nodes[ui->currentParent].label.assign(label);
            Context::Persistent & persistentState = ui->state(numericScope.id());
            Detail::NumericState & state = ui->numericState(persistentState);
            T current = value == nullptr ? T{} : *value;

            if(state.initialized == false)
            {
                if(options.displayZeroAsEmpty == true && current == T{})
                {
                    state.temporaryText.clear();
                }
                else if(Detail::numericText(current, options.precision, options.scientific, options.base, options.minimumDigits, &state.temporaryText) == false)
                {
                    state.temporaryText = "?";
                }

                Detail::storeNumericValue(state, current);
                state.initialized = true;
            }

            LayoutOptions controlsLayout;
            controlsLayout.width = SizeRule::Fill;
            controlsLayout.gap = 2.f;
            auto controls = Mosaic::row(ui, controlsLayout, location);

            TextInputOptions textOptions;
            textOptions.numeric = true;
            textOptions.readOnly = options.readOnly;

            //////////////////////////////////////////////////////////////////////////
            if constexpr(std::is_integral_v<T>)
            {
                textOptions.characterFilter = options.base == NumericBase::Hexadecimal ? TextCharacterFilter::Hexadecimal : TextCharacterFilter::Decimal;
            }

            Response response = Mosaic::inputText(ui, "Value", &state.temporaryText, textOptions, location);
            Detail::fillLastNode(ui, response.id);

            bool valueChanged = false;
            T parsed{};
            bool parsedValue = Detail::parseNumericText<T>(state.temporaryText, &parsed, options.base);

            if(parsedValue == false && options.parseEmptyAsZero == true && state.temporaryText.empty() == true)
            {
                parsed = T{};
            }

            bool applyParsedValue = value != nullptr;

            if(options.readOnly == true)
            {
                applyParsedValue = false;
            }

            bool parsedOrEmpty = parsedValue;

            if(options.parseEmptyAsZero == true)
            {
                if(state.temporaryText.empty() == true)
                {
                    parsedOrEmpty = true;
                }
            }

            if(parsedOrEmpty == false)
            {
                applyParsedValue = false;
            }

            if(applyParsedValue == true)
            {
                if(parsed == *value)
                {
                    applyParsedValue = false;
                }
            }

            if(applyParsedValue == true)
            {
                *value = parsed;
                valueChanged = true;
            }

            bool negativeStep = Detail::numericStepNegative(step);

            if(negativeStep == true)
            {
                ui->frame.diagnostics.emplace_back("Numeric input step must not be negative");
            }

            bool negativeFastStep = Detail::numericStepNegative(fastStep);

            if(negativeFastStep == true)
            {
                ui->frame.diagnostics.emplace_back("Numeric input fast step must not be negative");
            }

            if(step > T{})
            {
                auto readOnlyScope = Mosaic::disabledScope(ui, options.readOnly, location);
                ButtonOptions buttonOptions;
                buttonOptions.width = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
                buttonOptions.repeat = true;
                T increment = step;

                if(ui->input.modifiers.shift == true && fastStep > T{})
                {
                    increment = fastStep;
                }

                if(Mosaic::button(ui, Key("Decrease"), "-", buttonOptions, location).clicked() == true && value != nullptr)
                {
                    //////////////////////////////////////////////////////////////////////////
                    if constexpr(std::is_integral_v<T>)
                    {
                        *value = increment > 0 && *value < std::numeric_limits<T>::min() + increment ? std::numeric_limits<T>::min() : static_cast<T>(*value - increment);
                    }
                    else
                    {
                        *value = static_cast<T>(*value - increment);
                    }

                    valueChanged = true;
                }

                if(Mosaic::button(ui, Key("Increase"), "+", buttonOptions, location).clicked() == true && value != nullptr)
                {
                    //////////////////////////////////////////////////////////////////////////
                    if constexpr(std::is_integral_v<T>)
                    {
                        *value = increment > 0 && *value > std::numeric_limits<T>::max() - increment ? std::numeric_limits<T>::max() : static_cast<T>(*value + increment);
                    }
                    else
                    {
                        *value = static_cast<T>(*value + increment);
                    }

                    valueChanged = true;
                }
            }

            if(value != nullptr && valueChanged == true)
            {
                if(options.displayZeroAsEmpty == true && *value == T{})
                {
                    state.temporaryText.clear();
                }
                else if(Detail::numericText(*value, options.precision, options.scientific, options.base, options.minimumDigits, &state.temporaryText) == false)
                {
                    state.temporaryText = "?";
                }

                Detail::storeNumericValue(state, *value);
                Detail::setFlag(response, 6);
                Detail::setFlag(response, 8);
            }
            else if(value != nullptr && response.focused() == false)
            {
                String synchronizedText;

                if(options.displayZeroAsEmpty == false || *value != T{})
                {
                    if(Detail::numericText(*value, options.precision, options.scientific, options.base, options.minimumDigits, &synchronizedText) == false)
                    {
                        synchronizedText = "?";
                    }
                }

                if(Detail::numericValueMatches(state, *value) == false || state.temporaryText != synchronizedText)
                {
                    state.temporaryText = synchronizedText;
                    Detail::storeNumericValue(state, *value);
                }
            }

            return response;
        }

        template<class T> requires std::is_arithmetic_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response numericVector(Context * ui, StringView label, Span<T> values, T step, T fastStep, const NumericInputOptions & options, const SourceLocation & location)
        {
            auto vectorScope = Mosaic::scope(ui, {}, location);
            ui->nodes[ui->currentParent].label.assign(label);
            LayoutOptions layout;
            layout.width = SizeRule::Fill;
            layout.gap = 4.f;
            uint32_t columns = static_cast<uint32_t>(std::clamp(values.size(), size_t{1}, size_t{4}));
            auto components = Mosaic::grid(ui, columns, layout, location);
            Response result;
            result.id = vectorScope.id();
            constexpr Array<StringView, 4> componentLabels = {"X", "Y", "Z", "W"};
            for(size_t index = 0; index != values.size(); ++index)
            {
                auto componentScope = Mosaic::scope(ui, Key(index), location);
                String generated;

                if(index < componentLabels.size())
                {
                    generated = componentLabels[index];
                }
                else if(Detail::toString(index, &generated) == false)
                {
                    generated = "?";
                }

                Response component = Detail::numericInput(ui, generated, &values[index], step, fastStep, options, location);
                result.flags |= component.flags;
            }

            return result;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] T numericOptionStep(double value) noexcept
        {
            if(std::isfinite(value) == false)
            {
                return T{};
            }

            if(value <= 0.0)
            {
                return T{};
            }

            double maximum = static_cast<double>(std::numeric_limits<T>::max());

            if(value >= maximum)
            {
                return std::numeric_limits<T>::max();
            }

            T returnedValue = static_cast<T>(value);

            return returnedValue;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response integralInputWithOptions(Context * ui, StringView label, T * value, const NumericInputOptions & options, const SourceLocation & location)
        {
            auto returnedValue = Detail::numericInput(ui, label, value, Detail::numericOptionStep<T>(options.step), Detail::numericOptionStep<T>(options.fastStep), options, location);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int8_t * value, int8_t step, int8_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint8_t * value, uint8_t step, uint8_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int16_t * value, int16_t step, int16_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint16_t * value, uint16_t step, uint16_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int32_t * value, int32_t step, int32_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint32_t * value, uint32_t step, uint32_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int64_t * value, int64_t step, int64_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint64_t * value, uint64_t step, uint64_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericInput(ui, label, value, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int8_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint8_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int16_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint16_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int32_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint32_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, int64_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputInt(Context * ui, StringView label, uint64_t * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::integralInputWithOptions(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputFloat(Context * ui, StringView label, float * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::numericInput(ui, label, value, static_cast<float>(options.step), static_cast<float>(options.fastStep), options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputDouble(Context * ui, StringView label, double * value, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::numericInput(ui, label, value, options.step, options.fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputFloatVector(Context * ui, StringView label, FloatSpan values, const NumericInputOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::numericVector(ui, label, values, static_cast<float>(options.step), static_cast<float>(options.fastStep), options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputIntVector(Context * ui, StringView label, Int32Span values, int32_t step, int32_t fastStep, const SourceLocation & location)
    {
        NumericInputOptions options;
        options.precision = 0;
        auto returnedValue = Detail::numericVector(ui, label, values, step, fastStep, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputFloat3(Context * ui, StringView label, FloatSpan values, const NumericInputOptions & options, const SourceLocation & location)
    {
        size_t count = std::min(values.size(), size_t{3});
        auto returnedValue = Mosaic::inputFloatVector(ui, label, FloatSpan(values.data(), count), options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
