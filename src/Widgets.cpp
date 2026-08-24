#include "Context.hpp"
#include "ContextDetail.hpp"
#include "Interaction.hpp"
#include "Popup.hpp"
#include "Render.hpp"
#include "TextEdit.hpp"
#include "Utility.hpp"
#include "Window.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <type_traits>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        bool capturedPointerActive(const Context * ui, const PointerState * pointer, Id id) noexcept
        {
            if(pointer == nullptr)
            {
                return false;
            }

            if(ui->captured != id)
            {
                return false;
            }

            if(pointer->isDown() == true)
            {
                return true;
            }

            if(pointer->isPressed() == true)
            {
                return true;
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        void validationTooltip(Context * ui, const Response & response, Validation validation, StringView message, const SourceLocation & location)
        {
            if(validation == Validation::Normal)
            {
                return;
            }

            if(message.empty() == true)
            {
                return;
            }

            if(Context::Node * node = ui->findFrameNode(response.id); node != nullptr)
            {
                ui->nodeSemanticDescription(*node).assign(message);
            }

            ItemTooltipOptions options;
            options.delay = 0.15f;
            options.navigationFocus = true;
            Mosaic::itemTooltip(ui, response, message, options, location);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Color hsvToRgb(float hue, float saturation, float value, float alpha = 1.f) noexcept
        {
            hue -= std::floor(hue);
            saturation = std::clamp(saturation, 0.f, 1.f);
            value = std::clamp(value, 0.f, 1.f);

            if(saturation <= 0.f)
            {
                return {value, value, value, alpha};
            }

            float scaled = hue * 6.f;
            int sector = static_cast<int>(std::floor(scaled));
            float fraction = scaled - static_cast<float>(sector);
            float first = value * (1.f - saturation);
            float second = value * (1.f - saturation * fraction);
            float third = value * (1.f - saturation * (1.f - fraction));
            switch(sector % 6)
            {
            case 0:
                return {value, third, first, alpha};
            case 1:
                return {second, value, first, alpha};
            case 2:
                return {first, value, third, alpha};
            case 3:
                return {first, second, value, alpha};
            case 4:
                return {third, first, value, alpha};
            default:
                return {value, first, second, alpha};
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void rgbToHsv(const Color & color, float & hue, float & saturation, float & value) noexcept
        {
            float minimum = std::min({color.r, color.g, color.b});
            float maximum = std::max({color.r, color.g, color.b});
            value = maximum;
            float difference = maximum - minimum;
            saturation = maximum <= 0.f ? 0.f : difference / maximum;

            if(difference <= 0.f)
            {
                hue = 0.f;

                return;
            }

            if(maximum == color.r)
            {
                hue = (color.g - color.b) / difference;
            }
            else if(maximum == color.g)
            {
                hue = 2.f + (color.b - color.r) / difference;
            }
            else
            {
                hue = 4.f + (color.r - color.g) / difference;
            }

            hue /= 6.f;

            if(hue < 0.f)
            {
                hue += 1.f;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void canvasGradient(Canvas & canvas, const Rect & bounds, const Color & topLeft, const Color & topRight, const Color & bottomRight, const Color & bottomLeft)
        {
            canvas.gradient(bounds, topLeft, topRight, bottomRight, bottomLeft);
        }
        //////////////////////////////////////////////////////////////////////////
        void canvasOutline(Canvas & canvas, const Rect & bounds, const Color & color, float thickness = 1.f)
        {
            canvas.line({bounds.x, bounds.y}, {bounds.right(), bounds.y}, thickness, color);
            canvas.line({bounds.right(), bounds.y}, {bounds.right(), bounds.bottom()}, thickness, color);
            canvas.line({bounds.right(), bounds.bottom()}, {bounds.x, bounds.bottom()}, thickness, color);
            canvas.line({bounds.x, bounds.bottom()}, {bounds.x, bounds.y}, thickness, color);
        }
        //////////////////////////////////////////////////////////////////////////
        void canvasCheckerboard(Canvas & canvas, const Rect & bounds, float side)
        {
            Color dark = Color::fromBytes(72, 77, 86);
            Color light = Color::fromBytes(137, 143, 154);
            for(float y = 0.f; y < bounds.height; y += side)
            {
                for(float x = 0.f; x < bounds.width; x += side)
                {
                    int column = static_cast<int>(x / side);
                    int row = static_cast<int>(y / side);
                    canvas.rect({bounds.x + x, bounds.y + y, std::min(side, bounds.width - x), std::min(side, bounds.height - y)}, ((column + row) & 1) == 0 ? light : dark);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void canvasPolylineOwned(Context * ui, Id canvas, Vec2Vector && points, float thickness, const Color & color, bool closed)
        {
            Context::Node * node = ui->findFrameNode(canvas);

            if(node == nullptr)
            {
                return;
            }

            if(node->kind != Detail::NodeKind::Canvas)
            {
                return;
            }

            DrawCommand command(DrawCommandType::Polyline);
            command.payload.polyline.points = std::move(points);
            command.payload.polyline.thickness = thickness;
            command.payload.polyline.color = color;
            command.payload.polyline.closed = closed;
            ui->canvasCommands(*node).emplace_back(std::move(command));
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool overlaps(const Rect & first, const Rect & second) noexcept
        {
            auto returnedValue = first.empty() == false && second.empty() == false && first.x <= second.right() && first.right() >= second.x && first.y <= second.bottom() && first.bottom() >= second.y;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool descendsFrom(const Context * ui, Id node, Id ancestor) noexcept
        {
            const Context::Node * current = ui->findFrameNode(node);
            while(current != nullptr)
            {
                if(current->id == ancestor)
                {
                    return true;
                }

                if(current->parent >= ui->nodes.size())
                {
                    return false;
                }

                current = &ui->nodes[current->parent];
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool numericText(StringView value) noexcept
        {
            for(char character : value)
            {
                bool accepted = character >= '0' && character <= '9';
                switch(character)
                {
                case '-':
                case '+':
                case '.':
                case 'e':
                case 'E':
                {
                    accepted = true;
                    break;
                }
                default:
                {
                    break;
                }
                }

                if(accepted == false)
                {
                    return false;
                }
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        void setFlag(Response & response, unsigned bit, bool value) noexcept
        {
            if(value == true)
            {
                response.flags |= 1U << bit;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool modifierMatches(const Modifiers & actual, const Modifiers & expected) noexcept
        {
            auto returnedValue = (expected.shift == false || actual.shift == true) && (expected.control == false || actual.control == true) && (expected.alt == false || actual.alt == true) && (expected.super == false || actual.super == true) && (expected.primary == false || actual.primary == true || actual.control == true || actual.super == true);

            return returnedValue;
        }

        using StringViewQuad = Array<StringView, 4>;
        using FloatPointerQuad = Array<float *, 4>;
        using ColorPalette = Array<Color, 10>;
        using HueColorArray = Array<Color, 7>;
        using ColorByteQuad = Array<uint8_t, 4>;
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] uint8_t colorByte(float value) noexcept
        {
            auto returnedValue = static_cast<uint8_t>(std::clamp(value, 0.f, 1.f) * 255.f + 0.5f);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] char hexDigit(uint8_t value) noexcept
        {
            auto returnedValue = value < 10 ? static_cast<char>('0' + value) : static_cast<char>('A' + value - 10);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] int hexValue(char value) noexcept
        {
            if(value >= '0' && value <= '9')
            {
                return value - '0';
            }

            if(value >= 'a' && value <= 'f')
            {
                return value - 'a' + 10;
            }

            if(value >= 'A' && value <= 'F')
            {
                return value - 'A' + 10;
            }

            return -1;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] String colorToHex(const Color & color, bool includeAlpha)
        {
            Detail::ColorByteQuad channels = {Detail::colorByte(color.r), Detail::colorByte(color.g), Detail::colorByte(color.b), Detail::colorByte(color.a)};
            String result;
            result.reserve(includeAlpha ? 9 : 7);
            result += '#';
            size_t count = includeAlpha ? 4 : 3;
            for(size_t index = 0; index != count; ++index)
            {
                result += Detail::hexDigit(static_cast<uint8_t>(channels[index] >> 4U));
                result += Detail::hexDigit(static_cast<uint8_t>(channels[index] & 0x0fU));
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool colorFromHex(StringView text, bool includeAlpha, Color * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(text.empty() == false && text.front() == '#')
            {
                text.remove_prefix(1);
            }

            size_t channelCount = includeAlpha ? 4 : 3;

            if(text.size() != channelCount * 2)
            {
                return false;
            }

            Detail::ColorByteQuad channels = {0, 0, 0, Detail::colorByte(_out->a)};
            for(size_t index = 0; index != channelCount; ++index)
            {
                int high = Detail::hexValue(text[index * 2]);
                int low = Detail::hexValue(text[index * 2 + 1]);

                if(high < 0)
                {
                    return false;
                }

                if(low < 0)
                {
                    return false;
                }

                channels[index] = static_cast<uint8_t>((high << 4) | low);
            }

            Color color = *_out;
            color.r = static_cast<float>(channels[0]) / 255.f;
            color.g = static_cast<float>(channels[1]) / 255.f;
            color.b = static_cast<float>(channels[2]) / 255.f;

            if(includeAlpha == true)
            {
                color.a = static_cast<float>(channels[3]) / 255.f;
            }

            *_out = color;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool colorByteFromText(StringView text, uint8_t * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            unsigned parsed = 0;
            auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);

            if(result.ec != std::errc{})
            {
                return false;
            }

            if(result.ptr != text.data() + text.size())
            {
                return false;
            }

            if(parsed > 255)
            {
                return false;
            }

            *_out = static_cast<uint8_t>(parsed);

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        void synchronizeColorText(ColorEditorState & state, const Color & color, bool includeAlpha)
        {
            state.hexText = Detail::colorToHex(color, includeAlpha);
            for(String & channel : state.channelText)
            {
                channel = "?";
            }

            (void)Detail::toString(Detail::colorByte(color.r), &state.channelText[0]);
            (void)Detail::toString(Detail::colorByte(color.g), &state.channelText[1]);
            (void)Detail::toString(Detail::colorByte(color.b), &state.channelText[2]);
            (void)Detail::toString(Detail::colorByte(color.a), &state.channelText[3]);
        }

        Response colorEditor(Context * ui, StringView label, Color * color, bool includeAlpha, const ColorEditOptions & options, const SourceLocation & location);
        Response colorPicker(Context * ui, StringView label, Color * color, bool includeAlpha, const ColorPickerOptions & options, const SourceLocation & location);
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] LayoutOptions fixedLayout(float width, float height) noexcept
        {
            LayoutOptions layout;
            layout.width = Dimension::fixed(width);
            layout.height = Dimension::fixed(height);

            return layout;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Vec2 rotatePoint(const Vec2 & point, float cosine, float sine) noexcept
        {
            return {point.x * cosine - point.y * sine, point.x * sine + point.y * cosine};
        }
        //////////////////////////////////////////////////////////////////////////
        void triangleBarycentric(const Vec2 & point, const Vec2 & first, const Vec2 & second, const Vec2 & third, float & firstWeight, float & secondWeight, float & thirdWeight) noexcept
        {
            Vec2 firstEdge = second - first;
            Vec2 secondEdge = third - first;
            Vec2 offset = point - first;
            float firstDot = firstEdge.x * firstEdge.x + firstEdge.y * firstEdge.y;
            float secondDot = firstEdge.x * secondEdge.x + firstEdge.y * secondEdge.y;
            float thirdDot = secondEdge.x * secondEdge.x + secondEdge.y * secondEdge.y;
            float offsetFirst = offset.x * firstEdge.x + offset.y * firstEdge.y;
            float offsetSecond = offset.x * secondEdge.x + offset.y * secondEdge.y;
            float denominator = firstDot * thirdDot - secondDot * secondDot;

            if(std::abs(denominator) <= 0.000001f)
            {
                firstWeight = 1.f;
                secondWeight = 0.f;
                thirdWeight = 0.f;

                return;
            }

            secondWeight = (thirdDot * offsetFirst - secondDot * offsetSecond) / denominator;
            thirdWeight = (firstDot * offsetSecond - secondDot * offsetFirst) / denominator;
            firstWeight = 1.f - secondWeight - thirdWeight;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool triangleContains(const Vec2 & point, const Vec2 & first, const Vec2 & second, const Vec2 & third) noexcept
        {
            float firstWeight = 0.f;
            float secondWeight = 0.f;
            float thirdWeight = 0.f;
            Detail::triangleBarycentric(point, first, second, third, firstWeight, secondWeight, thirdWeight);

            return firstWeight >= 0.f && secondWeight >= 0.f && thirdWeight >= 0.f;
        }
        //////////////////////////////////////////////////////////////////////////
        void contextMenuTrigger(Context * ui, Response & ownerResponse, size_t ownerNode, size_t hitNode)
        {
            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer != nullptr && pointer->isPressed(PointerButton::Secondary) == true && Detail::contextMenuPointerHit(ui, ui->nodes[hitNode], *pointer) == true)
            {
                Detail::openContextMenu(ui, ownerResponse, ownerNode, pointer->position);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] StringView trimValueText(StringView text) noexcept
        {
            while(text.empty() == false && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n'))
            {
                text.remove_prefix(1);
            }
            while(text.empty() == false && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n'))
            {
                text.remove_suffix(1);
            }

            return text;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseIntegralValue(StringView text, T * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            text = Detail::trimValueText(text);

            if(text.empty() == true)
            {
                return false;
            }

            if(text.front() == '+')
            {
                text.remove_prefix(1);

                if(text.empty() == true)
                {
                    return false;
                }
            }

            //////////////////////////////////////////////////////////////////////////
            if constexpr(std::is_signed_v<T>)
            {
                int64_t parsed = 0;
                auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);

                if(result.ec != std::errc{})
                {
                    return false;
                }

                if(result.ptr != text.data() + text.size())
                {
                    return false;
                }

                if(parsed < static_cast<int64_t>(std::numeric_limits<T>::min()))
                {
                    return false;
                }

                if(parsed > static_cast<int64_t>(std::numeric_limits<T>::max()))
                {
                    return false;
                }

                *_out = static_cast<T>(parsed);
            }
            else
            {
                uint64_t parsed = 0;
                auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);

                if(result.ec != std::errc{})
                {
                    return false;
                }

                if(result.ptr != text.data() + text.size())
                {
                    return false;
                }

                if(parsed > static_cast<uint64_t>(std::numeric_limits<T>::max()))
                {
                    return false;
                }

                *_out = static_cast<T>(parsed);
            }

            return true;
        }

        template<class T> requires std::is_floating_point_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseFloatingValue(StringView text, T * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            text = Detail::trimValueText(text);

            if(text.empty() == true)
            {
                return false;
            }

            if(text.front() == '+')
            {
                text.remove_prefix(1);

                if(text.empty() == true)
                {
                    return false;
                }
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
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseBooleanValue(StringView text, bool * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            text = Detail::trimValueText(text);

            if(text == "true")
            {

                *_out = true;

                return true;
            }

            if(text == "TRUE")
            {

                *_out = true;

                return true;
            }

            if(text == "True")
            {

                *_out = true;

                return true;
            }

            if(text == "1")
            {

                *_out = true;

                return true;
            }

            if(text == "false")
            {

                *_out = false;

                return true;
            }

            if(text == "FALSE")
            {

                *_out = false;

                return true;
            }

            if(text == "False")
            {

                *_out = false;

                return true;
            }

            if(text == "0")
            {

                *_out = false;

                return true;
            }

            return false;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        void numberContextMenu(Context * ui, size_t node, Response & response, T * value, T minimum, T maximum, const SourceLocation & location)
        {
            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer != nullptr && pointer->isPressed(PointerButton::Secondary) == true && Detail::contextMenuPointerHit(ui, ui->nodes[node], *pointer) == true)
            {
                Detail::openContextMenu(ui, response, node, pointer->position);
            }

            if(Detail::contextMenuOpen(ui, response.id) == false)
            {
                return;
            }

            T pasted = value == nullptr ? T{} : *value;
            String clipboard;
            bool hasClipboard = ui->platform->getClipboardText(&clipboard);
            bool canPaste = value != nullptr && hasClipboard == true && Detail::parseIntegralValue(clipboard, &pasted);
            String currentValue;
            bool canCopy = value != nullptr && Detail::toString(*value, &currentValue) == true;
            Detail::ValueContextAction action = Detail::valueContextMenu(ui, response.id, currentValue, canCopy, canPaste, location);

            if(action == Detail::ValueContextAction::Copy)
            {
                ui->platform->setClipboardText(currentValue);
            }
            else if(action == Detail::ValueContextAction::Paste)
            {
                T updated = maximum >= minimum ? std::clamp(pasted, minimum, maximum) : pasted;

                if(updated != *value)
                {
                    *value = updated;
                    Detail::setFlag(response, 6);
                    Detail::setFlag(response, 8);
                    ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                    ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                }
            }
        }

        template<class T> requires std::is_floating_point_v<T>
        //////////////////////////////////////////////////////////////////////////
        void numberContextMenu(Context * ui, size_t node, Response & response, T * value, T minimum, T maximum, int precision, const SourceLocation & location)
        {
            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer != nullptr && pointer->isPressed(PointerButton::Secondary) == true && Detail::contextMenuPointerHit(ui, ui->nodes[node], *pointer) == true)
            {
                Detail::openContextMenu(ui, response, node, pointer->position);
            }

            if(Detail::contextMenuOpen(ui, response.id) == false)
            {
                return;
            }

            T pasted = value == nullptr ? T{} : *value;
            String clipboard;
            bool hasClipboard = ui->platform->getClipboardText(&clipboard);
            bool canPaste = value != nullptr && hasClipboard == true && Detail::parseFloatingValue(clipboard, &pasted);
            String currentValue;
            bool canCopy = value != nullptr && Detail::formatFloating(*value, precision, {}, &currentValue) == true;
            Detail::ValueContextAction action = Detail::valueContextMenu(ui, response.id, currentValue, canCopy, canPaste, location);

            if(action == Detail::ValueContextAction::Copy)
            {
                ui->platform->setClipboardText(currentValue);
            }
            else if(action == Detail::ValueContextAction::Paste)
            {
                T updated = maximum >= minimum ? std::clamp(pasted, minimum, maximum) : pasted;

                if(updated != *value)
                {
                    *value = updated;
                    Detail::setFlag(response, 6);
                    Detail::setFlag(response, 8);
                    ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                    ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void booleanContextMenu(Context * ui, size_t node, Response & response, bool * value, const SourceLocation & location)
        {
            const PointerState * pointer = ui->input.primaryPointer();

            if(pointer != nullptr && pointer->isPressed(PointerButton::Secondary) == true && Detail::contextMenuPointerHit(ui, ui->nodes[node], *pointer) == true)
            {
                Detail::openContextMenu(ui, response, node, pointer->position);
            }

            if(Detail::contextMenuOpen(ui, response.id) == false)
            {
                return;
            }

            bool pasted = value != nullptr && *value;
            String clipboard;
            bool hasClipboard = ui->platform->getClipboardText(&clipboard);
            bool canPaste = value != nullptr && hasClipboard == true && Detail::parseBooleanValue(clipboard, &pasted);
            StringView currentValue = value == nullptr ? StringView{} : (*value ? StringView("true") : StringView("false"));
            Detail::ValueContextAction action = Detail::valueContextMenu(ui, response.id, currentValue, value != nullptr, canPaste, location);

            if(action == Detail::ValueContextAction::Copy)
            {
                ui->platform->setClipboardText(*value ? StringView("true") : StringView("false"));
            }
            else if(action == Detail::ValueContextAction::Paste && pasted != *value)
            {
                *value = pasted;
                Detail::setFlag(response, 6);
                Detail::setFlag(response, 8);
                ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool colorContextMenu(Context * ui, size_t node, Response & response, Color * color, bool includeAlpha, const SourceLocation & location)
        {
            Detail::contextMenuTrigger(ui, response, node, node);

            if(Detail::contextMenuOpen(ui, response.id) == false)
            {
                return false;
            }

            Color pasted = color == nullptr ? Color{} : *color;
            String clipboard;
            bool hasClipboard = ui->platform->getClipboardText(&clipboard);
            bool canPaste = color != nullptr && hasClipboard == true && Detail::colorFromHex(Detail::trimValueText(clipboard), includeAlpha, &pasted);
            String currentValue = color == nullptr ? String{} : Detail::colorToHex(*color, includeAlpha);
            Detail::ValueContextAction action = Detail::valueContextMenu(ui, response.id, currentValue, color != nullptr, canPaste, location);
            bool changed = false;

            if(action == Detail::ValueContextAction::Copy)
            {
                ui->platform->setClipboardText(Detail::colorToHex(*color, includeAlpha));
            }
            else if(action == Detail::ValueContextAction::Paste && pasted != *color)
            {
                *color = pasted;
                changed = true;
                Detail::setFlag(response, 6);
                Detail::setFlag(response, 8);
            }

            return changed;
        }

        template<class T> requires std::is_arithmetic_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool parseNumber(StringView text, T * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(text.empty() == true)
            {
                return false;
            }

            const char * begin = text.data();
            const char * end = begin + text.size();
            std::from_chars_result result;
            T output{};

            //////////////////////////////////////////////////////////////////////////
            if constexpr(std::is_floating_point_v<T>)
            {
                result = std::from_chars(begin, end, output, std::chars_format::general);
            }
            else
            {
                result = std::from_chars(begin, end, output);
            }

            if(result.ec != std::errc{})
            {
                return false;
            }

            if(result.ptr != end)
            {
                return false;
            }

            *_out = output;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool dragAdjustment(const Context::Node & node, Context::Persistent & state, Detail::NumericState & numericState, const PointerState & pointer, float * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(numericState.dragThresholdPassed == false)
            {
                float distance = pointer.position.x - state.dragStartPosition.x;
                float threshold = std::max(0.f, node.style->behavior.dragThreshold);

                if(std::abs(distance) <= threshold)
                {
                    return false;
                }

                numericState.dragThresholdPassed = true;
                numericState.temporaryDoubleClickBlocked = true;
                float adjustment = distance - std::copysign(threshold, distance);
                state.dragLastPosition = pointer.position;

                if(adjustment == 0.f)
                {
                    return false;
                }

                *_out = adjustment;

                return true;
            }

            float adjustment = pointer.position.x - state.dragLastPosition.x;
            state.dragLastPosition = pointer.position;

            if(adjustment == 0.f)
            {
                return false;
            }

            *_out = adjustment;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] long double automaticDragSpeed(const Context::Node & node, const SliderOptions & options, long double minimum, long double maximum, long double valueStep) noexcept
        {
            if(options.dragSpeed > 0.0)
            {
                auto returnedValue = static_cast<long double>(options.dragSpeed);

                return returnedValue;
            }

            long double range = maximum - minimum;
            long double rangeSpeed = range > 0.L ? range * static_cast<long double>(std::max(0.f, node.style->behavior.dragSpeedDefaultRatio)) : 0.L;
            long double stepSpeed = valueStep * static_cast<long double>(std::max(0.f, node.style->behavior.dragSpeedMinimumStepRatio));
            auto returnedValue = std::max(rangeSpeed, stepSpeed);

            return returnedValue;
        }

        template<class T> requires std::is_arithmetic_v<T>
        //////////////////////////////////////////////////////////////////////////
        void temporaryNumberInput(Context * ui, size_t node, Response & response, T * value, T minimum, T maximum, StringView formattedValue, bool clampInput, bool clampZeroRange)
        {
            Context::Node & inputNode = ui->nodes[node];
            Context::Persistent & state = ui->state(inputNode);
            Detail::NumericState & numericState = ui->numericState(state);
            const PointerState * pointer = ui->input.primaryPointer();

            if(response.pressed() == true && pointer != nullptr && pointer->buttonClickCount() <= 1)
            {
                numericState.temporaryDoubleClickBlocked = false;
            }

            bool primaryModifier = ui->input.modifiers.primary || ui->input.modifiers.control == true || ui->input.modifiers.super;
            bool modifierClickToInput = response.pressed() && primaryModifier;
            bool plainClickToInput = ui->configuration.dragClickToInputText && response.clicked() && primaryModifier == false && numericState.dragThresholdPassed == false;
            bool beginTemporaryInput = response.doubleClicked();

            if(modifierClickToInput == true)
            {
                beginTemporaryInput = true;
            }

            if(plainClickToInput == true)
            {
                beginTemporaryInput = true;
            }

            if(numericState.temporaryDoubleClickBlocked == true)
            {
                beginTemporaryInput = false;
            }

            if(value == nullptr)
            {
                beginTemporaryInput = false;
            }

            if(beginTemporaryInput == true)
            {
                numericState.temporaryInput = true;
                numericState.temporaryReplace = true;
                numericState.temporaryText.assign(formattedValue);
                state.editing = true;
                ui->captured = InvalidId;
                ui->capturedPointer = 0;
                ui->active = InvalidId;
            }

            if(numericState.temporaryInput == false)
            {
                return;
            }

            bool commit = false;
            bool cancel = false;
            for(const String & text : ui->input.text)
            {
                if(Detail::numericText(text) == false)
                {
                    continue;
                }

                if(numericState.temporaryReplace == true)
                {
                    numericState.temporaryText.clear();
                    numericState.temporaryReplace = false;
                }

                numericState.temporaryText += text;
            }
            for(const KeyEvent & event : ui->input.keyboard)
            {
                if(event.pressed == false)
                {
                    continue;
                }

                bool primary = event.modifiers.primary || event.modifiers.control == true || event.modifiers.super;

                if(primary == true && event.key == KeyCode::A)
                {
                    numericState.temporaryReplace = true;
                }
                else if(primary == true && event.key == KeyCode::C)
                {
                    ui->platform->setClipboardText(numericState.temporaryText);
                }
                else if(primary == true && event.key == KeyCode::X)
                {
                    ui->platform->setClipboardText(numericState.temporaryText);
                    numericState.temporaryText.clear();
                    numericState.temporaryReplace = false;
                }
                else if(primary == true && event.key == KeyCode::V)
                {
                    String clipboard;
                    if(ui->platform->getClipboardText(&clipboard) == true && Detail::numericText(clipboard) == true)
                    {
                        if(numericState.temporaryReplace == true)
                        {
                            numericState.temporaryText.clear();
                        }

                        numericState.temporaryText += clipboard;
                        numericState.temporaryReplace = false;
                    }
                }
                else if(event.key == KeyCode::Backspace)
                {
                    if(numericState.temporaryReplace == true)
                    {
                        numericState.temporaryText.clear();
                        numericState.temporaryReplace = false;
                    }
                    else if(numericState.temporaryText.empty() == false)
                    {
                        numericState.temporaryText.erase(Detail::previousUtf8(numericState.temporaryText, numericState.temporaryText.size()));
                    }
                }
                else if(event.key == KeyCode::Enter)
                {
                    commit = true;
                }
                else if(event.key == KeyCode::Escape)
                {
                    cancel = true;
                }
            }

            if(ui->focused != response.id)
            {
                commit = true;
            }

            if(commit == true && value != nullptr)
            {
            T parsed{};
            if(Detail::parseNumber<T>(numericState.temporaryText, &parsed) == true)
                {
                    if(clampInput == true && (maximum > minimum || (clampZeroRange == true && maximum == minimum)))
                    {
                        parsed = std::clamp(parsed, minimum, maximum);
                    }

                    if(parsed != *value)
                    {
                        *value = parsed;
                        Detail::setFlag(response, 6);
                        ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(inputNode), inputNode.file, inputNode.line, ui->input.timestamp});
                    }

                    Detail::setFlag(response, 8);
                    numericState.temporaryInput = false;
                    state.editing = false;
                }
            }

            if(cancel == true)
            {
                numericState.temporaryInput = false;
                state.editing = false;
                Detail::setFlag(response, 9);
            }

            if(numericState.temporaryInput == true)
            {
                inputNode.numericInput = true;
                ui->nodeSemanticValue(inputNode) = numericState.temporaryText;
                ui->prepareValueText(inputNode, numericState.temporaryText);
                Detail::setFlag(response, 7);
            }
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] std::make_unsigned_t<T> integralOrdinal(T value) noexcept
        {
            using Unsigned = std::make_unsigned_t<T>;

            //////////////////////////////////////////////////////////////////////////
            if constexpr(std::is_signed_v<T>)
            {
                constexpr Unsigned signBit = Unsigned{1} << (std::numeric_limits<Unsigned>::digits - 1);
                auto returnedValue = static_cast<Unsigned>(value) ^ signBit;

                return returnedValue;
            }
            else
            {
                return value;
            }
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] T integralValue(std::make_unsigned_t<T> ordinal) noexcept
        {
            using Unsigned = std::make_unsigned_t<T>;

            //////////////////////////////////////////////////////////////////////////
            if constexpr(std::is_signed_v<T>)
            {
                constexpr Unsigned signBit = Unsigned{1} << (std::numeric_limits<Unsigned>::digits - 1);
                Unsigned moduloValue = ordinal ^ signBit;

                if(moduloValue <= static_cast<Unsigned>(std::numeric_limits<T>::max()))
                {
                    auto returnedValue = static_cast<T>(moduloValue);

                    return returnedValue;
                }

                Unsigned magnitude = static_cast<Unsigned>(~moduloValue) + Unsigned{1};

                if(magnitude == signBit)
                {
                    auto returnedValue = std::numeric_limits<T>::min();

                    return returnedValue;
                }

                auto returnedValue = static_cast<T>(-static_cast<T>(magnitude));

                return returnedValue;
            }
            else
            {
                return ordinal;
            }
        }

        template<class T> requires std::is_unsigned_v<T>
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] T scaleIntegralRange(T range, uint32_t numerator, uint32_t denominator) noexcept
        {
            uint64_t rangeValue = static_cast<uint64_t>(range);
            uint64_t denominatorValue = denominator;
            uint64_t numeratorValue = numerator;
            uint64_t quotientPart = (rangeValue / denominatorValue) * numeratorValue;
            uint64_t remainderProduct = (rangeValue % denominatorValue) * numeratorValue;
            uint64_t remainderPart = (remainderProduct + denominatorValue / 2U) / denominatorValue;
            uint64_t offset = remainderPart > rangeValue - quotientPart ? rangeValue : quotientPart + remainderPart;
            auto returnedValue = static_cast<T>(offset);

            return returnedValue;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response sliderIntegralBehavior(Context * ui, StringView label, T * value, T minimum, T maximum, const SliderOptions & options, const SourceLocation & location)
        {
            using Unsigned = std::make_unsigned_t<T>;
            constexpr uint32_t ratioScale = 1U << 24U;

            size_t node = ui->addNode(Detail::NodeKind::Slider, {}, label, {}, location, SemanticRole::Slider, true);
            ui->nodes[node].labelPlacement = options.labelPlacement;
            ui->nodes[node].showValuePopup = false;
            ui->nodes[node].showValueOnTrack = true;
            ui->nodes[node].showValueTooltip = false;
            ui->nodes[node].tooltipDelay = 0.25f;
            Context::Persistent & persistentState = ui->state(ui->nodes[node]);
            Detail::NumericState & numericState = ui->numericState(persistentState);
            Detail::SliderGeometry interactionGeometry = Detail::sliderGeometry(ui->nodes[node], persistentState.lastBounds);
            Response response = ui->interact(node, false, &interactionGeometry.control);
            const PointerState * pointer = ui->input.primaryPointer();
            Unsigned minimumOrdinal = Detail::integralOrdinal(minimum);
            Unsigned maximumOrdinal = Detail::integralOrdinal(maximum);
            bool descending = maximumOrdinal < minimumOrdinal;
            Unsigned range = descending ? minimumOrdinal - maximumOrdinal : maximumOrdinal - minimumOrdinal;
            String formattedValue = "?";
            (void)Detail::formatIntegral(value == nullptr ? T{} : *value, options.format, &formattedValue);

            if(options.temporaryInput == true)
            {
                Detail::temporaryNumberInput(ui, node, response, value, std::min(minimum, maximum), std::max(minimum, maximum), formattedValue, options.clampInput, options.clampZeroRange);
            }

            if(response.pressed() == true && numericState.temporaryInput == false)
            {
                persistentState.editing = true;

                if(value != nullptr && pointer != nullptr && range != 0)
                {
                    Detail::SliderGeometry geometry = Detail::sliderGeometry(ui->nodes[node], persistentState.lastBounds);
                    Unsigned valueOrdinal = Detail::integralOrdinal(*value);
                    Unsigned distance = descending ? (valueOrdinal >= minimumOrdinal ? Unsigned{0} : static_cast<Unsigned>(minimumOrdinal - valueOrdinal)) : (valueOrdinal <= minimumOrdinal ? Unsigned{0} : static_cast<Unsigned>(valueOrdinal - minimumOrdinal));
                    float scalar = static_cast<float>(std::min(distance, range)) / static_cast<float>(range);
                    float center = geometry.track.x + geometry.track.width * scalar;
                    float radius = geometry.maximumGrabSide * 0.5f;
                    numericState.sliderGrabOffset = std::abs(pointer->position.x - center) <= radius ? pointer->position.x - center : 0.f;
                }

                Detail::setFlag(response, 7);
                ui->frame.events.push_back({EventType::BeginEdit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            bool updateSlider = numericState.temporaryInput == false;

            if(value == nullptr)
            {
                updateSlider = false;
            }

            if(Detail::capturedPointerActive(ui, pointer, response.id) == false)
            {
                updateSlider = false;
            }

            if(persistentState.lastBounds.width <= 0.f)
            {
                updateSlider = false;
            }

            if(range == 0)
            {
                updateSlider = false;
            }

            if(updateSlider == true)
            {
                Detail::SliderGeometry geometry = Detail::sliderGeometry(ui->nodes[node], persistentState.lastBounds);
                float ratio = geometry.track.width <= 0.f ? 0.f : std::clamp((pointer->position.x - numericState.sliderGrabOffset - geometry.track.x) / geometry.track.width, 0.f, 1.f);
                uint32_t numerator = static_cast<uint32_t>(ratio * static_cast<float>(ratioScale) + 0.5f);
                Unsigned offset = Detail::scaleIntegralRange(range, numerator, ratioScale);
                T updated = Detail::integralValue<T>(descending ? minimumOrdinal - offset : minimumOrdinal + offset);

                if(updated != *value)
                {
                    *value = updated;
                    Detail::setFlag(response, 6);
                    Detail::setFlag(response, 7);
                    ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                }
            }

            if(response.released() == true && persistentState.editing == true && numericState.temporaryInput == false)
            {
                persistentState.editing = false;
                Detail::setFlag(response, 8);
                ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            Detail::numberContextMenu(ui, node, response, value, minimum, maximum, location);

            Unsigned valueOrdinal = value == nullptr ? minimumOrdinal : Detail::integralOrdinal(*value);
            Unsigned distance = descending ? (valueOrdinal >= minimumOrdinal ? Unsigned{0} : static_cast<Unsigned>(minimumOrdinal - valueOrdinal)) : (valueOrdinal <= minimumOrdinal ? Unsigned{0} : static_cast<Unsigned>(valueOrdinal - minimumOrdinal));
            Unsigned valueOffset = std::min(distance, range);
            float scalar = range == 0 ? 0.f : static_cast<float>(valueOffset) / static_cast<float>(range);
            ui->nodes[node].scalar = std::clamp(scalar, 0.f, 1.f);
            ui->nodeSemanticValue(ui->nodes[node]) = formattedValue;

            if(numericState.temporaryInput == false)
            {
                ui->prepareValueText(ui->nodes[node], formattedValue);
            }

            ui->nodes[node].response = response;

            return response;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response sliderIntegral(Context * ui, StringView label, T * value, T minimum, T maximum, LabelPlacement labelPlacement, StringView format, const SourceLocation & location)
        {
            SliderOptions options;
            options.labelPlacement = labelPlacement;
            options.format = format;
            auto returnedValue = Detail::sliderIntegralBehavior(ui, label, value, minimum, maximum, options, location);

            return returnedValue;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response sliderIntegral(Context * ui, StringView label, T * value, T minimum, T maximum, const SliderOptions & options, const SourceLocation & location)
        {
            Response response = Detail::sliderIntegralBehavior(ui, label, value, minimum, maximum, options, location);
            for(auto node = ui->nodes.rbegin(); node != ui->nodes.rend(); ++node)
            {
                if(node->id != response.id)
                {
                    continue;
                }

                node->showValuePopup = options.showValuePopup;
                node->showValueOnTrack = options.showValueOnTrack;
                node->showValueTooltip = options.showValueTooltip;
                node->tooltipDelay = std::max(0.f, options.valueTooltipDelay);
                node->layout.width = options.width;
                break;
            }
            Detail::validationTooltip(ui, response, options.validation, options.validationMessage, location);

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] long double floatingSliderRatio(long double value, long double minimum, long double maximum, bool logarithmic, long double zeroDeadzoneHalfSize = 0.L) noexcept
        {
            long double range = maximum - minimum;

            if(logarithmic == false)
            {
                auto returnedValue = range <= 0.L ? 0.L : (value - minimum) / range;

                return returnedValue;
            }

            if(range <= 0.L)
            {
                auto returnedValue = range <= 0.L ? 0.L : (value - minimum) / range;

                return returnedValue;
            }

            if(minimum > 0.L)
            {
                auto returnedValue = std::log(std::max(value, minimum) / minimum) / std::log(maximum / minimum);

                return returnedValue;
            }

            if(maximum < 0.L)
            {
                auto returnedValue = std::log(std::abs(minimum) / std::max(std::abs(value), std::abs(maximum))) / std::log(std::abs(minimum) / std::abs(maximum));

                return returnedValue;
            }

            long double negativeExtent = std::abs(minimum);
            long double positiveExtent = std::abs(maximum);
            long double zeroRatio = negativeExtent / std::max(negativeExtent + positiveExtent, std::numeric_limits<long double>::epsilon());
            long double deadzoneHalf = std::clamp(zeroDeadzoneHalfSize, 0.L, std::min(zeroRatio, 1.L - zeroRatio));
            long double negativeEnd = zeroRatio - deadzoneHalf;
            long double positiveStart = zeroRatio + deadzoneHalf;

            if(value < 0.L && negativeExtent > 0.L)
            {
                long double normalized = std::log1p(std::abs(value) * 9.L / negativeExtent) / std::log(10.L);
                auto returnedValue = negativeEnd * (1.L - normalized);

                return returnedValue;
            }

            if(value > 0.L && positiveExtent > 0.L)
            {
                long double normalized = std::log1p(value * 9.L / positiveExtent) / std::log(10.L);
                auto returnedValue = positiveStart + (1.L - positiveStart) * normalized;

                return returnedValue;
            }

            return zeroRatio;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] long double floatingSliderValue(long double ratio, long double minimum, long double maximum, bool logarithmic, long double zeroDeadzoneHalfSize = 0.L) noexcept
        {
            long double range = maximum - minimum;

            if(logarithmic == false)
            {
                return minimum + ratio * range;
            }

            if(range <= 0.L)
            {
                return minimum + ratio * range;
            }

            if(minimum > 0.L)
            {
                auto returnedValue = minimum * std::pow(maximum / minimum, ratio);

                return returnedValue;
            }

            if(maximum < 0.L)
            {
                auto returnedValue = -std::abs(minimum) * std::pow(std::abs(maximum) / std::abs(minimum), ratio);

                return returnedValue;
            }

            long double negativeExtent = std::abs(minimum);
            long double positiveExtent = std::abs(maximum);
            long double zeroRatio = negativeExtent / std::max(negativeExtent + positiveExtent, std::numeric_limits<long double>::epsilon());
            long double deadzoneHalf = std::clamp(zeroDeadzoneHalfSize, 0.L, std::min(zeroRatio, 1.L - zeroRatio));
            long double negativeEnd = zeroRatio - deadzoneHalf;
            long double positiveStart = zeroRatio + deadzoneHalf;

            if(ratio < negativeEnd && negativeEnd > 0.L)
            {
                long double normalized = 1.L - ratio / negativeEnd;
                auto returnedValue = -negativeExtent * std::expm1(normalized * std::log(10.L)) / 9.L;

                return returnedValue;
            }

            if(ratio > positiveStart && positiveStart < 1.L)
            {
                long double normalized = (ratio - positiveStart) / (1.L - positiveStart);
                auto returnedValue = positiveExtent * std::expm1(normalized * std::log(10.L)) / 9.L;

                return returnedValue;
            }

            return 0.L;
        }

        template<class T> requires std::is_floating_point_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response sliderFloating(Context * ui, StringView label, T * value, T minimum, T maximum, T step, int precision, bool showValuePopup, bool showValueOnTrack, bool showValueTooltip, float tooltipDelay, bool logarithmic, bool temporaryInput, bool clampInput, bool clampZeroRange, bool roundToFormat, Validation validation, LabelPlacement labelPlacement, StringView format, const SourceLocation & location)
        {
            size_t node = ui->addNode(Detail::NodeKind::Slider, {}, label, {}, location, SemanticRole::Slider, true);
            ui->nodes[node].labelPlacement = labelPlacement;
            ui->nodes[node].showValuePopup = showValuePopup;
            ui->nodes[node].showValueOnTrack = showValueOnTrack;
            ui->nodes[node].showValueTooltip = showValueTooltip;
            ui->nodes[node].tooltipDelay = std::max(0.f, tooltipDelay);
            Context::Persistent & persistentState = ui->state(ui->nodes[node]);
            Detail::NumericState & numericState = ui->numericState(persistentState);
            Detail::SliderGeometry interactionGeometry = Detail::sliderGeometry(ui->nodes[node], persistentState.lastBounds);
            Response response = ui->interact(node, false, &interactionGeometry.control);
            const PointerState * pointer = ui->input.primaryPointer();
            long double minimumValue = static_cast<long double>(minimum);
            long double maximumValue = static_cast<long double>(maximum);
            long double stepValue = static_cast<long double>(step);
            long double range = maximumValue - minimumValue;
            long double logarithmicZeroDeadzone = logarithmic && minimumValue < 0.L && maximumValue > 0.L && interactionGeometry.track.width > 0.f ? static_cast<long double>(std::max(0.f, ui->nodes[node].style->metrics.logarithmicSliderDeadzone)) * 0.5L / static_cast<long double>(interactionGeometry.track.width) : 0.L;
            ui->nodes[node].validation = validation;
            String formattedValue = "?";
            (void)Detail::formatFloating(value == nullptr ? T{} : *value, precision, format, &formattedValue);

            if(temporaryInput == true)
            {
                Detail::temporaryNumberInput(ui, node, response, value, minimum, maximum, formattedValue, clampInput, clampZeroRange);
            }

            if(response.pressed() == true && numericState.temporaryInput == false)
            {
                persistentState.editing = true;

                if(value != nullptr && pointer != nullptr && range > 0.L)
                {
                    Detail::SliderGeometry geometry = Detail::sliderGeometry(ui->nodes[node], persistentState.lastBounds);
                    long double scalar = Detail::floatingSliderRatio(static_cast<long double>(*value), minimumValue, maximumValue, logarithmic, logarithmicZeroDeadzone);
                    float center = geometry.track.x + geometry.track.width * static_cast<float>(std::clamp(scalar, 0.L, 1.L));
                    float radius = geometry.maximumGrabSide * 0.5f;
                    numericState.sliderGrabOffset = std::abs(pointer->position.x - center) <= radius ? pointer->position.x - center : 0.f;
                }

                Detail::setFlag(response, 7);
                ui->frame.events.push_back({EventType::BeginEdit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            bool updateSlider = numericState.temporaryInput == false;

            if(value == nullptr)
            {
                updateSlider = false;
            }

            if(Detail::capturedPointerActive(ui, pointer, response.id) == false)
            {
                updateSlider = false;
            }

            if(persistentState.lastBounds.width <= 0.f)
            {
                updateSlider = false;
            }

            if(range <= 0.L)
            {
                updateSlider = false;
            }

            if(updateSlider == true)
            {
                Detail::SliderGeometry geometry = Detail::sliderGeometry(ui->nodes[node], persistentState.lastBounds);
                long double ratio = std::clamp(geometry.track.width <= 0.f ? 0.L : static_cast<long double>((pointer->position.x - numericState.sliderGrabOffset - geometry.track.x) / geometry.track.width), 0.L, 1.L);
                long double calculated = Detail::floatingSliderValue(ratio, minimumValue, maximumValue, logarithmic, logarithmicZeroDeadzone);

                if(stepValue > 0.L)
                {
                    calculated = minimumValue + std::round((calculated - minimumValue) / stepValue) * stepValue;
                }
                else if(roundToFormat == true && precision >= 0)
                {
                    long double scale = std::pow(10.L, static_cast<long double>(std::clamp(precision, 0, 8)));
                    calculated = std::round(calculated * scale) / scale;
                }

                T updated = static_cast<T>(std::clamp(calculated, minimumValue, maximumValue));

                if(updated != *value)
                {
                    *value = updated;
                    Detail::setFlag(response, 6);
                    Detail::setFlag(response, 7);
                    ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                }
            }

            if(response.released() == true && persistentState.editing == true && numericState.temporaryInput == false)
            {
                persistentState.editing = false;
                Detail::setFlag(response, 8);
                ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            Detail::numberContextMenu(ui, node, response, value, minimum, maximum, precision, location);

            long double scalar = value == nullptr || range <= 0.L ? 0.L : Detail::floatingSliderRatio(static_cast<long double>(*value), minimumValue, maximumValue, logarithmic, logarithmicZeroDeadzone);
            ui->nodes[node].scalar = static_cast<float>(std::clamp(scalar, 0.L, 1.L));
            ui->nodeSemanticValue(ui->nodes[node]) = formattedValue;

            if(numericState.temporaryInput == false)
            {
                ui->prepareValueText(ui->nodes[node], formattedValue);
            }

            ui->nodes[node].response = response;

            return response;
        }

        template<class T> requires std::is_floating_point_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response dragValueFloating(Context * ui, StringView label, T * value, const SliderOptions & options, const SourceLocation & location)
        {
            String formattedValue = "?";
            (void)Detail::formatFloating(value == nullptr ? T{} : *value, options.precision, options.format, &formattedValue);
            String visible;

            if(options.labelPlacement != LabelPlacement::Hidden)
            {
                visible.assign(label);

                if(visible.empty() == false)
                {
                    visible += "  ";
                }
            }

            visible += formattedValue;

            size_t node = ui->addNode(Detail::NodeKind::DragValue, {}, label, {}, location, SemanticRole::Slider, true);
            ui->nodes[node].label = visible;
            ui->mutableStyle(ui->nodes[node]).metrics.font = MonospaceFont;
            ui->estimateNodeText(ui->nodes[node], visible);
            Response response = ui->interact(node, false);
            Context::Persistent & persistentState = ui->state(ui->nodes[node]);
            Detail::NumericState & numericState = ui->numericState(persistentState);
            const PointerState * pointer = ui->input.primaryPointer();
            ui->nodes[node].validation = options.validation;
            ui->nodeSemanticValue(ui->nodes[node]) = formattedValue;

            if(options.temporaryInput == true)
            {
                Detail::temporaryNumberInput(ui, node, response, value, static_cast<T>(options.minimum), static_cast<T>(options.maximum), formattedValue, options.clampInput, options.clampZeroRange);
            }

            if(response.pressed() == true && numericState.temporaryInput == false)
            {
                persistentState.editing = true;

                if(value != nullptr && pointer != nullptr)
                {
                    long double minimum = static_cast<long double>(options.minimum);
                    long double maximum = static_cast<long double>(options.maximum);
                    long double valueStep = options.step > 0.0 ? static_cast<long double>(options.step) : std::pow(10.L, -static_cast<long double>(std::clamp(options.precision, 0, 8)));
                    long double speed = Detail::automaticDragSpeed(ui->nodes[node], options, minimum, maximum, valueStep);
                    persistentState.dragStartPosition = pointer->position;
                    persistentState.dragLastPosition = pointer->position;
                    numericState.dragStartValue = static_cast<long double>(*value);
                    numericState.dragValueSpeed = speed;
                    numericState.dragAccumulator = 0.L;
                    numericState.dragLastApplied = static_cast<long double>(*value);
                    numericState.dragThresholdPassed = false;
                }

                Detail::setFlag(response, 7);
                ui->frame.events.push_back({EventType::BeginEdit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            if(numericState.temporaryInput == false && value != nullptr && pointer != nullptr && ui->captured == response.id && pointer->isDown() == true)
            {
                long double minimum = static_cast<long double>(options.minimum);
                long double maximum = static_cast<long double>(options.maximum);
                long double range = maximum - minimum;
                long double speed = numericState.dragValueSpeed;

                if(options.speedTweaks == true && ui->input.modifiers.shift == true)
                {
                    speed *= 10.L;
                }

                if(options.speedTweaks == true && ui->input.modifiers.alt == true)
                {
                    speed *= 0.01L;
                }

                float adjustment = 0.f;
                if(Detail::dragAdjustment(ui->nodes[node], persistentState, numericState, *pointer, &adjustment) == false)
                {
                    ui->nodes[node].response = response;

                    return response;
                }

                long double current = static_cast<long double>(*value);

                if(current != numericState.dragLastApplied)
                {
                    numericState.dragAccumulator = 0.L;
                    numericState.dragLastApplied = current;
                }

                numericState.dragAccumulator += static_cast<long double>(adjustment) * speed;
                long double previous = static_cast<long double>(*value);
                long double calculated = previous + numericState.dragAccumulator;

                if(options.step > 0.0)
                {
                    long double step = static_cast<long double>(options.step);
                    calculated = std::round(calculated / step) * step;
                }
                else if(options.roundToFormat == true && options.precision >= 0)
                {
                    long double scale = std::pow(10.L, static_cast<long double>(std::clamp(options.precision, 0, 8)));
                    calculated = std::round(calculated * scale) / scale;
                }

                if(range > 0.L)
                {
                    calculated = std::clamp(calculated, minimum, maximum);
                }

                T updated = static_cast<T>(calculated);

                if(updated != *value)
                {
                    *value = updated;
                    numericState.dragAccumulator -= static_cast<long double>(updated) - previous;
                    numericState.dragLastApplied = static_cast<long double>(updated);
                    Detail::setFlag(response, 6);
                    Detail::setFlag(response, 7);
                    ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                }
                else
                {
                    bool clearAccumulator = range > 0.L;
                    bool clampedAtMinimum = calculated <= minimum;

                    if(numericState.dragAccumulator >= 0.L)
                    {
                        clampedAtMinimum = false;
                    }

                    bool clampedAtMaximum = calculated >= maximum;

                    if(numericState.dragAccumulator <= 0.L)
                    {
                        clampedAtMaximum = false;
                    }

                    if(clampedAtMinimum == false)
                    {
                        if(clampedAtMaximum == false)
                        {
                            clearAccumulator = false;
                        }
                    }

                    if(clearAccumulator == true)
                    {
                        numericState.dragAccumulator = 0.L;
                    }
                }
            }

            if(response.released() == true && persistentState.editing == true && numericState.temporaryInput == false)
            {
                persistentState.editing = false;
                Detail::setFlag(response, 8);
                ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            Detail::numberContextMenu(ui, node, response, value, static_cast<T>(options.minimum), static_cast<T>(options.maximum), options.precision, location);

            ui->nodes[node].response = response;
            Detail::validationTooltip(ui, response, options.validation, options.validationMessage, location);

            return response;
        }

        template<class T> requires std::is_integral_v<T>
        //////////////////////////////////////////////////////////////////////////
        Response dragValueIntegral(Context * ui, StringView label, T * value, T minimum, T maximum, const SliderOptions & options, const SourceLocation & location)
        {
            String formattedValue = "?";
            (void)Detail::formatIntegral(value == nullptr ? T{} : *value, options.format, &formattedValue);
            String visible;

            if(options.labelPlacement != LabelPlacement::Hidden)
            {
                visible.assign(label);

                if(visible.empty() == false)
                {
                    visible += "  ";
                }
            }

            visible += formattedValue;

            size_t node = ui->addNode(Detail::NodeKind::DragValue, {}, label, {}, location, SemanticRole::Slider, true);
            ui->nodes[node].label = visible;
            ui->mutableStyle(ui->nodes[node]).metrics.font = MonospaceFont;
            ui->estimateNodeText(ui->nodes[node], visible);
            Response response = ui->interact(node, false);
            Context::Persistent & persistentState = ui->state(ui->nodes[node]);
            Detail::NumericState & numericState = ui->numericState(persistentState);
            const PointerState * pointer = ui->input.primaryPointer();
            ui->nodes[node].validation = options.validation;
            ui->nodeSemanticValue(ui->nodes[node]) = formattedValue;

            if(options.temporaryInput == true)
            {
                Detail::temporaryNumberInput(ui, node, response, value, minimum, maximum, formattedValue, options.clampInput, options.clampZeroRange);
            }

            if(response.pressed() == true && numericState.temporaryInput == false)
            {
                persistentState.editing = true;

                if(value != nullptr && pointer != nullptr)
                {
                    persistentState.dragStartPosition = pointer->position;
                    persistentState.dragLastPosition = pointer->position;
                    numericState.dragStartValue = static_cast<long double>(*value);
                    long double integralStep = std::max(1.L, std::round(static_cast<long double>(options.step)));
                    numericState.dragValueSpeed = Detail::automaticDragSpeed(ui->nodes[node], options, static_cast<long double>(minimum), static_cast<long double>(maximum), integralStep);
                    numericState.dragAccumulator = 0.L;
                    numericState.dragLastApplied = static_cast<long double>(*value);
                    numericState.dragThresholdPassed = false;
                }

                Detail::setFlag(response, 7);
                ui->frame.events.push_back({EventType::BeginEdit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            if(numericState.temporaryInput == false && value != nullptr && pointer != nullptr && ui->captured == response.id && pointer->isDown() == true)
            {
                long double speed = numericState.dragValueSpeed;

                if(options.speedTweaks == true && ui->input.modifiers.shift == true)
                {
                    speed *= 10.L;
                }

                if(options.speedTweaks == true && ui->input.modifiers.alt == true)
                {
                    speed *= 0.01L;
                }

                float adjustment = 0.f;
                bool adjusted = Detail::dragAdjustment(ui->nodes[node], persistentState, numericState, *pointer, &adjustment);

                if(adjusted == false)
                {
                    ui->nodes[node].response = response;

                    return response;
                }

                long double current = static_cast<long double>(*value);

                if(current != numericState.dragLastApplied)
                {
                    numericState.dragAccumulator = 0.L;
                    numericState.dragLastApplied = current;
                }

                numericState.dragAccumulator += static_cast<long double>(adjustment) * speed;
                long double previous = static_cast<long double>(*value);
                long double integralStep = std::max(1.L, std::round(static_cast<long double>(options.step)));
                long double calculated = previous + numericState.dragAccumulator;
                calculated = std::round(calculated / integralStep) * integralStep;

                if(maximum >= minimum)
                {
                    if(options.wrapAround == true)
                    {
                        long double first = static_cast<long double>(minimum);
                        long double span = static_cast<long double>(maximum) - first + 1.L;
                        calculated = first + std::fmod(std::fmod(calculated - first, span) + span, span);
                    }
                    else
                    {
                        calculated = std::clamp(calculated, static_cast<long double>(minimum), static_cast<long double>(maximum));
                    }
                }

                T updated = static_cast<T>(calculated);

                if(updated != *value)
                {
                    *value = updated;
                    numericState.dragAccumulator -= static_cast<long double>(updated) - previous;
                    numericState.dragLastApplied = static_cast<long double>(updated);
                    Detail::setFlag(response, 6);
                    Detail::setFlag(response, 7);
                    ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
                }
                else
                {
                    bool clearAccumulator = options.wrapAround == false;

                    if(maximum < minimum)
                    {
                        clearAccumulator = false;
                    }

                    bool clampedAtMinimum = calculated <= static_cast<long double>(minimum);

                    if(numericState.dragAccumulator >= 0.L)
                    {
                        clampedAtMinimum = false;
                    }

                    bool clampedAtMaximum = calculated >= static_cast<long double>(maximum);

                    if(numericState.dragAccumulator <= 0.L)
                    {
                        clampedAtMaximum = false;
                    }

                    if(clampedAtMinimum == false)
                    {
                        if(clampedAtMaximum == false)
                        {
                            clearAccumulator = false;
                        }
                    }

                    if(clearAccumulator == true)
                    {
                        numericState.dragAccumulator = 0.L;
                    }
                }
            }

            if(response.released() == true && persistentState.editing == true && numericState.temporaryInput == false)
            {
                persistentState.editing = false;
                Detail::setFlag(response, 8);
                ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }

            Detail::numberContextMenu(ui, node, response, value, minimum, maximum, location);
            ui->nodes[node].response = response;
            Detail::validationTooltip(ui, response, options.validation, options.validationMessage, location);

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] TreeScope treeNode(Context * ui, const Key & key, StringView label, const TreeNodeOptions & options, bool * open, const SourceLocation & location)
        {
            if(open != nullptr && *open == false)
            {
                return {};
            }

            LayoutOptions layout;

            if(options.framed == true || options.spanAvailableWidth == true || options.spanFullWidth == true || options.spanAllColumns == true)
            {
                layout.width = SizeRule::Fill;
            }

            size_t node = ui->addNode(Detail::NodeKind::Tree, key, label, layout, location, SemanticRole::TreeItem, true);
            ui->nodes[node].treeFramed = options.framed;
            ui->nodes[node].treeLeaf = options.leaf;
            ui->nodes[node].treeBullet = options.bullet;
            ui->nodes[node].selected = options.selected;
            ui->nodes[node].treeFramePadding = options.framePadding;
            ui->nodes[node].treeSpanLabelWidth = options.spanLabelWidth;
            ui->nodes[node].treeSpanAllColumns = options.spanAllColumns;
            ui->nodes[node].treeLabelSpanAllColumns = options.labelSpanAllColumns;
            ui->nodes[node].treeAlignLabelWithCurrentX = options.alignLabelWithCurrentX;
            ui->nodes[node].treeNavigationLeftJumpsToParent = options.navigationLeftJumpsToParent;
            ui->nodes[node].treeLines = options.lines;
            Context::Persistent & persistentState = ui->state(ui->nodes[node]);
            Context::Persistent * parentPersistentState = nullptr;

            if(persistentState.expandedInitialized == false)
            {
                persistentState.expanded = options.defaultExpanded;
                persistentState.expandedInitialized = true;
            }

            if(options.autoCloseChildNodes == true)
            {
                parentPersistentState = &ui->state(ui->nodes[ui->nodes[node].parent]);

                if(parentPersistentState->autoCloseTree != InvalidId && parentPersistentState->autoCloseTree != ui->nodes[node].id)
                {
                    persistentState.expanded = false;
                }
                else if(parentPersistentState->autoCloseTree == InvalidId && persistentState.expanded == true)
                {
                    parentPersistentState->autoCloseTree = ui->nodes[node].id;
                }
            }

            bool logExpanded = ui->textLogEnabled && ui->textLogAutoExpandTrees == true && node >= ui->textLogStartNode && Detail::textLogTreeDepth(ui, node, ui->textLogRootNode) < ui->textLogMaximumDepth;
            ui->nodes[node].treeCloseVisible = open != nullptr;
            Rect interactionBounds = persistentState.lastBounds;

            if(open != nullptr)
            {
                interactionBounds.width = std::max(0.f, interactionBounds.width - ui->currentStyle->metrics.controlHeight);
            }

            Detail::ItemBehaviorOptions behavior;
            behavior.allowOverlap = options.allowOverlap;
            behavior.pressPolicy = Detail::itemPressPolicy(options.pressPolicy);
            Response response = ui->interact(node, behavior, open == nullptr ? nullptr : &interactionBounds);
            const PointerState * pointer = ui->input.primaryPointer();

            if(open != nullptr && pointer != nullptr)
            {
                Rect closeBounds = {persistentState.lastBounds.right() - ui->currentStyle->metrics.controlHeight, persistentState.lastBounds.y, ui->currentStyle->metrics.controlHeight, persistentState.lastBounds.height};
                ui->nodes[node].treeCloseHovered = closeBounds.contains(pointer->position);

                if(ui->nodes[node].treeCloseHovered && pointer->isPressed(PointerButton::Primary) == true)
                {
                    *open = false;
                    Detail::setFlag(response, 6);
                }
            }

            bool arrowHit = pointer != nullptr && pointer->position.x >= persistentState.lastBounds.x && pointer->position.x <= persistentState.lastBounds.x + ui->currentStyle->metrics.indent;
            bool pointerToggle = options.leaf == false && (options.openOnArrow ? (response.clicked() && arrowHit == true) || (options.openOnDoubleClick && response.doubleClicked()) : options.openOnDoubleClick ? response.doubleClicked() : response.clicked());
            bool navigationToggle = false;

            if(options.leaf == false && ui->navigationFocused == ui->nodes[node].id)
            {
                if(ui->input.keyPressed(KeyCode::Right) == true && persistentState.expanded == false)
                {
                    persistentState.expanded = true;
                    navigationToggle = true;
                }
                else if(ui->input.keyPressed(KeyCode::Left) == true && persistentState.expanded == true)
                {
                    persistentState.expanded = false;
                    navigationToggle = true;
                }
                else if(ui->input.keyPressed(KeyCode::Left) == true && options.navigationLeftJumpsToParent == true)
                {
                    size_t parent = ui->nodes[node].parent;
                    while(parent != 0 && ui->nodes[parent].kind != Detail::NodeKind::Tree)
                    {
                        parent = ui->nodes[parent].parent;
                    }

                    if(ui->nodes[parent].kind == Detail::NodeKind::Tree)
                    {
                        ui->focused = ui->nodes[parent].id;
                        ui->navigationFocused = ui->nodes[parent].id;
                    }
                }
            }

            if(pointerToggle == true)
            {
                persistentState.expanded = !persistentState.expanded;
                navigationToggle = true;
            }

            if(options.autoCloseChildNodes == true && navigationToggle == true && persistentState.expanded == true)
            {
                parentPersistentState->autoCloseTree = ui->nodes[node].id;
            }
            else if(options.autoCloseChildNodes == true && navigationToggle == true && parentPersistentState->autoCloseTree == ui->nodes[node].id)
            {
                parentPersistentState->autoCloseTree = InvalidId;
            }

            if(navigationToggle == true)
            {
                Detail::setFlag(response, 6);
                Detail::setFlag(response, 12);
            }

            bool expanded = options.leaf == false && (persistentState.expanded == true || logExpanded == true);
            ui->nodes[node].expanded = expanded;
            ui->nodes[node].response = response;
            uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

            return {ui, token, ui->nodes[node].id, expanded};
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float comboPopupMaximumHeight(const Context * ui, ComboPopupHeight height) noexcept
        {
            float rowHeight = ui->currentStyle->metrics.controlHeight + ui->currentStyle->metrics.itemSpacing.y;
            switch(height)
            {
            case ComboPopupHeight::Small:
                return rowHeight * 4.f + ui->currentStyle->metrics.popupPadding.top + ui->currentStyle->metrics.popupPadding.bottom;
            case ComboPopupHeight::Regular:
                return rowHeight * 8.f + ui->currentStyle->metrics.popupPadding.top + ui->currentStyle->metrics.popupPadding.bottom;
            case ComboPopupHeight::Large:
                return rowHeight * 20.f + ui->currentStyle->metrics.popupPadding.top + ui->currentStyle->metrics.popupPadding.bottom;
            case ComboPopupHeight::Largest:
                auto returnedValue = std::numeric_limits<float>::max();

                return returnedValue;
            }
            auto returnedValue = std::numeric_limits<float>::max();

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Key comboPopupKey() noexcept
        {
            auto returnedValue = Key("Combo popup");

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Response comboControl(Context * ui, const Key & key, StringView label, StringView preview, const ComboOptions & options, const SourceLocation & location)
        {
            ComboOptions resolved = options;

            if(resolved.showArrow == false && resolved.showPreview == false)
            {
                resolved.showArrow = true;
            }

            LayoutOptions layout;
            layout.width = resolved.widthFitPreview ? Dimension(SizeRule::Content) : resolved.width;
            size_t node = ui->addNode(Detail::NodeKind::Combo, key, label, layout, location, SemanticRole::Button, true);
            Context::Node & comboNode = ui->nodes[node];
            comboNode.labelPlacement = resolved.labelPlacement;
            comboNode.comboShowArrow = resolved.showArrow;
            comboNode.comboShowPreview = resolved.showPreview;
            comboNode.comboWidthFitPreview = resolved.widthFitPreview;
            ui->prepareValueText(comboNode, preview);
            Response response = ui->interact(node);
            comboNode.response = response;

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] TreeScope comboPopup(Context * ui, const Response & response, PopupOptions popupOptions, const SourceLocation & location)
        {
            Key popupKey = Detail::comboPopupKey();
            popupOptions.owner = response.id;

            if(popupOptions.anchor.empty() == true)
            {
                if(Mosaic::debugBounds(ui, response.id, &popupOptions.anchor) == false)
                {
                    return {};
                }
            }

            if(popupOptions.anchor.width > 0.f)
            {
                float minimumWidth = std::max(0.f, popupOptions.minimumSize.x);
                float maximumWidth = std::max(minimumWidth, popupOptions.maximumSize.x);
                float popupWidth = std::clamp(popupOptions.anchor.width, minimumWidth, maximumWidth);
                popupOptions.minimumSize.x = popupWidth;
                popupOptions.maximumSize.x = popupWidth;
                popupOptions.anchor.height = std::max(0.f, popupOptions.anchor.height - ui->currentStyle->metrics.frameBorderSize);
            }

            Id id = Detail::popupId(ui, popupKey, response.id);
            const Context::PopupState * state = Detail::findPopup(ui, id);

            if(response.clicked() == true)
            {
                if(state != nullptr && state->open == true)
                {
                    Mosaic::closeCurrentPopup(ui);
                }
                else
                {
                    Mosaic::openPopup(ui, popupKey, popupOptions);
                }
            }

            const Context::PopupState * activeState = Detail::findPopup(ui, Detail::popupId(ui, popupKey, response.id));
            for(auto node = ui->nodes.rbegin(); node != ui->nodes.rend(); ++node)
            {
                if(node->id != response.id)
                {
                    continue;
                }

                node->selected = activeState != nullptr && activeState->open;
                break;
            }

            WindowScope result = Mosaic::popup(ui, popupKey, popupOptions, location);

            if(result.visible() == true)
            {
                ui->nodes[ui->currentParent].windowAttachedPopup = true;
            }

            auto returnedValue = TreeScope(std::move(result));

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ItemPressPolicy itemPressPolicy(ButtonPressPolicy policy) noexcept
        {
            switch(policy)
            {
            case ButtonPressPolicy::Press:
                return ItemPressPolicy::Press;
            case ButtonPressPolicy::DoubleClick:
                return ItemPressPolicy::DoubleClick;
            case ButtonPressPolicy::Release:
                return ItemPressPolicy::Release;
            }

            return ItemPressPolicy::Release;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ItemBehaviorOptions buttonBehaviorOptions(const ButtonOptions & options) noexcept
        {
            ItemBehaviorOptions behavior;
            behavior.keyboardActivation = options.keyboardActivation;
            behavior.pointerButton = options.pointerButton;
            behavior.pressPolicy = Detail::itemPressPolicy(options.pressPolicy);
            behavior.repeat = options.repeat;
            behavior.allowOverlap = options.allowOverlap;

            return behavior;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ItemBehaviorOptions selectableBehaviorOptions(const SelectableOptions & options) noexcept
        {
            ItemBehaviorOptions behavior;
            behavior.keyboardActivation = options.keyboardActivation;
            behavior.pointerButton = options.pointerButton;
            behavior.pressPolicy = Detail::itemPressPolicy(options.pressPolicy);
            behavior.allowOverlap = options.allowOverlap;
            behavior.allowDoubleClick = options.allowDoubleClick;

            return behavior;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ButtonPressPolicy selectionPressPolicy(bool selected, const SelectionOptions & options) noexcept
        {
            switch(options.pressPolicy)
            {
            case SelectionPressPolicy::Automatic:
                return selected ? ButtonPressPolicy::Release : ButtonPressPolicy::Press;
            case SelectionPressPolicy::Press:
                return ButtonPressPolicy::Press;
            case SelectionPressPolicy::Release:
                return ButtonPressPolicy::Release;
            }

            return ButtonPressPolicy::Release;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] SelectableOptions selectionItemOptions(bool selected, const SelectionOptions & options) noexcept
        {
            SelectableOptions item = options.item;
            item.pressPolicy = Detail::selectionPressPolicy(selected, options);

            return item;
        }
        //////////////////////////////////////////////////////////////////////////
        template<class Values, class Widget> Response componentVector(Context * ui, StringView label, Values values, Widget && widget, const SourceLocation & location)
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

                Response component = widget(generated, index);
                result.flags |= component.flags;
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        template<class Getter> Response comboItems(Context * ui, StringView label, int * selected, size_t itemCount, Getter && getter, const ComboOptions & options, const SourceLocation & location)
        {
            Id owner = combineId(ui->nodes[ui->currentParent].id, ui->localId(Detail::NodeKind::Combo, {}, location, false));
            bool wasOpen = Mosaic::isComboOpen(ui, owner);
            StringView preview = selected != nullptr && *selected >= 0 && static_cast<size_t>(*selected) < itemCount ? getter(static_cast<size_t>(*selected)) : StringView{};
            Response response;
            auto popupScope = Mosaic::beginCombo(ui, label, preview, options, location);
            response.id = popupScope.id();

            if(popupScope.expanded() == false)
            {
                return response;
            }

            ScrollOptions scrollOptions;
            scrollOptions.axes = ScrollAxes::Vertical;
            LayoutOptions listLayout;
            listLayout.width = SizeRule::Fill;
            listLayout.height = SizeRule::Content;
            auto list = Mosaic::scrollArea(ui, "Combo items", scrollOptions, listLayout, location);
            ListClipperOptions clipperOptions;
            clipperOptions.overscan = 2;
            float itemExtent = ui->currentStyle->metrics.controlHeight;

            if(wasOpen == false && selected != nullptr && *selected >= 0 && static_cast<size_t>(*selected) < itemCount)
            {
                Context::Persistent & listState = ui->state(list.id());
                float stride = itemExtent + ui->gap(ui->nodes[ui->currentParent]);
                listState.scrollPosition.y = std::max(0.f, static_cast<float>(*selected) * stride);
                listState.scrollTarget = listState.scrollPosition;
                listState.scrollVelocity = {};
                listState.scrollTargetInitialized = true;
            }

            VisibleRange range;
            (void)Mosaic::beginListClipper(ui, itemCount, itemExtent, &range, list.id(), clipperOptions, location);
            for(size_t index = range.begin; index != range.end; ++index)
            {
                auto itemScope = Mosaic::scope(ui, Key(index), location);
                Response item = Mosaic::selectable(ui, Key("item"), getter(index), selected != nullptr && *selected == static_cast<int>(index), location);

                if(selected != nullptr && *selected == static_cast<int>(index))
                {
                    Mosaic::setItemDefaultFocus(ui, item.id);
                }

                if(item.clicked() == false)
                {
                    continue;
                }

                if(selected != nullptr && *selected != static_cast<int>(index))
                {
                    *selected = static_cast<int>(index);
                    Detail::setFlag(response, 6);
                }
            }
            Mosaic::endListClipper(ui, range, itemCount, itemExtent, clipperOptions, location);

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
        void setComponentColorMarker(Context * ui, const Response & response, size_t component, bool enabled)
        {
            if(enabled == false)
            {
                return;
            }

            constexpr Array<Color, 4> markers = {{Color::fromBytes(240, 20, 20), Color::fromBytes(20, 240, 20), Color::fromBytes(20, 20, 240), Color::fromBytes(140, 140, 140)}};
            Context::Node * node = ui->findFrameNode(response.id);

            if(node == nullptr)
            {
                return;
            }

            node->colorMarkerEnabled = true;
            node->colorMarker = markers[std::min(component, markers.size() - 1)];
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Response text(Context * ui, StringView value, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::text(ui, value, TextOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response text(Context * ui, StringView value, const TextOptions & options, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Text, {}, value, options.layout, location, SemanticRole::Text, false, true);
        ui->nodes[node].wordWrap = options.wordWrap;
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response bullet(Context * ui, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Bullet, {}, {}, {}, location, SemanticRole::None, false, true);
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response bulletText(Context * ui, StringView value, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::BulletText, {}, value, {}, location, SemanticRole::Text, false, true);
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response button(Context * ui, StringView label, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::button(ui, {}, label, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response button(Context * ui, const Key & key, StringView label, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::button(ui, key, label, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response button(Context * ui, const Key & key, StringView label, const ButtonOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = options.width;
        layout.height = options.height;
        size_t node = ui->addNode(Detail::NodeKind::Button, key, label, layout, location, SemanticRole::Button, true);
        ui->nodes[node].fillBackground = options.fillBackground;
        ui->nodes[node].fillHoverBackground = options.fillBackground;
        Response response = ui->interact(node, Detail::buttonBehaviorOptions(options));
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response smallButton(Context * ui, StringView label, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::smallButton(ui, {}, label, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response smallButton(Context * ui, const Key & key, StringView label, const SourceLocation & location)
    {
        ButtonOptions options;
        options.height = Dimension::fixed(ui->currentStyle->metrics.lineHeight);
        auto returnedValue = Mosaic::button(ui, key, label, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response invisibleButton(Context * ui, const Key & key, const Vec2 & size, const ButtonOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = Dimension::fixed(std::max(0.f, size.x));
        layout.height = Dimension::fixed(std::max(0.f, size.y));
        size_t node = ui->addNode(Detail::NodeKind::Button, key, {}, layout, location, SemanticRole::Button, true);
        ui->nodes[node].fillBackground = false;
        ui->nodes[node].fillHoverBackground = false;
        Response response = ui->interact(node, Detail::buttonBehaviorOptions(options));
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response arrowButton(Context * ui, const Key & key, Direction direction, const ButtonOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = options.width.rule == SizeRule::Content ? Dimension::fixed(ui->currentStyle->metrics.controlHeight) : options.width;
        layout.height = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
        size_t node = ui->addNode(Detail::NodeKind::Button, key, {}, layout, location, SemanticRole::Button, true);
        ui->nodes[node].arrowButton = true;
        ui->nodes[node].direction = direction;
        ui->nodes[node].fillBackground = options.fillBackground;
        ui->nodes[node].fillHoverBackground = options.fillBackground;
        Response response = ui->interact(node, Detail::buttonBehaviorOptions(options));
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response hyperlink(Context * ui, StringView label, StringView url, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Button, {}, label, {}, location, SemanticRole::Button, true);
        Context::Node & linkNode = ui->nodes[node];
        linkNode.hyperlink = true;
        linkNode.fillBackground = false;
        linkNode.fillHoverBackground = false;
        Theme & style = ui->mutableStyle(linkNode);
        style.colors.text = style.colors.textLink;
        Response response = ui->interact(node);

        if(response.clicked() == true && url.empty() == false)
        {
            ui->platform->openUrl(url);
        }

        linkNode.response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response labelText(Context * ui, StringView label, StringView value, const SourceLocation & location)
    {
        auto labelScope = Mosaic::scope(ui, {}, location);
        auto labelRow = Mosaic::row(ui, {}, location);
        Mosaic::text(ui, value, location);
        auto returnedValue = Mosaic::text(ui, label, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response iconButton(Context * ui, const Key & key, TextureHandle texture, StringView description, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::IconButton, key, description, {}, location, SemanticRole::Button, true);
        ui->nodes[node].texture = texture;
        auto returnedValue = ui->interact(node);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response checkbox(Context * ui, StringView label, bool * value, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::checkbox(ui, {}, label, value, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response checkbox(Context * ui, const Key & key, StringView label, bool * value, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Checkbox, key, label, {}, location, SemanticRole::Checkbox, true);
        Response response = ui->interact(node);

        if(value != nullptr && response.clicked() == true)
        {
            *value = !*value;
            Detail::setFlag(response, 6);
            ui->nodes[node].response = response;
            ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
        }

        Detail::booleanContextMenu(ui, node, response, value, location);
        ui->nodes[node].checked = value != nullptr && *value;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response radioButton(Context * ui, StringView label, bool selected, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Radio, {}, label, {}, location, SemanticRole::Radio, true);
        ui->nodes[node].checked = selected;
        auto returnedValue = ui->interact(node);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response selectable(Context * ui, StringView label, bool selected, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::selectable(ui, {}, label, selected, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response selectable(Context * ui, const Key & key, StringView label, bool selected, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::selectable(ui, key, label, selected, SelectableOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response selectable(Context * ui, const Key & key, StringView label, bool selected, const SelectableOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = options.width;
        layout.height = options.height;
        size_t node = ui->addNode(Detail::NodeKind::Selectable, key, label, layout, location, SemanticRole::TreeItem, true);
        ui->nodes[node].selected = selected;
        ui->nodes[node].highlighted = options.highlight;
        ui->nodes[node].selectableSpanAllColumns = options.spanAllColumns;
        ui->nodes[node].overrideTextAlignment = options.overrideTextAlignment;
        ui->nodes[node].textAlignment = options.textAlignment;
        Context::Persistent & persistentState = ui->state(ui->nodes[node]);
        Response response = ui->interact(node, Detail::selectableBehaviorOptions(options));

        if(options.selectOnNavigation == true && ui->navigationFocused == response.id && persistentState.navigationFocusLastFrame + 1 != ui->frame.number)
        {
            Detail::setFlag(response, 5);
            Detail::setFlag(response, 14);
            Detail::setFlag(response, 15);
        }

        if(ui->navigationFocused == response.id)
        {
            persistentState.navigationFocusLastFrame = ui->frame.number;
        }

        ui->nodes[node].response = response;

        if(response.clicked() == true && options.autoClosePopups == true && ui->currentInputLayer != InvalidId)
        {
            const Context::PopupState * popupState = Detail::findPopup(ui, ui->currentInputLayer);

            if(popupState != nullptr && popupState->options.closeOnSelection == true)
            {
                Id popupId = popupState->id;
                Detail::closePopupById(ui, popupId);
            }
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response selectable(Context * ui, const Key & key, StringView label, SelectionModel * selection, Id item, IdSpan orderedItems, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::selectable(ui, key, label, selection, item, orderedItems, SelectionOptions{}, location);

        return returnedValue;
    }

    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Id selectionScopeId(const Context * ui, Id node, SelectionScope scope) noexcept
        {
            const Context::Node * candidate = ui->findFrameNode(node);
            Id rectangularScope = InvalidId;
            while(candidate != nullptr)
            {
                if(candidate->kind == NodeKind::Window)
                {
                    return scope == SelectionScope::Window || rectangularScope == InvalidId ? candidate->id : rectangularScope;
                }

                if(rectangularScope == InvalidId && (candidate->kind == NodeKind::Scroll || candidate->kind == NodeKind::Table))
                {
                    rectangularScope = candidate->id;
                }

                if(candidate->parent == 0)
                {
                    break;
                }

                if(candidate->parent >= ui->nodes.size())
                {
                    break;
                }

                candidate = &ui->nodes[candidate->parent];
            }

            return rectangularScope == InvalidId ? node : rectangularScope;
        }
        //////////////////////////////////////////////////////////////////////////
        Response applySelection(Context * ui, Response response, SelectionModel * selection, Id item, IdSpan orderedItems, const SelectionOptions & options)
        {
            if(selection == nullptr)
            {
                return response;
            }

            bool ownsKeyboardFocus = ui->navigationFocused == response.id;
            bool commandModifier = ui->input.modifiers.primary || ui->input.modifiers.control == true || ui->input.modifiers.super;
            bool selectAll = options.selectAll && ownsKeyboardFocus == true && commandModifier == true && ui->input.keyPressed(KeyCode::A) && orderedItems.empty() == false;
            bool clear = options.clearOnEscape && ownsKeyboardFocus == true && ui->input.keyPressed(KeyCode::Escape);
            bool movedWithKeyboard = ownsKeyboardFocus && (ui->input.keyPressed(KeyCode::Left) || ui->input.keyPressed(KeyCode::Right) || ui->input.keyPressed(KeyCode::Up) || ui->input.keyPressed(KeyCode::Down) || ui->input.keyPressed(KeyCode::Home) || ui->input.keyPressed(KeyCode::End) || ui->input.keyPressed(KeyCode::PageUp) || ui->input.keyPressed(KeyCode::PageDown));
            const PointerState * pointer = ui->input.primaryPointer();
            bool secondaryClicked = options.selectOnRightClick && response.hovered() && pointer != nullptr && pointer->isPressed(PointerButton::Secondary);
            bool selectedBefore = selection->selected(item);
            size_t selectionSizeBefore = selection->size();
            Id requestScope = ui->currentSelectionScope != InvalidId ? ui->currentSelectionScope : Detail::selectionScopeId(ui, response.id, options.scope);
            auto appendRequest = [ui, requestScope](SelectionRequest request)
            {
                request.scope = requestScope;
                ui->selectionRequests.push_back(request);
            };
            bool changed = false;

            if(selectAll == true)
            {
                SelectionRequest request;
                request.type = SelectionRequestType::SetAll;
                appendRequest(request);

                if(options.applyRequests == true)
                {
                    selection->selectAll(orderedItems);
                }

                changed = selectionSizeBefore != orderedItems.size();
            }
            else if(clear == true)
            {
                changed = selection->empty() == false;
                SelectionRequest request;
                request.type = SelectionRequestType::Clear;
                appendRequest(request);

                if(options.applyRequests == true)
                {
                    selection->clear();
                }
            }
            else if((response.clicked() == true || secondaryClicked == true) && options.autoSelect == true)
            {
                SelectionRequest request;
                request.item = item;

                if(options.singleSelect == true)
                {
                    request.type = SelectionRequestType::SetItem;
                    request.selected = true;
                    request.clearFirst = true;
                    appendRequest(request);

                    if(options.applyRequests == true)
                    {
                        selection->select(item);
                    }
                }
                else if(options.rangeSelect == true && ui->input.modifiers.shift == true && orderedItems.empty() == false)
                {
                    request.type = SelectionRequestType::SetRange;
                    request.anchor = selection->anchor();
                    request.selected = true;
                    request.clearFirst = true;
                    appendRequest(request);

                    if(options.applyRequests == true)
                    {
                        selection->selectRange(orderedItems, item);
                    }
                }
                else
                {
                    bool toggle = ui->input.modifiers.primary || ui->input.modifiers.control == true || ui->input.modifiers.super;
                    bool alreadySelected = selection->selected(item);
                    request.type = SelectionRequestType::SetItem;
                    request.selected = toggle ? alreadySelected == false : true;
                    request.clearFirst = toggle == false && options.autoClear;
                    appendRequest(request);

                    if(toggle == false && alreadySelected == true && options.autoClearOnReselect == false)
                    {
                        if(options.applyRequests == true)
                        {
                            selection->select(item, true);
                        }
                    }
                    else
                    {
                        bool extend = toggle || options.autoClear == false;

                        if(options.applyRequests == true)
                        {
                            selection->select(item, extend, toggle);
                        }
                    }
                }

                changed = true;
            }
            else if(movedWithKeyboard == true && commandModifier == false)
            {
                SelectionRequest request;
                request.item = item;

                if(options.rangeSelect == true && ui->input.modifiers.shift == true && orderedItems.empty() == false)
                {
                    request.type = SelectionRequestType::SetRange;
                    request.anchor = selection->anchor();
                    request.selected = true;
                    appendRequest(request);

                    if(options.applyRequests == true)
                    {
                        selection->selectRange(orderedItems, item);
                    }
                }
                else
                {
                    request.type = SelectionRequestType::SetItem;
                    request.selected = true;
                    request.clearFirst = true;
                    appendRequest(request);

                    if(options.applyRequests == true)
                    {
                        selection->select(item);
                    }
                }

                changed = true;
            }

            if(changed == true)
            {
                Detail::setFlag(response, 6);
            }

            const SelectionRequest * lastRequest = ui->selectionRequests.empty() == true ? nullptr : &ui->selectionRequests.back();
            bool requestMatches = lastRequest != nullptr && lastRequest->scope == requestScope && lastRequest->item == item;
            bool requestedSelection = requestMatches == true ? lastRequest->selected : selectedBefore;
            bool selectedAfter = options.applyRequests == true ? selection->selected(item) : requestedSelection;
            Detail::setFlag(response, 17, selectedBefore != selectedAfter);

            if(Context::Node * node = ui->findFrameNode(response.id); node != nullptr)
            {
                node->selected = selection->selected(item);
                node->response = response;
            }

            ui->selectionItems.push_back({selection, item, response.id, Detail::selectionScopeId(ui, response.id, options.scope), options.navigationWrapX});

            return response;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Response selectable(Context * ui, const Key & key, StringView label, SelectionModel * selection, Id item, IdSpan orderedItems, const SelectionOptions & options, const SourceLocation & location)
    {
        bool selected = selection != nullptr && selection->selected(item);
        Response response = Mosaic::selectable(ui, key, label, selected, Detail::selectionItemOptions(selected, options), location);
        auto returnedValue = Detail::applySelection(ui, response, selection, item, orderedItems, options);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope multiSelect(Context * ui, const Key & key, SelectionModel * selection, IdSpan orderedItems, const SelectionOptions & options, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Scope, key, key.debug(), {}, location, SemanticRole::Group);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);
        ui->currentSelectionModel = selection;
        ui->currentSelectionOrder = orderedItems;
        ui->currentSelectionOptions = options;
        ui->currentSelectionScope = ui->nodes[node].id;

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    SelectionRequestSpan selectionRequests(const Context * ui) noexcept
    {
        auto returnedValue = ui == nullptr ? SelectionRequestSpan{} : SelectionRequestSpan(ui->selectionRequests.data(), ui->selectionRequests.size());

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void applySelectionRequests(SelectionModel * selection, IdSpan orderedItems, SelectionRequestSpan requests, Id scope)
    {
        if(selection == nullptr)
        {
            return;
        }

        for(const SelectionRequest & request : requests)
        {
            if(scope != InvalidId && request.scope != scope)
            {
                continue;
            }

            switch(request.type)
            {
            case SelectionRequestType::Clear:
                selection->clear();
                break;
            case SelectionRequestType::SetAll:
                if(request.selected == true)
                {
                    selection->selectAll(orderedItems);
                }
                else
                {
                    selection->clear();
                }

                break;
            case SelectionRequestType::SetItem:
                if(request.clearFirst == true)
                {
                    selection->clear();
                }

                if(request.selected == true)
                {
                    selection->select(request.item, true);
                }
                else
                {
                    selection->remove(request.item);
                }

                break;
            case SelectionRequestType::SetRange:
                if(request.clearFirst == true)
                {
                    selection->clear();
                }

                if(request.selected == true)
                {
                    if(request.anchor != InvalidId)
                    {
                        selection->select(request.anchor, true);
                    }

                    selection->selectRange(orderedItems, request.item);
                }
                else
                {
                    auto first = std::find(orderedItems.begin(), orderedItems.end(), request.anchor);
                    auto last = std::find(orderedItems.begin(), orderedItems.end(), request.item);

                    if(first != orderedItems.end() && last != orderedItems.end())
                    {
                        auto rangeBegin = std::min(first, last);
                        auto rangeEnd = std::max(first, last);
                        for(auto item = rangeBegin; item != rangeEnd + 1; ++item)
                        {
                            selection->remove(*item);
                        }
                    }
                }

                break;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Response selectable(Context * ui, const Key & key, StringView label, Id item, const SourceLocation & location)
    {
        if(ui->currentSelectionModel == nullptr)
        {
            ui->frame.diagnostics.emplace_back("selectable item id requires an active multiSelect scope");
            auto returnedValue = Mosaic::selectable(ui, key, label, false, location);

            return returnedValue;
        }

        auto returnedValue = Mosaic::selectable(ui, key, label, ui->currentSelectionModel, item, ui->currentSelectionOrder, ui->currentSelectionOptions, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response boxSelect(Context * ui, Id surface, SelectionModel * selection, const BoxSelectionOptions & options, const SourceLocation & location)
    {
        (void)location;
        Response response;
        response.id = surface;
        auto cancel = [ui, surface]()
        {
            if(ui->activeBoxSelection != surface)
            {
                return;
            }

            ui->activeBoxSelection = InvalidId;
            ui->boxSelectionModel = nullptr;
            ui->boxSelectionOriginal.clear();

            if(ui->captured == surface)
            {
                ui->active = InvalidId;
                ui->captured = InvalidId;
                ui->capturedPointer = 0;
            }
        };

        if(selection == nullptr)
        {
            cancel();

            return response;
        }

        if(selection->mode() != SelectionMode::Multiple)
        {
            cancel();

            return response;
        }

        if(surface == InvalidId)
        {
            cancel();

            return response;
        }

        Context::Persistent & state = ui->state(surface);
        const PointerState * pointer = ui->input.primaryPointer();

        if(pointer == nullptr)
        {
            cancel();

            return response;
        }

        if(state.lastBounds.empty() == true)
        {
            return response;
        }

        if(state.lastClip.empty() == true)
        {
            return response;
        }

        bool inside = state.lastBounds.contains(pointer->position) && state.lastClip.contains(pointer->position);
        bool scrollbarHit = state.verticalScrollbarTrack.contains(pointer->position) || state.horizontalScrollbarTrack.contains(pointer->position);

        if(options.enabled == false)
        {
            bool clearSelection = options.clearOnClick;

            if(pointer->isPressed() == false)
            {
                clearSelection = false;
            }

            if(inside == false)
            {
                clearSelection = false;
            }

            if(scrollbarHit == true)
            {
                clearSelection = false;
            }

            if(ui->captured != InvalidId)
            {
                clearSelection = false;
            }

            if(selection->empty() == true)
            {
                clearSelection = false;
            }

            if(clearSelection == true)
            {
                selection->clear();
                Detail::setFlag(response, 6);
            }

            cancel();

            return response;
        }

        bool capturedSelectedItem = false;

        if(options.allowFromSelectedItems == true && ui->captured != InvalidId)
        {
            for(const Context::SelectionItem & item : ui->selectionItems)
            {
                if(item.model == selection && item.node == ui->captured && selection->selected(item.item) == true && Detail::descendsFrom(ui, item.node, surface) == true)
                {
                    capturedSelectedItem = true;
                    break;
                }
            }
        }

        if(pointer->isPressed() == true && inside == true && scrollbarHit == false && (ui->captured == InvalidId || capturedSelectedItem == true))
        {
            ui->activeBoxSelection = surface;
            ui->boxSelectionModel = selection;
            ui->boxSelectionOriginal.clear();
            bool preserve = ui->input.modifiers.primary || ui->input.modifiers.control == true || ui->input.modifiers.super;

            if(preserve == true)
            {
                ui->boxSelectionOriginal.assign(selection->values().begin(), selection->values().end());
            }
            else if(options.clearOnClick == true)
            {
                selection->clear();
            }

            state.dragStartPosition = pointer->position;
            state.dragLastPosition = pointer->position;
            ui->active = surface;
            ui->captured = surface;
            ui->capturedPointer = pointer->id;
            Detail::setFlag(response, 3);
        }

        if(ui->activeBoxSelection == surface && ui->boxSelectionModel != selection)
        {
            cancel();

            return response;
        }

        if(ui->activeBoxSelection != surface)
        {
            return response;
        }

        Vec2 current = pointer->position;
        state.dragLastPosition = current;
        bool oneDimensional = options.mode == BoxSelectionMode::OneDimensional;
        float left = oneDimensional ? state.lastBounds.x : std::min(state.dragStartPosition.x, current.x);
        float top = std::min(state.dragStartPosition.y, current.y);
        Rect selectionBounds = {left, top, oneDimensional ? state.lastBounds.width : std::abs(current.x - state.dragStartPosition.x), std::abs(current.y - state.dragStartPosition.y)};

        if(pointer->isDown() == true)
        {
            if(options.noScroll == false)
            {
                constexpr float edge = 20.f;
                float speed = 360.f * std::max(0.f, ui->input.deltaTime);
                Vec2 target = state.scrollTargetInitialized ? state.scrollTarget : state.scrollPosition;

                if(pointer->position.y < state.lastBounds.y + edge)
                {
                    target.y -= speed;
                }
                else if(pointer->position.y > state.lastBounds.bottom() - edge)
                {
                    target.y += speed;
                }

                if(oneDimensional == false)
                {
                    if(pointer->position.x < state.lastBounds.x + edge)
                    {
                        target.x -= speed;
                    }
                    else if(pointer->position.x > state.lastBounds.right() - edge)
                    {
                        target.x += speed;
                    }
                }

                Mosaic::scrollTo(ui, surface, target);
            }

            Detail::setFlag(response, 1);
            selection->clear();
            for(Id original : ui->boxSelectionOriginal)
            {
                selection->select(original, true);
            }
            for(const Context::SelectionItem & item : ui->selectionItems)
            {
                if(item.model != selection)
                {
                    continue;
                }

                if(Detail::descendsFrom(ui, item.node, surface) == false)
                {
                    continue;
                }

                const Context::Persistent * itemState = ui->findState(item.node);

                if(itemState != nullptr && Detail::overlaps(selectionBounds, itemState->lastBounds) == true)
                {
                    selection->select(item.item, true);
                }
            }
            for(const Context::SelectionItem & item : ui->selectionItems)
            {
                if(item.model != selection)
                {
                    continue;
                }

                if(Detail::descendsFrom(ui, item.node, surface) == false)
                {
                    continue;
                }

                if(Context::Node * node = ui->findFrameNode(item.node); node != nullptr)
                {
                    node->selected = selection->selected(item.item);
                }
            }
            Color fill = ui->currentStyle->colors.selection;
            fill.a *= 0.22f;
            Color border = ui->currentStyle->colors.accent;
            border.a *= 0.9f;
            ui->selectionOverlays.push_back({selectionBounds, Rect::intersection(state.lastClip, state.lastBounds), fill, border});
            Detail::setFlag(response, 6);
        }

        if(pointer->isReleased() == true)
        {
            Detail::setFlag(response, 4);
            cancel();
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response toggle(Context * ui, StringView label, bool * value, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Toggle, {}, label, {}, location, SemanticRole::Checkbox, true);
        Response response = ui->interact(node);

        if(value != nullptr && response.clicked() == true)
        {
            *value = !*value;
            Detail::setFlag(response, 6);
            ui->nodes[node].response = response;
        }

        Detail::booleanContextMenu(ui, node, response, value, location);
        ui->nodes[node].checked = value != nullptr && *value;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, double * value, const SliderOptions & options, const SourceLocation & location)
    {
        Response response = Detail::sliderFloating(ui, label, value, options.minimum, options.maximum, options.step, options.precision, options.showValuePopup, options.showValueOnTrack, options.showValueTooltip, options.valueTooltipDelay, options.logarithmic, options.temporaryInput, options.clampInput, options.clampZeroRange, options.roundToFormat, options.validation, options.labelPlacement, options.format, location);
        for(auto node = ui->nodes.rbegin(); node != ui->nodes.rend(); ++node)
        {
            if(node->id != response.id)
            {
                continue;
            }

            node->layout.width = options.width;
            break;
        }
        Detail::validationTooltip(ui, response, options.validation, options.validationMessage, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, float * value, const SliderOptions & options, const SourceLocation & location)
    {
        Response response = Detail::sliderFloating(ui, label, value, static_cast<float>(options.minimum), static_cast<float>(options.maximum), static_cast<float>(options.step), options.precision, options.showValuePopup, options.showValueOnTrack, options.showValueTooltip, options.valueTooltipDelay, options.logarithmic, options.temporaryInput, options.clampInput, options.clampZeroRange, options.roundToFormat, options.validation, options.labelPlacement, options.format, location);
        for(auto node = ui->nodes.rbegin(); node != ui->nodes.rend(); ++node)
        {
            if(node->id != response.id)
            {
                continue;
            }

            node->layout.width = options.width;
            break;
        }
        Detail::validationTooltip(ui, response, options.validation, options.validationMessage, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response verticalSlider(Context * ui, StringView label, float * value, float minimum, float maximum, const Vec2 & size, const SliderOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = Dimension::fixed(std::max(1.f, size.x));
        layout.height = Dimension::fixed(std::max(1.f, size.y));
        size_t node = ui->addNode(Detail::NodeKind::Slider, {}, label, layout, location, SemanticRole::Slider, true);
        Context::Node & sliderNode = ui->nodes[node];
        sliderNode.sliderVertical = true;
        sliderNode.labelPlacement = LabelPlacement::Hidden;
        sliderNode.showValuePopup = options.showValuePopup;
        sliderNode.showValueOnTrack = options.showValueOnTrack;
        sliderNode.showValueTooltip = options.showValueTooltip;
        sliderNode.tooltipDelay = std::max(0.f, options.valueTooltipDelay);
        Context::Persistent & persistentState = ui->state(sliderNode);
        Response response = ui->interact(node, false);
        const PointerState * pointer = ui->input.primaryPointer();
        float range = maximum - minimum;

        if(response.pressed() == true)
        {
            persistentState.editing = true;
            Detail::setFlag(response, 7);
        }

        bool updateSlider = value != nullptr;

        if(Detail::capturedPointerActive(ui, pointer, response.id) == false)
        {
            updateSlider = false;
        }

        if(persistentState.lastBounds.height <= 0.f)
        {
            updateSlider = false;
        }

        if(range == 0.f)
        {
            updateSlider = false;
        }

        if(updateSlider == true)
        {
            float grabLength = std::clamp(ui->currentStyle->metrics.grabMinimumSize, 1.f, persistentState.lastBounds.height);
            float usableLength = std::max(1.f, persistentState.lastBounds.height - grabLength);
            float ratio = std::clamp(1.f - (pointer->position.y - persistentState.lastBounds.y - grabLength * 0.5f) / usableLength, 0.f, 1.f);
            float updated = minimum + ratio * range;

            if(options.step > 0.0)
            {
                float step = static_cast<float>(options.step);
                updated = minimum + std::round((updated - minimum) / step) * step;
            }

            updated = range > 0.f ? std::clamp(updated, minimum, maximum) : std::clamp(updated, maximum, minimum);

            if(updated != *value)
            {
                *value = updated;
                Detail::setFlag(response, 6);
                Detail::setFlag(response, 7);
            }
        }

        if(response.released() == true && persistentState.editing == true)
        {
            persistentState.editing = false;
            Detail::setFlag(response, 8);
        }

        float scalar = value == nullptr || range == 0.f ? 0.f : (*value - minimum) / range;
        sliderNode.scalar = std::clamp(scalar, 0.f, 1.f);
        String formatted = "?";
        (void)Detail::formatFloating(value == nullptr ? 0.f : *value, options.precision, options.format, &formatted);
        ui->nodeSemanticValue(sliderNode) = formatted;
        ui->prepareValueText(sliderNode, formatted);
        sliderNode.response = response;
        Detail::numberContextMenu(ui, node, response, value, std::min(minimum, maximum), std::max(minimum, maximum), options.precision, location);
        Detail::validationTooltip(ui, response, options.validation, options.validationMessage, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response verticalSlider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const Vec2 & size, const SliderOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = Dimension::fixed(std::max(1.f, size.x));
        layout.height = Dimension::fixed(std::max(1.f, size.y));
        size_t node = ui->addNode(Detail::NodeKind::Slider, {}, label, layout, location, SemanticRole::Slider, true);
        Context::Node & sliderNode = ui->nodes[node];
        sliderNode.sliderVertical = true;
        sliderNode.labelPlacement = LabelPlacement::Hidden;
        sliderNode.showValuePopup = options.showValuePopup;
        sliderNode.showValueOnTrack = options.showValueOnTrack;
        sliderNode.showValueTooltip = options.showValueTooltip;
        sliderNode.tooltipDelay = std::max(0.f, options.valueTooltipDelay);
        Context::Persistent & persistentState = ui->state(sliderNode);
        Response response = ui->interact(node, false);
        const PointerState * pointer = ui->input.primaryPointer();
        int64_t minimumValue = minimum;
        int64_t maximumValue = maximum;
        int64_t range = maximumValue - minimumValue;

        if(response.pressed() == true)
        {
            persistentState.editing = true;
            Detail::setFlag(response, 7);
        }

        bool updateSlider = value != nullptr;

        if(Detail::capturedPointerActive(ui, pointer, response.id) == false)
        {
            updateSlider = false;
        }

        if(persistentState.lastBounds.height <= 0.f)
        {
            updateSlider = false;
        }

        if(range == 0)
        {
            updateSlider = false;
        }

        if(updateSlider == true)
        {
            float grabLength = std::clamp(ui->currentStyle->metrics.grabMinimumSize, 1.f, persistentState.lastBounds.height);
            float usableLength = std::max(1.f, persistentState.lastBounds.height - grabLength);
            float ratio = std::clamp(1.f - (pointer->position.y - persistentState.lastBounds.y - grabLength * 0.5f) / usableLength, 0.f, 1.f);
            int64_t distance = range > 0 ? range : -range;
            int64_t offset = static_cast<int64_t>(std::llround(static_cast<double>(distance) * static_cast<double>(ratio)));
            int64_t calculated = range > 0 ? minimumValue + offset : minimumValue - offset;
            int32_t updated = static_cast<int32_t>(std::clamp(calculated, std::min(minimumValue, maximumValue), std::max(minimumValue, maximumValue)));

            if(updated != *value)
            {
                *value = updated;
                Detail::setFlag(response, 6);
                Detail::setFlag(response, 7);
            }
        }

        if(response.released() == true && persistentState.editing == true)
        {
            persistentState.editing = false;
            Detail::setFlag(response, 8);
        }

        float scalar = value == nullptr || range == 0 ? 0.f : static_cast<float>(static_cast<int64_t>(*value) - minimumValue) / static_cast<float>(range);
        sliderNode.scalar = std::clamp(scalar, 0.f, 1.f);
        String formatted = "?";
        (void)Detail::formatIntegral(value == nullptr ? int32_t{} : *value, options.format, &formatted);
        ui->nodeSemanticValue(sliderNode) = formatted;
        ui->prepareValueText(sliderNode, formatted);
        sliderNode.response = response;
        Detail::numberContextMenu(ui, node, response, value, std::min(minimum, maximum), std::max(minimum, maximum), location);
        Detail::validationTooltip(ui, response, options.validation, options.validationMessage, location);

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, labelPlacement, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, float * value, float minimum, float maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderFloating(ui, label, value, minimum, maximum, 0.f, 3, false, true, false, 0.25f, false, true, false, false, true, Validation::Normal, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response slider(Context * ui, StringView label, double * value, double minimum, double maximum, const SourceLocation & location)
    {
        auto returnedValue = Detail::sliderFloating(ui, label, value, minimum, maximum, 0.0, 3, false, true, false, 0.25f, false, true, false, false, true, Validation::Normal, LabelPlacement::Before, {}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response progressBar(Context * ui, float value, StringView label, const SourceLocation & location)
    {
        String overlay(label);

        if(overlay.empty() == true && value >= 0.f)
        {
            overlay = "?";
            (void)Detail::formatFloating(std::clamp(value, 0.f, 1.f) * 100.f, 0, {}, &overlay);
            overlay += "%";
        }

        size_t node = ui->addNode(Detail::NodeKind::Progress, {}, overlay, {}, location);
        ui->nodes[node].selected = value < 0.f;
        ui->nodes[node].scalar = value < 0.f ? 0.f : std::clamp(value, 0.f, 1.f);
        ui->nodes[node].secondaryScalar = value < 0.f ? std::fmod(std::abs(value), 1.f) : 0.f;
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response plotLines(Context * ui, StringView label, ConstFloatSpan values, const PlotOptions & options, const SourceLocation & location)
    {
        auto getter = [](void * userData, size_t index)
        {
            const auto * samples = static_cast<const ConstFloatSpan *>(userData);
            auto returnedValue = (*samples)[index];

            return returnedValue;
        };
        auto returnedValue = Mosaic::plotLines(ui, label, values.size(), getter, &values, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response plotLines(Context * ui, StringView label, size_t valueCount, PlotValueGetter getter, void * userData, const PlotOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = options.width;
        layout.height = Dimension::fixed(std::max(1.f, options.height));
        Canvas plot = Mosaic::canvas(ui, label, layout, location);
        Response response = ui->nodes[ui->currentParent].response;
        Rect bounds;
        if(plot.contentRect(&bounds) == false)
        {
            return response;
        }

        plot.rect({0.f, 0.f, bounds.width, bounds.height}, ui->currentStyle->colors.frame);

        if(getter != nullptr && valueCount > 1 && bounds.width > 0.f && bounds.height > 0.f)
        {
            float range = std::max(std::numeric_limits<float>::epsilon(), options.maximum - options.minimum);
            Vec2Vector points;
            points.reserve(valueCount);
            for(size_t point = 0; point != valueCount; ++point)
            {
                size_t sample = (point + options.offset) % valueCount;
                float scalar = std::clamp((getter(userData, sample) - options.minimum) / range, 0.f, 1.f);
                points.push_back({static_cast<float>(point) * bounds.width / static_cast<float>(valueCount - 1), (1.f - scalar) * std::max(0.f, bounds.height - 1.f)});
            }
            Detail::canvasPolylineOwned(ui, plot.id(), std::move(points), 1.f, response.hovered() ? ui->currentStyle->colors.plotLinesHovered : ui->currentStyle->colors.plotLines, false);
        }

        if(options.overlay.empty() == false)
        {
            Mosaic::text(ui, options.overlay, location);
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response plotHistogram(Context * ui, StringView label, ConstFloatSpan values, const PlotOptions & options, const SourceLocation & location)
    {
        auto getter = [](void * userData, size_t index)
        {
            const auto * samples = static_cast<const ConstFloatSpan *>(userData);
            auto returnedValue = (*samples)[index];

            return returnedValue;
        };
        auto returnedValue = Mosaic::plotHistogram(ui, label, values.size(), getter, &values, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response plotHistogram(Context * ui, StringView label, size_t valueCount, PlotValueGetter getter, void * userData, const PlotOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = options.width;
        layout.height = Dimension::fixed(std::max(1.f, options.height));
        Canvas plot = Mosaic::canvas(ui, label, layout, location);
        Response response = ui->nodes[ui->currentParent].response;
        Rect bounds;
        if(plot.contentRect(&bounds) == false)
        {
            return response;
        }

        plot.rect({0.f, 0.f, bounds.width, bounds.height}, ui->currentStyle->colors.frame);

        if(getter != nullptr && valueCount != 0 && bounds.width > 0.f && bounds.height > 0.f)
        {
            float range = std::max(std::numeric_limits<float>::epsilon(), options.maximum - options.minimum);
            float barWidth = bounds.width / static_cast<float>(valueCount);
            for(size_t bar = 0; bar != valueCount; ++bar)
            {
                size_t sample = (bar + options.offset) % valueCount;
                float scalar = std::clamp((getter(userData, sample) - options.minimum) / range, 0.f, 1.f);
                float height = scalar * bounds.height;
                float left = static_cast<float>(bar) * barWidth;
                float right = left + std::max(1.f, barWidth - 1.f);
                float top = bounds.height - height;
                plot.rect({left, top, right - left, height}, response.hovered() ? ui->currentStyle->colors.plotHistogramHovered : ui->currentStyle->colors.plotHistogram);
            }
        }

        if(options.overlay.empty() == false)
        {
            Mosaic::text(ui, options.overlay, location);
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response separator(Context * ui, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Separator, {}, {}, {}, location, SemanticRole::None, false, true);
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response separatorText(Context * ui, StringView label, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = SizeRule::Fill;
        size_t node = ui->addNode(Detail::NodeKind::SeparatorText, {}, label, layout, location, SemanticRole::Text, false);
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response spacer(Context * ui, float size, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Spacer, {}, {}, {}, location, SemanticRole::None, false, true);
        ui->nodes[node].scalar = std::max(0.f, size);
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response image(Context * ui, TextureHandle texture, const Vec2 & size, const Rect & uv, const Color & tint, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Image, {}, "image", {}, location, SemanticRole::Image, false, true);
        ui->nodes[node].texture = texture;
        ui->nodes[node].measured = size;
        ui->nodes[node].uv = uv;
        ui->nodes[node].tint = tint;
        Response response;
        response.id = ui->nodes[node].id;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response image(Context * ui, TextureHandle texture, const Vec2 & size, const ImageOptions & options, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Image, {}, "image", {}, location, SemanticRole::Image, false, true);
        Context::Node & imageNode = ui->nodes[node];
        imageNode.texture = texture;
        imageNode.imagePadding = std::max(0.f, options.padding);
        imageNode.measured = {size.x + imageNode.imagePadding * 2.f, size.y + imageNode.imagePadding * 2.f};
        imageNode.uv = options.uv;
        imageNode.tint = options.tint;
        imageNode.imageBackground = options.background;
        imageNode.imageBackgroundEnabled = options.backgroundEnabled;
        Response response;
        response.id = imageNode.id;
        imageNode.response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response imageButton(Context * ui, const Key & key, TextureHandle texture, const Vec2 & size, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::imageButton(ui, key, texture, size, ImageButtonOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response imageButton(Context * ui, const Key & key, TextureHandle texture, const Vec2 & size, const ImageButtonOptions & options, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::ImageButton, key, "image", {}, location, SemanticRole::Button, true);
        ui->nodes[node].texture = texture;
        ui->nodes[node].measured = size;
        ui->nodes[node].uv = options.uv;
        ui->nodes[node].tint = options.tint;
        ui->nodes[node].imageBackground = options.background;
        ui->nodes[node].imagePadding = std::max(0.f, options.padding);
        ui->nodes[node].imageBackgroundEnabled = options.backgroundEnabled;
        auto returnedValue = ui->interact(node);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope treeNode(Context * ui, const Key & key, StringView label, bool defaultExpanded, const SourceLocation & location)
    {
        TreeNodeOptions options;
        options.defaultExpanded = defaultExpanded;
        auto returnedValue = Detail::treeNode(ui, key, label, options, nullptr, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope treeNode(Context * ui, const Key & key, StringView label, const TreeNodeOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::treeNode(ui, key, label, options, nullptr, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope treeNode(Context * ui, const Key & key, StringView label, SelectionModel * selection, Id item, IdSpan orderedItems, const TreeNodeOptions & treeOptions, const SelectionOptions & selectionOptions, const SourceLocation & location)
    {
        TreeNodeOptions resolved = treeOptions;
        resolved.selected = selection != nullptr && selection->selected(item);
        resolved.pressPolicy = Detail::selectionPressPolicy(resolved.selected, selectionOptions);
        TreeScope result = Detail::treeNode(ui, key, label, resolved, nullptr, location);
        Response response;
        if(Mosaic::itemResponse(ui, result.id(), &response) == false)
        {
            return result;
        }

        (void)Detail::applySelection(ui, response, selection, item, orderedItems, selectionOptions);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    Canvas canvas(Context * ui, StringView label, const LayoutOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::canvas(ui, {}, label, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Canvas canvas(Context * ui, const Key & key, StringView label, const LayoutOptions & options, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::Canvas, key, label, options, location, SemanticRole::Group, true);
        (void)ui->interact(node, false);
        uint64_t token = ui->pushScope(node, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[node].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, float * value, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueFloating(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, double * value, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueFloating(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragRange(Context * ui, StringView label, float * minimumValue, float * maximumValue, float lowerBound, float upperBound, const RangeOptions & options, const SourceLocation & location)
    {
        auto rangeScope = Mosaic::scope(ui, {}, location);
        ui->nodes[ui->currentParent].label.assign(label);
        Response response;
        response.id = rangeScope.id();
        {
            auto values = Mosaic::row(ui, {}, location);
            SliderOptions minimumOptions = options.minimum;
            minimumOptions.minimum = lowerBound;
            minimumOptions.maximum = maximumValue == nullptr ? upperBound : std::clamp(*maximumValue, lowerBound, upperBound);
            SliderOptions maximumOptions = options.maximum;
            maximumOptions.minimum = minimumValue == nullptr ? lowerBound : std::clamp(*minimumValue, lowerBound, upperBound);
            maximumOptions.maximum = upperBound;
            {
                auto component = Mosaic::scope(ui, Key("minimum"), location);
                Response minimumResponse = Mosaic::dragValue(ui, "minimum", minimumValue, minimumOptions, location);
                response.flags |= minimumResponse.flags;
            }
            {
                auto component = Mosaic::scope(ui, Key("maximum"), location);
                Response maximumResponse = Mosaic::dragValue(ui, "maximum", maximumValue, maximumOptions, location);
                response.flags |= maximumResponse.flags;
            }
        }

        if(Context::Node * node = ui->findFrameNode(response.id); node != nullptr)
        {
            node->response = response;
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragRange(Context * ui, StringView label, int32_t * minimumValue, int32_t * maximumValue, int32_t lowerBound, int32_t upperBound, const RangeOptions & options, const SourceLocation & location)
    {
        auto rangeScope = Mosaic::scope(ui, {}, location);
        ui->nodes[ui->currentParent].label.assign(label);
        Response response;
        response.id = rangeScope.id();
        {
            auto values = Mosaic::row(ui, {}, location);
            SliderOptions minimumOptions = options.minimum;
            minimumOptions.minimum = static_cast<double>(lowerBound);
            minimumOptions.maximum = maximumValue == nullptr ? static_cast<double>(upperBound) : static_cast<double>(std::clamp(*maximumValue, lowerBound, upperBound));
            SliderOptions maximumOptions = options.maximum;
            maximumOptions.minimum = minimumValue == nullptr ? static_cast<double>(lowerBound) : static_cast<double>(std::clamp(*minimumValue, lowerBound, upperBound));
            maximumOptions.maximum = static_cast<double>(upperBound);
            {
                auto component = Mosaic::scope(ui, Key("minimum"), location);
                Response minimumResponse = Mosaic::dragValue(ui, "minimum", minimumValue, lowerBound, upperBound, minimumOptions, location);
                response.flags |= minimumResponse.flags;
            }
            {
                auto component = Mosaic::scope(ui, Key("maximum"), location);
                Response maximumResponse = Mosaic::dragValue(ui, "maximum", maximumValue, lowerBound, upperBound, maximumOptions, location);
                response.flags |= maximumResponse.flags;
            }
        }

        if(Context::Node * node = ui->findFrameNode(response.id); node != nullptr)
        {
            node->response = response;
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, int32_t * value, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, static_cast<int32_t>(options.minimum), static_cast<int32_t>(options.maximum), options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, int8_t * value, int8_t minimum, int8_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, uint8_t * value, uint8_t minimum, uint8_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, int16_t * value, int16_t minimum, int16_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, uint16_t * value, uint16_t minimum, uint16_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, int32_t * value, int32_t minimum, int32_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, uint32_t * value, uint32_t minimum, uint32_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, int64_t * value, int64_t minimum, int64_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragValue(Context * ui, StringView label, uint64_t * value, uint64_t minimum, uint64_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Detail::dragValueIntegral(ui, label, value, minimum, maximum, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragFloatVector(Context * ui, StringView label, FloatSpan values, const SliderOptions & options, const SourceLocation & location)
    {
        SliderOptions componentOptions = options;
        componentOptions.width = SizeRule::Fill;
        Response returnedValue = Detail::componentVector(
            ui, label, values,
            [ui, values, &componentOptions, &location](StringView componentLabel, size_t index)
            {
                Response response = Mosaic::dragValue(ui, componentLabel, &values[index], componentOptions, location);
                Detail::setComponentColorMarker(ui, response, index, componentOptions.colorMarkers);

                return response;
            },
            location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response dragIntVector(Context * ui, StringView label, Int32Span values, int32_t minimum, int32_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        SliderOptions componentOptions = options;
        componentOptions.width = SizeRule::Fill;
        Response returnedValue = Detail::componentVector(
            ui, label, values,
            [ui, values, minimum, maximum, &componentOptions, &location](StringView componentLabel, size_t index)
            {
                Response response = Mosaic::dragValue(ui, componentLabel, &values[index], minimum, maximum, componentOptions, location);
                Detail::setComponentColorMarker(ui, response, index, componentOptions.colorMarkers);

                return response;
            },
            location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response sliderFloatVector(Context * ui, StringView label, FloatSpan values, const SliderOptions & options, const SourceLocation & location)
    {
        SliderOptions componentOptions = options;
        componentOptions.width = SizeRule::Fill;
        Response returnedValue = Detail::componentVector(
            ui, label, values,
            [ui, values, &componentOptions, &location](StringView componentLabel, size_t index)
            {
                Response response = Mosaic::slider(ui, componentLabel, &values[index], componentOptions, location);
                Detail::setComponentColorMarker(ui, response, index, componentOptions.colorMarkers);

                return response;
            },
            location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response sliderIntVector(Context * ui, StringView label, Int32Span values, int32_t minimum, int32_t maximum, const SliderOptions & options, const SourceLocation & location)
    {
        SliderOptions componentOptions = options;
        componentOptions.width = SizeRule::Fill;
        Response returnedValue = Detail::componentVector(
            ui, label, values,
            [ui, values, minimum, maximum, &componentOptions, &location](StringView componentLabel, size_t index)
            {
                Response response = Mosaic::slider(ui, componentLabel, &values[index], minimum, maximum, componentOptions, location);
                Detail::setComponentColorMarker(ui, response, index, componentOptions.colorMarkers);

                return response;
            },
            location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response sliderAngle(Context * ui, StringView label, float * radians, float minimumDegrees, float maximumDegrees, StringView format, LabelPlacement labelPlacement, const SourceLocation & location)
    {
        constexpr float radiansToDegrees = 57.29577951308232f;
        constexpr float degreesToRadians = 0.017453292519943295f;
        float degrees = radians == nullptr ? 0.f : *radians * radiansToDegrees;
        SliderOptions options;
        options.minimum = minimumDegrees;
        options.maximum = maximumDegrees;
        options.precision = 0;
        options.format = format;
        options.labelPlacement = labelPlacement;
        Response response = Mosaic::slider(ui, label, &degrees, options, location);

        if(radians != nullptr && response.changed() == true)
        {
            *radians = degrees * degreesToRadians;
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response searchField(Context * ui, StringView label, String * value, const SourceLocation & location)
    {
        TextInputOptions options;
        options.hint = "Search";
        auto returnedValue = Mosaic::inputText(ui, label, value, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope collapsingHeader(Context * ui, StringView label, bool defaultExpanded, const SourceLocation & location)
    {
        TreeNodeOptions options;
        options.defaultExpanded = defaultExpanded;
        options.framed = true;
        options.spanAvailableWidth = true;
        auto returnedValue = Detail::treeNode(ui, {}, label, options, nullptr, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope collapsingHeader(Context * ui, StringView label, bool * open, bool defaultExpanded, const SourceLocation & location)
    {
        TreeNodeOptions options;
        options.defaultExpanded = defaultExpanded;
        options.framed = true;
        options.spanAvailableWidth = true;
        auto returnedValue = Detail::treeNode(ui, {}, label, options, open, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response comboBox(Context * ui, StringView label, int * selected, StringViewSpan items, const SourceLocation & location)
    {
        ComboOptions options;
        options.labelPlacement = LabelPlacement::Hidden;
        auto returnedValue = Mosaic::comboBox(ui, label, selected, items, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response comboBox(Context * ui, StringView label, int * selected, StringViewSpan items, const ComboOptions & options, const SourceLocation & location)
    {
        Response returnedValue = Detail::comboItems(
            ui, label, selected, items.size(),
            [items](size_t index)
            {
                return items[index];
            },
            options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response comboBox(Context * ui, StringView label, int * selected, size_t itemCount, ComboItemGetter getter, void * userData, const SourceLocation & location)
    {
        ComboOptions options;
        options.labelPlacement = LabelPlacement::Hidden;
        auto returnedValue = Mosaic::comboBox(ui, label, selected, itemCount, getter, userData, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response comboBox(Context * ui, StringView label, int * selected, size_t itemCount, ComboItemGetter getter, void * userData, const ComboOptions & options, const SourceLocation & location)
    {
        if(getter == nullptr)
        {
            itemCount = 0;
        }

        Response returnedValue = Detail::comboItems(
            ui, label, selected, itemCount,
            [getter, userData](size_t index)
            {
                auto returnedValue = getter == nullptr ? StringView{} : getter(userData, index);

                return returnedValue;
            },
            options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope beginCombo(Context * ui, StringView label, StringView preview, const PopupOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::beginCombo(ui, {}, label, preview, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope beginCombo(Context * ui, const Key & key, StringView label, StringView preview, const PopupOptions & options, const SourceLocation & location)
    {
        ComboOptions comboOptions;
        comboOptions.labelPlacement = LabelPlacement::Hidden;
        Response response = Detail::comboControl(ui, key, label, preview, comboOptions, location);
        PopupOptions popupOptions = options;
        popupOptions.maximumSize.y = std::min(popupOptions.maximumSize.y, Detail::comboPopupMaximumHeight(ui, ComboPopupHeight::Regular));
        auto returnedValue = Detail::comboPopup(ui, response, popupOptions, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool isComboOpen(const Context * ui, const Key & key) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        Id owner = combineId(ui->nodes[ui->currentParent].id, key.value());
        auto returnedValue = Mosaic::isPopupOpen(ui, Detail::comboPopupKey(), owner);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool isComboOpen(const Context * ui, Id combo) noexcept
    {
        auto returnedValue = ui != nullptr && combo != InvalidId && Mosaic::isPopupOpen(ui, Detail::comboPopupKey(), combo);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope beginCombo(Context * ui, StringView label, StringView preview, const ComboOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::beginCombo(ui, {}, label, preview, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope beginCombo(Context * ui, const Key & key, StringView label, StringView preview, const ComboOptions & options, const SourceLocation & location)
    {
        Response response = Detail::comboControl(ui, key, label, preview, options, location);
        PopupOptions popupOptions;
        popupOptions.minimumSize.x = 0.f;
        popupOptions.maximumSize.x = std::numeric_limits<float>::max();
        popupOptions.maximumSize.y = Detail::comboPopupMaximumHeight(ui, options.popupHeight);
        popupOptions.horizontalAlignment = options.popupAlignLeft ? PopupHorizontalAlignment::End : PopupHorizontalAlignment::Start;
        auto returnedValue = Detail::comboPopup(ui, response, popupOptions, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response listBox(Context * ui, StringView label, int * selected, StringViewSpan items, const LayoutOptions & layout, const SourceLocation & location)
    {
        ListBoxOptions options;
        options.layout = layout;
        auto returnedValue = Mosaic::listBox(ui, label, selected, items, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response listBox(Context * ui, StringView label, int * selected, StringViewSpan items, const ListBoxOptions & options, const SourceLocation & location)
    {
        if(options.hovered != nullptr)
        {
            *options.hovered = -1;
        }

        auto listScope = Mosaic::beginListBox(ui, label, options, location);
        Response aggregate;
        aggregate.id = listScope.id();
        ListClipperOptions clipperOptions;
        clipperOptions.overscan = 2;
        float itemExtent = ui->currentStyle->metrics.controlHeight;
        VisibleRange range;
        (void)Mosaic::beginListClipper(ui, items.size(), itemExtent, &range, listScope.id(), clipperOptions, location);
        for(size_t index = range.begin; index != range.end; ++index)
        {
            auto itemScope = Mosaic::scope(ui, Key(index), location);
            bool itemSelected = selected != nullptr && *selected == static_cast<int>(index);
            bool itemHighlighted = options.highlighted == static_cast<int>(index);
            Response response = Mosaic::selectable(ui, Key("Item"), items[index], itemSelected || itemHighlighted, location);

            if(response.hovered() == true && options.hovered != nullptr)
            {
                *options.hovered = static_cast<int>(index);
            }

            if(response.clicked() == true && selected != nullptr)
            {
                *selected = static_cast<int>(index);
                Detail::setFlag(response, 6);
            }

            aggregate.flags |= response.flags;
        }
        Mosaic::endListClipper(ui, range, items.size(), itemExtent, clipperOptions, location);

        return aggregate;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope beginListBox(Context * ui, StringView label, const ListBoxOptions & options, const SourceLocation & location)
    {
        ScrollOptions scrollOptions;
        scrollOptions.axes = ScrollAxes::Vertical;
        scrollOptions.contentOrientation = Orientation::Vertical;
        scrollOptions.framed = true;
        scrollOptions.frameStyle = true;
        auto returnedValue = Mosaic::scrollArea(ui, label, scrollOptions, options.layout, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::tabs(ui, label, selected, items, TabsOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, const TabsOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::tabs(ui, label, selected, items, BoolSpan{}, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, BoolSpan open, const TabsOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::tabs(ui, label, selected, items, open, TabItemOptionsSpan{}, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response tabs(Context * ui, StringView label, int * selected, StringViewSpan items, BoolSpan open, TabItemOptionsSpan itemOptions, const TabsOptions & options, const SourceLocation & location)
    {
        auto tabScope = Mosaic::scope(ui, {}, location);
        ui->nodes[ui->currentParent].label.assign(label);
        Context::TabState & tabState = ui->tabState(tabScope.id());

        if(open.empty() == false && open.size() != items.size())
        {
            ui->frame.diagnostics.emplace_back("tabs open state must match the item count");
            open = {};
        }

        if(itemOptions.empty() == false && itemOptions.size() != items.size())
        {
            ui->frame.diagnostics.emplace_back("tabs item options must match the item count");
            itemOptions = {};
        }

        if(tabState.order.size() != items.size())
        {
            SizeVector nextOrder;
            nextOrder.reserve(items.size());
            for(size_t existing : tabState.order)
            {
                if(existing < items.size())
                {
                    nextOrder.push_back(existing);
                }
            }
            for(size_t index = 0; index != items.size(); ++index)
            {
                if(std::find(nextOrder.begin(), nextOrder.end(), index) == nextOrder.end())
                {
                    nextOrder.push_back(index);
                }
            }
            tabState.order = std::move(nextOrder);

            if(options.autoSelectNewTabs == true && selected != nullptr && items.size() > tabState.itemCount)
            {
                *selected = static_cast<int>(items.size() - 1);
            }

            tabState.itemCount = items.size();
        }

        size_t visibleCount = open.empty() == true ? items.size() : static_cast<size_t>(std::count(open.begin(), open.end(), true));

        if(options.autoSelectNewTabs == true && selected != nullptr && visibleCount > tabState.visibleCount)
        {
            for(size_t index = items.size(); index > 0; --index)
            {
                if(open.empty() == true)
                {
                    *selected = static_cast<int>(index - 1);
                    break;
                }

                if(open[index - 1])
                {
                    *selected = static_cast<int>(index - 1);
                    break;
                }
            }
        }

        tabState.visibleCount = visibleCount;

        if(selected != nullptr)
        {
            bool selectedVisible = *selected >= 0 && static_cast<size_t>(*selected) < items.size() && (open.empty() == true || open[static_cast<size_t>(*selected)]);

            if(selectedVisible == false)
            {
                *selected = -1;
                for(size_t index : tabState.order)
                {
                    if(open.empty() == true)
                    {
                        *selected = static_cast<int>(index);
                        break;
                    }

                    if(open[index])
                    {
                        *selected = static_cast<int>(index);
                        break;
                    }
                }
            }
        }

        TabFittingPolicy fittingPolicy = options.fitAvailableWidth ? TabFittingPolicy::Shrink : options.fittingPolicy;
        float availableWidth = ui->state(tabScope.id()).lastBounds.width > 0.f ? ui->state(tabScope.id()).lastBounds.width : ui->viewport.bounds.width;
        float requestedWidth = visibleCount > 1 ? static_cast<float>(visibleCount - 1) : 0.f;
        float minimumWidth = requestedWidth;
        for(size_t index = 0; index != items.size(); ++index)
        {
            if(open.empty() == false && open[index] == false)
            {
                continue;
            }

            TabItemOptions defaultItemOptions;
            const TabItemOptions & currentItemOptions = itemOptions.empty() == true ? defaultItemOptions : itemOptions[index];
            float decorationWidth = ((open.empty() == false && currentItemOptions.closeButton == true) ? ui->currentStyle->metrics.controlHeight * 0.7f : 0.f) + (currentItemOptions.unsavedDocument ? ui->currentStyle->metrics.fontSize * 0.75f : 0.f);
            requestedWidth += std::max(ui->measureText(items[index], *ui->currentStyle).x + ui->currentStyle->metrics.framePadding.left + ui->currentStyle->metrics.framePadding.right + decorationWidth, ui->currentStyle->metrics.tabMinimumWidthBase);
            minimumWidth += std::max(ui->currentStyle->metrics.tabMinimumWidthForShrink, decorationWidth + ui->currentStyle->metrics.controlHeight);
        }

        if(fittingPolicy == TabFittingPolicy::Mixed && requestedWidth > availableWidth)
        {
            fittingPolicy = minimumWidth <= availableWidth ? TabFittingPolicy::Shrink : TabFittingPolicy::Scroll;
        }

        if(options.scrollToFit == false && fittingPolicy == TabFittingPolicy::Scroll)
        {
            fittingPolicy = TabFittingPolicy::Mixed;
        }

        bool fillTabs = fittingPolicy == TabFittingPolicy::Shrink;
        bool scrollTabs = fittingPolicy == TabFittingPolicy::Scroll && options.scrollToFit;
        LayoutOptions rowLayout;
        rowLayout.gap = 1.f;

        if(fillTabs == true)
        {
            rowLayout.width = SizeRule::Fill;
        }

        Response aggregate;
        aggregate.id = tabScope.id();
        Id tabScroll = InvalidId;
        Vec2 previousScrollRange;
        bool hasPreviousScrollRange = tabState.scrollArea != InvalidId && Mosaic::scrollRange(ui, tabState.scrollArea, &previousScrollRange);
        bool showScrollingButtons = options.scrollingButtons && scrollTabs == true && hasPreviousScrollRange == true && previousScrollRange.x > 0.f;
        PopupOptions tabListPopupOptions;
        tabListPopupOptions.owner = tabScope.id();
        {
            Scope hostRow;

            if(options.tabListPopupButton == true || options.leadingContent != nullptr || options.trailingContent != nullptr || showScrollingButtons == true)
            {
                LayoutOptions hostLayout;
                hostLayout.width = SizeRule::Fill;
                hostLayout.gap = 1.f;
                hostRow = Mosaic::row(ui, hostLayout, location);

                if(options.tabListPopupButton == true)
                {
                    ButtonOptions listButtonOptions;
                    listButtonOptions.width = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
                    Response listButton = Mosaic::button(ui, Key("Tab list"), "▾", listButtonOptions, location);

                    if(listButton.clicked() == true)
                    {
                        const Context::Persistent * buttonState = ui->findState(listButton.id);

                        if(buttonState != nullptr)
                        {
                            tabListPopupOptions.anchor = buttonState->lastBounds;
                            tabListPopupOptions.placement = PopupPlacement::Below;
                        }

                        Mosaic::openPopup(ui, Key("Tab list popup"), tabListPopupOptions);
                    }
                }
            }

            if(options.leadingContent != nullptr)
            {
                options.leadingContent(ui, options.contentUserData);
            }

            if(showScrollingButtons == true)
            {
                ButtonOptions scrollButtonOptions;
                scrollButtonOptions.width = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
                Response scrollLeft = Mosaic::button(ui, Key("Tabs scroll left"), "‹", scrollButtonOptions, location);

                if(scrollLeft.clicked() == true)
                {
                    Vec2 target;
                    if(Mosaic::scrollOffset(ui, tabState.scrollArea, &target) == true)
                    {
                        target.x -= ui->currentStyle->metrics.controlHeight * 4.f;
                        Mosaic::scrollTo(ui, tabState.scrollArea, target);
                    }
                }
            }

            {
                Scope scrollScope;

                if(scrollTabs == true)
                {
                    ScrollOptions scrollOptions;
                    scrollOptions.axes = ScrollAxes::Horizontal;
                    scrollOptions.contentOrientation = Orientation::Horizontal;
                    scrollOptions.visibility = ScrollbarVisibility::Hidden;
                    scrollOptions.wheelStep = ui->currentStyle->metrics.controlHeight * 2.f;
                    LayoutOptions scrollLayout;
                    scrollLayout.width = SizeRule::Fill;
                    scrollLayout.height = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
                    scrollLayout.gap = 0.f;
                    scrollScope = Mosaic::scrollArea(ui, "Tabs scroll area", scrollOptions, scrollLayout, location);
                    tabScroll = scrollScope.id();
                    tabState.scrollArea = tabScroll;
                }

                auto tabRow = Mosaic::row(ui, rowLayout, location);
                const PointerState * pointer = ui->input.primaryPointer();
                for(uint8_t itemBand = 0; itemBand != 3; ++itemBand)
                {
                    for(size_t index : tabState.order)
                    {
                        if(open.empty() == false && open[index] == false)
                        {
                            continue;
                        }

                        TabItemOptions defaultItemOptions;
                        const TabItemOptions & currentItemOptions = itemOptions.empty() == true ? defaultItemOptions : itemOptions[index];

                        if(currentItemOptions.setSelected == true && selected != nullptr)
                        {
                            *selected = static_cast<int>(index);
                        }

                        uint8_t currentBand = currentItemOptions.leading ? 0 : currentItemOptions.trailing ? 2 : 1;

                        if(currentBand != itemBand)
                        {
                            continue;
                        }

                        auto itemScope = Mosaic::scope(ui, Key(index), location);
                        LayoutOptions tabLayout;

                        if(fillTabs == true)
                        {
                            tabLayout.width = SizeRule::Fill;
                            tabLayout.minimum.x = ui->currentStyle->metrics.tabMinimumWidthForShrink;
                        }
                        else
                        {
                            tabLayout.minimum.x = ui->currentStyle->metrics.tabMinimumWidthBase;
                        }

                        size_t node = ui->addNode(Detail::NodeKind::Tab, Key(index), items[index], tabLayout, location, SemanticRole::Tab, true);
                        ui->nodes[node].checked = selected != nullptr && *selected == static_cast<int>(index);
                        ui->nodes[node].disabled = ui->nodes[node].disabled || currentItemOptions.disabled;
                        ui->nodes[node].tabSelectedOverline = options.selectedOverline;
                        ui->nodes[node].tabUnsavedDocument = currentItemOptions.unsavedDocument;

                        if(tabScroll != InvalidId && ui->nodes[node].checked == true)
                        {
                            Mosaic::scrollToItem(ui, tabScroll, ui->nodes[node].id);
                        }

                        Context::Node & tabNode = ui->nodes[node];
                        Context::Persistent & tabPersistentState = ui->state(tabNode);
                        Rect tabInteractionBounds = tabPersistentState.lastBounds;
                        bool tabHovered = pointer != nullptr && pointer->type != PointerType::Touch && tabInteractionBounds.contains(pointer->position);
                        float closeMinimumWidth = tabNode.checked ? tabNode.style->metrics.tabCloseButtonMinimumWidthSelected : tabNode.style->metrics.tabCloseButtonMinimumWidthUnselected;
                        float closeExtent = std::min(tabInteractionBounds.height, ui->currentStyle->metrics.controlHeight * 0.7f);
                        tabNode.tabCloseVisible = open.empty() == false && currentItemOptions.closeButton == true && (closeMinimumWidth < 0.f || (tabHovered && tabInteractionBounds.width >= std::max(closeExtent, closeMinimumWidth)));
                        Rect closeBounds;

                        if(ui->nodes[node].tabCloseVisible == true && tabInteractionBounds.empty() == false)
                        {
                            closeBounds = {tabInteractionBounds.right() - closeExtent, tabInteractionBounds.y, closeExtent, tabInteractionBounds.height};
                            tabInteractionBounds.width = std::max(0.f, tabInteractionBounds.width - closeExtent);
                            ui->nodes[node].tabCloseHovered = pointer != nullptr && pointer->type != PointerType::Touch && closeBounds.contains(pointer->position);
                        }

                        Response response = ui->interact(node, true, tabInteractionBounds.empty() == true ? nullptr : &tabInteractionBounds);

                        if(ui->nodes[node].tabCloseVisible == true && pointer != nullptr && pointer->isPressed(PointerButton::Primary) == true && closeBounds.contains(pointer->position) == true)
                        {
                            open[index] = false;
                            Detail::setFlag(response, 6);

                            if(selected != nullptr && *selected == static_cast<int>(index))
                            {
                                for(size_t replacement : tabState.order)
                                {
                                    if(open[replacement])
                                    {
                                        *selected = static_cast<int>(replacement);
                                        break;
                                    }
                                }
                            }
                        }

                        bool middleClose = open.empty() == false;

                        if(options.closeWithMiddleMouse == false)
                        {
                            middleClose = false;
                        }

                        if(currentItemOptions.closeWithMiddleMouse == false)
                        {
                            middleClose = false;
                        }

                        if(pointer == nullptr)
                        {
                            middleClose = false;
                        }

                        if(middleClose == true)
                        {
                            middleClose = pointer->isPressed(PointerButton::Middle);
                        }

                        if(middleClose == true)
                        {
                            middleClose = tabPersistentState.lastBounds.contains(pointer->position);
                        }

                        if(middleClose == true)
                        {
                            open[index] = false;
                            Detail::setFlag(response, 6);

                            if(selected != nullptr && *selected == static_cast<int>(index))
                            {
                                for(size_t replacement : tabState.order)
                                {
                                    if(open[replacement])
                                    {
                                        *selected = static_cast<int>(replacement);
                                        break;
                                    }
                                }
                            }
                        }

                        if(response.clicked() == true && selected != nullptr)
                        {
                            *selected = static_cast<int>(index);
                            Detail::setFlag(response, 6);
                            ui->nodes[node].checked = true;
                            ui->nodes[node].response = response;
                        }

                        if(options.reorderable == true && currentItemOptions.reorderable == true && response.pressed() == true)
                        {
                            tabState.dragging = index;
                        }

                        bool reorderTab = options.reorderable;

                        if(pointer == nullptr)
                        {
                            reorderTab = false;
                        }

                        if(reorderTab == true)
                        {
                            reorderTab = pointer->isDown();
                        }

                        if(tabState.dragging == std::numeric_limits<size_t>::max())
                        {
                            reorderTab = false;
                        }

                        if(reorderTab == true)
                        {
                            reorderTab = tabPersistentState.lastBounds.contains(pointer->position);
                        }

                        if(tabState.dragging == index)
                        {
                            reorderTab = false;
                        }

                        if(reorderTab == true)
                        {
                            const TabItemOptions & draggedOptions = itemOptions.empty() == true ? defaultItemOptions : itemOptions[tabState.dragging];
                            uint8_t draggedBand = draggedOptions.leading ? 0 : draggedOptions.trailing ? 2 : 1;

                            if(draggedBand == currentBand && draggedOptions.reorderable == true && currentItemOptions.reorderable == true)
                            {
                                auto dragged = std::find(tabState.order.begin(), tabState.order.end(), tabState.dragging);
                                auto target = std::find(tabState.order.begin(), tabState.order.end(), index);

                                if(dragged != tabState.order.end() && target != tabState.order.end())
                                {
                                    std::iter_swap(dragged, target);
                                }
                            }
                        }

                        if(response.released() == true)
                        {
                            tabState.dragging = std::numeric_limits<size_t>::max();
                        }

                        if(options.tooltips == true && currentItemOptions.noTooltip == false && tabPersistentState.lastBounds.width > 0.f)
                        {
                            float decorationWidth = (ui->nodes[node].tabCloseVisible ? ui->currentStyle->metrics.controlHeight * 0.7f : 0.f) + (ui->nodes[node].tabUnsavedDocument ? ui->currentStyle->metrics.fontSize * 0.75f : 0.f);
                            float availableTextWidth = std::max(0.f, tabPersistentState.lastBounds.width - decorationWidth - ui->currentStyle->metrics.padding * 2.f);

                            if(ui->nodes[node].textSize.x > availableTextWidth)
                            {
                                Mosaic::itemTooltip(ui, response, items[index]);
                            }
                        }

                        aggregate.flags |= response.flags;
                    }
                }
            }

            if(showScrollingButtons == true)
            {
                ButtonOptions scrollButtonOptions;
                scrollButtonOptions.width = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
                Response scrollRight = Mosaic::button(ui, Key("Tabs scroll right"), "›", scrollButtonOptions, location);

                if(scrollRight.clicked() == true)
                {
                    Vec2 target;
                    if(Mosaic::scrollOffset(ui, tabState.scrollArea, &target) == true)
                    {
                        target.x += ui->currentStyle->metrics.controlHeight * 4.f;
                        Mosaic::scrollTo(ui, tabState.scrollArea, target);
                    }
                }
            }

            if(options.trailingContent != nullptr)
            {
                options.trailingContent(ui, options.contentUserData);
            }
        }

        if(options.tabListPopupButton == true)
        {
            auto popup = Mosaic::popup(ui, Key("Tab list popup"), tabListPopupOptions);

            if(popup.visible() == true)
            {
                for(size_t index = 0; index != items.size(); ++index)
                {
                    if(open.empty() == false && open[index] == false)
                    {
                        continue;
                    }

                    MenuItemOptions itemOptions;
                    itemOptions.selected = selected != nullptr && *selected == static_cast<int>(index);

                    if(Mosaic::menuItem(ui, Key(index), items[index], itemOptions, location).clicked() == true && selected != nullptr)
                    {
                        *selected = static_cast<int>(index);
                    }
                }
            }
        }

        return aggregate;
    }
    //////////////////////////////////////////////////////////////////////////
    TabBarScope beginTabBar(Context * ui, StringView label, const TabsOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::beginTabBar(ui, {}, label, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TabBarScope beginTabBar(Context * ui, const Key & key, StringView label, const TabsOptions & options, const SourceLocation & location)
    {
        LayoutOptions barLayout;
        barLayout.width = SizeRule::Fill;
        barLayout.gap = ui->currentStyle->metrics.itemSpacing.y;
        size_t barNode = ui->addNode(Detail::NodeKind::Column, key, label, barLayout, location, SemanticRole::Group);
        Context::TabState & state = ui->tabState(ui->nodes[barNode].id);
        bool selectedItemRemoved = state.lastFrame != 0;

        if(state.selectedItem == InvalidId)
        {
            selectedItemRemoved = false;
        }

        if(state.submittedItems.empty() == true)
        {
            selectedItemRemoved = false;
        }

        if(selectedItemRemoved == true)
        {
            selectedItemRemoved = std::find(state.submittedItems.begin(), state.submittedItems.end(), state.selectedItem) == state.submittedItems.end();
        }

        if(selectedItemRemoved == true)
        {
            state.selectedItem = state.submittedItems.front();
        }

        state.previousItems.swap(state.submittedItems);
        state.submittedItems.clear();
        state.lastFrame = ui->frame.number;
        state.options = options;

        state.orderIds.erase(std::remove_if(state.orderIds.begin(), state.orderIds.end(),
                                            [&state](Id item)
                                            {
                                                auto returnedValue = std::find(state.previousItems.begin(), state.previousItems.end(), item) == state.previousItems.end();

                                                return returnedValue;
                                            }),
                             state.orderIds.end());
        for(Id item : state.previousItems)
        {
            if(std::find(state.orderIds.begin(), state.orderIds.end(), item) == state.orderIds.end())
            {
                state.orderIds.push_back(item);
            }
        }

        size_t previousParent = ui->currentParent;
        ui->currentParent = barNode;
        LayoutOptions headerLayout;
        headerLayout.width = SizeRule::Fill;
        headerLayout.gap = 1.f;
        state.headerNode = ui->addNode(Detail::NodeKind::Row, Key("Tab bar headers"), {}, headerLayout, location, SemanticRole::Group);
        state.hostNode = state.headerNode;

        if(options.tabListPopupButton == true)
        {
            ButtonOptions buttonOptions;
            buttonOptions.width = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
            Response button = Mosaic::button(ui, Key("Tab list"), "▾", buttonOptions, location);

            if(button.clicked() == true)
            {
                PopupOptions popupOptions;
                popupOptions.owner = ui->nodes[barNode].id;

                if(Mosaic::debugBounds(ui, button.id, &popupOptions.anchor) == false)
                {
                    return {};
                }

                popupOptions.placement = PopupPlacement::Below;
                Mosaic::openPopup(ui, Key("Tab list popup"), popupOptions);
            }
        }

        if(options.leadingContent != nullptr)
        {
            options.leadingContent(ui, options.contentUserData);
        }

        ui->currentParent = previousParent;

        uint64_t token = ui->pushScope(barNode, ui->currentStyle, ui->currentDisabled);
        ui->nodes[barNode].tabSelectedOverline = options.selectedOverline;

        return {ui, token, ui->nodes[barNode].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope beginTabItem(Context * ui, StringView label, bool * open, const TabItemOptions & options, const SourceLocation & location)
    {
        auto returnedValue = Mosaic::beginTabItem(ui, {}, label, open, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope beginTabItem(Context * ui, const Key & key, StringView label, bool * open, const TabItemOptions & options, const SourceLocation & location)
    {
        if(ui->currentParent >= ui->nodes.size())
        {
            return {};
        }

        Context::Node & barNode = ui->nodes[ui->currentParent];
        Context::TabState & state = ui->tabState(barNode.id);

        if(barNode.kind != Detail::NodeKind::Column)
        {
            ui->frame.diagnostics.emplace_back("beginTabItem must be called directly inside beginTabBar");

            return {};
        }

        if(state.lastFrame != ui->frame.number)
        {
            ui->frame.diagnostics.emplace_back("beginTabItem must be called directly inside beginTabBar");

            return {};
        }

        if(state.headerNode >= ui->nodes.size())
        {
            ui->frame.diagnostics.emplace_back("beginTabItem must be called directly inside beginTabBar");

            return {};
        }

        if(ui->nodes[state.headerNode].parent != ui->currentParent)
        {
            ui->frame.diagnostics.emplace_back("beginTabItem must be called directly inside beginTabBar");

            return {};
        }

        if(open != nullptr && *open == false)
        {
            return {};
        }

        size_t barIndex = ui->currentParent;
        ui->currentParent = state.headerNode;
        size_t node = ui->addNode(Detail::NodeKind::Tab, key, label, {}, location, SemanticRole::Tab, true);
        ui->currentParent = barIndex;

        Context::Node & tabNode = ui->nodes[node];
        state.submittedItems.push_back(tabNode.id);

        if(std::find(state.orderIds.begin(), state.orderIds.end(), tabNode.id) == state.orderIds.end())
        {
            state.orderIds.push_back(tabNode.id);
        }

        bool newItem = std::find(state.previousItems.begin(), state.previousItems.end(), tabNode.id) == state.previousItems.end();

        if(state.selectedItem == InvalidId || options.setSelected == true || (newItem == true && state.options.autoSelectNewTabs == true))
        {
            state.selectedItem = tabNode.id;
        }

        tabNode.checked = state.selectedItem == tabNode.id;
        tabNode.disabled = tabNode.disabled || options.disabled;
        tabNode.tabSelectedOverline = barNode.tabSelectedOverline;
        tabNode.tabLeading = options.leading;
        tabNode.tabTrailing = options.trailing;
        tabNode.tabUnsavedDocument = options.unsavedDocument;

        const PointerState * pointer = ui->input.primaryPointer();
        Context::Persistent & persistentState = ui->state(tabNode);
        Rect interactionBounds = persistentState.lastBounds;
        bool tabHovered = pointer != nullptr && pointer->type != PointerType::Touch && interactionBounds.contains(pointer->position);
        float closeMinimumWidth = tabNode.checked ? tabNode.style->metrics.tabCloseButtonMinimumWidthSelected : tabNode.style->metrics.tabCloseButtonMinimumWidthUnselected;
        float closeExtent = std::min(interactionBounds.height, ui->currentStyle->metrics.controlHeight * 0.7f);
        tabNode.tabCloseVisible = open != nullptr && options.closeButton == true && (closeMinimumWidth < 0.f || (tabHovered && interactionBounds.width >= std::max(closeExtent, closeMinimumWidth)));
        Rect closeBounds;

        if(tabNode.tabCloseVisible == true && interactionBounds.empty() == false)
        {
            closeBounds = {interactionBounds.right() - closeExtent, interactionBounds.y, closeExtent, interactionBounds.height};
            interactionBounds.width = std::max(0.f, interactionBounds.width - closeExtent);
            tabNode.tabCloseHovered = pointer != nullptr && pointer->type != PointerType::Touch && closeBounds.contains(pointer->position);
        }

        Response response = ui->interact(node, true, interactionBounds.empty() == true ? nullptr : &interactionBounds);

        if(response.clicked() == true)
        {
            state.selectedItem = tabNode.id;
            tabNode.checked = true;
            Detail::setFlag(response, 6);
        }

        auto currentOrder = std::find(state.orderIds.begin(), state.orderIds.end(), tabNode.id);

        if(state.options.reorderable == true && options.reorderable == true && response.pressed() == true && currentOrder != state.orderIds.end())
        {
            state.dragging = static_cast<size_t>(currentOrder - state.orderIds.begin());
        }

        bool reorderTab = state.options.reorderable;

        if(pointer == nullptr)
        {
            reorderTab = false;
        }

        if(reorderTab == true)
        {
            reorderTab = pointer->isDown();
        }

        if(state.dragging >= state.orderIds.size())
        {
            reorderTab = false;
        }

        if(reorderTab == true)
        {
            reorderTab = persistentState.lastBounds.contains(pointer->position);
        }

        if(currentOrder == state.orderIds.end())
        {
            reorderTab = false;
        }

        if(reorderTab == true)
        {
            size_t currentIndex = static_cast<size_t>(currentOrder - state.orderIds.begin());
            const Context::Node * draggedNode = ui->findFrameNode(state.orderIds[state.dragging]);

            if(currentIndex != state.dragging && draggedNode != nullptr && draggedNode->tabLeading == tabNode.tabLeading && draggedNode->tabTrailing == tabNode.tabTrailing)
            {
                std::swap(state.orderIds[state.dragging], state.orderIds[currentIndex]);
                state.dragging = currentIndex;
            }
        }

        bool primaryClose = tabNode.tabCloseVisible && pointer != nullptr && pointer->isPressed(PointerButton::Primary) && closeBounds.contains(pointer->position);
        bool middleClose = open != nullptr && state.options.closeWithMiddleMouse == true && options.closeWithMiddleMouse == true && pointer != nullptr && pointer->isPressed(PointerButton::Middle) && persistentState.lastBounds.contains(pointer->position);

        if(open != nullptr && (primaryClose == true || middleClose == true))
        {
            *open = false;

            if(state.selectedItem == tabNode.id)
            {
                state.selectedItem = InvalidId;
            }

            state.submittedItems.erase(std::remove(state.submittedItems.begin(), state.submittedItems.end(), tabNode.id), state.submittedItems.end());
            state.orderIds.erase(std::remove(state.orderIds.begin(), state.orderIds.end(), tabNode.id), state.orderIds.end());
            tabNode.checked = false;
            Detail::setFlag(response, 6);
        }

        if(response.released() == true)
        {
            state.dragging = std::numeric_limits<size_t>::max();
        }

        if(state.options.tooltips == true && options.noTooltip == false && persistentState.lastBounds.width > 0.f)
        {
            float decorationWidth = (tabNode.tabCloseVisible ? ui->currentStyle->metrics.controlHeight * 0.7f : 0.f) + (tabNode.tabUnsavedDocument ? ui->currentStyle->metrics.fontSize * 0.75f : 0.f);
            float availableTextWidth = std::max(0.f, persistentState.lastBounds.width - decorationWidth - ui->currentStyle->metrics.padding * 2.f);

            if(tabNode.textSize.x > availableTextWidth)
            {
                Mosaic::itemTooltip(ui, response, label);
            }
        }

        tabNode.response = response;

        if(tabNode.checked == false)
        {
            return {};
        }

        if((open != nullptr && *open == false))
        {
            return {};
        }

        LayoutOptions contentLayout;
        contentLayout.width = SizeRule::Fill;
        size_t contentNode = ui->addNode(Detail::NodeKind::Column, Key(tabNode.id), {}, contentLayout, location, SemanticRole::Group);
        uint64_t token = ui->pushScope(contentNode, ui->currentStyle, ui->currentDisabled);

        return {ui, token, ui->nodes[contentNode].id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    void setTabItemClosed(Context * ui, const Key & key) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->currentParent >= ui->nodes.size())
        {
            return;
        }

        Context::Node & barNode = ui->nodes[ui->currentParent];

        if(barNode.kind != Detail::NodeKind::Column)
        {
            ui->frame.diagnostics.emplace_back("setTabItemClosed must be called inside beginTabBar");

            return;
        }

        Context::TabState & state = ui->tabState(barNode.id);

        if(state.headerNode >= ui->nodes.size())
        {
            ui->frame.diagnostics.emplace_back("setTabItemClosed must be called inside beginTabBar");

            return;
        }

        Id item = combineId(ui->nodes[state.headerNode].id, key.value());
        auto eraseItem = [item](IdVector & values)
        {
            values.erase(std::remove(values.begin(), values.end(), item), values.end());
        };
        eraseItem(state.previousItems);
        eraseItem(state.submittedItems);
        eraseItem(state.orderIds);

        if(state.selectedItem == item)
        {
            state.selectedItem = state.orderIds.empty() == true ? InvalidId : state.orderIds.front();
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Detail::finishTabBar(Context * ui, Id tabBar) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(tabBar == InvalidId)
        {
            return;
        }

        if(ui->findFrameNodeIndex(tabBar) >= ui->nodes.size())
        {
            return;
        }

        Context::TabState & state = ui->tabState(tabBar);

        if(state.lastFrame != ui->frame.number)
        {
            return;
        }

        if(state.headerNode >= ui->nodes.size())
        {
            return;
        }

        if(state.options.trailingContent != nullptr)
        {
            TabBarContentCallback callback = state.options.trailingContent;
            void * userData = state.options.contentUserData;
            size_t headerNode = state.headerNode;
            size_t previousParent = ui->currentParent;
            ui->currentParent = headerNode;
            callback(ui, userData);
            ui->currentParent = previousParent;
        }

        // User content is allowed to create nested tab bars. That can grow the
        // tab-state table, so reacquire this entry after invoking the callback.
        Context::TabState & currentState = ui->tabState(tabBar);
        currentState.orderIds.erase(std::remove_if(currentState.orderIds.begin(), currentState.orderIds.end(),
                                                   [&currentState](Id item)
                                                   {
                                                       auto returnedValue = std::find(currentState.submittedItems.begin(), currentState.submittedItems.end(), item) == currentState.submittedItems.end();

                                                       return returnedValue;
                                                   }),
                                    currentState.orderIds.end());
        for(Id item : currentState.submittedItems)
        {
            if(std::find(currentState.orderIds.begin(), currentState.orderIds.end(), item) == currentState.orderIds.end())
            {
                currentState.orderIds.push_back(item);
            }
        }

        Context::Node & header = ui->nodes[currentState.headerNode];
        SizeVector & prefix = currentState.prefixNodes;
        SizeVector & suffix = currentState.suffixNodes;
        SizeVector & leadingTabs = currentState.leadingNodes;
        SizeVector & centralTabs = currentState.centralNodes;
        SizeVector & trailingTabs = currentState.trailingNodes;
        SizeVector & ordered = currentState.layoutNodes;
        prefix.clear();
        suffix.clear();
        leadingTabs.clear();
        centralTabs.clear();
        trailingTabs.clear();
        ordered.clear();
        bool foundTab = false;
        for(size_t child = header.firstChild; child < ui->nodes.size(); child = ui->nodes[child].nextSibling)
        {
            const Context::Node & node = ui->nodes[child];

            if(node.kind == Detail::NodeKind::Tab && node.semanticRole == SemanticRole::Tab)
            {
                foundTab = true;
                continue;
            }

            (foundTab ? suffix : prefix).push_back(child);
        }

        float requestedWidth = 0.f;
        float minimumWidth = 0.f;
        size_t tabCount = 0;
        for(uint8_t band = 0; band != 3; ++band)
        {
            for(Id item : currentState.orderIds)
            {
                size_t nodeIndex = ui->findFrameNodeIndex(item);

                if(nodeIndex >= ui->nodes.size())
                {
                    continue;
                }

                if(ui->nodes[nodeIndex].parent != currentState.headerNode)
                {
                    continue;
                }

                Context::Node & node = ui->nodes[nodeIndex];
                uint8_t nodeBand = node.tabLeading ? 0 : node.tabTrailing ? 2 : 1;

                if(nodeBand != band)
                {
                    continue;
                }

                SizeVector & bandItems = nodeBand == 0 ? leadingTabs : nodeBand == 1 ? centralTabs : trailingTabs;
                bandItems.push_back(nodeIndex);
                ++tabCount;
                float decorationWidth = (node.tabCloseVisible ? node.style->metrics.controlHeight * 0.7f : 0.f) + (node.tabUnsavedDocument ? node.style->metrics.fontSize * 0.75f : 0.f);
                requestedWidth += std::max(node.textSize.x + node.style->metrics.framePadding.left + node.style->metrics.framePadding.right + decorationWidth, node.style->metrics.tabMinimumWidthBase);
                minimumWidth += std::max(node.style->metrics.tabMinimumWidthForShrink, decorationWidth + node.style->metrics.controlHeight);
            }
        }

        if(tabCount > 1)
        {
            requestedWidth += static_cast<float>(tabCount - 1);
            minimumWidth += static_cast<float>(tabCount - 1);
        }

        TabFittingPolicy policy = currentState.options.fitAvailableWidth ? TabFittingPolicy::Shrink : currentState.options.fittingPolicy;
        const Context::Persistent & barState = ui->state(tabBar);
        float headerWidth = barState.lastBounds.width > 0.f ? barState.lastBounds.width : ui->viewport.bounds.width;
        auto nodesWidth = [ui, headerWidth](const SizeVector & nodes)
        {
            float width = 0.f;
            for(size_t nodeIndex : nodes)
            {
                const Context::Persistent * persistent = ui->findState(ui->nodes[nodeIndex].id);
                width += persistent != nullptr && persistent->lastBounds.width > 0.f ? persistent->lastBounds.width : ui->measureNode(nodeIndex, {headerWidth, ui->currentStyle->metrics.controlHeight}).x;
            }

            return width;
        };
        float fixedWidth = nodesWidth(prefix) + nodesWidth(suffix);
        size_t fixedCount = prefix.size() + suffix.size();
        float fixedGaps = fixedCount > 0 ? static_cast<float>(fixedCount) : 0.f;
        float availableWidth = std::max(0.f, headerWidth - fixedWidth - fixedGaps);

        if(policy == TabFittingPolicy::Mixed && requestedWidth > availableWidth)
        {
            policy = minimumWidth <= availableWidth ? TabFittingPolicy::Shrink : TabFittingPolicy::Scroll;
        }

        if(currentState.options.scrollToFit == false && policy == TabFittingPolicy::Scroll)
        {
            policy = TabFittingPolicy::Mixed;
        }

        Id previousScrollArea = currentState.scrollArea;
        Vec2 previousRange;
        bool hasPreviousRange = previousScrollArea != InvalidId && Mosaic::scrollRange(ui, previousScrollArea, &previousRange);
        bool previousOverflow = hasPreviousRange == true && previousRange.x > 0.f;
        bool showScrollingButtons = currentState.options.scrollingButtons && policy == TabFittingPolicy::Scroll && (requestedWidth > availableWidth || previousOverflow == true);

        if(showScrollingButtons == true)
        {
            availableWidth = std::max(0.f, availableWidth - ui->currentStyle->metrics.controlHeight * 2.f - 2.f);
        }

        if(policy == TabFittingPolicy::Mixed && requestedWidth > availableWidth)
        {
            policy = minimumWidth <= availableWidth ? TabFittingPolicy::Shrink : TabFittingPolicy::Scroll;
        }

        for(const SizeVector * band : {&leadingTabs, &centralTabs, &trailingTabs})
        {
            for(size_t nodeIndex : *band)
            {
                Context::Node & node = ui->nodes[nodeIndex];
                node.layout.width = policy == TabFittingPolicy::Shrink && tabCount > 0 ? Dimension(SizeRule::Fill) : Dimension(SizeRule::Content);
                node.layout.minimum.x = policy == TabFittingPolicy::Shrink ? node.style->metrics.tabMinimumWidthForShrink : node.style->metrics.tabMinimumWidthBase;
            }
        }

        ordered.insert(ordered.end(), prefix.begin(), prefix.end());
        ordered.insert(ordered.end(), leadingTabs.begin(), leadingTabs.end());
        bool scrollLeftClicked = false;
        bool scrollRightClicked = false;
        size_t scrollLeftNode = std::numeric_limits<size_t>::max();
        size_t scrollRightNode = std::numeric_limits<size_t>::max();

        if(showScrollingButtons == true)
        {
            size_t previousParent = ui->currentParent;
            ui->currentParent = currentState.headerNode;
            ButtonOptions buttonOptions;
            buttonOptions.width = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
            Response left = Mosaic::button(ui, Key("Tabs scroll left"), "‹", buttonOptions);
            Response right = Mosaic::button(ui, Key("Tabs scroll right"), "›", buttonOptions);
            scrollLeftClicked = left.clicked();
            scrollRightClicked = right.clicked();
            scrollLeftNode = ui->findFrameNodeIndex(left.id);
            scrollRightNode = ui->findFrameNodeIndex(right.id);
            ui->currentParent = previousParent;

            if(scrollLeftNode < ui->nodes.size())
            {
                ordered.push_back(scrollLeftNode);
            }
        }

        if(policy == TabFittingPolicy::Scroll && centralTabs.empty() == false)
        {
            size_t previousParent = ui->currentParent;
            size_t headerNode = currentState.headerNode;
            Id scrollId = InvalidId;
            Id rowId = InvalidId;
            ui->currentParent = headerNode;
            {
                ScrollOptions scrollOptions;
                scrollOptions.axes = ScrollAxes::Horizontal;
                scrollOptions.contentOrientation = Orientation::Horizontal;
                scrollOptions.visibility = ScrollbarVisibility::Hidden;
                scrollOptions.wheelStep = ui->currentStyle->metrics.controlHeight * 2.f;
                LayoutOptions scrollLayout;
                scrollLayout.width = SizeRule::Fill;
                scrollLayout.height = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
                scrollLayout.gap = 0.f;
                auto scroll = Mosaic::scrollArea(ui, "Composable tab scroll area", scrollOptions, scrollLayout);
                scrollId = scroll.id();
                LayoutOptions rowLayout;
                rowLayout.gap = 1.f;
                auto tabRow = Mosaic::row(ui, rowLayout);
                rowId = tabRow.id();
            }
            ui->currentParent = previousParent;
            currentState.scrollArea = scrollId;
            size_t scrollNode = ui->findFrameNodeIndex(scrollId);
            size_t rowNode = ui->findFrameNodeIndex(rowId);

            if(scrollNode < ui->nodes.size() && rowNode < ui->nodes.size())
            {
                Context::Node & row = ui->nodes[rowNode];
                row.firstChild = centralTabs.front();
                row.lastChild = centralTabs.back();
                for(size_t index = 0; index != centralTabs.size(); ++index)
                {
                    Context::Node & tab = ui->nodes[centralTabs[index]];
                    tab.parent = rowNode;
                    tab.parentId = row.id;
                    tab.nextSibling = index + 1 < centralTabs.size() ? centralTabs[index + 1] : std::numeric_limits<size_t>::max();
                }
                ordered.push_back(scrollNode);

                if(currentState.selectedItem != InvalidId)
                {
                    Mosaic::scrollToItem(ui, scrollId, currentState.selectedItem);
                }

                if(scrollLeftClicked == true || scrollRightClicked == true)
                {
                    Vec2 target;
                    if(Mosaic::scrollOffset(ui, scrollId, &target) == true)
                    {
                        float step = ui->currentStyle->metrics.controlHeight * 4.f;
                        target.x += scrollRightClicked ? step : -step;
                        Mosaic::scrollTo(ui, scrollId, target);
                    }
                }
            }
            else
            {
                ordered.insert(ordered.end(), centralTabs.begin(), centralTabs.end());
            }
        }
        else
        {
            currentState.scrollArea = InvalidId;
            ordered.insert(ordered.end(), centralTabs.begin(), centralTabs.end());
        }

        if(scrollRightNode < ui->nodes.size())
        {
            ordered.push_back(scrollRightNode);
        }

        ordered.insert(ordered.end(), trailingTabs.begin(), trailingTabs.end());
        ordered.insert(ordered.end(), suffix.begin(), suffix.end());

        Context::Node & finalHeader = ui->nodes[currentState.headerNode];
        finalHeader.firstChild = ordered.empty() == true ? std::numeric_limits<size_t>::max() : ordered.front();
        finalHeader.lastChild = ordered.empty() == true ? std::numeric_limits<size_t>::max() : ordered.back();
        for(size_t index = 0; index != ordered.size(); ++index)
        {
            ui->nodes[ordered[index]].nextSibling = index + 1 < ordered.size() ? ordered[index + 1] : std::numeric_limits<size_t>::max();
        }

        if(currentState.options.tabListPopupButton == true)
        {
            PopupOptions popupOptions;
            popupOptions.owner = tabBar;
            auto popup = Mosaic::popup(ui, Key("Tab list popup"), popupOptions);

            if(popup.visible() == true)
            {
                for(Id item : currentState.orderIds)
                {
                    const Context::Node * node = ui->findFrameNode(item);

                    if(node == nullptr)
                    {
                        continue;
                    }

                    MenuItemOptions itemOptions;
                    itemOptions.selected = currentState.selectedItem == item;

                    if(Mosaic::menuItem(ui, Key(item), node->label, itemOptions).clicked() == true)
                    {
                        currentState.selectedItem = item;
                    }
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Response tabItemButton(Context * ui, const Key & key, StringView label, const TabItemButtonOptions & options, const SourceLocation & location)
    {
        LayoutOptions layout;
        layout.width = options.width;
        size_t node = ui->addNode(Detail::NodeKind::Tab, key, label, layout, location, SemanticRole::Button, true);
        ui->nodes[node].checked = false;
        Detail::ItemBehaviorOptions behavior;
        behavior.repeat = options.repeat;
        Response response = ui->interact(node, behavior);
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response Detail::colorEditor(Context * ui, StringView label, Color * color, bool includeAlpha, const ColorEditOptions & options, const SourceLocation & location)
    {
        auto editorScope = Mosaic::scope(ui, {}, location);
        LayoutOptions editorLayout;
        editorLayout.width = options.width;
        editorLayout.height = options.height;
        size_t node = ui->addNode(Detail::NodeKind::ColorEdit, Key("ColorEdit"), label, editorLayout, location, SemanticRole::Button, true);
        Context::Node & editorNode = ui->nodes[node];
        editorNode.labelPlacement = options.label ? options.labelPlacement : LabelPlacement::Hidden;
        editorNode.colorShowInputs = options.showInputs;
        editorNode.colorShowPreview = options.showPreview && options.smallPreview;
        editorNode.colorInputMode = options.inputMode;
        editorNode.colorAlphaBackground = options.alphaBackground;
        editorNode.colorAlphaPreviewHalf = options.alphaPreviewHalf;
        editorNode.colorBorder = options.border;
        editorNode.colorMarkers = options.colorMarkers;
        editorNode.colorHdr = options.hdr;
        editorNode.colorComponents = options.inputMode == ColorInputMode::Hexadecimal ? 1U : (includeAlpha ? 4U : 3U);
        Context::Persistent & initialState = ui->state(editorNode);
        ColorEditorState & colorState = ui->colorEditorState(editorNode.id);
        Detail::ColorEditGeometry interactionGeometry = Detail::colorEditGeometry(editorNode, initialState.lastBounds);
        Response response = ui->interact(node, true, &interactionGeometry.control);

        if(color == nullptr)
        {
            ui->nodes[node].response = response;

            return response;
        }

        if(colorState.initialized == false || colorState.last != *color)
        {
            float synchronizedHue = 0.f;
            float synchronizedSaturation = 0.f;
            float synchronizedValue = 0.f;
            Detail::rgbToHsv(*color, synchronizedHue, synchronizedSaturation, synchronizedValue);

            if(colorState.initialized == false || synchronizedSaturation > 0.0001f)
            {
                colorState.hue = synchronizedHue;
            }

            if(colorState.initialized == false || synchronizedValue > 0.0001f)
            {
                colorState.saturation = synchronizedSaturation;
            }

            colorState.value = synchronizedValue;
            colorState.last = *color;
            colorState.initialized = true;
            Detail::synchronizeColorText(colorState, *color, includeAlpha);
        }

        float hue = colorState.hue;
        float saturation = colorState.saturation;
        float value = colorState.value;
        Key pickerPopupKey("Color picker popup");
        bool pickerWasOpen = colorState.pickerOpen;
        bool pickerOpen = Mosaic::isPopupOpen(ui, pickerPopupKey, response.id);
        bool changed = false;
        const PointerState * editorPointer = ui->input.primaryPointer();

        if(response.pressed() == true && editorPointer != nullptr)
        {
            colorState.activeChannel = 0xfeU;
            for(uint8_t channel = 0; channel != interactionGeometry.channelCount; ++channel)
            {
                if(interactionGeometry.channels[channel].contains(editorPointer->position) == true)
                {
                    colorState.activeChannel = channel;
                    initialState.dragStartPosition = editorPointer->position;
                    Array<float, 4> displayChannels = options.inputMode == ColorInputMode::HsvFloat ? Array<float, 4>{hue, saturation, value, color->a} : Array<float, 4>{color->r, color->g, color->b, color->a};
                    colorState.dragStartValue = displayChannels[channel];
                    break;
                }
            }
            bool previewPressed = colorState.activeChannel == 0xfeU;

            if(options.showPreview == false)
            {
                previewPressed = false;
            }

            if(options.smallPreview == false)
            {
                previewPressed = false;
            }

            if(previewPressed == true)
            {
                previewPressed = interactionGeometry.preview.contains(editorPointer->position);
            }

            if(previewPressed == true)
            {
                colorState.activeChannel = 0xffU;
            }
        }

        bool dragChannel = Detail::capturedPointerActive(ui, editorPointer, response.id);

        if(colorState.activeChannel >= editorNode.colorComponents)
        {
            dragChannel = false;
        }

        if(options.inputMode == ColorInputMode::Hexadecimal)
        {
            dragChannel = false;
        }

        if(dragChannel == true)
        {
            bool alphaChannel = colorState.activeChannel == 3;
            bool bounded = options.hdr == false || alphaChannel == true || options.inputMode == ColorInputMode::HsvFloat;
            float updated = colorState.dragStartValue + (editorPointer->position.x - initialState.dragStartPosition.x) / 255.f;

            if(bounded == true)
            {
                updated = std::clamp(updated, 0.f, 1.f);
            }
            else
            {
                updated = std::max(0.f, updated);
            }

            float previous = 0.f;

            if(options.inputMode == ColorInputMode::HsvFloat)
            {
                float * channels[] = {&hue, &saturation, &value, &color->a};
                previous = *channels[colorState.activeChannel];
                *channels[colorState.activeChannel] = updated;
                float alpha = color->a;
                *color = Detail::hsvToRgb(hue, saturation, value, includeAlpha ? alpha : 1.f);
            }
            else
            {
                float * channels[] = {&color->r, &color->g, &color->b, &color->a};
                previous = *channels[colorState.activeChannel];
                *channels[colorState.activeChannel] = updated;
            }

            if(updated != previous)
            {
                float updatedHue = hue;
                float updatedSaturation = saturation;

                if(options.inputMode != ColorInputMode::HsvFloat)
                {
                    Detail::rgbToHsv(*color, updatedHue, updatedSaturation, value);

                    if(updatedSaturation > 0.0001f)
                    {
                        hue = updatedHue;
                    }

                    if(value > 0.0001f)
                    {
                        saturation = updatedSaturation;
                    }
                }

                Detail::synchronizeColorText(colorState, *color, includeAlpha);
                changed = true;
                Detail::setFlag(response, 6);
                Detail::setFlag(response, 7);
            }
        }

        Color sampledColor;
        if(ui->platform->consumeScreenColorPick(response.id, &sampledColor) == true)
        {
            float preservedAlpha = color->a;

            *color = sampledColor;

            if(includeAlpha == false)
            {
                color->a = preservedAlpha;
            }

            Detail::rgbToHsv(*color, hue, saturation, value);
            Detail::synchronizeColorText(colorState, *color, includeAlpha);
            changed = true;
        }

        if(options.picker == true && options.openPickerOnClick == true && response.clicked() == true && colorState.activeChannel == 0xffU)
        {
            pickerOpen = !pickerOpen;

            if(pickerOpen == true)
            {
                constexpr float pickerWidth = 356.f;
                float pickerHeight = includeAlpha ? 445.f : 405.f;
                PopupOptions pickerOptions;
                pickerOptions.owner = response.id;
                pickerOptions.anchor = initialState.lastBounds;
                pickerOptions.minimumSize = {pickerWidth, pickerHeight};
                pickerOptions.maximumSize = {pickerWidth, pickerHeight};
                pickerOptions.closeOnSelection = false;
                Mosaic::openPopup(ui, pickerPopupKey, pickerOptions);
                Detail::setFlag(response, 7);
                ui->frame.events.push_back({EventType::BeginEdit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }
            else
            {
                Mosaic::closePopup(ui, pickerPopupKey, response.id);
                Detail::setFlag(response, 8);
                ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
            }
        }

        if(response.released() == true)
        {
            colorState.activeChannel = 0xfeU;
        }

        if(pickerOpen == true)
        {
            constexpr float pickerWidth = 356.f;
            float pickerHeight = includeAlpha ? 445.f : 405.f;
            PopupOptions pickerOptions;
            pickerOptions.owner = response.id;
            pickerOptions.anchor = initialState.lastBounds;
            pickerOptions.minimumSize = {pickerWidth, pickerHeight};
            pickerOptions.maximumSize = {pickerWidth, pickerHeight};
            pickerOptions.closeOnSelection = false;
            {
                auto picker = Mosaic::popup(ui, pickerPopupKey, pickerOptions, location);

                if(picker.visible() == true)
                {
                    LayoutOptions pickerContentLayout;
                    pickerContentLayout.width = SizeRule::Fill;
                    pickerContentLayout.height = SizeRule::Fill;
                    pickerContentLayout.gap = 4.f;
                    auto pickerContent = Mosaic::column(ui, pickerContentLayout, location);
                    constexpr float stripWidth = 334.f;
                    String headerLabel(label.empty() == true ? StringView("Color") : label);
                    headerLabel += includeAlpha ? "  RGBA" : "  RGB";
                    {
                        auto header = Mosaic::row(ui, {}, location);
                        Mosaic::text(ui, headerLabel, location);
                        float labelWidth = ui->estimateText(headerLabel, *ui->currentStyle).x;
                        float fillerWidth = std::max(0.f, stripWidth - labelWidth - 28.f);
                        {
                            auto filler = Mosaic::column(ui, Detail::fixedLayout(fillerWidth, 20.f), location);
                        }
                        {
                            auto close = Mosaic::canvas(ui, "Close color picker", Detail::fixedLayout(20.f, 20.f), location);
                            const Context::Node & closeNode = ui->nodes.back();
                            Response closeResponse = closeNode.response;

                            if(closeResponse.hovered() == true)
                            {
                                close.roundedRect({0.f, 0.f, 20.f, 20.f}, 4.f, ui->currentStyle->colors.panelHovered);
                            }

                            Color closeColor = closeResponse.hovered() ? ui->currentStyle->colors.text : ui->currentStyle->colors.textDisabled;
                            close.line({6.f, 6.f}, {14.f, 14.f}, 1.5f, closeColor);
                            close.line({14.f, 6.f}, {6.f, 14.f}, 1.5f, closeColor);

                            if(closeResponse.clicked() == true)
                            {
                                Mosaic::closePopup(ui, pickerPopupKey, response.id);
                                pickerOpen = false;
                            }
                        }
                    }

                    constexpr float saturationWidth = 306.f;
                    constexpr float pickerAreaHeight = 156.f;
                    constexpr float hueWidth = 20.f;
                    {
                        auto pickerRow = Mosaic::row(ui, {}, location);
                        const PointerState * pointer = ui->input.primaryPointer();
                        {
                            auto saturationCanvas = Mosaic::canvas(ui, "Saturation and value", Detail::fixedLayout(saturationWidth, pickerAreaHeight), location);
                            size_t saturationNode = ui->nodes.size() - 1;
                            Detail::contextMenuTrigger(ui, response, node, saturationNode);
                            Rect saturationBounds = {0.f, 0.f, saturationWidth, pickerAreaHeight};
                            Color hueColor = Detail::hsvToRgb(hue, 1.f, 1.f);
                            Detail::canvasGradient(saturationCanvas, saturationBounds, Color{1.f, 1.f, 1.f, 1.f}, hueColor, hueColor, Color{1.f, 1.f, 1.f, 1.f});
                            Detail::canvasGradient(saturationCanvas, saturationBounds, Color{0.f, 0.f, 0.f, 0.f}, Color{0.f, 0.f, 0.f, 0.f}, Color{0.f, 0.f, 0.f, 1.f}, Color{0.f, 0.f, 0.f, 1.f});
                            Detail::canvasOutline(saturationCanvas, saturationBounds, ui->currentStyle->colors.borderStrong);

                            float selectionX = saturation * saturationWidth;
                            float selectionY = (1.f - value) * pickerAreaHeight;
                            Rect outerMarker = {selectionX - 5.f, selectionY - 5.f, 10.f, 10.f};
                            Detail::canvasOutline(saturationCanvas, outerMarker, Color{0.f, 0.f, 0.f, 0.88f}, 3.f);
                            Detail::canvasOutline(saturationCanvas, outerMarker, Color{1.f, 1.f, 1.f, 0.96f});

                            if(pointer != nullptr && ui->captured == saturationCanvas.id() && pointer->isDown() == true)
                            {
                                Vec2 local;
                                if(saturationCanvas.localPointerPosition(&local) == true)
                                {
                                    float updatedSaturation = std::clamp(local.x / saturationWidth, 0.f, 1.f);
                                    float updatedValue = 1.f - std::clamp(local.y / pickerAreaHeight, 0.f, 1.f);

                                    if(updatedSaturation != saturation || updatedValue != value)
                                    {
                                        saturation = updatedSaturation;
                                        value = updatedValue;
                                        float alpha = color->a;
                                        *color = Detail::hsvToRgb(hue, saturation, value, alpha);
                                        changed = true;
                                    }
                                }
                            }
                        }

                        {
                            auto hueCanvas = Mosaic::canvas(ui, "Hue", Detail::fixedLayout(hueWidth, pickerAreaHeight), location);
                            size_t hueNode = ui->nodes.size() - 1;
                            Detail::contextMenuTrigger(ui, response, node, hueNode);
                            constexpr Detail::HueColorArray hueColors = {Color{1.f, 0.f, 0.f, 1.f}, Color{1.f, 1.f, 0.f, 1.f}, Color{0.f, 1.f, 0.f, 1.f}, Color{0.f, 1.f, 1.f, 1.f}, Color{0.f, 0.f, 1.f, 1.f}, Color{1.f, 0.f, 1.f, 1.f}, Color{1.f, 0.f, 0.f, 1.f}};
                            float hueSegment = pickerAreaHeight / 6.f;
                            for(size_t index = 0; index != 6; ++index)
                            {
                                Rect segment = {0.f, hueSegment * static_cast<float>(index), hueWidth, hueSegment + 0.5f};
                                Detail::canvasGradient(hueCanvas, segment, hueColors[index], hueColors[index], hueColors[index + 1], hueColors[index + 1]);
                            }
                            Detail::canvasOutline(hueCanvas, {0.f, 0.f, hueWidth, pickerAreaHeight}, ui->currentStyle->colors.borderStrong);
                            float hueY = hue * pickerAreaHeight;
                            Rect hueMarker = {-2.f, hueY - 2.f, hueWidth + 4.f, 4.f};
                            Detail::canvasOutline(hueCanvas, hueMarker, Color{0.f, 0.f, 0.f, 0.9f}, 3.f);
                            Detail::canvasOutline(hueCanvas, hueMarker, Color{1.f, 1.f, 1.f, 1.f});

                            if(pointer != nullptr && ui->captured == hueCanvas.id() && pointer->isDown() == true)
                            {
                                Vec2 local;
                                if(hueCanvas.localPointerPosition(&local) == true)
                                {
                                    float updatedHue = std::clamp(local.y / pickerAreaHeight, 0.f, 1.f);

                                    if(updatedHue != hue)
                                    {
                                        hue = updatedHue >= 1.f ? 0.f : updatedHue;
                                        float alpha = color->a;
                                        *color = Detail::hsvToRgb(hue, saturation, value, alpha);
                                        changed = true;
                                    }
                                }
                            }
                        }
                    }

                    constexpr float stripHeight = 14.f;
                    const PointerState * pointer = ui->input.primaryPointer();

                    if(includeAlpha == true)
                    {
                        auto alphaCanvas = Mosaic::canvas(ui, "Alpha", Detail::fixedLayout(stripWidth, stripHeight), location);
                        size_t alphaNode = ui->nodes.size() - 1;
                        Detail::contextMenuTrigger(ui, response, node, alphaNode);
                        Rect alphaBounds = {0.f, 0.f, stripWidth, stripHeight};
                        Detail::canvasCheckerboard(alphaCanvas, alphaBounds, 7.f);
                        Color transparent = {color->r, color->g, color->b, 0.f};
                        Color opaque = {color->r, color->g, color->b, 1.f};
                        Detail::canvasGradient(alphaCanvas, alphaBounds, transparent, opaque, opaque, transparent);
                        Detail::canvasOutline(alphaCanvas, alphaBounds, ui->currentStyle->colors.borderStrong);
                        float alphaX = color->a * stripWidth;
                        Rect alphaMarker = {alphaX - 2.f, -2.f, 4.f, stripHeight + 4.f};
                        Detail::canvasOutline(alphaCanvas, alphaMarker, Color{0.f, 0.f, 0.f, 0.9f}, 3.f);
                        Detail::canvasOutline(alphaCanvas, alphaMarker, Color{1.f, 1.f, 1.f, 1.f});

                        if(pointer != nullptr && ui->captured == alphaCanvas.id() && pointer->isDown() == true)
                        {
                            Vec2 local;
                            if(alphaCanvas.localPointerPosition(&local) == true)
                            {
                                float updatedAlpha = std::clamp(local.x / stripWidth, 0.f, 1.f);

                                if(updatedAlpha != color->a)
                                {
                                    color->a = updatedAlpha;
                                    changed = true;
                                }
                            }
                        }
                    }

                    {
                        auto previewRow = Mosaic::row(ui, {}, location);
                        constexpr StringView previewLabel = "Current";
                        Mosaic::text(ui, previewLabel, location);
                        bool screenPickerAvailable = ui->platform->supportsScreenColorPicker();
                        constexpr float pickerButtonWidth = 28.f;
                        float labelWidth = ui->estimateText(previewLabel, *ui->currentStyle).x;
                        float buttonAndGap = screenPickerAvailable ? pickerButtonWidth + ui->currentStyle->metrics.gap : 0.f;
                        float previewWidth = std::max(48.f, stripWidth - labelWidth - ui->currentStyle->metrics.gap - buttonAndGap);
                        {
                            auto preview = Mosaic::canvas(ui, "Current color", Detail::fixedLayout(previewWidth, ui->currentStyle->metrics.controlHeight), location);
                            size_t previewNode = ui->nodes.size() - 1;
                            Detail::contextMenuTrigger(ui, response, node, previewNode);
                            Rect previewBounds = {0.f, 0.f, previewWidth, ui->currentStyle->metrics.controlHeight};
                            Detail::canvasCheckerboard(preview, previewBounds, 6.f);
                            preview.roundedRect(previewBounds, ui->currentStyle->metrics.cornerRadius, includeAlpha ? *color : Color{color->r, color->g, color->b, 1.f});
                            Detail::canvasOutline(preview, previewBounds, ui->currentStyle->colors.borderStrong);
                        }

                        if(screenPickerAvailable == true)
                        {
                            auto eyedropper = Mosaic::canvas(ui, "Pick screen color", Detail::fixedLayout(pickerButtonWidth, ui->currentStyle->metrics.controlHeight), location);
                            Context::Node & eyedropperNode = ui->nodes.back();
                            eyedropperNode.semanticRole = SemanticRole::Button;
                            Response eyedropperResponse = eyedropperNode.response;
                            Rect buttonBounds = {0.f, 0.f, pickerButtonWidth, ui->currentStyle->metrics.controlHeight};
                            Color buttonColor = eyedropperResponse.active() ? ui->currentStyle->colors.panelActive : (eyedropperResponse.hovered() ? ui->currentStyle->colors.panelHovered : ui->currentStyle->colors.input);
                            eyedropper.roundedRect(buttonBounds, ui->currentStyle->metrics.cornerRadius, buttonColor);
                            Detail::canvasOutline(eyedropper, buttonBounds, eyedropperResponse.hovered() ? ui->currentStyle->colors.accent : ui->currentStyle->colors.borderStrong);
                            Color iconColor = eyedropperResponse.hovered() ? ui->currentStyle->colors.text : ui->currentStyle->colors.textDisabled;
                            eyedropper.line({8.f, 16.f}, {16.f, 8.f}, 2.f, iconColor);
                            eyedropper.line({11.f, 18.f}, {18.f, 11.f}, 2.f, iconColor);
                            eyedropper.line({8.f, 16.f}, {11.f, 18.f}, 1.5f, iconColor);
                            eyedropper.line({15.f, 7.f}, {19.f, 11.f}, 2.f, iconColor);
                            eyedropper.line({17.f, 5.f}, {21.f, 9.f}, 1.5f, iconColor);
                            eyedropper.line({6.f, 19.f}, {10.f, 19.f}, 1.5f, iconColor);

                            if(eyedropperResponse.clicked() == true)
                            {
                                ui->platform->beginScreenColorPick(response.id);
                            }
                        }
                    }

                    Mosaic::text(ui, includeAlpha ? StringView("RGBA Palette") : StringView("RGB Palette"), location);
                    constexpr float paletteHeight = 24.f;
                    {
                        auto paletteCanvas = Mosaic::canvas(ui, "Color presets", Detail::fixedLayout(stripWidth, paletteHeight), location);
                        size_t paletteNode = ui->nodes.size() - 1;
                        Detail::contextMenuTrigger(ui, response, node, paletteNode);
                        constexpr Detail::ColorPalette palette = {Color::fromBytes(236, 239, 244), Color::fromBytes(143, 151, 164), Color::fromBytes(34, 38, 46), Color::fromBytes(226, 72, 82), Color::fromBytes(239, 145, 52), Color::fromBytes(237, 202, 67), Color::fromBytes(70, 188, 116), Color::fromBytes(61, 191, 203), Color::fromBytes(68, 136, 236), Color::fromBytes(178, 92, 222)};
                        constexpr float paletteGap = 4.f;
                        float swatchWidth = (stripWidth - paletteGap * static_cast<float>(palette.size() - 1)) / static_cast<float>(palette.size());
                        for(size_t index = 0; index != palette.size(); ++index)
                        {
                            Rect swatch = {static_cast<float>(index) * (swatchWidth + paletteGap), 0.f, swatchWidth, paletteHeight};
                            paletteCanvas.roundedRect(swatch, 3.f, palette[index]);
                            Detail::canvasOutline(paletteCanvas, swatch, ui->currentStyle->colors.borderStrong);
                        }

                        if(pointer != nullptr && ui->captured == paletteCanvas.id() && pointer->isPressed() == true)
                        {
                            Vec2 local;
                            if(paletteCanvas.localPointerPosition(&local) == true)
                            {
                                size_t index = std::min(palette.size() - 1, static_cast<size_t>(std::max(0.f, local.x) / (swatchWidth + paletteGap)));
                                float alpha = color->a;
                                *color = palette[index];
                                color->a = alpha;
                                Detail::rgbToHsv(*color, hue, saturation, value);
                                changed = true;
                            }
                        }
                    }

                    bool hexFocused = false;
                    {
                        auto hexRow = Mosaic::row(ui, {}, location);
                        Mosaic::text(ui, "HEX", location);
                        TextInputOptions hexOptions;
                        hexOptions.selectAllOnFocus = true;
                        hexOptions.maximumBytes = includeAlpha ? 9 : 7;
                        Response hexResponse = Mosaic::inputText(ui, "HEX value", &colorState.hexText, hexOptions, location);
                        Context::Node & hexNode = ui->nodes.back();
                        hexNode.layout.width = Dimension::fixed(224.f);
                        hexFocused = hexResponse.focused();

                        if(hexResponse.changed() == true)
                        {
                            Color parsed = *color;
                            if(Detail::colorFromHex(colorState.hexText, includeAlpha, &parsed) == true)
                            {
                                *color = parsed;
                                float updatedHue = hue;
                                float updatedSaturation = saturation;
                                Detail::rgbToHsv(*color, updatedHue, updatedSaturation, value);

                                if(updatedSaturation > 0.0001f)
                                {
                                    hue = updatedHue;
                                }

                                if(value > 0.0001f)
                                {
                                    saturation = updatedSaturation;
                                }

                                changed = true;
                            }
                        }

                        Color validated = *color;
                        if(hexResponse.committed() == true && Detail::colorFromHex(colorState.hexText, includeAlpha, &validated) == false)
                        {
                            colorState.hexText = Detail::colorToHex(*color, includeAlpha);
                        }

                        if(Mosaic::button(ui, Key("Copy HEX"), "Copy", location).clicked() == true)
                        {
                            ui->platform->setClipboardText(Detail::colorToHex(*color, includeAlpha));
                        }
                    }

                    Detail::StringViewQuad names = {"R", "G", "B", "A"};
                    Detail::FloatPointerQuad channels = {&color->r, &color->g, &color->b, &color->a};
                    size_t channelCount = includeAlpha ? 4 : 3;
                    bool rgbChanged = false;
                    for(size_t index = 0; index != channelCount; ++index)
                    {
                        auto channelScope = Mosaic::scope(ui, Key(index), location);
                        auto channelRow = Mosaic::row(ui, {}, location);
                        Mosaic::text(ui, names[index], location);
                        Response channelSlider = Mosaic::slider(ui, StringView{}, channels[index], 0.f, 1.f, location);
                        Context::Node & sliderNode = ui->nodes.back();
                        sliderNode.layout.width = Dimension::fixed(224.f);
                        changed = changed || channelSlider.changed();
                        rgbChanged = rgbChanged || (index < 3 && channelSlider.changed());

                        TextInputOptions channelOptions;
                        channelOptions.numeric = true;
                        channelOptions.selectAllOnFocus = true;
                        channelOptions.maximumBytes = 3;
                        Response channelInput = Mosaic::inputText(ui, "Channel value", &colorState.channelText[index], channelOptions, location);
                        Context::Node & inputNode = ui->nodes.back();
                        inputNode.layout.width = Dimension::fixed(54.f);

                        if(channelInput.changed() == true)
                        {
                            uint8_t parsed = 0;
                            if(Detail::colorByteFromText(colorState.channelText[index], &parsed) == true)
                            {
                                float updated = static_cast<float>(parsed) / 255.f;

                                if(updated != *channels[index])
                                {
                                    *channels[index] = updated;
                                    changed = true;
                                    rgbChanged = rgbChanged || index < 3;
                                }
                            }
                        }

                        if(channelInput.focused() == false)
                        {
                            colorState.channelText[index] = "?";
                            (void)Detail::toString(Detail::colorByte(*channels[index]), &colorState.channelText[index]);
                        }
                    }

                    if(rgbChanged == true)
                    {
                        float updatedHue = hue;
                        float updatedSaturation = saturation;
                        Detail::rgbToHsv(*color, updatedHue, updatedSaturation, value);

                        if(updatedSaturation > 0.0001f)
                        {
                            hue = updatedHue;
                        }

                        if(value > 0.0001f)
                        {
                            saturation = updatedSaturation;
                        }
                    }

                    if(changed == true && hexFocused == false)
                    {
                        colorState.hexText = Detail::colorToHex(*color, includeAlpha);
                    }
                }
            }
        }

        pickerOpen = Mosaic::isPopupOpen(ui, pickerPopupKey, response.id);

        bool contextChanged = options.optionsMenu && Detail::colorContextMenu(ui, node, response, color, includeAlpha, location);

        if(contextChanged == true)
        {
            Detail::rgbToHsv(*color, hue, saturation, value);
            Detail::synchronizeColorText(colorState, *color, includeAlpha);
            changed = true;
        }

        if(options.tooltip == true)
        {
            String colorTooltip = Detail::colorToHex(*color, includeAlpha);
            Mosaic::itemTooltip(ui, response, colorTooltip);
        }

        if(pickerWasOpen == true && pickerOpen == false && response.clicked() == false)
        {
            Detail::setFlag(response, 8);
            ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
        }

        if(changed == true)
        {
            Detail::setFlag(response, 6);
            Detail::setFlag(response, 7, pickerOpen);
            ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
        }

        if(contextChanged == true)
        {
            ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].file, ui->nodes[node].line, ui->input.timestamp});
        }

        colorState.pickerOpen = pickerOpen;
        colorState.initialized = true;
        colorState.hue = hue;
        colorState.saturation = saturation;
        colorState.value = value;
        colorState.last = *color;
        ui->nodes[node].tint = includeAlpha && options.alphaOpaque == false ? *color : Color{color->r, color->g, color->b, 1.f};
        Context::StringQuad channelText;

        if(options.inputMode == ColorInputMode::RgbFloat)
        {
            for(String & channel : channelText)
            {
                channel = "?";
            }

            (void)Detail::formatFloating(color->r, 3, {}, &channelText[0]);
            (void)Detail::formatFloating(color->g, 3, {}, &channelText[1]);
            (void)Detail::formatFloating(color->b, 3, {}, &channelText[2]);
            (void)Detail::formatFloating(options.alphaOpaque ? 1.f : color->a, 3, {}, &channelText[3]);
        }
        else if(options.inputMode == ColorInputMode::HsvFloat)
        {
            for(String & channel : channelText)
            {
                channel = "?";
            }

            (void)Detail::formatFloating(hue, 3, {}, &channelText[0]);
            (void)Detail::formatFloating(saturation, 3, {}, &channelText[1]);
            (void)Detail::formatFloating(value, 3, {}, &channelText[2]);
            (void)Detail::formatFloating(options.alphaOpaque ? 1.f : color->a, 3, {}, &channelText[3]);
        }
        else if(options.inputMode == ColorInputMode::Hexadecimal)
        {
            channelText[0] = Detail::colorToHex(*color, includeAlpha && options.alphaOpaque == false);
        }
        else
        {
            for(String & channel : channelText)
            {
                channel = "?";
            }

            (void)Detail::toString(Detail::colorByte(color->r), &channelText[0]);
            (void)Detail::toString(Detail::colorByte(color->g), &channelText[1]);
            (void)Detail::toString(Detail::colorByte(color->b), &channelText[2]);
            (void)Detail::toString(Detail::colorByte(options.alphaOpaque ? 1.f : color->a), &channelText[3]);
        }

        for(uint8_t channel = 0; channel != ui->nodes[node].colorComponents; ++channel)
        {
            ui->prepareColorChannelText(ui->nodes[node], channel, channelText[channel]);
        }
        ui->nodes[node].checked = pickerOpen;
        ui->nodes[node].response = response;

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response colorEditorRgb(Context * ui, StringView label, Color * color, const SourceLocation & location)
    {
        auto returnedValue = Detail::colorEditor(ui, label, color, false, ColorEditOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response colorEditorRgb(Context * ui, StringView label, Color * color, const ColorEditOptions & options, const SourceLocation & location)
    {
        if(options.inputIsHsv == true && color != nullptr)
        {
            Color rgb = Detail::hsvToRgb(color->r, color->g, color->b, color->a);
            Color before = rgb;
            ColorEditOptions rgbOptions = options;
            rgbOptions.inputIsHsv = false;
            Response response = Detail::colorEditor(ui, label, &rgb, false, rgbOptions, location);

            if(rgb != before)
            {
                Detail::rgbToHsv(rgb, color->r, color->g, color->b);
                color->a = rgb.a;
            }

            return response;
        }

        auto returnedValue = Detail::colorEditor(ui, label, color, false, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response colorEditorRgba(Context * ui, StringView label, Color * color, const SourceLocation & location)
    {
        auto returnedValue = Detail::colorEditor(ui, label, color, true, ColorEditOptions{}, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response colorEditorRgba(Context * ui, StringView label, Color * color, const ColorEditOptions & options, const SourceLocation & location)
    {
        if(options.inputIsHsv == true && color != nullptr)
        {
            Color rgb = Detail::hsvToRgb(color->r, color->g, color->b, color->a);
            Color before = rgb;
            ColorEditOptions rgbOptions = options;
            rgbOptions.inputIsHsv = false;
            Response response = Detail::colorEditor(ui, label, &rgb, true, rgbOptions, location);

            if(rgb != before)
            {
                Detail::rgbToHsv(rgb, color->r, color->g, color->b);
                color->a = rgb.a;
            }

            return response;
        }

        auto returnedValue = Detail::colorEditor(ui, label, color, true, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response colorButton(Context * ui, const Key & key, Color * color, bool includeAlpha, const ColorEditOptions & options, const SourceLocation & location)
    {
        auto colorScope = Mosaic::scope(ui, key, location);
        ColorEditOptions buttonOptions = options;
        buttonOptions.labelPlacement = LabelPlacement::Hidden;
        buttonOptions.showInputs = false;
        buttonOptions.showPreview = true;
        buttonOptions.openPickerOnClick = false;
        auto returnedValue = Detail::colorEditor(ui, {}, color, includeAlpha, buttonOptions, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response Detail::colorPicker(Context * ui, StringView label, Color * color, bool includeAlpha, const ColorPickerOptions & options, const SourceLocation & location)
    {
        auto pickerScope = Mosaic::scope(ui, {}, location);
        ui->nodes[ui->currentParent].label.assign(label);
        Response aggregate;
        aggregate.id = pickerScope.id();

        if(color == nullptr)
        {
            return aggregate;
        }

        ColorEditorState & state = ui->colorEditorState(pickerScope.id());

        if(state.initialized == false || state.last != *color)
        {
            float hue = 0.f;
            float saturation = 0.f;
            float value = 0.f;
            Detail::rgbToHsv(*color, hue, saturation, value);

            if(state.initialized == false || saturation > 0.0001f)
            {
                state.hue = hue;
            }

            if(state.initialized == false || value > 0.0001f)
            {
                state.saturation = saturation;
            }

            state.value = value;
            state.last = *color;
            state.initialized = true;
        }

        float hue = state.hue;
        float saturation = state.saturation;
        float value = state.value;
        float gap = ui->currentStyle->metrics.gap;
        float alphaHeight = includeAlpha && options.alpha ? 14.f : 0.f;
        float areaHeight = std::max(40.f, options.size.y - (alphaHeight > 0.f ? alphaHeight + gap : 0.f));
        float hueWidth = options.hueWheel ? std::min(areaHeight, options.size.x) : 20.f;
        float areaWidth = options.hueWheel ? 0.f : std::max(40.f, options.size.x - hueWidth - gap);
        const PointerState * pointer = ui->input.primaryPointer();
        {
            auto pickerRow = Mosaic::row(ui, {}, location);

            if(options.hueWheel == false)
            {
                auto saturationCanvas = Mosaic::canvas(ui, "Saturation and value", Detail::fixedLayout(areaWidth, areaHeight), location);
                Response saturationResponse = ui->nodes[ui->currentParent].response;
                aggregate.flags |= saturationResponse.flags;
                Rect saturationBounds = {0.f, 0.f, areaWidth, areaHeight};
                Color hueColor = Detail::hsvToRgb(hue, 1.f, 1.f);
                Detail::canvasGradient(saturationCanvas, saturationBounds, Color{1.f, 1.f, 1.f, 1.f}, hueColor, hueColor, Color{1.f, 1.f, 1.f, 1.f});
                Detail::canvasGradient(saturationCanvas, saturationBounds, Color{0.f, 0.f, 0.f, 0.f}, Color{0.f, 0.f, 0.f, 0.f}, Color{0.f, 0.f, 0.f, 1.f}, Color{0.f, 0.f, 0.f, 1.f});
                Detail::canvasOutline(saturationCanvas, saturationBounds, ui->currentStyle->colors.borderStrong);
                float selectionX = saturation * areaWidth;
                float selectionY = (1.f - value) * areaHeight;
                Detail::canvasOutline(saturationCanvas, {selectionX - 4.f, selectionY - 4.f, 8.f, 8.f}, Color{1.f, 1.f, 1.f, 1.f}, 2.f);

                if(pointer != nullptr && ui->captured == saturationCanvas.id() && pointer->isDown() == true)
                {
                    Vec2 local;
                    if(saturationCanvas.localPointerPosition(&local) == true)
                    {
                        saturation = std::clamp(local.x / areaWidth, 0.f, 1.f);
                        value = 1.f - std::clamp(local.y / areaHeight, 0.f, 1.f);
                        float alpha = color->a;
                        *color = Detail::hsvToRgb(hue, saturation, value, includeAlpha ? alpha : 1.f);
                        Detail::setFlag(aggregate, 6);
                    }
                }
            }

            {
                auto hueCanvas = Mosaic::canvas(ui, options.hueWheel ? "Hue wheel" : "Hue", Detail::fixedLayout(hueWidth, areaHeight), location);
                Response hueResponse = ui->nodes[ui->currentParent].response;
                aggregate.flags |= hueResponse.flags;

                if(options.hueWheel == true)
                {
                    constexpr size_t segmentCount = 64;
                    constexpr float tau = 6.28318530718f;
                    constexpr float halfPi = 1.57079632679f;
                    Vec2 center = {hueWidth * 0.5f, areaHeight * 0.5f};
                    float radius = std::max(10.f, std::min(hueWidth, areaHeight) * 0.5f - 9.f);
                    float thickness = std::clamp(radius * 0.22f, 7.f, 14.f);
                    Array<Color, segmentCount> ringColors;
                    for(size_t index = 0; index != segmentCount; ++index)
                    {
                        float firstRatio = static_cast<float>(index) / static_cast<float>(segmentCount);
                        float secondRatio = static_cast<float>(index + 1) / static_cast<float>(segmentCount);
                        ringColors[index] = Detail::hsvToRgb((firstRatio + secondRatio) * 0.5f, 1.f, 1.f);
                    }
                    hueCanvas.gradientRing(center, radius - thickness * 0.5f, radius + thickness * 0.5f, -halfPi, ringColors);
                    float markerAngle = hue * tau - halfPi;
                    Vec2 marker = {center.x + std::cos(markerAngle) * radius, center.y + std::sin(markerAngle) * radius};
                    Detail::canvasOutline(hueCanvas, {marker.x - 4.f, marker.y - 4.f, 8.f, 8.f}, Color{1.f, 1.f, 1.f, 1.f}, 2.f);
                    float triangleRadius = std::max(4.f, radius - thickness * 0.72f - 3.f);
                    float triangleAngle = options.rotateHueWheelTriangle ? hue * tau - halfPi : tau / 12.f;
                    float triangleCosine = std::cos(triangleAngle);
                    float triangleSine = std::sin(triangleAngle);
                    Vec2 triangleHue = Detail::rotatePoint({triangleRadius, 0.f}, triangleCosine, triangleSine);
                    Vec2 triangleBlack = Detail::rotatePoint({-triangleRadius * 0.5f, -triangleRadius * 0.8660254f}, triangleCosine, triangleSine);
                    Vec2 triangleWhite = Detail::rotatePoint({-triangleRadius * 0.5f, triangleRadius * 0.8660254f}, triangleCosine, triangleSine);
                    Array<ColoredPoint, 3> trianglePoints = {{{center + triangleHue, Detail::hsvToRgb(hue, 1.f, 1.f)}, {center + triangleBlack, Color{0.f, 0.f, 0.f, 1.f}}, {center + triangleWhite, Color{1.f, 1.f, 1.f, 1.f}}}};
                    hueCanvas.gradientPolygon(trianglePoints);
                    Vec2 triangleOutline[] = {center + triangleHue, center + triangleBlack, center + triangleWhite};
                    hueCanvas.polyline(Vec2Span(triangleOutline, 3), 1.5f, ui->currentStyle->colors.borderStrong, true);
                    Vec2 saturationPoint = triangleWhite + (triangleHue - triangleWhite) * saturation;
                    Vec2 valuePoint = triangleBlack + (saturationPoint - triangleBlack) * value;
                    Detail::canvasOutline(hueCanvas, {center.x + valuePoint.x - 4.f, center.y + valuePoint.y - 4.f, 8.f, 8.f}, Color{1.f, 1.f, 1.f, 1.f}, 2.f);

                    if(pointer != nullptr && ui->captured == hueCanvas.id() && pointer->isDown() == true)
                    {
                        Vec2 local;
                        Rect canvasBounds;
                        bool hasLocal = hueCanvas.localPointerPosition(&local);
                        bool hasBounds = Mosaic::debugBounds(ui, hueCanvas.id(), &canvasBounds);

                        if(hasLocal == true && hasBounds == true)
                        {
                            Vec2 relative = local - center;
                            Vec2 initialRelative = pointer->pressPosition() - Vec2{canvasBounds.x + center.x, canvasBounds.y + center.y};
                            float initialDistance = std::sqrt(initialRelative.x * initialRelative.x + initialRelative.y * initialRelative.y);

                            if(initialDistance >= radius - thickness && initialDistance <= radius + thickness)
                            {
                                float angle = std::atan2(relative.y, relative.x) + halfPi;

                                if(angle < 0.f)
                                {
                                    angle += tau;
                                }

                                hue = std::clamp(angle / tau, 0.f, 1.f);

                                if(hue >= 1.f)
                                {
                                    hue = 0.f;
                                }

                                float alpha = color->a;
                                *color = Detail::hsvToRgb(hue, saturation, value, includeAlpha ? alpha : 1.f);
                                Detail::setFlag(aggregate, 6);
                            }
                            else
                            {
                                Vec2 unrotatedInitial = Detail::rotatePoint(initialRelative, triangleCosine, -triangleSine);
                                constexpr float rootThreeHalf = 0.8660254f;
                                Vec2 baseHue = {triangleRadius, 0.f};
                                Vec2 baseBlack = {-triangleRadius * 0.5f, -triangleRadius * rootThreeHalf};
                                Vec2 baseWhite = {-triangleRadius * 0.5f, triangleRadius * rootThreeHalf};

                                if(Detail::triangleContains(unrotatedInitial, baseHue, baseBlack, baseWhite) == true)
                                {
                                    Vec2 unrotatedCurrent = Detail::rotatePoint(relative, triangleCosine, -triangleSine);
                                    float hueWeight = 0.f;
                                    float blackWeight = 0.f;
                                    float whiteWeight = 0.f;
                                    Detail::triangleBarycentric(unrotatedCurrent, baseHue, baseBlack, baseWhite, hueWeight, blackWeight, whiteWeight);
                                    hueWeight = std::max(0.f, hueWeight);
                                    blackWeight = std::max(0.f, blackWeight);
                                    whiteWeight = std::max(0.f, whiteWeight);
                                    float weightSum = hueWeight + blackWeight + whiteWeight;

                                    if(weightSum > 0.f)
                                    {
                                        hueWeight /= weightSum;
                                        blackWeight /= weightSum;
                                        value = std::clamp(1.f - blackWeight, 0.0001f, 1.f);
                                        saturation = std::clamp(hueWeight / value, 0.0001f, 1.f);
                                        float alpha = color->a;
                                        *color = Detail::hsvToRgb(hue, saturation, value, includeAlpha ? alpha : 1.f);
                                        Detail::setFlag(aggregate, 6);
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    constexpr Detail::HueColorArray hueColors = {Color{1.f, 0.f, 0.f, 1.f}, Color{1.f, 1.f, 0.f, 1.f}, Color{0.f, 1.f, 0.f, 1.f}, Color{0.f, 1.f, 1.f, 1.f}, Color{0.f, 0.f, 1.f, 1.f}, Color{1.f, 0.f, 1.f, 1.f}, Color{1.f, 0.f, 0.f, 1.f}};
                    float segmentHeight = areaHeight / 6.f;
                    for(size_t index = 0; index != 6; ++index)
                    {
                        Rect segment = {0.f, segmentHeight * static_cast<float>(index), hueWidth, segmentHeight + 0.5f};
                        Detail::canvasGradient(hueCanvas, segment, hueColors[index], hueColors[index], hueColors[index + 1], hueColors[index + 1]);
                    }
                    Detail::canvasOutline(hueCanvas, {0.f, 0.f, hueWidth, areaHeight}, ui->currentStyle->colors.borderStrong);
                    Detail::canvasOutline(hueCanvas, {-2.f, hue * areaHeight - 2.f, hueWidth + 4.f, 4.f}, Color{1.f, 1.f, 1.f, 1.f}, 2.f);

                    if(pointer != nullptr && ui->captured == hueCanvas.id() && pointer->isDown() == true)
                    {
                        Vec2 local;
                        if(hueCanvas.localPointerPosition(&local) == true)
                        {
                            hue = std::clamp(local.y / areaHeight, 0.f, 1.f);

                            if(hue >= 1.f)
                            {
                                hue = 0.f;
                            }

                            float alpha = color->a;
                            *color = Detail::hsvToRgb(hue, saturation, value, includeAlpha ? alpha : 1.f);
                            Detail::setFlag(aggregate, 6);
                        }
                    }
                }
            }
        }

        if(alphaHeight > 0.f)
        {
            auto alphaCanvas = Mosaic::canvas(ui, "Alpha", Detail::fixedLayout(options.size.x, alphaHeight), location);
            Response alphaResponse = ui->nodes[ui->currentParent].response;
            aggregate.flags |= alphaResponse.flags;
            Rect alphaBounds = {0.f, 0.f, options.size.x, alphaHeight};
            Detail::canvasCheckerboard(alphaCanvas, alphaBounds, 7.f);
            Detail::canvasGradient(alphaCanvas, alphaBounds, Color{color->r, color->g, color->b, 0.f}, Color{color->r, color->g, color->b, 1.f}, Color{color->r, color->g, color->b, 1.f}, Color{color->r, color->g, color->b, 0.f});
            Detail::canvasOutline(alphaCanvas, alphaBounds, ui->currentStyle->colors.borderStrong);
            Detail::canvasOutline(alphaCanvas, {color->a * options.size.x - 2.f, -2.f, 4.f, alphaHeight + 4.f}, Color{1.f, 1.f, 1.f, 1.f}, 2.f);

            if(pointer != nullptr && ui->captured == alphaCanvas.id() && pointer->isDown() == true)
            {
                Vec2 local;
                if(alphaCanvas.localPointerPosition(&local) == true)
                {
                    color->a = std::clamp(local.x / options.size.x, 0.f, 1.f);
                    Detail::setFlag(aggregate, 6);
                }
            }
        }

        if(options.showPreview == true && options.reference != nullptr)
        {
            auto previews = Mosaic::row(ui, {}, location);
            Mosaic::text(ui, "Current", location);
            ColorEditOptions previewOptions;
            previewOptions.width = Dimension::fixed(64.f);
            previewOptions.dragDrop = options.dragDrop;
            Response current = Mosaic::colorButton(ui, Key("current color"), color, includeAlpha, previewOptions, location);
            aggregate.flags |= current.flags;
            Mosaic::text(ui, "Previous", location);
            Color referenceColor = *options.reference;
            Response previous = Mosaic::colorButton(ui, Key("previous color"), &referenceColor, includeAlpha, previewOptions, location);

            if(previous.clicked() == true)
            {
                *color = *options.reference;
                Detail::rgbToHsv(*color, hue, saturation, value);
                Detail::setFlag(aggregate, 6);
            }
        }

        if(options.showInputs == true || (options.showPreview == true && options.reference == nullptr))
        {
            ColorEditOptions editOptions;
            editOptions.labelPlacement = LabelPlacement::Hidden;
            editOptions.showInputs = options.showInputs;
            editOptions.showPreview = options.showPreview && options.reference == nullptr;
            editOptions.inputMode = options.inputMode;
            editOptions.alphaOpaque = options.alphaOpaque;
            editOptions.alphaBackground = options.alphaBackground;
            editOptions.alphaPreviewHalf = options.alphaPreviewHalf;
            editOptions.optionsMenu = options.optionsMenu;
            editOptions.dragDrop = options.dragDrop;
            editOptions.colorMarkers = options.colorMarkers;
            editOptions.hdr = options.hdr;
            editOptions.border = options.border;
            auto inputsScope = Mosaic::scope(ui, Key("picker values"), location);
            Response editResponse = Detail::colorEditor(ui, "Value", color, includeAlpha, editOptions, location);
            aggregate.flags |= editResponse.flags;
        }

        state.hue = hue;
        state.saturation = saturation;
        state.value = value;
        state.last = *color;

        return aggregate;
    }
    //////////////////////////////////////////////////////////////////////////
    Response colorPickerRgb(Context * ui, StringView label, Color * color, const ColorPickerOptions & options, const SourceLocation & location)
    {
        if(options.inputIsHsv == true && color != nullptr)
        {
            Color rgb = Detail::hsvToRgb(color->r, color->g, color->b, color->a);
            Color before = rgb;
            ColorPickerOptions rgbOptions = options;
            rgbOptions.inputIsHsv = false;
            Response response = Detail::colorPicker(ui, label, &rgb, false, rgbOptions, location);

            if(rgb != before)
            {
                Detail::rgbToHsv(rgb, color->r, color->g, color->b);
                color->a = rgb.a;
            }

            return response;
        }

        auto returnedValue = Detail::colorPicker(ui, label, color, false, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response colorPickerRgba(Context * ui, StringView label, Color * color, const ColorPickerOptions & options, const SourceLocation & location)
    {
        if(options.inputIsHsv == true && color != nullptr)
        {
            Color rgb = Detail::hsvToRgb(color->r, color->g, color->b, color->a);
            Color before = rgb;
            ColorPickerOptions rgbOptions = options;
            rgbOptions.inputIsHsv = false;
            Response response = Detail::colorPicker(ui, label, &rgb, true, rgbOptions, location);

            if(rgb != before)
            {
                Detail::rgbToHsv(rgb, color->r, color->g, color->b);
                color->a = rgb.a;
            }

            return response;
        }

        auto returnedValue = Detail::colorPicker(ui, label, color, true, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Response vectorEditor(Context * ui, StringView label, FloatSpan values, float minimum, float maximum, const SourceLocation & location)
    {
        SliderOptions options;
        options.minimum = minimum;
        options.maximum = maximum;
        options.step = maximum > minimum ? static_cast<double>(maximum - minimum) / 800.0 : 0.01;
        options.precision = 2;
        auto returnedValue = Mosaic::dragFloatVector(ui, label, values, options, location);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
