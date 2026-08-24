#include "HelloDemo.hpp"
#include "HelloDemoTables.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>

namespace MosaicExample
{
    namespace Detail
    {
        int tableOpenAction = -1;
        //////////////////////////////////////////////////////////////////////////
        template<class T> [[nodiscard]] Mosaic::String demoNumber(T value)
        {
            char buffer[64] = {};
            auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::String demoFixed(double value, int precision)
        {
            char buffer[64] = {};
            auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, precision);
            auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::String demoCodepoint(char32_t value)
        {
            char buffer[16] = {};
            auto result = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<uint32_t>(value), 16);

            if(result.ec != std::errc{})
            {
                return "?";
            }

            Mosaic::String digits(buffer, result.ptr);
            for(char & character : digits)
            {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
            while(digits.size() < 4)
            {
                digits.insert(digits.begin(), '0');
            }
            auto returnedValue = Mosaic::String("0x") + digits;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::String demoPointer(const void * value)
        {
            char buffer[2 + sizeof(uintptr_t) * 2] = {'0', 'x'};
            auto result = std::to_chars(buffer + 2, buffer + sizeof(buffer), reinterpret_cast<uintptr_t>(value), 16);
            auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t decodeUtf8(Mosaic::StringView value, size_t offset, char32_t & codepoint) noexcept
        {
            if(offset >= value.size())
            {
                codepoint = U'\0';

                return 0;
            }

            uint8_t first = static_cast<uint8_t>(value[offset]);
            size_t length = 1;

            if((first & 0x80U) == 0)
            {
                codepoint = first;

                return length;
            }

            if((first & 0xe0U) == 0xc0U)
            {
                codepoint = first & 0x1fU;
                length = 2;
            }
            else if((first & 0xf0U) == 0xe0U)
            {
                codepoint = first & 0x0fU;
                length = 3;
            }
            else if((first & 0xf8U) == 0xf0U)
            {
                codepoint = first & 0x07U;
                length = 4;
            }
            else
            {
                codepoint = U'\ufffd';

                return 1;
            }

            if(offset + length > value.size())
            {
                codepoint = U'\ufffd';

                return 1;
            }

            for(size_t index = 1; index != length; ++index)
            {
                uint8_t continuation = static_cast<uint8_t>(value[offset + index]);

                if((continuation & 0xc0U) != 0x80U)
                {
                    codepoint = U'\ufffd';

                    return 1;
                }

                codepoint = static_cast<char32_t>((codepoint << 6U) | (continuation & 0x3fU));
            }

            return length;
        }
        //////////////////////////////////////////////////////////////////////////
        void applyTableSizing(Mosaic::TableColumnOptions & options, int policy, float fixedWidth, float stretchWeight) noexcept
        {
            switch(policy)
            {
            case 0:
                options.sizing = Mosaic::TableSizing::FixedFit;
                options.widthOrWeight = fixedWidth;
                break;
            case 1:
                options.sizing = Mosaic::TableSizing::FixedSame;
                options.widthOrWeight = fixedWidth;
                break;
            case 2:
                options.sizing = Mosaic::TableSizing::StretchProportional;
                options.widthOrWeight = stretchWeight;
                break;
            default:
                options.sizing = Mosaic::TableSizing::StretchSame;
                options.widthOrWeight = 1.f;
                break;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void tabFittingPolicyControls(Mosaic::Context * ui, Mosaic::TabFittingPolicy * policy)
        {
            if(policy == nullptr)
            {
                return;
            }

            constexpr Mosaic::Array<Mosaic::StringView, 3> labels = {"MosaicTabBarFlags_FittingPolicyMixed", "MosaicTabBarFlags_FittingPolicyShrink", "MosaicTabBarFlags_FittingPolicyScroll"};
            constexpr Mosaic::Array<Mosaic::TabFittingPolicy, 3> values = {Mosaic::TabFittingPolicy::Mixed, Mosaic::TabFittingPolicy::Shrink, Mosaic::TabFittingPolicy::Scroll};
            for(size_t index = 0; index != values.size(); ++index)
            {
                if(Mosaic::radioButton(ui, labels[index], *policy == values[index]).clicked() == true)
                {
                    *policy = values[index];
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::StringView cursorShapeName(Mosaic::CursorShape shape) noexcept
        {
            switch(shape)
            {
            case Mosaic::CursorShape::Arrow:
                return "Arrow";
            case Mosaic::CursorShape::Hand:
                return "Hand";
            case Mosaic::CursorShape::Text:
                return "TextInput";
            case Mosaic::CursorShape::ResizeHorizontal:
                return "ResizeEW";
            case Mosaic::CursorShape::ResizeVertical:
                return "ResizeNS";
            case Mosaic::CursorShape::ResizeDiagonalNesw:
                return "ResizeNESW";
            case Mosaic::CursorShape::ResizeDiagonalNwse:
                return "ResizeNWSE";
            case Mosaic::CursorShape::ResizeAll:
                return "ResizeAll";
            case Mosaic::CursorShape::Crosshair:
                return "Crosshair";
            case Mosaic::CursorShape::Wait:
                return "Wait";
            case Mosaic::CursorShape::Progress:
                return "Progress";
            case Mosaic::CursorShape::NotAllowed:
                return "NotAllowed";
            }

            return "Arrow";
        }
        //////////////////////////////////////////////////////////////////////////
        void completionInputCallback(Mosaic::TextInputCallbackData & data)
        {
            if(data.value == nullptr)
            {
                return;
            }

            if(data.event != Mosaic::TextInputCallbackEvent::Completion)
            {
                return;
            }

            size_t wordBegin = std::min(data.cursor, data.value->size());
            while(wordBegin > 0 && std::isspace(static_cast<unsigned char>((*data.value)[wordBegin - 1])) == 0)
            {
                --wordBegin;
            }
            constexpr Mosaic::StringView completion = "Mosaic";
            data.value->replace(wordBegin, data.cursor - wordBegin, completion);
            data.cursor = wordBegin + completion.size();
            data.anchor = data.cursor;
            data.changed = true;
        }
        //////////////////////////////////////////////////////////////////////////
        void historyInputCallback(Mosaic::TextInputCallbackData & data)
        {
            if(data.value == nullptr)
            {
                return;
            }

            if(data.userData == nullptr)
            {
                return;
            }

            constexpr Mosaic::Array<Mosaic::StringView, 4> history = {"HELP", "HISTORY", "CLASSIFY application", "CLEAR"};
            int & index = *static_cast<int *>(data.userData);

            if(data.event == Mosaic::TextInputCallbackEvent::HistoryPrevious)
            {
                index = std::min(index + 1, static_cast<int>(history.size()) - 1);
            }
            else if(data.event == Mosaic::TextInputCallbackEvent::HistoryNext)
            {
                index = std::max(index - 1, -1);
            }
            else
            {
                return;
            }

            *data.value = index < 0 ? Mosaic::String{} : Mosaic::String(history[index]);
            data.cursor = data.value->size();
            data.anchor = data.cursor;
            data.changed = true;
        }
        //////////////////////////////////////////////////////////////////////////
        void countInputCallback(Mosaic::TextInputCallbackData & data)
        {
            if(data.userData == nullptr)
            {
                return;
            }

            ++*static_cast<int32_t *>(data.userData);
        }
        //////////////////////////////////////////////////////////////////////////
        void editInputCallback(Mosaic::TextInputCallbackData & data)
        {
            if(data.value != nullptr && data.value->empty() == false)
            {
                char & first = data.value->front();

                if((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z'))
                {
                    first ^= 32;
                    data.changed = true;
                }
            }

            Detail::countInputCallback(data);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Id demoRowId(size_t row) noexcept
        {
            auto returnedValue = Mosaic::combineId(Mosaic::hashBytes("Mosaic demo table row"), row + 1);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::StringView comboFunctionItem(void *, size_t index) noexcept
        {
            constexpr Mosaic::Array<Mosaic::StringView, 8> items = {"AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH"};
            auto returnedValue = index < items.size() ? items[index] : Mosaic::StringView{};

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::LayoutOptions fillLayout(float height = 0.f) noexcept
        {
            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.height = height > 0.f ? Mosaic::Dimension::fixed(height) : Mosaic::Dimension(Mosaic::SizeRule::Fill);

            return layout;
        }
        //////////////////////////////////////////////////////////////////////////
        void linkedBullet(Mosaic::Context * ui, Mosaic::StringView text, Mosaic::StringView url)
        {
            auto item = Mosaic::scope(ui, Mosaic::Key(url));
            auto row = Mosaic::row(ui);
            Mosaic::bullet(ui);
            Mosaic::text(ui, text);
            Mosaic::hyperlink(ui, url, url);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::LayoutOptions indentedLayout(const Mosaic::Context * ui) noexcept
        {
            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.padding.left = Mosaic::getTheme(ui).metrics.indent;

            return layout;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Theme mosaicTheme(const Mosaic::Theme & source) noexcept
        {
            Mosaic::Theme theme = source;
            theme.colors.background = Mosaic::Color::fromBytes(18, 18, 18);
            theme.colors.panel = Mosaic::Color::fromBytes(24, 24, 24);
            theme.colors.panelHeader = Mosaic::Color::fromBytes(24, 24, 24);
            theme.colors.input = Mosaic::Color::fromBytes(39, 62, 94);
            theme.colors.frame = Mosaic::Color::fromBytes(39, 62, 94);
            theme.colors.frameHovered = Mosaic::Color::fromBytes(66, 100, 150);
            theme.colors.frameActive = Mosaic::Color::fromBytes(52, 88, 136);
            theme.colors.button = Mosaic::Color::fromBytes(39, 62, 94);
            theme.colors.buttonHovered = Mosaic::Color::fromBytes(66, 100, 150);
            theme.colors.buttonActive = Mosaic::Color::fromBytes(52, 88, 136);
            theme.colors.header = Mosaic::Color::fromBytes(40, 72, 110);
            theme.colors.headerHovered = Mosaic::Color::fromBytes(66, 100, 150);
            theme.colors.headerActive = Mosaic::Color::fromBytes(52, 88, 136);
            theme.colors.menu = Mosaic::Color::fromBytes(24, 24, 24);
            theme.colors.menuHovered = Mosaic::Color::fromBytes(66, 100, 150);
            theme.colors.menuActive = Mosaic::Color::fromBytes(52, 88, 136);
            theme.colors.popup = Mosaic::Color::fromBytes(20, 20, 20);
            theme.colors.popupBorder = Mosaic::Color::fromBytes(110, 110, 110);
            theme.colors.tab = Mosaic::Color::fromBytes(40, 72, 110);
            theme.colors.tabHovered = Mosaic::Color::fromBytes(66, 100, 150);
            theme.colors.tabActive = Mosaic::Color::fromBytes(52, 88, 136);
            theme.colors.border = Mosaic::Color::fromBytes(70, 70, 70);
            theme.colors.borderStrong = Mosaic::Color::fromBytes(82, 82, 82);
            theme.colors.separator = Mosaic::Color::fromBytes(70, 70, 70);
            theme.colors.selection = Mosaic::Color::fromBytes(40, 72, 110);
            theme.colors.text = Mosaic::Color::fromBytes(230, 230, 230);
            theme.colors.textDisabled = Mosaic::Color::fromBytes(128, 128, 128);
            theme.metrics.font = Mosaic::MonospaceFont;
            theme.metrics.fontSize = 13.f;
            theme.metrics.lineHeight = 17.f;
            theme.metrics.padding = 4.f;
            theme.metrics.gap = 4.f;
            theme.metrics.framePadding = {4.f, 3.f};
            theme.metrics.itemSpacing = {8.f, 4.f};
            theme.metrics.innerSpacing = {4.f, 4.f};
            theme.metrics.cellPadding = {4.f, 2.f};
            theme.metrics.popupPadding = {8.f, 8.f};
            theme.metrics.borderWidth = 1.f;
            theme.metrics.cornerRadius = 0.f;
            theme.metrics.childCornerRadius = 0.f;
            theme.metrics.frameCornerRadius = 0.f;
            theme.metrics.popupCornerRadius = 0.f;
            theme.metrics.tabCornerRadius = 0.f;
            theme.metrics.scrollbarCornerRadius = 9.f;
            theme.metrics.grabCornerRadius = 0.f;
            theme.metrics.controlHeight = 21.f;
            theme.metrics.windowTitleHeight = 21.f;
            theme.metrics.minimumControlWidth = 100.f;
            theme.behavior.animationsEnabled = false;

            return theme;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Color demoHsv(float hue, float saturation, float value) noexcept
        {
            hue -= std::floor(hue);
            float scaled = hue * 6.f;
            int sector = static_cast<int>(std::floor(scaled));
            float fraction = scaled - static_cast<float>(sector);
            float first = value * (1.f - saturation);
            float second = value * (1.f - saturation * fraction);
            float third = value * (1.f - saturation * (1.f - fraction));
            switch(sector % 6)
            {
            case 0:
                return {value, third, first, 1.f};
            case 1:
                return {second, value, first, 1.f};
            case 2:
                return {first, value, third, 1.f};
            case 3:
                return {first, second, value, 1.f};
            case 4:
                return {third, first, value, 1.f};
            default:
                return {value, first, second, 1.f};
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool containsIgnoreCase(Mosaic::StringView value, Mosaic::StringView pattern) noexcept
        {
            if(pattern.empty() == true)
            {
                return true;
            }

            if(pattern.size() > value.size())
            {
                return false;
            }

            for(size_t offset = 0; offset + pattern.size() <= value.size(); ++offset)
            {
                bool matches = true;
                for(size_t index = 0; index != pattern.size(); ++index)
                {
                    unsigned char left = static_cast<unsigned char>(value[offset + index]);
                    unsigned char right = static_cast<unsigned char>(pattern[index]);

                    if(std::tolower(left) != std::tolower(right))
                    {
                        matches = false;
                        break;
                    }
                }

                if(matches == true)
                {
                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool passesTextFilter(Mosaic::StringView value, Mosaic::StringView filter) noexcept
        {
            bool hasInclude = false;
            bool includeMatched = false;
            size_t begin = 0;
            while(begin <= filter.size())
            {
                size_t comma = filter.find(',', begin);
                size_t end = comma == Mosaic::StringView::npos ? filter.size() : comma;
                while(begin < end && std::isspace(static_cast<unsigned char>(filter[begin])))
                {
                    ++begin;
                }
                size_t trimmedEnd = end;
                while(trimmedEnd > begin && std::isspace(static_cast<unsigned char>(filter[trimmedEnd - 1])))
                {
                    --trimmedEnd;
                }
                Mosaic::StringView token = filter.substr(begin, trimmedEnd - begin);

                if(token.empty() == false && token.front() == '-')
                {
                    token.remove_prefix(1);

                    if(token.empty() == false && Detail::containsIgnoreCase(value, token) == true)
                    {
                        return false;
                    }
                }
                else if(token.empty() == false)
                {
                    hasInclude = true;
                    includeMatched = includeMatched || Detail::containsIgnoreCase(value, token);
                }

                if(comma == Mosaic::StringView::npos)
                {
                    break;
                }

                begin = comma + 1;
            }

            return hasInclude == false || includeMatched;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool secondaryClicked(Mosaic::Context * ui, const Mosaic::Response & response) noexcept
        {
            const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();

            if(pointer == nullptr)
            {
                return false;
            }

            if(pointer->isReleased(Mosaic::PointerButton::Secondary) == false)
            {
                return false;
            }

            Mosaic::Rect bounds;
            if(Mosaic::debugBounds(ui, response.id, &bounds) == false)
            {
                return false;
            }

            if(bounds.contains(pointer->position) == false)
            {
                return false;
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::StringView keyName(Mosaic::KeyCode key) noexcept
        {
            switch(key)
            {
            case Mosaic::KeyCode::Tab:
                return "Tab";
            case Mosaic::KeyCode::Enter:
                return "Enter";
            case Mosaic::KeyCode::Escape:
                return "Escape";
            case Mosaic::KeyCode::Space:
                return "Space";
            case Mosaic::KeyCode::Backspace:
                return "Backspace";
            case Mosaic::KeyCode::Delete:
                return "Delete";
            case Mosaic::KeyCode::Left:
                return "Left";
            case Mosaic::KeyCode::Right:
                return "Right";
            case Mosaic::KeyCode::Up:
                return "Up";
            case Mosaic::KeyCode::Down:
                return "Down";
            case Mosaic::KeyCode::Home:
                return "Home";
            case Mosaic::KeyCode::End:
                return "End";
            case Mosaic::KeyCode::PageUp:
                return "PageUp";
            case Mosaic::KeyCode::PageDown:
                return "PageDown";
            case Mosaic::KeyCode::A:
                return "A";
            case Mosaic::KeyCode::B:
                return "B";
            case Mosaic::KeyCode::C:
                return "C";
            case Mosaic::KeyCode::D:
                return "D";
            case Mosaic::KeyCode::E:
                return "E";
            case Mosaic::KeyCode::F:
                return "F";
            case Mosaic::KeyCode::G:
                return "G";
            case Mosaic::KeyCode::H:
                return "H";
            case Mosaic::KeyCode::I:
                return "I";
            case Mosaic::KeyCode::J:
                return "J";
            case Mosaic::KeyCode::K:
                return "K";
            case Mosaic::KeyCode::L:
                return "L";
            case Mosaic::KeyCode::M:
                return "M";
            case Mosaic::KeyCode::N:
                return "N";
            case Mosaic::KeyCode::O:
                return "O";
            case Mosaic::KeyCode::P:
                return "P";
            case Mosaic::KeyCode::Q:
                return "Q";
            case Mosaic::KeyCode::R:
                return "R";
            case Mosaic::KeyCode::S:
                return "S";
            case Mosaic::KeyCode::T:
                return "T";
            case Mosaic::KeyCode::U:
                return "U";
            case Mosaic::KeyCode::V:
                return "V";
            case Mosaic::KeyCode::W:
                return "W";
            case Mosaic::KeyCode::X:
                return "X";
            case Mosaic::KeyCode::Y:
                return "Y";
            case Mosaic::KeyCode::Z:
                return "Z";
            case Mosaic::KeyCode::D0:
                return "0";
            case Mosaic::KeyCode::D1:
                return "1";
            case Mosaic::KeyCode::D2:
                return "2";
            case Mosaic::KeyCode::D3:
                return "3";
            case Mosaic::KeyCode::D4:
                return "4";
            case Mosaic::KeyCode::D5:
                return "5";
            case Mosaic::KeyCode::D6:
                return "6";
            case Mosaic::KeyCode::D7:
                return "7";
            case Mosaic::KeyCode::D8:
                return "8";
            case Mosaic::KeyCode::D9:
                return "9";
            case Mosaic::KeyCode::F1:
                return "F1";
            case Mosaic::KeyCode::F2:
                return "F2";
            case Mosaic::KeyCode::F3:
                return "F3";
            case Mosaic::KeyCode::F4:
                return "F4";
            case Mosaic::KeyCode::F5:
                return "F5";
            case Mosaic::KeyCode::F6:
                return "F6";
            case Mosaic::KeyCode::F7:
                return "F7";
            case Mosaic::KeyCode::F8:
                return "F8";
            case Mosaic::KeyCode::F9:
                return "F9";
            case Mosaic::KeyCode::F10:
                return "F10";
            case Mosaic::KeyCode::F11:
                return "F11";
            case Mosaic::KeyCode::F12:
                return "F12";
            default:
                return "Unknown";
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void demoFormLabel(Mosaic::Context * ui, Mosaic::StringView label, Mosaic::StringView help = {})
        {
            (void)Mosaic::propertyGridLabel(ui);
            auto labelRow = Mosaic::row(ui);
            Mosaic::text(ui, label);

            if(help.empty() == false)
            {
                Mosaic::helpMarker(ui, help);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawTableTree(Mosaic::Context * ui, const Mosaic::Key & key, Mosaic::StringView label, const Mosaic::TableOptions & options, bool fixedColumns = false)
        {
            auto section = Mosaic::treeNode(ui, key, label);

            if(Detail::tableOpenAction != -1)
            {
                Mosaic::setTreeExpanded(ui, section.id(), Detail::tableOpenAction != 0);
            }

            if(section.expanded() == false)
            {
                return;
            }

            if(label == "Basic")
            {
                Mosaic::helpMarker(ui, "Using tableNextRow() and tableSetColumn() before every cell.");
                {
                    auto table = Mosaic::table(ui, "Basic table 1", 3, options);
                    for(size_t row = 0; row != 4; ++row)
                    {
                        Mosaic::tableNextRow(ui);
                        for(uint32_t column = 0; column != 3; ++column)
                        {
                            if(Mosaic::tableSetColumn(ui, column) == false)
                            {
                                continue;
                            }

                            Mosaic::String value = "Row ";
                            value += Detail::demoNumber(row);
                            value += " Column ";
                            value += Detail::demoNumber(column);
                            Mosaic::text(ui, value);
                        }
                    }
                }
                Mosaic::helpMarker(ui, "Using tableNextRow() and tableNextColumn() before each manually submitted cell.");
                {
                    auto table = Mosaic::table(ui, "Basic table 2", 3, options);
                    for(size_t row = 0; row != 4; ++row)
                    {
                        Mosaic::tableNextRow(ui);

                        if(Mosaic::tableNextColumn(ui) == true)
                        {
                            Mosaic::String value = "Row ";
                            value += Detail::demoNumber(row);
                            Mosaic::text(ui, value);
                        }

                        if(Mosaic::tableNextColumn(ui) == true)
                        {
                            Mosaic::text(ui, "Some contents");
                        }

                        if(Mosaic::tableNextColumn(ui) == true)
                        {
                            Mosaic::text(ui, "123.456");
                        }
                    }
                }
                Mosaic::helpMarker(ui, "Only using tableNextColumn(); the cursor wraps to a new row automatically.");
                {
                    auto table = Mosaic::table(ui, "Basic table 3", 3, options);
                    for(size_t item = 0; item != 14; ++item)
                    {
                        if(Mosaic::tableNextColumn(ui) == false)
                        {
                            continue;
                        }

                        Mosaic::String value = "Item ";
                        value += Detail::demoNumber(item);
                        Mosaic::text(ui, value);
                    }
                }

                return;
            }

            if(label == "Row height")
            {
                Mosaic::helpMarker(ui, "Each tableNextRow() call can provide a minimum row height. Cell padding still "
                                       "contributes to the final height.");
                Mosaic::TableOptions heightOptions = options;
                heightOptions.headers = false;
                heightOptions.bordersInnerHorizontal = true;
                heightOptions.bordersInnerVertical = true;
                heightOptions.bordersOuterHorizontal = true;
                heightOptions.bordersOuterVertical = true;
                {
                    auto table = Mosaic::table(ui, "Minimum row heights", 1, heightOptions);
                    for(size_t row = 0; row != 8; ++row)
                    {
                        Mosaic::TableRowOptions rowOptions;
                        rowOptions.minimumHeight = Mosaic::getTheme(ui).metrics.lineHeight * (0.3f * static_cast<float>(row)) + Mosaic::getTheme(ui).metrics.cellPadding.top + Mosaic::getTheme(ui).metrics.cellPadding.bottom;
                        Mosaic::tableNextRow(ui, rowOptions);
                        (void)Mosaic::tableSetColumn(ui, 0);
                        auto rowScope = Mosaic::scope(ui, Mosaic::Key(row));
                        Mosaic::String value = "minimum height = ";
                        value += Detail::demoFixed(rowOptions.minimumHeight, 2);
                        Mosaic::text(ui, value);
                    }
                }

                Mosaic::helpMarker(ui, "A row grows to the tallest cell; multi-line content in another cell shares the "
                                       "same final row height.");
                {
                    auto table = Mosaic::table(ui, "Shared row heights", 2, heightOptions);
                    for(size_t row = 0; row != 2; ++row)
                    {
                        Mosaic::tableNextRow(ui);
                        (void)Mosaic::tableSetColumn(ui, 0);
                        {
                            auto cell = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(row, 0)));
                            Mosaic::ButtonOptions swatchOptions;
                            swatchOptions.width = Mosaic::Dimension::fixed(40.f);
                            swatchOptions.height = Mosaic::Dimension::fixed(40.f);
                            Mosaic::Theme swatchTheme = Mosaic::getTheme(ui);
                            swatchTheme.colors.button = Mosaic::Color::fromBytes(33, 66, 102);
                            auto swatchStyle = Mosaic::styleScope(ui, swatchTheme);
                            Mosaic::button(ui, Mosaic::Key("row height swatch"), {}, swatchOptions);
                        }
                        (void)Mosaic::tableSetColumn(ui, 1);
                        {
                            auto cell = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(row, 1)));
                            Mosaic::text(ui, row == 0 ? "Line 1\nLine 2" : "Line 1, sharing the row height\nLine 2");
                        }
                    }
                }

                return;
            }

            if(label == "Resizable, mixed")
            {
                Mosaic::helpMarker(ui, "Fixed columns keep pixel widths while trailing stretch columns share the remaining width.");
                Mosaic::TableOptions mixedOptions = options;
                mixedOptions.headers = true;
                mixedOptions.rowBackground = true;
                mixedOptions.bordersInnerHorizontal = true;
                mixedOptions.bordersInnerVertical = true;
                mixedOptions.bordersOuterHorizontal = true;
                mixedOptions.bordersOuterVertical = true;
                constexpr Mosaic::Array<Mosaic::StringView, 6> names = {"AAA", "BBB", "CCC", "DDD", "EEE", "FFF"};
                auto drawMixedTable = [ui, &mixedOptions, &names](Mosaic::StringView tableLabel, uint32_t columnCount, uint32_t firstStretch)
                {
                    auto table = Mosaic::table(ui, tableLabel, columnCount, mixedOptions);
                    for(uint32_t column = 0; column != columnCount; ++column)
                    {
                        Mosaic::TableColumnOptions columnOptions;
                        columnOptions.sizing = column < firstStretch ? Mosaic::TableSizing::FixedFit : Mosaic::TableSizing::Stretch;
                        columnOptions.widthOrWeight = column < firstStretch ? 0.f : 1.f;

                        if(column == 2 && columnCount == 6)
                        {
                            columnOptions.visible = false;
                        }

                        if(column == 5 && columnCount == 6)
                        {
                            columnOptions.visible = false;
                        }

                        Mosaic::tableSetupColumn(ui, column, names[column], columnOptions);
                    }
                    Mosaic::tableHeadersRow(ui);
                    for(size_t row = 0; row != 5; ++row)
                    {
                        Mosaic::tableNextRow(ui);
                        for(uint32_t column = 0; column != columnCount; ++column)
                        {
                            if(Mosaic::tableSetColumn(ui, column) == false)
                            {
                                continue;
                            }

                            auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                            Mosaic::String value = column < firstStretch ? "Fixed " : "Stretch ";
                            value += Detail::demoNumber(column);
                            value += ",";
                            value += Detail::demoNumber(row);
                            Mosaic::text(ui, value);
                        }
                    }
                };
                drawMixedTable("Mixed table 1", 3, 2);
                drawMixedTable("Mixed table 2", 6, 3);

                return;
            }

            if(label == "Reorderable, hideable, with headers")
            {
                Mosaic::helpMarker(ui, "Drag headers to reorder columns. Right-click a header to change column visibility.");
                Mosaic::TableOptions reorderOptions = options;
                reorderOptions.headers = true;
                reorderOptions.resizable = true;
                reorderOptions.reorderable = true;
                reorderOptions.hideable = true;
                reorderOptions.bordersInnerVertical = true;
                reorderOptions.bordersOuterHorizontal = true;
                reorderOptions.bordersOuterVertical = true;
                auto drawReorderTable = [ui, &reorderOptions](Mosaic::StringView tableLabel, bool compact)
                {
                    Mosaic::LayoutOptions tableLayout;
                    tableLayout.width = compact ? Mosaic::Dimension(Mosaic::SizeRule::Content) : Mosaic::Dimension(Mosaic::SizeRule::Fill);
                    auto table = Mosaic::table(ui, tableLabel, 3, reorderOptions, tableLayout);
                    for(uint32_t column = 0; column != 3; ++column)
                    {
                        Mosaic::TableColumnOptions columnOptions;

                        if(compact == true)
                        {
                            columnOptions.sizing = Mosaic::TableSizing::FixedFit;
                            columnOptions.widthOrWeight = 0.f;
                        }

                        Mosaic::StringView header = column == 0 ? "One" : column == 1 ? "Two" : "Three";
                        Mosaic::tableSetupColumn(ui, column, header, columnOptions);
                    }
                    Mosaic::tableHeadersRow(ui);
                    for(size_t row = 0; row != 6; ++row)
                    {
                        Mosaic::tableNextRow(ui);
                        for(uint32_t column = 0; column != 3; ++column)
                        {
                            if(Mosaic::tableSetColumn(ui, column) == false)
                            {
                                continue;
                            }

                            auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                            Mosaic::String value = compact ? "Fixed " : "Hello ";
                            value += Detail::demoNumber(column);
                            value += ",";
                            value += Detail::demoNumber(row);
                            Mosaic::text(ui, value);
                        }
                    }
                };
                drawReorderTable("Reorder table 1", false);
                drawReorderTable("Reorder table 2", true);

                return;
            }

            if(Detail::drawReferenceTableDemo(ui, label, options) == true)
            {
                return;
            }

            static bool rowBackground = true;
            static bool bordersHorizontal = true;
            static bool bordersOuterHorizontal = true;
            static bool bordersInnerHorizontal = true;
            static bool bordersVertical = true;
            static bool bordersOuterVertical = true;
            static bool bordersInnerVertical = true;
            static bool noBordersInBody = false;
            static bool displayHeaders = true;
            static int borderContentsType = 0;
            static float angledHeaderAngle = 35.f;
            static float angledHeaderTextAlignment = 0.5f;
            static bool angledScrollHorizontal = true;
            static bool angledScrollVertical = true;
            static bool angledResizable = true;
            static bool angledSortable = false;
            static bool angledBordersInBody = true;
            static bool angledHighlightHoveredColumn = true;
            static bool angledHeaderContributesToWidth = true;
            static int32_t angledFrozenColumns = 1;
            static int32_t angledFrozenRows = 2;
            static Mosaic::Array<bool, 14 * 12> angledValues = {};
            static Mosaic::Array<bool, 3> customHeaderSelection = {};
            static Mosaic::Array<Mosaic::TableColumnOptions, 3> columnFlags = []
            {
                Mosaic::Array<Mosaic::TableColumnOptions, 3> value;
                value[0].defaultSort = true;
                value[2].visible = false;

                return value;
            }();

            if(label == "Padding")
            {
                static bool padOuterHorizontal = false;
                static bool noPadOuterHorizontal = false;
                static bool noPadInnerHorizontal = false;
                static bool paddingBordersOuterVertical = true;
                static bool paddingBordersInnerVertical = true;
                static bool paddingShowHeaders = false;
                static bool paddingSecondBorders = true;
                static bool paddingSecondRowBackground = true;
                static bool showWidgetFrameBackground = true;
                static Mosaic::Array<float, 2> cellPadding = {0.f, 0.f};
                static Mosaic::Array<Mosaic::String, 15> cellValues = []
                {
                    Mosaic::Array<Mosaic::String, 15> values;
                    values.fill("edit me");

                    return values;
                }();

                Mosaic::helpMarker(ui, "Outer and inner horizontal padding can be controlled independently from vertical borders.");
                Mosaic::checkbox(ui, "MosaicTableFlags_PadOuterX", &padOuterHorizontal);
                Mosaic::checkbox(ui, "MosaicTableFlags_NoPadOuterX", &noPadOuterHorizontal);

                if(padOuterHorizontal == true && noPadOuterHorizontal == true)
                {
                    noPadOuterHorizontal = false;
                }

                Mosaic::checkbox(ui, "MosaicTableFlags_NoPadInnerX", &noPadInnerHorizontal);
                Mosaic::checkbox(ui, "MosaicTableFlags_BordersOuterV", &paddingBordersOuterVertical);
                Mosaic::checkbox(ui, "MosaicTableFlags_BordersInnerV", &paddingBordersInnerVertical);
                Mosaic::checkbox(ui, "show_headers", &paddingShowHeaders);
                Mosaic::TableOptions firstOptions;
                firstOptions.headers = paddingShowHeaders;
                firstOptions.bordersOuterVertical = paddingBordersOuterVertical;
                firstOptions.bordersInnerVertical = paddingBordersInnerVertical;
                firstOptions.padOuterHorizontal = padOuterHorizontal && noPadOuterHorizontal == false;
                firstOptions.padInnerHorizontal = noPadInnerHorizontal == false;
                {
                    auto table = Mosaic::table(ui, "table_padding", 3, firstOptions);
                    for(uint32_t column = 0; column != 3; ++column)
                    {
                        Mosaic::String header = column == 0 ? "One" : column == 1 ? "Two" : "Three";
                        Mosaic::tableSetupColumn(ui, column, header);
                    }

                    if(paddingShowHeaders == true)
                    {
                        Mosaic::tableHeadersRow(ui);
                    }

                    for(size_t row = 0; row != 5; ++row)
                    {
                        Mosaic::tableNextRow(ui);
                        for(uint32_t column = 0; column != 3; ++column)
                        {
                            if(Mosaic::tableSetColumn(ui, column) == false)
                            {
                                continue;
                            }

                            auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(row, column)));
                            Mosaic::ButtonOptions buttonOptions;
                            buttonOptions.width = Mosaic::SizeRule::Fill;
                            Mosaic::String cell = "Hello ";
                            cell += Detail::demoNumber(column);
                            cell += ",";
                            cell += Detail::demoNumber(row);
                            Mosaic::button(ui, Mosaic::Key("padding button"), cell, buttonOptions);
                        }
                    }
                }

                Mosaic::separator(ui);
                Mosaic::helpMarker(ui, "The second table changes CellPadding and contains an editable control in every cell.");
                Mosaic::checkbox(ui, "MosaicTableFlags_Borders", &paddingSecondBorders);
                Mosaic::checkbox(ui, "MosaicTableFlags_RowBg", &paddingSecondRowBackground);
                Mosaic::checkbox(ui, "show_widget_frame_bg", &showWidgetFrameBackground);
                Mosaic::vectorEditor(ui, "CellPadding", Mosaic::FloatSpan(cellPadding), 0.f, 10.f);
                Mosaic::Theme paddingTheme = Mosaic::getTheme(ui);
                paddingTheme.metrics.cellPadding = Mosaic::EdgeInsets(cellPadding[0], cellPadding[1]);

                if(showWidgetFrameBackground == false)
                {
                    paddingTheme.colors.frame.a = 0.f;
                    paddingTheme.colors.frameHovered.a = 0.f;
                    paddingTheme.colors.frameActive.a = 0.f;
                }

                auto paddingStyle = Mosaic::styleScope(ui, paddingTheme);
                Mosaic::TableOptions secondOptions;
                secondOptions.rowBackground = paddingSecondRowBackground;
                secondOptions.bordersOuterHorizontal = paddingSecondBorders;
                secondOptions.bordersInnerHorizontal = paddingSecondBorders;
                secondOptions.bordersOuterVertical = paddingSecondBorders;
                secondOptions.bordersInnerVertical = paddingSecondBorders;
                auto table = Mosaic::table(ui, "table_padding_2", 3, secondOptions);
                for(size_t cell = 0; cell != cellValues.size(); ++cell)
                {
                    if(Mosaic::tableNextColumn(ui) == false)
                    {
                        continue;
                    }

                    auto cellScope = Mosaic::scope(ui, Mosaic::Key(cell));
                    Mosaic::TextInputOptions inputOptions;
                    Mosaic::inputText(ui, {}, &cellValues[cell], inputOptions);
                }

                return;
            }

            Mosaic::TableOptions effectiveOptions = options;

            if(label == "Borders, background")
            {
                Mosaic::checkbox(ui, "RowBg", &rowBackground);
                Mosaic::checkbox(ui, "BordersH", &bordersHorizontal);
                {
                    auto horizontal = Mosaic::column(ui, Detail::indentedLayout(ui));
                    Mosaic::checkbox(ui, "BordersOuterH", &bordersOuterHorizontal);
                    Mosaic::checkbox(ui, "BordersInnerH", &bordersInnerHorizontal);
                }
                Mosaic::checkbox(ui, "BordersV", &bordersVertical);
                {
                    auto vertical = Mosaic::column(ui, Detail::indentedLayout(ui));
                    Mosaic::checkbox(ui, "BordersOuterV", &bordersOuterVertical);
                    Mosaic::checkbox(ui, "BordersInnerV", &bordersInnerVertical);
                }
                Mosaic::checkbox(ui, "NoBordersInBody", &noBordersInBody);
                {
                    auto contents = Mosaic::row(ui);
                    Mosaic::text(ui, "Cell contents:");

                    if(Mosaic::radioButton(ui, "Text", borderContentsType == 0).clicked() == true)
                    {
                        borderContentsType = 0;
                    }

                    if(Mosaic::radioButton(ui, "FillButton", borderContentsType == 1).clicked() == true)
                    {
                        borderContentsType = 1;
                    }
                }
                Mosaic::checkbox(ui, "Display headers", &displayHeaders);
                effectiveOptions.headers = displayHeaders;
                effectiveOptions.rowBackground = rowBackground;
                effectiveOptions.bordersInnerHorizontal = bordersHorizontal && bordersInnerHorizontal;
                effectiveOptions.bordersOuterHorizontal = bordersHorizontal && bordersOuterHorizontal;
                effectiveOptions.bordersInnerVertical = bordersVertical && bordersInnerVertical;
                effectiveOptions.bordersOuterVertical = bordersVertical && bordersOuterVertical;
                effectiveOptions.bordersInBody = noBordersInBody == false;
            }
            else if(label == "Sizing policies")
            {
                constexpr Mosaic::Array<Mosaic::StringView, 4> policyNames = {"SizingFixedFit", "SizingFixedSame", "SizingStretchProp", "SizingStretchSame"};
                static Mosaic::Array<int, 4> policySelections = {0, 1, 2, 3};
                static bool sizingResizable = false;
                static bool noHostExtendHorizontal = false;
                Mosaic::checkbox(ui, "MosaicTableFlags_Resizable", &sizingResizable);
                Mosaic::checkbox(ui, "MosaicTableFlags_NoHostExtendX", &noHostExtendHorizontal);
                Mosaic::helpMarker(ui, "Each sizing policy is shown with equal-width and different-width contents.");

                auto sizingFromSelection = [](int selection)
                {
                    switch(selection)
                    {
                    case 0:
                        return Mosaic::TableSizing::FixedFit;
                    case 1:
                        return Mosaic::TableSizing::FixedSame;
                    case 2:
                        return Mosaic::TableSizing::StretchProportional;
                    default:
                        return Mosaic::TableSizing::StretchSame;
                    }
                };
                auto drawSizingTable = [ui, &sizingFromSelection](Mosaic::StringView tableName, int selection, bool variedContents)
                {
                    Mosaic::TableOptions sizingOptions;
                    sizingOptions.headers = false;
                    sizingOptions.resizable = sizingResizable;
                    sizingOptions.reorderable = false;
                    sizingOptions.hideable = false;
                    sizingOptions.rowBackground = true;
                    sizingOptions.bordersInnerVertical = true;
                    sizingOptions.bordersOuterHorizontal = true;
                    sizingOptions.bordersOuterVertical = true;
                    sizingOptions.contextMenuInBody = true;
                    sizingOptions.extendHostHorizontal = noHostExtendHorizontal == false;
                    Mosaic::LayoutOptions sizingLayout;
                    sizingLayout.width = noHostExtendHorizontal ? Mosaic::Dimension(Mosaic::SizeRule::Content) : Mosaic::Dimension(Mosaic::SizeRule::Fill);
                    auto table = Mosaic::table(ui, tableName, 3, sizingOptions, sizingLayout);
                    for(uint32_t column = 0; column != 3; ++column)
                    {
                        Mosaic::TableColumnOptions columnOptions;
                        columnOptions.sizing = sizingFromSelection(selection);
                        columnOptions.widthOrWeight = columnOptions.sizing == Mosaic::TableSizing::StretchProportional ? static_cast<float>(column + 1) : columnOptions.sizing == Mosaic::TableSizing::StretchSame ? 1.f : 0.f;
                        Mosaic::tableSetupColumn(ui, column, {}, columnOptions);
                    }
                    constexpr Mosaic::Array<Mosaic::StringView, 3> varied = {"AAAA", "BBBBBBBB", "CCCCCCCCCCCC"};
                    for(size_t row = 0; row != 3; ++row)
                    {
                        Mosaic::tableNextRow(ui);
                        for(uint32_t column = 0; column != 3; ++column)
                        {
                            if(Mosaic::tableSetColumn(ui, column) == false)
                            {
                                continue;
                            }

                            Mosaic::text(ui, variedContents ? varied[column] : "Oh dear");
                        }
                    }
                };

                for(size_t policy = 0; policy != policySelections.size(); ++policy)
                {
                    auto policyScope = Mosaic::scope(ui, Mosaic::Key(policy));
                    Mosaic::comboBox(ui, "Sizing policy", &policySelections[policy], policyNames);
                    Mosaic::String equalTable = "Equal contents ";
                    equalTable += Detail::demoNumber(policy);
                    Mosaic::String variedTable = "Varied contents ";
                    variedTable += Detail::demoNumber(policy);
                    drawSizingTable(equalTable, policySelections[policy], false);
                    drawSizingTable(variedTable, policySelections[policy], true);
                }

                Mosaic::separatorText(ui, "Advanced");
                Mosaic::helpMarker(ui, "Interact with sizing, scrolling and content options to inspect column allocation.");
                static int advancedPolicy = 2;
                static int advancedContents = 0;
                static int32_t advancedColumns = 3;
                static bool advancedResizable = true;
                static bool advancedPreciseWidths = false;
                static bool advancedScrollHorizontal = false;
                static bool advancedScrollVertical = true;
                static bool advancedNoClip = false;
                static Mosaic::String advancedInput;
                constexpr Mosaic::Array<Mosaic::StringView, 6> contentNames = {"Show width", "Short Text", "Long Text", "Button", "Fill Button", "InputText"};
                Mosaic::comboBox(ui, "Sizing policy", &advancedPolicy, policyNames);
                Mosaic::comboBox(ui, "Contents", &advancedContents, contentNames);
                Mosaic::dragValue(ui, "Columns", &advancedColumns, int32_t{1}, int32_t{64});
                Mosaic::checkbox(ui, "MosaicTableFlags_Resizable", &advancedResizable);
                Mosaic::checkbox(ui, "MosaicTableFlags_PreciseWidths", &advancedPreciseWidths);
                Mosaic::helpMarker(ui, "Precise widths do not distribute remainder pixels between stretch columns.");
                Mosaic::checkbox(ui, "MosaicTableFlags_ScrollX", &advancedScrollHorizontal);
                Mosaic::checkbox(ui, "MosaicTableFlags_ScrollY", &advancedScrollVertical);
                Mosaic::checkbox(ui, "MosaicTableFlags_NoClip", &advancedNoClip);

                uint32_t advancedColumnCount = static_cast<uint32_t>(std::clamp(advancedColumns, int32_t{1}, int32_t{64}));
                Mosaic::TableOptions advancedOptions;
                advancedOptions.headers = false;
                advancedOptions.resizable = advancedResizable;
                advancedOptions.reorderable = false;
                advancedOptions.hideable = false;
                advancedOptions.rowBackground = true;
                advancedOptions.bordersInnerHorizontal = true;
                advancedOptions.bordersInnerVertical = true;
                advancedOptions.bordersOuterHorizontal = true;
                advancedOptions.bordersOuterVertical = true;
                advancedOptions.clipCells = advancedNoClip == false;
                advancedOptions.preciseWidths = advancedPreciseWidths;
                advancedOptions.scrollHorizontal = advancedScrollHorizontal;
                advancedOptions.scrollVertical = advancedScrollVertical;
                advancedOptions.innerWidth = advancedScrollHorizontal ? 960.f : 0.f;
                Mosaic::LayoutOptions advancedLayout;
                advancedLayout.width = Mosaic::SizeRule::Fill;
                advancedLayout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 7.f);
                auto advancedTable = Mosaic::table(ui, "Advanced sizing table", advancedColumnCount, advancedOptions, advancedLayout);
                for(uint32_t column = 0; column != advancedColumnCount; ++column)
                {
                    Mosaic::TableColumnOptions columnOptions;
                    columnOptions.sizing = sizingFromSelection(advancedPolicy);
                    columnOptions.widthOrWeight = columnOptions.sizing == Mosaic::TableSizing::StretchProportional ? static_cast<float>(column + 1) : columnOptions.sizing == Mosaic::TableSizing::StretchSame ? 1.f : 0.f;
                    Mosaic::tableSetupColumn(ui, column, {}, columnOptions);
                }
                for(size_t cell = 0; cell != static_cast<size_t>(advancedColumnCount) * 10; ++cell)
                {
                    if(Mosaic::tableNextColumn(ui) == false)
                    {
                        continue;
                    }

                    uint32_t column = static_cast<uint32_t>(cell % advancedColumnCount);
                    size_t row = cell / advancedColumnCount;
                    auto cellScope = Mosaic::scope(ui, Mosaic::Key(cell));
                    Mosaic::String cellLabel = "Hello ";
                    cellLabel += Detail::demoNumber(column);
                    cellLabel += ",";
                    cellLabel += Detail::demoNumber(row);
                    switch(advancedContents)
                    {
                    case 0:
                    {
                        Mosaic::TableColumnStatus status;
                        if(Mosaic::tableColumnStatus(ui, advancedTable.id(), column, &status) == false)
                        {
                            break;
                        }

                        Mosaic::String width = "W: ";
                        width += Detail::demoFixed(status.width, 1);
                        Mosaic::text(ui, width);
                        break;
                    }
                    case 1:
                        Mosaic::text(ui, cellLabel);
                        break;
                    case 2:
                        Mosaic::text(ui, column == 0 ? "Some long text\nOver two lines.." : "Some longeeer text\nOver two lines..");
                        break;
                    case 3:
                        Mosaic::button(ui, Mosaic::Key("advanced button"), cellLabel);
                        break;
                    case 4:
                    {
                        Mosaic::ButtonOptions buttonOptions;
                        buttonOptions.width = Mosaic::SizeRule::Fill;
                        Mosaic::button(ui, Mosaic::Key("advanced fill button"), cellLabel, buttonOptions);
                        break;
                    }
                    default:
                        Mosaic::inputText(ui, {}, &advancedInput);
                        break;
                    }
                }

                return;
            }
            else if(label == "Columns flags")
            {
                Mosaic::TableOptions editorOptions;
                editorOptions.headers = false;
                editorOptions.resizable = false;
                editorOptions.reorderable = false;
                editorOptions.hideable = false;
                editorOptions.sortable = false;
                auto editor = Mosaic::table(ui, "Column flags editor", 3, editorOptions);
                for(uint32_t column = 0; column != 3; ++column)
                {
                    Mosaic::TableColumnOptions editorColumn;
                    editorColumn.sortable = false;
                    Mosaic::tableSetupColumn(ui, column, {}, editorColumn);
                }
                Mosaic::tableNextRow(ui);
                for(uint32_t column = 0; column != 3; ++column)
                {
                    (void)Mosaic::tableSetColumn(ui, column);
                    auto columnScope = Mosaic::scope(ui, Mosaic::Key(column));
                    Mosaic::String name = "'Column ";
                    name += Detail::demoNumber(column);
                    name += "'";
                    Mosaic::separatorText(ui, name);
                    Mosaic::TableColumnOptions & flags = columnFlags[column];
                    bool disabled = flags.enabled == false;
                    if(Mosaic::checkbox(ui, "_Disabled", &disabled).changed() == true)
                    {
                        flags.enabled = disabled == false;
                    }

                    bool defaultHidden = flags.visible == false;
                    if(Mosaic::checkbox(ui, "_DefaultHide", &defaultHidden).changed() == true)
                    {
                        flags.visible = defaultHidden == false;
                    }

                    Mosaic::checkbox(ui, "_DefaultSort", &flags.defaultSort);

                    if(Mosaic::radioButton(ui, "_WidthStretch", flags.sizing == Mosaic::TableSizing::Stretch).clicked() == true)
                    {
                        flags.sizing = Mosaic::TableSizing::Stretch;
                    }

                    if(Mosaic::radioButton(ui, "_WidthFixed", flags.sizing == Mosaic::TableSizing::Fixed).clicked() == true)
                    {
                        flags.sizing = Mosaic::TableSizing::Fixed;
                    }

                    bool noResize = flags.resizable == false;
                    if(Mosaic::checkbox(ui, "_NoResize", &noResize).changed() == true)
                    {
                        flags.resizable = noResize == false;
                    }

                    bool noReorder = flags.reorderable == false;
                    if(Mosaic::checkbox(ui, "_NoReorder", &noReorder).changed() == true)
                    {
                        flags.reorderable = noReorder == false;
                    }

                    bool noHide = flags.hideable == false;
                    if(Mosaic::checkbox(ui, "_NoHide", &noHide).changed() == true)
                    {
                        flags.hideable = noHide == false;
                    }

                    bool noClip = flags.clip == false;
                    if(Mosaic::checkbox(ui, "_NoClip", &noClip).changed() == true)
                    {
                        flags.clip = noClip == false;
                    }

                    bool noSort = flags.sortable == false;
                    if(Mosaic::checkbox(ui, "_NoSort", &noSort).changed() == true)
                    {
                        flags.sortable = noSort == false;
                    }

                    bool noAscending = flags.sortAscending == false;
                    if(Mosaic::checkbox(ui, "_NoSortAscending", &noAscending).changed() == true)
                    {
                        flags.sortAscending = noAscending == false;
                    }

                    bool noDescending = flags.sortDescending == false;
                    if(Mosaic::checkbox(ui, "_NoSortDescending", &noDescending).changed() == true)
                    {
                        flags.sortDescending = noDescending == false;
                    }

                    bool noHeaderLabel = flags.headerLabelVisible == false;
                    if(Mosaic::checkbox(ui, "_NoHeaderLabel", &noHeaderLabel).changed() == true)
                    {
                        flags.headerLabelVisible = noHeaderLabel == false;
                    }

                    bool noHeaderWidth = flags.headerContributesToWidth == false;
                    if(Mosaic::checkbox(ui, "_NoHeaderWidth", &noHeaderWidth).changed() == true)
                    {
                        flags.headerContributesToWidth = noHeaderWidth == false;
                    }

                    if(Mosaic::radioButton(ui, "_IndentDefault", flags.indent == Mosaic::TableColumnIndent::Default).clicked() == true)
                    {
                        flags.indent = Mosaic::TableColumnIndent::Default;
                    }

                    if(Mosaic::radioButton(ui, "_IndentEnable", flags.indent == Mosaic::TableColumnIndent::Enable).clicked() == true)
                    {
                        flags.indent = Mosaic::TableColumnIndent::Enable;
                    }

                    if(Mosaic::radioButton(ui, "_IndentDisable", flags.indent == Mosaic::TableColumnIndent::Disable).clicked() == true)
                    {
                        flags.indent = Mosaic::TableColumnIndent::Disable;
                    }

                    if(Mosaic::radioButton(ui, "_PreferSortAscending", flags.preferredSort == Mosaic::SortDirection::Ascending).clicked() == true)
                    {
                        flags.preferredSort = Mosaic::SortDirection::Ascending;
                    }

                    if(Mosaic::radioButton(ui, "_PreferSortDescending", flags.preferredSort == Mosaic::SortDirection::Descending).clicked() == true)
                    {
                        flags.preferredSort = Mosaic::SortDirection::Descending;
                    }

                    Mosaic::checkbox(ui, "_AngledHeader", &flags.angledHeader);
                }
                effectiveOptions.scrollHorizontal = true;
                effectiveOptions.scrollVertical = true;
                effectiveOptions.rowBackground = true;
                effectiveOptions.bordersOuterHorizontal = true;
                effectiveOptions.bordersOuterVertical = true;
                effectiveOptions.bordersInnerVertical = true;
                effectiveOptions.sortable = true;
            }
            else if(label == "Synced instances")
            {
                Mosaic::text(ui, "Tables sharing one stable ID also share column widths, visibility, order and sort specifications.");
            }
            else if(label == "Angled headers")
            {
                Mosaic::checkbox(ui, "_ScrollX", &angledScrollHorizontal);
                Mosaic::checkbox(ui, "_ScrollY", &angledScrollVertical);
                Mosaic::checkbox(ui, "_Resizable", &angledResizable);
                Mosaic::checkbox(ui, "_Sortable", &angledSortable);
                bool noBordersInBody = angledBordersInBody == false;
                if(Mosaic::checkbox(ui, "_NoBordersInBody", &noBordersInBody).changed() == true)
                {
                    angledBordersInBody = noBordersInBody == false;
                }

                Mosaic::checkbox(ui, "_HighlightHoveredColumn", &angledHighlightHoveredColumn);
                Mosaic::dragValue(ui, "Frozen columns", &angledFrozenColumns, int32_t{0}, int32_t{2});
                Mosaic::dragValue(ui, "Frozen rows", &angledFrozenRows, int32_t{0}, int32_t{2});
                bool noHeaderWidth = angledHeaderContributesToWidth == false;
                if(Mosaic::checkbox(ui, "Disable header contributing to column width", &noHeaderWidth).changed() == true)
                {
                    angledHeaderContributesToWidth = noHeaderWidth == false;
                }

                effectiveOptions.scrollHorizontal = angledScrollHorizontal;
                effectiveOptions.scrollVertical = angledScrollVertical;
                effectiveOptions.resizable = angledResizable;
                effectiveOptions.sortable = angledSortable;
                effectiveOptions.hideable = true;
                effectiveOptions.reorderable = true;
                effectiveOptions.bordersOuterHorizontal = true;
                effectiveOptions.bordersOuterVertical = true;
                effectiveOptions.bordersInnerHorizontal = true;
                effectiveOptions.bordersInBody = angledBordersInBody;
                effectiveOptions.highlightHoveredColumn = angledHighlightHoveredColumn;
                effectiveOptions.frozenColumns = static_cast<uint32_t>(std::max(angledFrozenColumns, int32_t{0}));
                effectiveOptions.frozenRows = static_cast<uint32_t>(std::max(angledFrozenRows, int32_t{0}));
                effectiveOptions.innerWidth = angledScrollHorizontal ? 760.f : 0.f;
            }

            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.height = Mosaic::Dimension::fixed(effectiveOptions.scrollHorizontal || effectiveOptions.scrollVertical == true || label == "Angled headers" ? 190.f : 132.f);
            Mosaic::Theme tableTheme = Mosaic::getTheme(ui);

            if(label == "Angled headers")
            {
                tableTheme.metrics.tableAngledHeadersAngleDegrees = angledHeaderAngle;
                tableTheme.metrics.tableAngledHeadersTextAlignment = angledHeaderTextAlignment;
            }

            auto style = Mosaic::styleScope(ui, tableTheme);

            if(label == "Angled headers")
            {
                constexpr Mosaic::Array<Mosaic::StringView, 14> columnNames = {"Track", "cabasa", "ride", "smash", "tom-hi", "tom-mid", "tom-low", "hihat-o", "hihat-c", "snare-s", "snare-c", "clap", "rim", "kick"};
                constexpr uint32_t columnCount = static_cast<uint32_t>(columnNames.size());
                constexpr size_t rowCount = 12;
                Mosaic::LayoutOptions angledLayout;
                angledLayout.width = Mosaic::SizeRule::Fill;
                angledLayout.height = Mosaic::Dimension::fixed(228.f);
                auto table = Mosaic::table(ui, "Angled header tracks", columnCount, effectiveOptions, angledLayout);
                for(uint32_t column = 0; column != columnCount; ++column)
                {
                    Mosaic::TableColumnOptions columnOptions;
                    columnOptions.sizing = Mosaic::TableSizing::FixedFit;
                    columnOptions.widthOrWeight = column == 0 ? 78.f : 32.f;
                    columnOptions.hideable = column != 0;
                    columnOptions.reorderable = column != 0;
                    columnOptions.angledHeader = column != 0;
                    columnOptions.headerContributesToWidth = column == 0 || angledHeaderContributesToWidth;
                    Mosaic::tableSetupColumn(ui, column, columnNames[column], columnOptions);
                }
                Mosaic::tableAngledHeadersRow(ui);
                Mosaic::tableHeadersRow(ui);
                for(size_t row = 0; row != rowCount; ++row)
                {
                    Mosaic::tableNextRow(ui);

                    if(Mosaic::tableSetColumn(ui, 0) == true)
                    {
                        Mosaic::String track = "Track ";
                        track += Detail::demoNumber(row);
                        Mosaic::text(ui, track);
                    }

                    for(uint32_t column = 1; column != columnCount; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == false)
                        {
                            continue;
                        }

                        auto cell = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                        Mosaic::checkbox(ui, Mosaic::Key("track enabled"), {}, &angledValues[row * columnCount + column]);
                    }
                }

                auto settings = Mosaic::treeNode(ui, Mosaic::Key("angled style settings"), "Style settings");

                if(settings.expanded() == true)
                {
                    Mosaic::helpMarker(ui, "These controls edit the active table angled-header style.");
                    Mosaic::slider(ui, "Angled headers angle", &angledHeaderAngle, -50.f, 50.f);
                    Mosaic::slider(ui, "Angled headers text alignment", &angledHeaderTextAlignment, 0.f, 1.f);
                }

                return;
            }

            if(label == "Synced instances")
            {
                Mosaic::Id sharedSettings = Mosaic::hashBytes("Hello demo synchronized tables");
                Mosaic::LayoutOptions instancesLayout;
                instancesLayout.width = Mosaic::SizeRule::Fill;
                auto instances = Mosaic::grid(ui, 2, instancesLayout);
                for(size_t instance = 0; instance != 4; ++instance)
                {
                    auto instanceScope = Mosaic::scope(ui, Mosaic::Key(instance));
                    Mosaic::String instanceLabel = "Instance ";
                    instanceLabel += Detail::demoNumber(instance + 1);
                    Mosaic::text(ui, instanceLabel);
                    Mosaic::TableOptions synchronizedOptions = effectiveOptions;
                    synchronizedOptions.settingsId = sharedSettings;
                    synchronizedOptions.sortable = true;
                    Mosaic::LayoutOptions synchronizedLayout;
                    synchronizedLayout.width = instance % 2 == 0 ? Mosaic::SizeRule::Fill : Mosaic::Dimension::fixed(260.f);
                    synchronizedLayout.height = Mosaic::Dimension::fixed(instance < 2 ? 104.f : 132.f);
                    auto synchronizedTable = Mosaic::table(ui, "Synchronized table", 3, synchronizedOptions, synchronizedLayout);
                    Mosaic::tableSetupColumn(ui, 0, "Name");
                    Mosaic::tableSetupColumn(ui, 1, "Type");
                    Mosaic::tableSetupColumn(ui, 2, "Value");
                    Mosaic::tableHeadersRow(ui);
                    for(size_t row = 0; row != 3; ++row)
                    {
                        Mosaic::tableNextRow(ui);
                        for(uint32_t column = 0; column != 3; ++column)
                        {
                            if(Mosaic::tableSetColumn(ui, column) == false)
                            {
                                continue;
                            }

                            Mosaic::String cell = "Cell ";
                            cell += Detail::demoNumber(column);
                            cell += ",";
                            cell += Detail::demoNumber(row);
                            Mosaic::text(ui, cell);
                        }
                    }
                }

                return;
            }

            if(label == "Columns widths")
            {
                constexpr uint32_t columnCount = 7;
                Mosaic::TableOptions widthOptions = effectiveOptions;
                widthOptions.scrollHorizontal = true;
                widthOptions.bordersInnerVertical = true;
                widthOptions.bordersOuterVertical = true;
                widthOptions.innerWidth = 760.f;
                Mosaic::LayoutOptions widthLayout;
                widthLayout.width = Mosaic::SizeRule::Fill;
                widthLayout.height = Mosaic::Dimension::fixed(184.f);
                auto widthTable = Mosaic::table(ui, "Column width policies", columnCount, widthOptions, widthLayout);
                for(uint32_t column = 0; column != columnCount; ++column)
                {
                    Mosaic::TableColumnOptions columnOptions;
                    columnOptions.sizing = column < 4 ? Mosaic::TableSizing::Fixed : Mosaic::TableSizing::Stretch;
                    columnOptions.widthOrWeight = column < 4 ? 54.f + static_cast<float>(column) * 24.f : static_cast<float>(column - 3);
                    Mosaic::String columnLabel = column < 4 ? "Fixed " : "Stretch ";
                    columnLabel += Detail::demoNumber(column);
                    Mosaic::tableSetupColumn(ui, column, columnLabel, columnOptions);
                }
                Mosaic::tableHeadersRow(ui);
                for(size_t row = 0; row != 5; ++row)
                {
                    Mosaic::tableNextRow(ui);
                    for(uint32_t column = 0; column != columnCount; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == false)
                        {
                            continue;
                        }

                        auto cell = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                        Mosaic::TableColumnStatus status;
                        if(Mosaic::tableColumnStatus(ui, widthTable.id(), column, &status) == false)
                        {
                            continue;
                        }

                        Mosaic::String value = "W=";
                        value += Detail::demoFixed(status.width, 0);
                        Mosaic::text(ui, value);
                    }
                }

                return;
            }

            Mosaic::Id tableId = Mosaic::InvalidId;
            {
                Mosaic::String tableIdentity(label);
                auto table = Mosaic::table(ui, tableIdentity, 3, effectiveOptions, layout);
                tableId = table.id();
                for(uint32_t column = 0; column != 3; ++column)
                {
                    Mosaic::String columnLabel = "Column ";
                    columnLabel += Detail::demoNumber(column);

                    if(label == "Custom headers")
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 3> headerLabels = {"Apricot", "Banana", "Cherry"};
                        columnLabel = headerLabels[column];
                    }

                    Mosaic::TableColumnOptions columnOptions;
                    columnOptions.angledHeader = label == "Angled headers";

                    if(fixedColumns == true)
                    {
                        columnOptions.sizing = Mosaic::TableSizing::Fixed;
                        columnOptions.widthOrWeight = 110.f + static_cast<float>(column) * 18.f;
                    }
                    else if(label == "Resizable, mixed")
                    {
                        columnOptions.sizing = column == 1 ? Mosaic::TableSizing::Stretch : Mosaic::TableSizing::Fixed;
                        columnOptions.widthOrWeight = column == 1 ? 1.f : 110.f;
                    }

                    if(label == "Columns flags")
                    {
                        columnOptions = columnFlags[column];

                        if(columnOptions.sizing == Mosaic::TableSizing::Fixed && columnOptions.widthOrWeight <= 1.f)
                        {
                            columnOptions.widthOrWeight = 110.f;
                        }
                    }

                    Mosaic::tableSetupColumn(ui, column, columnLabel, columnOptions);
                }

                if(label == "Custom headers")
                {
                    Mosaic::tableNextRow(ui);
                    for(uint32_t column = 0; column != 3; ++column)
                    {
                        (void)Mosaic::tableSetColumn(ui, column);
                        auto headerScope = Mosaic::scope(ui, Mosaic::Key(column));
                        auto headerRow = Mosaic::row(ui);
                        Mosaic::checkbox(ui, Mosaic::Key("select column"), {}, &customHeaderSelection[column]);
                        Mosaic::StringView columnName;
                        if(Mosaic::tableColumnName(ui, table.id(), column, &columnName) == true)
                        {
                            Mosaic::tableHeader(ui, column, columnName);
                        }
                    }
                }
                else
                {
                    Mosaic::tableHeadersRow(ui);
                }

                size_t rowCount = effectiveOptions.scrollVertical ? 50 : label == "Row height" ? 8 : 6;
                Mosaic::Array<size_t, 50> rowOrder = {};
                for(size_t row = 0; row != rowCount; ++row)
                {
                    rowOrder[row] = row;
                }

                if(effectiveOptions.sortable == true)
                {
                    Mosaic::TableSortSpecSpan specs;
                    if(Mosaic::tableSortSpecs(ui, table.id(), &specs) == true)
                    {
                        for(size_t specIndex = specs.size(); specIndex > 0; --specIndex)
                        {
                            const Mosaic::TableSortSpec & spec = specs[specIndex - 1];
                            std::stable_sort(rowOrder.begin(), rowOrder.begin() + rowCount,
                                             [spec, rowCount](size_t first, size_t second)
                                             {
                                                 size_t firstValue = spec.column == 0 ? first : spec.column == 1 ? (first * 7) % rowCount : rowCount - first;
                                                 size_t secondValue = spec.column == 0 ? second : spec.column == 1 ? (second * 7) % rowCount : rowCount - second;

                                                 return spec.direction == Mosaic::SortDirection::Ascending ? firstValue < secondValue : firstValue > secondValue;
                                             });
                        }
                    }
                }

                Mosaic::VisibleRange rows = {0, rowCount};

                if(effectiveOptions.scrollVertical == true)
                {
                    (void)Mosaic::tableVisibleRows(ui, rowCount, Mosaic::getTheme(ui).metrics.controlHeight, &rows);
                }

                for(size_t rowIndex = rows.begin; rowIndex != rows.end; ++rowIndex)
                {
                    size_t row = rowOrder[rowIndex];

                    if(label == "Row height")
                    {
                        Mosaic::TableRowOptions rowOptions;
                        rowOptions.minimumHeight = row % 2 == 0 ? 38.f : 24.f;
                        Mosaic::tableNextRow(ui, rowOptions);
                    }
                    else
                    {
                        Mosaic::tableNextRow(ui);
                    }

                    for(uint32_t column = 0; column != 3; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == false)
                        {
                            continue;
                        }

                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), column)));
                        Mosaic::String cell = "Cell ";
                        cell += Detail::demoNumber(column);
                        cell += ",";
                        cell += Detail::demoNumber(row);

                        if(label == "Item width")
                        {
                            static float itemWidthValue = 0.5f;
                            Mosaic::StringView itemLabel = column == 0 ? "float0" : column == 1 ? "float1" : Mosaic::StringView{};
                            Mosaic::slider(ui, itemLabel, &itemWidthValue, 0.f, 1.f);
                        }
                        else if(label == "Borders, background" && borderContentsType == 1)
                        {
                            Mosaic::ButtonOptions buttonOptions;
                            buttonOptions.width = Mosaic::SizeRule::Fill;
                            Mosaic::button(ui, Mosaic::Key("border fill button"), cell, buttonOptions);
                        }
                        else if(label == "Row height" && column == 1 && row % 2 == 0)
                        {
                            cell += "\nMulti-line cell";
                            Mosaic::text(ui, cell);
                        }
                        else if(label == "Nested tables" && row == 1 && column == 1)
                        {
                            Mosaic::TableOptions nestedOptions;
                            nestedOptions.resizable = false;
                            nestedOptions.reorderable = false;
                            nestedOptions.hideable = false;
                            Mosaic::LayoutOptions nestedLayout;
                            nestedLayout.width = Mosaic::SizeRule::Fill;
                            auto nested = Mosaic::table(ui, "Nested", 2, nestedOptions, nestedLayout);
                            Mosaic::tableSetupColumn(ui, 0, "Key");
                            Mosaic::tableSetupColumn(ui, 1, "Value");
                            Mosaic::tableHeadersRow(ui);
                            Mosaic::tableNextRow(ui);
                            (void)Mosaic::tableSetColumn(ui, 0);
                            Mosaic::text(ui, "Child");
                            (void)Mosaic::tableSetColumn(ui, 1);
                            Mosaic::text(ui, "Table");
                        }
                        else if(label == "Custom headers")
                        {
                            Mosaic::selectable(ui, Mosaic::Key("custom cell"), cell, customHeaderSelection[column]);
                        }
                        else
                        {
                            Mosaic::text(ui, cell);
                        }
                    }
                }
            }

            if(label == "Nested tables")
            {
                Mosaic::text(ui, "The next table is submitted as a regular item inside the current layout and owns "
                                 "independent columns.");
            }

            if(label == "Columns flags")
            {
                Mosaic::separatorText(ui, "Output flags");
                for(uint32_t column = 0; column != 3; ++column)
                {
                    Mosaic::TableColumnStatus status;
                    if(Mosaic::tableColumnStatus(ui, tableId, column, &status) == false)
                    {
                        continue;
                    }

                    Mosaic::String output = "Column ";
                    output += Detail::demoNumber(column);
                    output += ": enabled=";
                    output += status.enabled ? "1" : "0";
                    output += ", visible=";
                    output += status.visible ? "1" : "0";
                    output += ", sorted=";
                    output += status.sorted ? "1" : "0";
                    output += ", hovered=";
                    output += status.hovered ? "1" : "0";
                    Mosaic::text(ui, output);
                }
            }

            if(label == "Custom headers" || label == "Angled headers")
            {
                Mosaic::text(ui, label == "Angled headers" ? "Custom angled header labels: Track, Pin, Lock" : "Custom header row with application-defined controls");

                if(label == "Angled headers")
                {
                    auto settings = Mosaic::treeNode(ui, Mosaic::Key("angled style settings"), "Style settings");

                    if(settings.expanded() == true)
                    {
                        Mosaic::slider(ui, "Angled headers angle", &angledHeaderAngle, -50.f, 50.f);
                        Mosaic::slider(ui, "Angled headers text alignment", &angledHeaderTextAlignment, 0.f, 1.f);
                    }
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
} // namespace MosaicExample

namespace MosaicExample
{
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::setRenderMetrics(const Mosaic::RenderMesh & mesh) noexcept
    {
        m_renderMesh = &mesh;
        m_renderMetrics = mesh.metrics;
        m_renderVertexCount = mesh.vertices.size();
        m_renderIndexCount = mesh.indices.size();
        m_renderBatchCount = mesh.batches.size();
    }
    //////////////////////////////////////////////////////////////////////////
    const Mosaic::Color & HelloDemo::clearColor() const noexcept
    {
        return m_clearColor;
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawExampleMenuFile(Mosaic::Context * ui)
    {
        Mosaic::menuItem(ui, "(demo menu)", false);
        Mosaic::menuItem(ui, "New");
        Mosaic::MenuItemOptions open;
        open.shortcut = "Command-O";
        Mosaic::menuItem(ui, "Open", open);
        {
            auto recent = Mosaic::menu(ui, "Open Recent");

            if(recent.expanded() == true)
            {
                Mosaic::menuItem(ui, "fish_hat.c");
                Mosaic::menuItem(ui, "fish_hat.inl");
                Mosaic::menuItem(ui, "fish_hat.h");
                auto more = Mosaic::menu(ui, "More..");

                if(more.expanded() == true)
                {
                    Mosaic::menuItem(ui, "Hello");
                    Mosaic::menuItem(ui, "Sailor");
                    auto recurse = Mosaic::menu(ui, "Recurse..");

                    if(recurse.expanded() == true)
                    {
                        HelloDemo::drawExampleMenuFile(ui);
                    }
                }
            }
        }
        Mosaic::MenuItemOptions save;
        save.shortcut = "Command-S";
        Mosaic::menuItem(ui, "Save", save);
        Mosaic::menuItem(ui, "Save As..");
        Mosaic::separator(ui);
        {
            auto options = Mosaic::menu(ui, Mosaic::Key("Options"), "Options");

            if(options.expanded() == true)
            {
                Mosaic::MenuItemOptions enabled;
                enabled.checked = &m_enabled;
                enabled.closeOnActivate = false;
                Mosaic::menuItem(ui, "Enabled", enabled);
                Mosaic::LayoutOptions scrollingLayout;
                scrollingLayout.width = Mosaic::Dimension::fixed(220.f);
                scrollingLayout.height = Mosaic::Dimension::fixed(100.f);
                {
                    auto scrolling = Mosaic::scrollArea(ui, "menu scrolling text", Mosaic::ScrollOptions{}, scrollingLayout);
                    for(size_t index = 0; index != 10; ++index)
                    {
                        Mosaic::String line = "Scrolling Text ";
                        line += Detail::demoNumber(index);
                        Mosaic::text(ui, line);
                    }
                }
                Mosaic::slider(ui, "Value", &m_drag, 0.f, 1.f);
                Mosaic::inputFloat(ui, "Input", &m_drag);
                constexpr Mosaic::Array<Mosaic::StringView, 3> values = {"Yes", "No", "Maybe"};
                Mosaic::comboBox(ui, "Combo", &m_menuCombo, values);
            }
        }
        {
            auto colors = Mosaic::menu(ui, "Colors");

            if(colors.expanded() == true)
            {
                constexpr Mosaic::Array<Mosaic::StringView, 81> names = {"Background",
                                                                         "Panel",
                                                                         "PanelHeader",
                                                                         "TitleBackground",
                                                                         "TitleBackgroundActive",
                                                                         "TitleBackgroundCollapsed",
                                                                         "Input",
                                                                         "PanelHovered",
                                                                         "PanelActive",
                                                                         "Frame",
                                                                         "FrameHovered",
                                                                         "FrameActive",
                                                                         "Button",
                                                                         "ButtonHovered",
                                                                         "ButtonActive",
                                                                         "Header",
                                                                         "HeaderHovered",
                                                                         "HeaderActive",
                                                                         "Popup",
                                                                         "PopupBorder",
                                                                         "Menu",
                                                                         "MenuBarBackground",
                                                                         "MenuHovered",
                                                                         "MenuActive",
                                                                         "Tab",
                                                                         "TabHovered",
                                                                         "TabActive",
                                                                         "TabSelected",
                                                                         "TabSelectedOverline",
                                                                         "TabDimmed",
                                                                         "TabDimmedSelected",
                                                                         "TabDimmedSelectedOverline",
                                                                         "Scrollbar",
                                                                         "ScrollbarHovered",
                                                                         "ScrollbarActive",
                                                                         "ScrollbarGrab",
                                                                         "ScrollbarGrabHovered",
                                                                         "ScrollbarGrabActive",
                                                                         "SliderGrab",
                                                                         "SliderGrabHovered",
                                                                         "SliderGrabActive",
                                                                         "Separator",
                                                                         "SeparatorHovered",
                                                                         "SeparatorActive",
                                                                         "ResizeGrip",
                                                                         "ResizeGripHovered",
                                                                         "ResizeGripActive",
                                                                         "Text",
                                                                         "TextDisabled",
                                                                         "TextSelectionBackground",
                                                                         "TextCursor",
                                                                         "Accent",
                                                                         "Border",
                                                                         "BorderStrong",
                                                                         "BorderShadow",
                                                                         "Selection",
                                                                         "Warning",
                                                                         "Error",
                                                                         "Success",
                                                                         "TextLink",
                                                                         "CheckMark",
                                                                         "CheckboxSelectedBackground",
                                                                         "TreeLines",
                                                                         "TableHeader",
                                                                         "TableBorderStrong",
                                                                         "TableBorderLight",
                                                                         "TableRow",
                                                                         "TableRowAlternate",
                                                                         "DragDropTarget",
                                                                         "DragDropTargetBackground",
                                                                         "DockingPreview",
                                                                         "DockingEmptyBackground",
                                                                         "NavigationCursor",
                                                                         "NavigationWindowingHighlight",
                                                                         "NavigationWindowingDimBackground",
                                                                         "UnsavedMarker",
                                                                         "ModalDimBackground",
                                                                         "PlotLines",
                                                                         "PlotLinesHovered",
                                                                         "PlotHistogram",
                                                                         "PlotHistogramHovered"};
                const Mosaic::StyleColors & style = Mosaic::getTheme(ui).colors;
                Mosaic::Array<const Mosaic::Color *, 81> values = {&style.background,
                                                                         &style.panel,
                                                                         &style.panelHeader,
                                                                         &style.titleBackground,
                                                                         &style.titleBackgroundActive,
                                                                         &style.titleBackgroundCollapsed,
                                                                         &style.input,
                                                                         &style.panelHovered,
                                                                         &style.panelActive,
                                                                         &style.frame,
                                                                         &style.frameHovered,
                                                                         &style.frameActive,
                                                                         &style.button,
                                                                         &style.buttonHovered,
                                                                         &style.buttonActive,
                                                                         &style.header,
                                                                         &style.headerHovered,
                                                                         &style.headerActive,
                                                                         &style.popup,
                                                                         &style.popupBorder,
                                                                         &style.menu,
                                                                         &style.menuBarBackground,
                                                                         &style.menuHovered,
                                                                         &style.menuActive,
                                                                         &style.tab,
                                                                         &style.tabHovered,
                                                                         &style.tabActive,
                                                                         &style.tabSelected,
                                                                         &style.tabSelectedOverline,
                                                                         &style.tabDimmed,
                                                                         &style.tabDimmedSelected,
                                                                         &style.tabDimmedSelectedOverline,
                                                                         &style.scrollbar,
                                                                         &style.scrollbarHovered,
                                                                         &style.scrollbarActive,
                                                                         &style.scrollbarGrab,
                                                                         &style.scrollbarGrabHovered,
                                                                         &style.scrollbarGrabActive,
                                                                         &style.sliderGrab,
                                                                         &style.sliderGrabHovered,
                                                                         &style.sliderGrabActive,
                                                                         &style.separator,
                                                                         &style.separatorHovered,
                                                                         &style.separatorActive,
                                                                         &style.resizeGrip,
                                                                         &style.resizeGripHovered,
                                                                         &style.resizeGripActive,
                                                                         &style.text,
                                                                         &style.textDisabled,
                                                                         &style.textSelectionBackground,
                                                                         &style.textCursor,
                                                                         &style.accent,
                                                                         &style.border,
                                                                         &style.borderStrong,
                                                                         &style.borderShadow,
                                                                         &style.selection,
                                                                         &style.warning,
                                                                         &style.error,
                                                                         &style.success,
                                                                         &style.textLink,
                                                                         &style.checkMark,
                                                                         &style.checkboxSelectedBackground,
                                                                         &style.treeLines,
                                                                         &style.tableHeader,
                                                                         &style.tableBorderStrong,
                                                                         &style.tableBorderLight,
                                                                         &style.tableRow,
                                                                         &style.tableRowAlternate,
                                                                         &style.dragDropTarget,
                                                                         &style.dragDropTargetBackground,
                                                                         &style.dockingPreview,
                                                                         &style.dockingEmptyBackground,
                                                                         &style.navigationCursor,
                                                                         &style.navigationWindowingHighlight,
                                                                         &style.navigationWindowingDimBackground,
                                                                         &style.unsavedMarker,
                                                                         &style.modalDimBackground,
                                                                         &style.plotLines,
                                                                         &style.plotLinesHovered,
                                                                         &style.plotHistogram,
                                                                         &style.plotHistogramHovered};
                for(size_t index = 0; index != names.size(); ++index)
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key(index));
                    auto colorRow = Mosaic::row(ui);
                    Mosaic::Theme swatchTheme = Mosaic::getTheme(ui);
                    swatchTheme.colors.button = *values[index];
                    swatchTheme.colors.buttonHovered = *values[index];
                    swatchTheme.colors.buttonActive = *values[index];
                    Mosaic::ButtonOptions swatchOptions;
                    swatchOptions.width = Mosaic::Dimension::fixed(swatchTheme.metrics.controlHeight);
                    swatchOptions.height = Mosaic::Dimension::fixed(swatchTheme.metrics.controlHeight);
                    {
                        auto swatchStyle = Mosaic::styleScope(ui, swatchTheme);
                        (void)Mosaic::button(ui, Mosaic::Key("color swatch"), {}, swatchOptions);
                    }
                    Mosaic::menuItem(ui, names[index]);
                }
            }
        }
        {
            auto options = Mosaic::menu(ui, Mosaic::Key("Options"), "Options");

            if(options.expanded() == true)
            {
                Mosaic::MenuItemOptions someOption;
                someOption.checked = &m_popupOption;
                someOption.closeOnActivate = false;
                Mosaic::menuItem(ui, "SomeOption", someOption);
            }
        }
        {
            Mosaic::MenuOptions disabledOptions;
            auto disabledScope = Mosaic::disabledScope(ui, true);
            (void)Mosaic::menu(ui, "Disabled", disabledOptions);
        }
        Mosaic::MenuItemOptions checked;
        checked.selected = true;
        Mosaic::menuItem(ui, "Checked", checked);
        Mosaic::separator(ui);
        Mosaic::MenuItemOptions quit;
        quit.shortcut = "Alt-F4";
        Mosaic::menuItem(ui, "Quit", quit);
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawMenuBar(Mosaic::Context * ui)
    {
        auto bar = Mosaic::menuBar(ui);
        {
            Mosaic::MenuOptions options;
            options.width = 48.f;
            auto menu = Mosaic::menu(ui, "Menu", options);

            if(menu.expanded() == true)
            {
                HelloDemo::drawExampleMenuFile(ui);
            }
        }
        {
            Mosaic::MenuOptions options;
            options.width = 68.f;
            auto examples = Mosaic::menu(ui, "Examples", options);

            if(examples.expanded() == true)
            {
                Mosaic::MenuItemOptions mainMenuBar;
                mainMenuBar.checked = &m_showMainMenuBar;
                mainMenuBar.closeOnActivate = false;
                Mosaic::menuItem(ui, "Main menu bar", mainMenuBar);
                Mosaic::separator(ui);
                Mosaic::text(ui, "Mini apps");
                Mosaic::MenuItemOptions assets;
                assets.checked = &m_showAssetsBrowser;
                assets.closeOnActivate = false;
                Mosaic::menuItem(ui, "Assets Browser", assets);
                Mosaic::MenuItemOptions console;
                console.checked = &m_showConsole;
                console.closeOnActivate = false;
                Mosaic::menuItem(ui, "Console", console);
                Mosaic::MenuItemOptions custom;
                custom.checked = &m_showCustomRendering;
                custom.closeOnActivate = false;
                Mosaic::menuItem(ui, "Custom rendering", custom);
                Mosaic::MenuItemOptions documents;
                documents.checked = &m_showDocuments;
                documents.closeOnActivate = false;
                Mosaic::menuItem(ui, "Documents", documents);
                Mosaic::MenuItemOptions dockspace;
                dockspace.checked = &m_showDockspace;
                dockspace.closeOnActivate = false;
                Mosaic::menuItem(ui, "Dockspace", dockspace);
                Mosaic::MenuItemOptions imageViewer;
                imageViewer.checked = &m_showImageViewer;
                imageViewer.closeOnActivate = false;
                Mosaic::menuItem(ui, "Image Viewer", imageViewer);
                Mosaic::MenuItemOptions log;
                log.checked = &m_showLog;
                log.closeOnActivate = false;
                Mosaic::menuItem(ui, "Log", log);
                Mosaic::MenuItemOptions property;
                property.checked = &m_showPropertyEditor;
                property.closeOnActivate = false;
                Mosaic::menuItem(ui, "Property editor", property);
                Mosaic::MenuItemOptions layout;
                layout.checked = &m_showSimpleLayout;
                layout.closeOnActivate = false;
                Mosaic::menuItem(ui, "Simple layout", layout);
                Mosaic::MenuItemOptions overlay;
                overlay.checked = &m_showSimpleOverlay;
                overlay.closeOnActivate = false;

                if(Mosaic::menuItem(ui, "Simple overlay", overlay).changed() == true && m_showSimpleOverlay == true)
                {
                    m_overlayOpen = true;
                }

                Mosaic::separator(ui);
                Mosaic::text(ui, "Concepts");
                Mosaic::MenuItemOptions autoResize;
                autoResize.checked = &m_showAutoResize;
                autoResize.closeOnActivate = false;
                Mosaic::menuItem(ui, "Auto-resizing window", autoResize);
                Mosaic::MenuItemOptions constrained;
                constrained.checked = &m_showConstrainedResize;
                constrained.closeOnActivate = false;
                Mosaic::menuItem(ui, "Constrained-resizing window", constrained);
                Mosaic::MenuItemOptions fullscreen;
                fullscreen.checked = &m_showFullscreen;
                fullscreen.closeOnActivate = false;
                Mosaic::menuItem(ui, "Fullscreen window", fullscreen);
                Mosaic::MenuItemOptions longText;
                longText.checked = &m_showLongText;
                longText.closeOnActivate = false;
                Mosaic::menuItem(ui, "Long text display", longText);
                Mosaic::MenuItemOptions titles;
                titles.checked = &m_showWindowTitles;
                titles.closeOnActivate = false;
                Mosaic::menuItem(ui, "Manipulating window titles", titles);
            }
        }
        {
            Mosaic::MenuOptions options;
            options.width = 46.f;
            auto tools = Mosaic::menu(ui, "Tools", options);

            if(tools.expanded() == true)
            {
                Mosaic::MenuItemOptions metrics;
                metrics.checked = &m_showMetrics;
                metrics.closeOnActivate = false;
                Mosaic::menuItem(ui, "Metrics/Debugger", metrics);
                auto debug = Mosaic::menu(ui, "Debug Options");

                if(debug.expanded() == true)
                {
                    auto unavailable = Mosaic::disabledScope(ui, true);
                    Mosaic::checkbox(ui, "Highlight ID Conflicts", &m_configurationSecondary[9]);
                    Mosaic::checkbox(ui, "Assert on error recovery", &m_configurationFlags[29]);
                    Mosaic::text(ui, "(see Demo->Configuration for more)");
                }

                Mosaic::MenuItemOptions debugLog;
                debugLog.checked = &m_showDebugLog;
                debugLog.closeOnActivate = false;
                Mosaic::menuItem(ui, "Debug Log", debugLog);
                Mosaic::MenuItemOptions idStack;
                idStack.checked = &m_showIdStack;
                idStack.closeOnActivate = false;
                Mosaic::menuItem(ui, "ID Stack Tool", idStack);
                Mosaic::MenuItemOptions picker;
                picker.checked = &m_showItemPicker;
                picker.closeOnActivate = false;
                Mosaic::menuItem(ui, "Item Picker", picker);
                Mosaic::MenuItemOptions styleEditor;
                styleEditor.checked = &m_showStyleEditor;
                styleEditor.closeOnActivate = false;
                Mosaic::menuItem(ui, "Style Editor", styleEditor);
                Mosaic::MenuItemOptions about;
                about.checked = &m_showAbout;
                about.closeOnActivate = false;
                Mosaic::menuItem(ui, "About Mosaic", about);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawHelp(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Help");

        if(section.expanded() == false)
        {
            return;
        }

        Mosaic::separatorText(ui, "ABOUT THIS DEMO:");
        Mosaic::bulletText(ui, "Sections below are demonstrating many aspects of the library.");
        Mosaic::bulletText(ui, "The \"Examples\" menu above leads to more demo contents.");
        Mosaic::bulletText(ui, "The \"Tools\" menu above gives access to: About Box, Style Editor,\n"
                               "and Metrics/Debugger (general purpose Mosaic debugging tool).");
        Detail::linkedBullet(ui, "Source repository: ", "https://github.com/irov/Mosaic");

        Mosaic::separatorText(ui, "PROGRAMMER GUIDE:");
        Mosaic::bulletText(ui, "See the drawDemoWindow() code in HelloDemo.cpp. <- you are here!");
        Mosaic::bulletText(ui, "See the library implementation in the src/ folder.");
        Mosaic::bulletText(ui, "See example applications in the examples/ folder.");
        Detail::linkedBullet(ui, "Project documentation: ", "https://github.com/irov/Mosaic");
        Mosaic::bulletText(ui, "Set 'io.ConfigFlags |= NavEnableKeyboard' for keyboard controls.");
        Mosaic::bulletText(ui, "The macOS backend submits pointer, keyboard, text and IME input.");

        Mosaic::separatorText(ui, "USER GUIDE:");
        Mosaic::bulletText(ui, "Double-click on title bar to collapse window.");
        Mosaic::bulletText(ui, "Click and drag on a lower corner or border to resize window.\n"
                               "(double-click to auto fit window to its contents)");
        Mosaic::bulletText(ui, "Ctrl+Click on a slider or drag box to input value as text.");
        Mosaic::bulletText(ui, "Tab/Shift+Tab to cycle through keyboard editable fields.");
        Mosaic::bulletText(ui, "Ctrl+Tab/Ctrl+Shift+Tab to focus windows.");
        Mosaic::bulletText(ui, "While inputting text:");
        Mosaic::text(ui, "    • Ctrl+Left/Right to word jump.");
        Mosaic::text(ui, "    • Ctrl+A or double-click to select all.");
        Mosaic::text(ui, "    • Ctrl+X/C/V to use clipboard cut/copy/paste.");
        Mosaic::text(ui, "    • Ctrl+Z to undo, Ctrl+Y/Ctrl+Shift+Z to redo.");
        Mosaic::text(ui, "    • Escape to revert.");
        Mosaic::bulletText(ui, "With Keyboard controls enabled:");
        Mosaic::text(ui, "    • Arrow keys or Home/End/PageUp/PageDown to navigate.");
        Mosaic::text(ui, "    • Space to activate a widget.");
        Mosaic::text(ui, "    • Return to input text into a widget.");
        Mosaic::text(ui, "    • Escape to deactivate a widget, close popup or clear focus.");
        Mosaic::text(ui, "    • Alt jumps to the menu layer when the backend exposes it.");
        Mosaic::text(ui, "    • Menu or Shift+F10 opens a context menu where supported.");
        Mosaic::bulletText(ui, "With Gamepad controls enabled:");

        if(m_navGamepad == false)
        {
            Mosaic::text(ui, "    • Unavailable: this macOS example does not submit gamepad events.");

            return;
        }

        Mosaic::text(ui, "    • D-Pad: navigate, tweak and resize in windowing mode.");
        Mosaic::text(ui, "    • South face: activate, open or toggle; hold for text input.");
        Mosaic::text(ui, "    • East face: cancel, close or exit.");
        Mosaic::text(ui, "    • West face: toggle menu; hold for windowing mode.");
        Mosaic::text(ui, "    • North face: open context menu.");
        Mosaic::text(ui, "    • L1/R1: tweak slower/faster or focus previous/next.");
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawConfiguration(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Configuration");

        if(section.expanded() == false)
        {
            return;
        }

        {
            auto configuration = Mosaic::treeNode(ui, Mosaic::Key("configuration"), "Configuration");

            if(configuration.expanded() == true)
            {
                auto unavailable = [ui](Mosaic::StringView label, bool * value, Mosaic::StringView reason)
                {
                    Mosaic::Response response;
                    {
                        auto disabled = Mosaic::disabledScope(ui, true);
                        response = Mosaic::checkbox(ui, label, value);
                    }
                    (void)response;
                    Mosaic::helpMarker(ui, reason);
                };
                Mosaic::separatorText(ui, "General");
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "io.ConfigFlags: NavEnableKeyboard", &m_navKeyboard);
                    Mosaic::helpMarker(ui, "Enable keyboard controls.");
                }
                {
                    auto row = Mosaic::row(ui);
                    unavailable("io.ConfigFlags: NavEnableGamepad", &m_navGamepad, "The current macOS example does not submit gamepad events.");
                    Mosaic::helpMarker(ui, "Enable gamepad controls. The platform backend must provide gamepad input.");
                }
                Mosaic::checkbox(ui, "io.ConfigFlags: NoMouse", &m_configurationFlags[0]);
                Mosaic::helpMarker(ui, "Disable pointer input and interactions.");

                if(m_configurationFlags[0])
                {
                    if(std::fmod(Mosaic::input(ui).timestamp, 0.4) < 0.2)
                    {
                        Mosaic::text(ui, "<<PRESS SPACE TO DISABLE>>");
                    }

                    if(Mosaic::input(ui).keyPressed(Mosaic::KeyCode::Space) == true || m_configurationFlags[2] == true)
                    {
                        m_configurationFlags[0] = false;
                    }
                }

                Mosaic::checkbox(ui, "io.ConfigFlags: NoMouseCursorChange", &m_configurationFlags[1]);
                Mosaic::checkbox(ui, "io.ConfigFlags: NoKeyboard", &m_configurationFlags[2]);
                unavailable("io.ConfigInputTrickleEventQueue", &m_configurationFlags[3], "Event trickling belongs to a queued-input backend and is not enabled here.");
                unavailable("io.MouseDrawCursor", &m_configurationFlags[4], "This backend uses the native AppKit cursor.");

                Mosaic::separatorText(ui, "Keyboard/Gamepad Navigation");
                unavailable("io.ConfigNavSwapGamepadButtons", &m_navSwapGamepadButtons, "No gamepad input is submitted by this example backend.");
                unavailable("io.ConfigNavMoveSetMousePos", &m_configurationFlags[5], "The macOS adapter does not warp the system pointer.");
                Mosaic::checkbox(ui, "io.ConfigNavCaptureKeyboard", &m_configurationFlags[6]);
                Mosaic::checkbox(ui, "io.ConfigNavEscapeClearFocusItem", &m_configurationFlags[7]);
                Mosaic::checkbox(ui, "io.ConfigNavEscapeClearFocusWindow", &m_configurationFlags[8]);
                Mosaic::checkbox(ui, "io.ConfigNavCursorVisibleAuto", &m_configurationFlags[9]);
                Mosaic::checkbox(ui, "io.ConfigNavCursorVisibleAlways", &m_configurationFlags[10]);

                Mosaic::separatorText(ui, "Docking");
                Mosaic::checkbox(ui, "io.ConfigFlags: DockingEnable", &m_configurationFlags[11]);

                if(m_configurationFlags[11])
                {
                    auto docking = Mosaic::column(ui, Detail::indentedLayout(ui));
                    Mosaic::checkbox(ui, "io.ConfigDockingNoSplit", &m_configurationFlags[12]);
                    Mosaic::checkbox(ui, "io.ConfigDockingNoDockingOver", &m_configurationFlags[13]);
                    Mosaic::checkbox(ui, "io.ConfigDockingWithShift", &m_configurationFlags[14]);
                    unavailable("io.ConfigDockingAlwaysTabBar", &m_configurationFlags[15], "Single floating windows do not create a synthetic tab bar yet.");
                    unavailable("io.ConfigDockingTransparentPayload", &m_configurationFlags[16], "Dock payload opacity is not exposed by the renderer yet.");
                }

                Mosaic::separatorText(ui, "Multi-viewports");
                {
                    unavailable("io.ConfigFlags: ViewportsEnable", &m_configurationFlags[17], "Native multi-viewport lifecycle is not implemented yet.");
                    auto viewports = Mosaic::column(ui, Detail::indentedLayout(ui));
                    unavailable("io.ConfigViewportsNoAutoMerge", &m_configurationFlags[18], "Requires native multi-viewports.");
                    unavailable("io.ConfigViewportsNoTaskBarIcon", &m_configurationFlags[19], "Requires native multi-viewports.");
                    unavailable("io.ConfigViewportsNoDecoration", &m_configurationFlags[20], "Requires native multi-viewports.");
                    unavailable("io.ConfigViewportsNoDefaultParent", &m_configurationFlags[21], "Requires native multi-viewports.");
                    unavailable("io.ConfigViewportsPlatformFocusSetsMosaicFocus", &m_configurationFlags[22], "Requires native multi-viewports.");
                }

                Mosaic::separatorText(ui, "Windows");
                Mosaic::checkbox(ui, "io.ConfigWindowsResizeFromEdges", &m_configurationSecondary[4]);
                Mosaic::checkbox(ui, "io.ConfigWindowsMoveFromTitleBarOnly", &m_configurationSecondary[5]);
                Mosaic::checkbox(ui, "io.ConfigWindowsCopyContentsWithCtrlC", &m_configurationFlags[23]);
                Mosaic::helpMarker(ui, "Copy the focused window text with the platform primary modifier and C. Active "
                                       "text editors keep normal selection copy behavior.");
                Mosaic::checkbox(ui, "io.ConfigScrollbarScrollByPage", &m_configurationFlags[24]);

                Mosaic::separatorText(ui, "Widgets");
                Mosaic::checkbox(ui, "io.ConfigInputTextCursorBlink", &m_configurationSecondary[6]);
                Mosaic::checkbox(ui, "io.ConfigInputTextEnterKeepActive", &m_configurationFlags[25]);
                Mosaic::checkbox(ui, "io.ConfigDragClickToInputText", &m_configurationSecondary[7]);
                unavailable("io.ConfigMacOSXBehaviors", &m_configurationFlags[26], "The macOS backend always maps Command to the primary modifier.");
                Mosaic::text(ui, "Also see Style->Rendering for rendering options.");

                Mosaic::separatorText(ui, "Settings");
                unavailable("io.ConfigIniSettingsSaveLastUsedDate", &m_configurationFlags[27], "Persistence metadata does not store last-used timestamps.");

                Mosaic::separatorText(ui, "Error Handling");
                unavailable("io.ConfigErrorRecovery", &m_configurationFlags[28], "Recoverable scope repair is not configurable yet.");
                Mosaic::helpMarker(ui, "Recoverable error handling eases development but should not be relied on "
                                       "during normal execution.");
                unavailable("io.ConfigErrorRecoveryEnableAssert", &m_configurationFlags[29], "Error recovery is not configurable yet.");
                unavailable("io.ConfigErrorRecoveryEnableDebugLog", &m_configurationFlags[30], "Error recovery is not configurable yet.");
                unavailable("io.ConfigErrorRecoveryEnableTooltip", &m_configurationFlags[31], "Error recovery is not configurable yet.");

                Mosaic::separatorText(ui, "Debug");
                unavailable("io.ConfigDebugIsDebuggerPresent", &m_configurationSecondary[8], "Debugger break integration is not exposed by PlatformAdapter.");
                unavailable("io.ConfigDebugHighlightIdConflicts", &m_configurationSecondary[9], "ID collisions are always reported in Frame::diagnostics.");
                {
                    auto disabled = Mosaic::disabledScope(ui, true);
                    Mosaic::checkbox(ui, "io.ConfigDebugBeginReturnValueOnce", &m_configurationSecondary[10]);
                }
                unavailable("io.ConfigDebugBeginReturnValueLoop", &m_configurationSecondary[10], "Synthetic Begin return failures are not supported.");
                unavailable("io.ConfigDebugIgnoreFocusLoss", &m_configurationSecondary[11], "Focus events are owned by the platform backend.");
                unavailable("io.ConfigDebugIniSettings", &m_popupOption, "Persistence debug comments are not implemented.");
            }
        }

        {
            auto backend = Mosaic::treeNode(ui, Mosaic::Key("backend flags"), "Backend Flags");

            if(backend.expanded() == true)
            {
                Mosaic::helpMarker(ui, "Backend capability flags are read-only to avoid breaking platform interaction.");
                auto disabled = Mosaic::disabledScope(ui, true);
                Mosaic::checkbox(ui, "io.BackendFlags: HasGamepad", &m_navGamepad);
                Mosaic::checkbox(ui, "io.BackendFlags: HasMouseCursors", &m_backendFlags[0]);
                Mosaic::checkbox(ui, "io.BackendFlags: HasSetMousePos", &m_backendFlags[1]);
                Mosaic::checkbox(ui, "io.BackendFlags: PlatformHasViewports", &m_backendFlags[2]);
                Mosaic::checkbox(ui, "io.BackendFlags: HasMouseHoveredViewport", &m_backendFlags[3]);
                Mosaic::checkbox(ui, "io.BackendFlags: HasParentViewport", &m_backendFlags[4]);
                Mosaic::checkbox(ui, "io.BackendFlags: RendererHasVtxOffset", &m_backendFlags[5]);
                Mosaic::checkbox(ui, "io.BackendFlags: RendererHasTextures", &m_backendFlags[6]);
                Mosaic::checkbox(ui, "io.BackendFlags: RendererHasViewports", &m_backendFlags[7]);
                Mosaic::text(ui, "Platform: macOS / AppKit");
                Mosaic::text(ui, "Renderer: Metal");
            }
        }
        {
            auto style = Mosaic::treeNode(ui, Mosaic::Key("style fonts"), "Style, Fonts");

            if(style.expanded() == true)
            {
                auto row = Mosaic::row(ui);
                Mosaic::checkbox(ui, "Style Editor", &m_showStyleEditor);
                Mosaic::helpMarker(ui, "The same contents are available from Tools->Style Editor.");
            }
        }
        {
            auto capture = Mosaic::treeNode(ui, Mosaic::Key("capture logging"), "Capture/Logging");

            if(capture.expanded() == true)
            {
                Mosaic::helpMarker(ui, "Logging redirects submitted text so a window or block can be captured.");
                static int32_t logDepth = 2;
                Mosaic::dragValue(ui, "Tree depth", &logDepth, int32_t{0}, int32_t{32});
                auto row = Mosaic::row(ui);
                bool logTerminal = Mosaic::button(ui, "Log To TTY").clicked();
                bool logFile = Mosaic::button(ui, "Log To File").clicked();
                bool logClipboard = Mosaic::button(ui, "Log To Clipboard").clicked();
                Mosaic::TextLogOptions logOptions;
                logOptions.maximumDepth = static_cast<size_t>(std::max(logDepth, int32_t{0}));

                if(logTerminal == true)
                {
                    Mosaic::beginTextLog(ui, Mosaic::TextLogTarget::Terminal, logOptions);
                }
                else if(logFile == true)
                {
                    Mosaic::beginTextLog(ui, Mosaic::TextLogTarget::File, logOptions);
                }
                else if(logClipboard == true)
                {
                    Mosaic::beginTextLog(ui, Mosaic::TextLogTarget::Clipboard, logOptions);
                }

                if(Mosaic::button(ui, "Copy \"Hello, world!\" to clipboard").clicked() == true)
                {
                    Mosaic::platform(ui).setClipboardText("Hello, world!");
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawWindowOptions(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Window options");

        if(section.expanded() == false)
        {
            return;
        }

        {
            auto table = Mosaic::table(ui, "window options", 3);
            Mosaic::tableNextRow(ui);
            (void)Mosaic::tableSetColumn(ui, 0);
            Mosaic::checkbox(ui, "No titlebar", &m_noTitleBar);
            (void)Mosaic::tableSetColumn(ui, 1);
            Mosaic::checkbox(ui, "No scrollbar", &m_noScrollbar);
            (void)Mosaic::tableSetColumn(ui, 2);
            Mosaic::checkbox(ui, "No menu", &m_noMenu);
            Mosaic::tableNextRow(ui);
            (void)Mosaic::tableSetColumn(ui, 0);
            Mosaic::checkbox(ui, "No move", &m_noMove);
            (void)Mosaic::tableSetColumn(ui, 1);
            Mosaic::checkbox(ui, "No resize", &m_noResize);
            (void)Mosaic::tableSetColumn(ui, 2);
            Mosaic::checkbox(ui, "No collapse", &m_noCollapse);
            Mosaic::tableNextRow(ui);
            (void)Mosaic::tableSetColumn(ui, 0);
            Mosaic::checkbox(ui, "No close", &m_noClose);
            (void)Mosaic::tableSetColumn(ui, 1);
            Mosaic::checkbox(ui, "No nav", &m_noNavigation);
            (void)Mosaic::tableSetColumn(ui, 2);
            Mosaic::checkbox(ui, "No background", &m_noBackground);
            Mosaic::tableNextRow(ui);
            (void)Mosaic::tableSetColumn(ui, 0);
            Mosaic::checkbox(ui, "No bring to front", &m_noBringToFront);
            (void)Mosaic::tableSetColumn(ui, 1);
            Mosaic::checkbox(ui, "No docking", &m_noDocking);
            (void)Mosaic::tableSetColumn(ui, 2);
            Mosaic::checkbox(ui, "Unsaved document", &m_unsavedDocument);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawWidgets(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Widgets");

        if(section.expanded() == false)
        {
            return;
        }

        auto enabled = Mosaic::disabledScope(ui, m_enabled == false);

        Mosaic::Scope disabledSections = Mosaic::disabledScope(ui, m_disabledSection);
        {
            auto basic = Mosaic::treeNode(ui, Mosaic::Key("widgets basic"), "Basic");

            if(basic.expanded() == true)
            {
                Mosaic::separatorText(ui, "General");
                {
                    auto row = Mosaic::row(ui);

                    if(Mosaic::button(ui, Mosaic::Key("basic button"), "Button").clicked() == true)
                    {
                        ++m_basicButtonClicks;
                    }

                    if((m_basicButtonClicks & 1U) != 0)
                    {
                        Mosaic::text(ui, "Thanks for clicking me!");
                    }
                }
                Mosaic::checkbox(ui, "checkbox", &m_basicCheckbox);
                {
                    auto row = Mosaic::row(ui);

                    if(Mosaic::radioButton(ui, "radio a", m_radio == 0).clicked() == true)
                    {
                        m_radio = 0;
                    }

                    if(Mosaic::radioButton(ui, "radio b", m_radio == 1).clicked() == true)
                    {
                        m_radio = 1;
                    }

                    if(Mosaic::radioButton(ui, "radio c", m_radio == 2).clicked() == true)
                    {
                        m_radio = 2;
                    }
                }
                Mosaic::hyperlink(ui, "Hyperlink", "https://github.com/irov/Mosaic");
                {
                    auto coloredButtons = Mosaic::row(ui);
                    for(size_t index = 0; index != 7; ++index)
                    {
                        auto buttonScope = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::Theme theme = Mosaic::getTheme(ui);
                        float hue = static_cast<float>(index) / 7.f;
                        theme.colors.button = Detail::demoHsv(hue, 0.6f, 0.6f);
                        theme.colors.buttonHovered = Detail::demoHsv(hue, 0.7f, 0.7f);
                        theme.colors.buttonActive = Detail::demoHsv(hue, 0.8f, 0.8f);
                        auto style = Mosaic::styleScope(ui, theme);
                        Mosaic::button(ui, "Click");
                    }
                }
                {
                    auto repeatRow = Mosaic::row(ui);
                    Mosaic::text(ui, "Hold to repeat:");
                    Mosaic::ButtonOptions repeat;
                    repeat.repeat = true;

                    if(Mosaic::arrowButton(ui, Mosaic::Key("repeat left"), Mosaic::Direction::Left, repeat).clicked() == true)
                    {
                        --m_repeatCounter;
                    }

                    if(Mosaic::arrowButton(ui, Mosaic::Key("repeat right"), Mosaic::Direction::Right, repeat).clicked() == true)
                    {
                        ++m_repeatCounter;
                    }

                    Mosaic::text(ui, Detail::demoNumber(m_repeatCounter));
                }
                Mosaic::Response tooltipButton = Mosaic::button(ui, "Tooltip");
                Mosaic::itemTooltip(ui, tooltipButton, "I am a tooltip");
                Mosaic::labelText(ui, "label", "Value");

                Mosaic::separatorText(ui, "Inputs");
                {
                    auto form = Mosaic::propertyGrid(ui, "Basic inputs form");
                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::inputText(ui, "input text", &m_singleLine);
                    Detail::demoFormLabel(ui, "input text",
                                          "Hold Shift or use the mouse to select text. Use word navigation, clipboard, "
                                          "undo and redo shortcuts as in a native editor.");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::TextInputOptions hint;
                    hint.hint = "enter text here";
                    Mosaic::inputText(ui, "input text (w/ hint)", &m_hintInput, hint);
                    Detail::demoFormLabel(ui, "input text (w/ hint)");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::inputInt(ui, "input int", &m_inputInteger);
                    Detail::demoFormLabel(ui, "input int");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::NumericInputOptions inputFloatOptions;
                    inputFloatOptions.step = 0.01;
                    inputFloatOptions.fastStep = 1.0;
                    inputFloatOptions.precision = 3;
                    Mosaic::inputFloat(ui, "input float", &m_inputFloat, inputFloatOptions);
                    Detail::demoFormLabel(ui, "input float");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::NumericInputOptions inputDoubleOptions;
                    inputDoubleOptions.step = 0.01;
                    inputDoubleOptions.fastStep = 1.0;
                    inputDoubleOptions.precision = 8;
                    Mosaic::inputDouble(ui, "input double", &m_double, inputDoubleOptions);
                    Detail::demoFormLabel(ui, "input double");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::NumericInputOptions scientificOptions;
                    scientificOptions.precision = 6;
                    scientificOptions.scientific = true;
                    Mosaic::inputFloat(ui, "input scientific", &m_inputScientific, scientificOptions);
                    Detail::demoFormLabel(ui, "input scientific", "Scientific notation is accepted, for example 1e+8.");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::NumericInputOptions vectorOptions;
                    vectorOptions.precision = 3;
                    Mosaic::inputFloat3(ui, "input float3", m_inputFloat3, vectorOptions);
                    Detail::demoFormLabel(ui, "input float3");
                }

                Mosaic::separatorText(ui, "Drags");
                {
                    auto form = Mosaic::propertyGrid(ui, "Basic drags form");
                    Mosaic::SliderOptions dragOptions;
                    dragOptions.minimum = static_cast<double>(std::numeric_limits<int32_t>::min());
                    dragOptions.maximum = static_cast<double>(std::numeric_limits<int32_t>::max());
                    dragOptions.step = 1.0;
                    dragOptions.precision = 0;
                    dragOptions.labelPlacement = Mosaic::LabelPlacement::Hidden;

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::dragValue(ui, "drag int", &m_dragInteger, dragOptions);
                    Detail::demoFormLabel(ui, "drag int", "Click and drag to edit. Shift and Alt change the speed. Double-click to type a value.");

                    Mosaic::propertyGridNextRow(ui);
                    dragOptions.minimum = 0.0;
                    dragOptions.maximum = 100.0;
                    dragOptions.format = "%d%%";
                    Mosaic::dragValue(ui, "drag int 0..100", &m_dragPercent, dragOptions);
                    Detail::demoFormLabel(ui, "drag int 0..100");

                    Mosaic::propertyGridNextRow(ui);
                    dragOptions.minimum = 100.0;
                    dragOptions.maximum = 200.0;
                    dragOptions.format = "%d";
                    dragOptions.wrapAround = true;
                    Mosaic::dragValue(ui, "drag int wrap 100..200", &m_dragWrap, dragOptions);
                    Detail::demoFormLabel(ui, "drag int wrap 100..200");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::SliderOptions dragFloatOptions;
                    dragFloatOptions.minimum = -1000000.0;
                    dragFloatOptions.maximum = 1000000.0;
                    dragFloatOptions.step = 0.005;
                    dragFloatOptions.precision = 3;
                    dragFloatOptions.labelPlacement = Mosaic::LabelPlacement::Hidden;
                    Mosaic::dragValue(ui, "drag float", &m_drag, dragFloatOptions);
                    Detail::demoFormLabel(ui, "drag float");

                    Mosaic::propertyGridNextRow(ui);
                    dragFloatOptions.step = 0.0001;
                    dragFloatOptions.precision = 6;
                    dragFloatOptions.format = "%.6f ns";
                    Mosaic::dragValue(ui, "drag small float", &m_dragSmall, dragFloatOptions);
                    Detail::demoFormLabel(ui, "drag small float");
                }

                Mosaic::separatorText(ui, "Sliders");
                {
                    auto form = Mosaic::propertyGrid(ui, "Basic sliders form");
                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::SliderOptions sliderIntOptions;
                    sliderIntOptions.labelPlacement = Mosaic::LabelPlacement::Hidden;
                    Mosaic::slider(ui, "slider int", &m_sliderInteger, int32_t{-1}, int32_t{3}, sliderIntOptions);
                    Detail::demoFormLabel(ui, "slider int", "Double-click to type a value.");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::SliderOptions sliderOptions;
                    sliderOptions.minimum = 0.0;
                    sliderOptions.maximum = 1.0;
                    sliderOptions.format = "ratio = %.3f";
                    sliderOptions.labelPlacement = Mosaic::LabelPlacement::Hidden;
                    Mosaic::slider(ui, "slider float", &m_basicSliderFloat, sliderOptions);
                    Detail::demoFormLabel(ui, "slider float");

                    Mosaic::propertyGridNextRow(ui);
                    sliderOptions.minimum = -10.0;
                    sliderOptions.maximum = 10.0;
                    sliderOptions.precision = 4;
                    sliderOptions.format = "%.4f";
                    sliderOptions.logarithmic = true;
                    Mosaic::slider(ui, "slider float (log)", &m_basicSliderLog, sliderOptions);
                    Detail::demoFormLabel(ui, "slider float (log)");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::sliderAngle(ui, "slider angle", &m_basicSliderAngle, -360.f, 360.f, "%.0f deg", Mosaic::LabelPlacement::Hidden);
                    Detail::demoFormLabel(ui, "slider angle");

                    Mosaic::propertyGridNextRow(ui);
                    constexpr Mosaic::Array<Mosaic::StringView, 4> elementNames = {"Fire", "Earth", "Air", "Water"};
                    sliderIntOptions.format = m_sliderElement >= 0 && m_sliderElement < 4 ? elementNames[static_cast<size_t>(m_sliderElement)] : Mosaic::StringView("Unknown");
                    Mosaic::slider(ui, "slider enum", &m_sliderElement, int32_t{0}, int32_t{3}, sliderIntOptions);
                    Detail::demoFormLabel(ui, "slider enum", "The display format can show a name instead of the underlying integer.");
                }

                Mosaic::separatorText(ui, "Selectors/Pickers");
                {
                    auto form = Mosaic::propertyGrid(ui, "Basic selectors form");
                    Mosaic::ColorEditOptions colorOptions;
                    colorOptions.labelPlacement = Mosaic::LabelPlacement::Hidden;

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::colorEditorRgb(ui, "color 1", &m_color1, colorOptions);
                    Detail::demoFormLabel(ui, "color 1", "Click the color field to open the RGB picker. Right-click for value actions.");

                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::colorEditorRgba(ui, "color 2", &m_color2, colorOptions);
                    Detail::demoFormLabel(ui, "color 2");

                    constexpr Mosaic::Array<Mosaic::StringView, 11> comboItems = {"AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH", "IIIIIII", "JJJJ", "KKKKKKK"};
                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::comboBox(ui, "combo", &m_combo, comboItems);
                    Detail::demoFormLabel(ui, "combo", "The simplified combo opens a scrolling popup list.");

                    constexpr Mosaic::Array<Mosaic::StringView, 9> listItems = {"Apple", "Banana", "Cherry", "Kiwi", "Mango", "Orange", "Pineapple", "Strawberry", "Watermelon"};
                    Mosaic::propertyGridNextRow(ui);
                    Mosaic::LayoutOptions listLayout;
                    listLayout.width = Mosaic::SizeRule::Fill;
                    listLayout.height = Mosaic::Dimension::fixed(92.f);
                    Mosaic::listBox(ui, "listbox", &m_list, listItems, listLayout);
                    Detail::demoFormLabel(ui, "listbox", "The list box displays four visible items and scrolls for the rest.");
                }
            }
        }
        {
            auto bullets = Mosaic::treeNode(ui, Mosaic::Key("bullets"), "Bullets");

            if(bullets.expanded() == true)
            {
                Mosaic::bulletText(ui, "Bullet point 1");
                Mosaic::bulletText(ui, "Bullet point 2\nOn multiple lines");
                {
                    auto tree = Mosaic::treeNode(ui, Mosaic::Key("bullet tree"), "Tree node");

                    if(tree.expanded() == true)
                    {
                        Mosaic::bulletText(ui, "Another bullet point");
                    }
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::bullet(ui);
                    Mosaic::text(ui, "Bullet point 3 (two calls)");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::bullet(ui);
                    Mosaic::smallButton(ui, "Button");
                }
            }
        }
        {
            auto headers = Mosaic::treeNode(ui, Mosaic::Key("headers"), "Collapsing Headers");

            if(headers.expanded() == true)
            {
                Mosaic::checkbox(ui, "Show 2nd header", &m_showSecondHeader);
                auto first = Mosaic::collapsingHeader(ui, "Header");

                if(first.expanded() == true)
                {
                    Mosaic::Response firstResponse;
                    bool hasFirstResponse = Mosaic::itemResponse(ui, first.id(), &firstResponse);
                    Mosaic::String status = "IsItemHovered: ";
                    status += hasFirstResponse == true && firstResponse.hovered() == true ? "1" : "0";
                    Mosaic::text(ui, status);
                    for(size_t index = 0; index != 5; ++index)
                    {
                        Mosaic::String content = "Some content ";
                        content += Detail::demoNumber(index);
                        Mosaic::text(ui, content);
                    }
                }

                auto second = Mosaic::collapsingHeader(ui, "Header with a close button", &m_showSecondHeader);

                if(second.expanded() == true)
                {
                    Mosaic::Response secondResponse;
                    bool hasSecondResponse = Mosaic::itemResponse(ui, second.id(), &secondResponse);
                    Mosaic::String status = "IsItemHovered: ";
                    status += hasSecondResponse == true && secondResponse.hovered() == true ? "1" : "0";
                    Mosaic::text(ui, status);
                    for(size_t index = 0; index != 5; ++index)
                    {
                        Mosaic::String content = "More content ";
                        content += Detail::demoNumber(index);
                        Mosaic::text(ui, content);
                    }
                }

                Mosaic::TreeNodeOptions bulletOptions;
                bulletOptions.bullet = true;
                bulletOptions.framed = true;
                bulletOptions.framePadding = true;
                bulletOptions.spanAvailableWidth = true;
                auto bulletHeader = Mosaic::treeNode(ui, Mosaic::Key("header with bullet"), "Header with a bullet", bulletOptions);

                if(bulletHeader.expanded() == true)
                {
                    Mosaic::text(ui, "The framed header uses a bullet instead of an arrow.");
                }
            }
        }
        {
            auto combo = Mosaic::treeNode(ui, Mosaic::Key("combo widgets"), "Combo");

            if(combo.expanded() == true)
            {
                Mosaic::Response alignLeft = Mosaic::checkbox(ui, "MosaicComboFlags_PopupAlignLeft", &m_comboPopupAlignLeft);
                Mosaic::itemTooltip(ui, alignLeft, "Only makes a difference if the popup is larger than the combo");
                Mosaic::Response noArrow = Mosaic::checkbox(ui, "MosaicComboFlags_NoArrowButton", &m_comboNoArrowButton);

                if(noArrow.changed() == true && m_comboNoArrowButton == true)
                {
                    m_comboNoPreview = false;
                }

                Mosaic::Response noPreview = Mosaic::checkbox(ui, "MosaicComboFlags_NoPreview", &m_comboNoPreview);

                if(noPreview.changed() == true && m_comboNoPreview == true)
                {
                    m_comboNoArrowButton = false;
                    m_comboWidthFitPreview = false;
                }

                Mosaic::Response widthFit = Mosaic::checkbox(ui, "MosaicComboFlags_WidthFitPreview", &m_comboWidthFitPreview);

                if(widthFit.changed() == true && m_comboWidthFitPreview == true)
                {
                    m_comboNoPreview = false;
                }

                Mosaic::Response heightSmall = Mosaic::checkbox(ui, "MosaicComboFlags_HeightSmall", &m_comboHeightSmall);

                if(heightSmall.changed() == true && m_comboHeightSmall == true)
                {
                    m_comboHeightRegular = false;
                    m_comboHeightLargest = false;
                }

                Mosaic::Response heightRegular = Mosaic::checkbox(ui, "MosaicComboFlags_HeightRegular", &m_comboHeightRegular);

                if(heightRegular.changed() == true && m_comboHeightRegular == true)
                {
                    m_comboHeightSmall = false;
                    m_comboHeightLargest = false;
                }

                Mosaic::Response heightLargest = Mosaic::checkbox(ui, "MosaicComboFlags_HeightLargest", &m_comboHeightLargest);

                if(heightLargest.changed() == true && m_comboHeightLargest == true)
                {
                    m_comboHeightSmall = false;
                    m_comboHeightRegular = false;
                }

                constexpr Mosaic::Array<Mosaic::StringView, 14> items = {"AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH", "IIII", "JJJJ", "KKKK", "LLLLLLL", "MMMM", "OOOOOOO"};
                Mosaic::ComboOptions comboOptions;
                comboOptions.popupAlignLeft = m_comboPopupAlignLeft;
                comboOptions.showArrow = m_comboNoArrowButton == false;
                comboOptions.showPreview = m_comboNoPreview == false;
                comboOptions.widthFitPreview = m_comboWidthFitPreview;
                comboOptions.popupHeight = m_comboHeightSmall ? Mosaic::ComboPopupHeight::Small : (m_comboHeightLargest ? Mosaic::ComboPopupHeight::Largest : Mosaic::ComboPopupHeight::Regular);

                Mosaic::StringView preview = m_comboSelected >= 0 && static_cast<size_t>(m_comboSelected) < items.size() ? items[static_cast<size_t>(m_comboSelected)] : Mosaic::StringView{};
                {
                    auto popup = Mosaic::beginCombo(ui, "combo 1", preview, comboOptions);

                    if(popup.expanded() == true)
                    {
                        Mosaic::ScrollOptions scrollOptions;
                        Mosaic::LayoutOptions listLayout;
                        listLayout.width = Mosaic::SizeRule::Fill;
                        listLayout.height = Mosaic::SizeRule::Content;
                        auto list = Mosaic::scrollArea(ui, "combo 1 items", scrollOptions, listLayout);
                        for(size_t index = 0; index != items.size(); ++index)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key("combo 1 item"), items[index], m_comboSelected == static_cast<int>(index));

                            if(m_comboSelected == static_cast<int>(index))
                            {
                                Mosaic::setItemDefaultFocus(ui, item.id);
                            }

                            if(item.clicked() == true)
                            {
                                m_comboSelected = static_cast<int>(index);
                            }
                        }
                    }
                }
                {
                    Mosaic::Key comboKey("combo 2 filter");
                    bool wasOpen = Mosaic::isComboOpen(ui, comboKey);
                    auto popup = Mosaic::beginCombo(ui, comboKey, "combo 2 (w/ filter)", preview, comboOptions);

                    if(popup.expanded() == true)
                    {
                        Mosaic::TextInputOptions filterOptions;
                        filterOptions.hint = "Filter";
                        Mosaic::Response filter;
                        {
                            auto filterScope = Mosaic::scope(ui, Mosaic::Key("combo filter"));
                            filter = Mosaic::inputText(ui, {}, &m_comboFilter, filterOptions);
                        }

                        if(wasOpen == false)
                        {
                            m_comboFilter.clear();
                            Mosaic::focus(ui, filter.id);
                        }

                        if(Mosaic::input(ui).modifiers.primary == true && Mosaic::input(ui).keyPressed(Mosaic::KeyCode::F) == true)
                        {
                            Mosaic::focus(ui, filter.id);
                        }

                        Mosaic::ScrollOptions scrollOptions;
                        Mosaic::LayoutOptions listLayout;
                        listLayout.width = Mosaic::SizeRule::Fill;
                        listLayout.height = Mosaic::SizeRule::Content;
                        auto list = Mosaic::scrollArea(ui, "combo 2 items", scrollOptions, listLayout);
                        for(size_t index = 0; index != items.size(); ++index)
                        {
                            if(Detail::passesTextFilter(items[index], m_comboFilter) == false)
                            {
                                continue;
                            }

                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key("combo 2 item"), items[index], m_comboSelected == static_cast<int>(index));

                            if(item.clicked() == true)
                            {
                                m_comboSelected = static_cast<int>(index);
                            }
                        }
                    }
                }

                Mosaic::spacer(ui, 2.f);
                Mosaic::separatorText(ui, "One-liner variants");
                Mosaic::helpMarker(ui, "The Combo() function is not greatly useful apart from cases where you want to "
                                       "embed all options in a single string.\nFlags above don't apply to this section.");
                constexpr Mosaic::Array<Mosaic::StringView, 5> oneLinerItems = {"aaaa", "bbbb", "cccc", "dddd", "eeee"};
                Mosaic::ComboOptions oneLinerOptions;
                Mosaic::comboBox(ui, "combo 3 (one-liner)", &m_comboOneLiner, oneLinerItems, oneLinerOptions);
                Mosaic::comboBox(ui, "combo 4 (array)", &m_comboArray, items, oneLinerOptions);
                Mosaic::comboBox(ui, "combo 5 (function)", &m_comboFunction, size_t{8}, Detail::comboFunctionItem, nullptr, oneLinerOptions);
            }
        }
        {
            auto colors = Mosaic::treeNode(ui, Mosaic::Key("color picker"), "Color/Picker Widgets");

            if(colors.expanded() == true)
            {
                Mosaic::separatorText(ui, "Options");
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoAlpha", &m_colorNoAlpha);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_AlphaOpaque", &m_colorAlphaOpaque);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_AlphaNoBg", &m_colorAlphaNoBackground);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_AlphaPreviewHalf", &m_colorAlphaPreviewHalf);
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicColorEditFlags_NoOptions", &m_colorNoOptions);
                    Mosaic::helpMarker(ui, "Right-click on the individual color widget to show options.");
                }
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoDragDrop", &m_colorNoDragDrop);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoColorMarkers", &m_colorNoMarkers);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoTooltip", &m_colorNoTooltip);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoPicker", &m_colorNoPicker);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoSmallPreview", &m_colorNoSmallPreview);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoLabel", &m_colorNoLabel);
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicColorEditFlags_HDR", &m_colorHdr);
                    Mosaic::helpMarker(ui, "HDR lifts the normal 0..1 limits while editing color channels.");
                }

                Mosaic::separatorText(ui, "Inline color editor");
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::text(ui, "Color widget:");
                    Mosaic::helpMarker(ui, "Click the color square to open a picker. Double-click an individual "
                                           "component to type an exact value.");
                }
                Mosaic::ColorEditOptions rgbOptions;
                rgbOptions.showInputs = true;
                rgbOptions.alphaOpaque = m_colorAlphaOpaque;
                rgbOptions.alphaBackground = m_colorAlphaNoBackground == false;
                rgbOptions.alphaPreviewHalf = m_colorAlphaPreviewHalf;
                rgbOptions.optionsMenu = m_colorNoOptions == false;
                rgbOptions.dragDrop = m_colorNoDragDrop == false;
                rgbOptions.colorMarkers = m_colorNoMarkers == false;
                rgbOptions.hdr = m_colorHdr;
                rgbOptions.tooltip = m_colorNoTooltip == false;
                rgbOptions.picker = m_colorNoPicker == false;
                rgbOptions.smallPreview = m_colorNoSmallPreview == false;
                rgbOptions.label = m_colorNoLabel == false;
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key("rgb color widget"));
                    Mosaic::colorEditorRgb(ui, "MyColor", &m_color2, rgbOptions);
                }

                Mosaic::text(ui, "Color widget HSV with Alpha:");
                Mosaic::ColorEditOptions rgbaOptions;
                rgbaOptions.showInputs = true;
                rgbaOptions.inputMode = Mosaic::ColorInputMode::HsvFloat;
                rgbaOptions.alphaOpaque = m_colorAlphaOpaque;
                rgbaOptions.alphaBackground = m_colorAlphaNoBackground == false;
                rgbaOptions.alphaPreviewHalf = m_colorAlphaPreviewHalf;
                rgbaOptions.optionsMenu = m_colorNoOptions == false;
                rgbaOptions.dragDrop = m_colorNoDragDrop == false;
                rgbaOptions.colorMarkers = m_colorNoMarkers == false;
                rgbaOptions.hdr = m_colorHdr;
                rgbaOptions.tooltip = m_colorNoTooltip == false;
                rgbaOptions.picker = m_colorNoPicker == false;
                rgbaOptions.smallPreview = m_colorNoSmallPreview == false;
                rgbaOptions.label = m_colorNoLabel == false;
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key("hsv alpha color widget"));

                    if(m_colorNoAlpha == true)
                    {
                        Mosaic::colorEditorRgb(ui, "MyColor", &m_color2, rgbaOptions);
                    }
                    else
                    {
                        Mosaic::colorEditorRgba(ui, "MyColor", &m_color2, rgbaOptions);
                    }
                }

                Mosaic::text(ui, "Color widget with Float Display:");
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key("float color widget"));
                    Mosaic::ColorEditOptions floatOptions = rgbaOptions;
                    floatOptions.inputMode = Mosaic::ColorInputMode::RgbFloat;

                    if(m_colorNoAlpha == true)
                    {
                        Mosaic::colorEditorRgb(ui, "MyColor", &m_color2, floatOptions);
                    }
                    else
                    {
                        Mosaic::colorEditorRgba(ui, "MyColor", &m_color2, floatOptions);
                    }
                }

                {
                    auto row = Mosaic::row(ui);
                    Mosaic::text(ui, "Color button with Picker:");
                    Mosaic::helpMarker(ui, "A color editor may hide its numeric inputs and label while retaining the picker and tooltip.");
                }
                Mosaic::ColorEditOptions buttonOnly;
                buttonOnly.labelPlacement = Mosaic::LabelPlacement::Hidden;
                buttonOnly.showInputs = false;
                buttonOnly.showPreview = true;
                buttonOnly.alphaOpaque = m_colorAlphaOpaque;
                buttonOnly.alphaBackground = m_colorAlphaNoBackground == false;
                buttonOnly.alphaPreviewHalf = m_colorAlphaPreviewHalf;
                buttonOnly.optionsMenu = m_colorNoOptions == false;
                buttonOnly.dragDrop = m_colorNoDragDrop == false;
                buttonOnly.border = m_colorNoBorder == false;
                buttonOnly.tooltip = m_colorNoTooltip == false;
                buttonOnly.picker = m_colorNoPicker == false;
                buttonOnly.smallPreview = m_colorNoSmallPreview == false;
                buttonOnly.label = false;
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key("picker color button"));
                    Mosaic::colorEditorRgba(ui, "MyColor", &m_color2, buttonOnly);
                }

                Mosaic::text(ui, "Color button with Custom Picker Popup:");

                if(m_colorPaletteInitialized == false)
                {
                    for(size_t index = 0; index != m_colorPalette.size(); ++index)
                    {
                        m_colorPalette[index] = Detail::demoHsv(static_cast<float>(index) / 31.f, 0.8f, 0.8f);
                    }
                    m_colorPaletteInitialized = true;
                }

                bool openCustomPicker = false;
                Mosaic::Response paletteButton;
                {
                    auto paletteRow = Mosaic::row(ui);
                    Mosaic::ColorEditOptions customPickerButton;
                    customPickerButton.width = Mosaic::Dimension::fixed(84.f);
                    Mosaic::Response colorButton = Mosaic::colorButton(ui, Mosaic::Key("MyColor"), &m_color2, true, customPickerButton);
                    paletteButton = Mosaic::button(ui, "Palette");

                    if(colorButton.clicked() == true)
                    {
                        m_colorBackup = m_color2;
                        openCustomPicker = true;
                    }
                }
                Mosaic::PopupOptions palettePopupOptions;
                palettePopupOptions.owner = paletteButton.id;
                (void)Mosaic::debugBounds(ui, paletteButton.id, &palettePopupOptions.anchor);
                palettePopupOptions.placement = Mosaic::PopupPlacement::Below;
                palettePopupOptions.minimumSize = {500.f, 0.f};

                if(paletteButton.clicked() == true || openCustomPicker == true)
                {
                    m_colorBackup = m_color2;
                    Mosaic::openPopup(ui, Mosaic::Key("mypicker"), palettePopupOptions);
                }

                auto palettePopup = Mosaic::popup(ui, Mosaic::Key("mypicker"), palettePopupOptions);

                if(palettePopup.visible() == true)
                {
                    Mosaic::text(ui, "MY CUSTOM COLOR PICKER WITH AN AMAZING PALETTE!");
                    Mosaic::separator(ui);
                    Mosaic::ColorPickerOptions pickerOptions;
                    pickerOptions.size = {300.f, 180.f};
                    pickerOptions.alpha = m_colorNoAlpha == false;
                    pickerOptions.showPreview = false;
                    {
                        auto pickerScope = Mosaic::scope(ui, Mosaic::Key("custom picker"));
                        Mosaic::colorPickerRgba(ui, {}, &m_color2, pickerOptions);
                    }
                    {
                        auto previewRow = Mosaic::row(ui);
                        Mosaic::text(ui, "Current");
                        {
                            auto colorScope = Mosaic::scope(ui, Mosaic::Key("current color"));
                            Mosaic::colorButton(ui, Mosaic::Key("current"), &m_color2, true, buttonOnly);
                        }
                        Mosaic::text(ui, "Previous");
                        Mosaic::Response previous;
                        {
                            auto colorScope = Mosaic::scope(ui, Mosaic::Key("previous color"));
                            previous = Mosaic::colorButton(ui, Mosaic::Key("previous"), &m_colorBackup, true, buttonOnly);
                        }

                        if(previous.clicked() == true)
                        {
                            m_color2 = m_colorBackup;
                        }
                    }
                    Mosaic::separator(ui);
                    Mosaic::text(ui, "Palette");
                    for(size_t rowIndex = 0; rowIndex != 4; ++rowIndex)
                    {
                        auto paletteRow = Mosaic::row(ui);
                        for(size_t column = 0; column != 8; ++column)
                        {
                            size_t index = rowIndex * 8 + column;
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::ColorEditOptions swatchOptions;
                            swatchOptions.width = Mosaic::Dimension::fixed(28.f);
                            swatchOptions.height = Mosaic::Dimension::fixed(28.f);
                            swatchOptions.dragDrop = m_colorNoDragDrop == false;

                            if(Mosaic::colorButton(ui, Mosaic::Key("palette color"), &m_colorPalette[index], false, swatchOptions).clicked() == true)
                            {
                                float alpha = m_color2.a;
                                m_color2 = m_colorPalette[index];
                                m_color2.a = alpha;
                            }
                        }
                    }
                }

                Mosaic::text(ui, "Color button only:");
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoBorder", &m_colorNoBorder);
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key("borderless color button"));
                    Mosaic::colorButton(ui, Mosaic::Key("MyColor"), &m_color2, true, buttonOnly);
                }

                Mosaic::separatorText(ui, "Color picker");
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoAlpha", &m_colorNoAlpha);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_AlphaBar", &m_colorPickerAlpha);
                Mosaic::checkbox(ui, "MosaicColorEditFlags_NoSidePreview", &m_colorPickerNoSidePreview);

                if(m_colorPickerNoSidePreview == true)
                {
                    auto referenceRow = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "With Ref Color", &m_colorReferenceEnabled);

                    if(m_colorReferenceEnabled == true)
                    {
                        auto colorScope = Mosaic::scope(ui, Mosaic::Key("reference color"));
                        Mosaic::colorEditorRgba(ui, {}, &m_colorReference, buttonOnly);
                    }
                }

                Mosaic::checkbox(ui, "MosaicColorEditFlags_PickerNoRotate", &m_colorPickerNoRotate);
                constexpr Mosaic::Array<Mosaic::StringView, 3> pickerModes = {"Auto/Current", "MosaicColorEditFlags_PickerHueBar", "MosaicColorEditFlags_PickerHueWheel"};
                Mosaic::comboBox(ui, "Picker Mode", &m_colorPickerMode, pickerModes);
                constexpr Mosaic::Array<Mosaic::StringView, 5> displayModes = {"Auto/Current", "MosaicColorEditFlags_NoInputs", "MosaicColorEditFlags_DisplayRGB", "MosaicColorEditFlags_DisplayHSV", "MosaicColorEditFlags_DisplayHex"};
                Mosaic::comboBox(ui, "Display Mode", &m_colorDisplayMode, displayModes);
                Mosaic::ColorPickerOptions mainPicker;
                mainPicker.reference = m_colorReferenceEnabled ? &m_colorReference : nullptr;
                mainPicker.alpha = m_colorNoAlpha == false && m_colorPickerAlpha;
                mainPicker.hueWheel = m_colorPickerMode == 2;
                mainPicker.rotateHueWheelTriangle = m_colorPickerNoRotate == false;
                mainPicker.showInputs = m_colorDisplayMode != 1;
                mainPicker.showPreview = m_colorPickerNoSidePreview == false;
                mainPicker.inputMode = m_colorDisplayMode == 3 ? Mosaic::ColorInputMode::HsvFloat : m_colorDisplayMode == 4 ? Mosaic::ColorInputMode::Hexadecimal : m_colorHdr ? Mosaic::ColorInputMode::RgbFloat : Mosaic::ColorInputMode::RgbByte;
                mainPicker.alphaOpaque = m_colorAlphaOpaque;
                mainPicker.alphaBackground = m_colorAlphaNoBackground == false;
                mainPicker.alphaPreviewHalf = m_colorAlphaPreviewHalf;
                mainPicker.optionsMenu = m_colorNoOptions == false;
                mainPicker.dragDrop = m_colorNoDragDrop == false;
                mainPicker.colorMarkers = m_colorNoMarkers == false;
                mainPicker.hdr = m_colorHdr;
                mainPicker.border = m_colorNoBorder == false;
                {
                    auto pickerScope = Mosaic::scope(ui, Mosaic::Key("main color picker"));

                    if(m_colorNoAlpha == true)
                    {
                        Mosaic::colorPickerRgb(ui, "MyColor", &m_color2, mainPicker);
                    }
                    else
                    {
                        Mosaic::colorPickerRgba(ui, "MyColor", &m_color2, mainPicker);
                    }
                }

                Mosaic::text(ui, "Set defaults in code:");
                Mosaic::helpMarker(ui, "Application defaults can be set at startup, while explicit per-widget options override them.");

                if(Mosaic::button(ui, "Overwrite default: Uint8 + HSV + Hue Bar").clicked() == true)
                {
                    m_colorPickerMode = 1;
                    m_colorDisplayMode = 3;
                    m_colorHdr = false;
                }

                if(Mosaic::button(ui, "Overwrite default: Float + HDR + Hue Wheel").clicked() == true)
                {
                    m_colorPickerMode = 2;
                    m_colorDisplayMode = 2;
                    m_colorHdr = true;
                }

                Mosaic::text(ui, "Both types:");
                {
                    auto pickerRow = Mosaic::row(ui);
                    Mosaic::ColorPickerOptions compactPicker;
                    compactPicker.size = {190.f, 145.f};
                    compactPicker.alpha = false;
                    compactPicker.showInputs = false;
                    compactPicker.showPreview = false;
                    {
                        auto pickerScope = Mosaic::scope(ui, Mosaic::Key("hue bar picker"));
                        Mosaic::colorPickerRgb(ui, {}, &m_color2, compactPicker);
                    }
                    compactPicker.hueWheel = true;
                    {
                        auto pickerScope = Mosaic::scope(ui, Mosaic::Key("hue wheel picker"));
                        Mosaic::colorPickerRgb(ui, {}, &m_color2, compactPicker);
                    }
                }

                Mosaic::spacer(ui, 2.f);
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::text(ui, "HSV encoded colors");
                    Mosaic::helpMarker(ui, "HSV input avoids RGB/HSV round trips and preserves hue when saturation or value is zero.");
                }
                Mosaic::text(ui, "Color widget with InputHSV:");
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key("hsv shown as rgb"));
                    Mosaic::ColorEditOptions hsvAsRgb = rgbaOptions;
                    hsvAsRgb.inputMode = Mosaic::ColorInputMode::RgbFloat;
                    hsvAsRgb.inputIsHsv = true;
                    Mosaic::colorEditorRgba(ui, "HSV shown as RGB", &m_colorHsv, hsvAsRgb);
                }
                {
                    auto colorScope = Mosaic::scope(ui, Mosaic::Key("hsv shown as hsv"));
                    Mosaic::ColorEditOptions hsvAsHsv = rgbaOptions;
                    hsvAsHsv.inputMode = Mosaic::ColorInputMode::HsvFloat;
                    hsvAsHsv.inputIsHsv = true;
                    Mosaic::colorEditorRgba(ui, "HSV shown as HSV", &m_colorHsv, hsvAsHsv);
                }
                Mosaic::vectorEditor(ui, "Raw HSV values", Mosaic::FloatSpan(&m_colorHsv.r, 4), 0.f, 1.f);
            }
        }
        {
            auto data = Mosaic::treeNode(ui, Mosaic::Key("data types"), "Data Types");

            if(data.expanded() == true)
            {
                Mosaic::separatorText(ui, "Drags");
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "Clamp integers to 0..50", &m_dataClamp);
                    Mosaic::helpMarker(ui, "Values change only after an interaction. Double-click a drag to type an exact value.");
                }
                Mosaic::SliderOptions integralDrag;
                integralDrag.step = 1.0;
                int8_t s8Minimum = m_dataClamp ? int8_t{0} : std::numeric_limits<int8_t>::min();
                int8_t s8Maximum = m_dataClamp ? int8_t{50} : std::numeric_limits<int8_t>::max();
                uint8_t u8Maximum = m_dataClamp ? uint8_t{50} : std::numeric_limits<uint8_t>::max();
                int16_t s16Minimum = m_dataClamp ? int16_t{0} : std::numeric_limits<int16_t>::min();
                int16_t s16Maximum = m_dataClamp ? int16_t{50} : std::numeric_limits<int16_t>::max();
                uint16_t u16Maximum = m_dataClamp ? uint16_t{50} : std::numeric_limits<uint16_t>::max();
                int32_t s32Minimum = m_dataClamp ? int32_t{0} : std::numeric_limits<int32_t>::min();
                int32_t s32Maximum = m_dataClamp ? int32_t{50} : std::numeric_limits<int32_t>::max();
                uint32_t u32Maximum = m_dataClamp ? uint32_t{50} : std::numeric_limits<uint32_t>::max();
                int64_t s64Minimum = m_dataClamp ? int64_t{0} : std::numeric_limits<int64_t>::min();
                int64_t s64Maximum = m_dataClamp ? int64_t{50} : std::numeric_limits<int64_t>::max();
                uint64_t u64Maximum = m_dataClamp ? uint64_t{50} : std::numeric_limits<uint64_t>::max();
                Mosaic::dragValue(ui, "drag s8", &m_dataS8, s8Minimum, s8Maximum, integralDrag);
                integralDrag.format = "%u ms";
                Mosaic::dragValue(ui, "drag u8", &m_dataU8, uint8_t{0}, u8Maximum, integralDrag);
                integralDrag.format = {};
                Mosaic::dragValue(ui, "drag s16", &m_dataS16, s16Minimum, s16Maximum, integralDrag);
                integralDrag.format = "%u ms";
                Mosaic::dragValue(ui, "drag u16", &m_dataU16, uint16_t{0}, u16Maximum, integralDrag);
                integralDrag.format = {};
                Mosaic::dragValue(ui, "drag s32", &m_dataS32, s32Minimum, s32Maximum, integralDrag);
                integralDrag.format = "0x%08X";
                Mosaic::dragValue(ui, "drag s32 hex", &m_dataS32, s32Minimum, s32Maximum, integralDrag);
                integralDrag.format = "%u ms";
                Mosaic::dragValue(ui, "drag u32", &m_dataU32, uint32_t{0}, u32Maximum, integralDrag);
                integralDrag.format = {};
                Mosaic::dragValue(ui, "drag s64", &m_dataS64, s64Minimum, s64Maximum, integralDrag);
                Mosaic::dragValue(ui, "drag u64", &m_dataU64, uint64_t{0}, u64Maximum, integralDrag);
                Mosaic::SliderOptions floatDrag;
                floatDrag.minimum = 0.0;
                floatDrag.maximum = 1.0;
                floatDrag.step = 0.005;
                floatDrag.precision = 6;
                floatDrag.format = "%f";
                Mosaic::dragValue(ui, "drag float", &m_dataFloat, floatDrag);
                floatDrag.logarithmic = true;
                Mosaic::dragValue(ui, "drag float log", &m_dataFloat, floatDrag);
                Mosaic::SliderOptions doubleDrag;
                doubleDrag.minimum = 0.0;
                doubleDrag.maximum = std::numeric_limits<double>::max();
                doubleDrag.step = 0.0005;
                doubleDrag.precision = 10;
                doubleDrag.format = "%.10f grams";
                Mosaic::dragValue(ui, "drag double", &m_dataDouble, doubleDrag);
                doubleDrag.maximum = 1.0;
                doubleDrag.logarithmic = true;
                doubleDrag.format = "0 < %.10f < 1";
                Mosaic::dragValue(ui, "drag double log", &m_dataDouble, doubleDrag);

                Mosaic::separatorText(ui, "Sliders");
                Mosaic::SliderOptions sliderOptions;
                Mosaic::slider(ui, "slider s8 full", &m_dataS8, std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max(), sliderOptions);
                sliderOptions.format = "%u";
                Mosaic::slider(ui, "slider u8 full", &m_dataU8, uint8_t{0}, std::numeric_limits<uint8_t>::max(), sliderOptions);
                sliderOptions.format = {};
                Mosaic::slider(ui, "slider s16 full", &m_dataS16, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max(), sliderOptions);
                sliderOptions.format = "%u";
                Mosaic::slider(ui, "slider u16 full", &m_dataU16, uint16_t{0}, std::numeric_limits<uint16_t>::max(), sliderOptions);
                sliderOptions.format = {};
                Mosaic::slider(ui, "slider s32 low", &m_dataS32, int32_t{0}, int32_t{50}, sliderOptions);
                Mosaic::slider(ui, "slider s32 high", &m_dataS32, std::numeric_limits<int32_t>::max() / 2 - 100, std::numeric_limits<int32_t>::max() / 2, sliderOptions);
                Mosaic::slider(ui, "slider s32 full", &m_dataS32, std::numeric_limits<int32_t>::min() / 2, std::numeric_limits<int32_t>::max() / 2, sliderOptions);
                sliderOptions.format = "0x%04X";
                Mosaic::slider(ui, "slider s32 hex", &m_dataS32, int32_t{0}, int32_t{50}, sliderOptions);
                sliderOptions.format = "%u";
                Mosaic::slider(ui, "slider u32 low", &m_dataU32, uint32_t{0}, uint32_t{50}, sliderOptions);
                Mosaic::slider(ui, "slider u32 high", &m_dataU32, std::numeric_limits<uint32_t>::max() / 2 - 100, std::numeric_limits<uint32_t>::max() / 2, sliderOptions);
                Mosaic::slider(ui, "slider u32 full", &m_dataU32, uint32_t{0}, std::numeric_limits<uint32_t>::max() / 2, sliderOptions);
                sliderOptions.format = {};
                Mosaic::slider(ui, "slider s64 low", &m_dataS64, int64_t{0}, int64_t{50}, sliderOptions);
                Mosaic::slider(ui, "slider s64 high", &m_dataS64, std::numeric_limits<int64_t>::max() / 2 - 100, std::numeric_limits<int64_t>::max() / 2, sliderOptions);
                Mosaic::slider(ui, "slider s64 full", &m_dataS64, std::numeric_limits<int64_t>::min() / 2, std::numeric_limits<int64_t>::max() / 2, sliderOptions);
                sliderOptions.format = "%u ms";
                Mosaic::slider(ui, "slider u64 low", &m_dataU64, uint64_t{0}, uint64_t{50}, sliderOptions);
                Mosaic::slider(ui, "slider u64 high", &m_dataU64, std::numeric_limits<uint64_t>::max() / 2 - 100, std::numeric_limits<uint64_t>::max() / 2, sliderOptions);
                Mosaic::slider(ui, "slider u64 full", &m_dataU64, uint64_t{0}, std::numeric_limits<uint64_t>::max() / 2, sliderOptions);
                Mosaic::SliderOptions floatSlider;
                floatSlider.minimum = 0.0;
                floatSlider.maximum = 1.0;
                Mosaic::slider(ui, "slider float low", &m_dataFloat, floatSlider);
                floatSlider.precision = 10;
                floatSlider.logarithmic = true;
                Mosaic::slider(ui, "slider float low log", &m_dataFloat, floatSlider);
                floatSlider.minimum = -10000000000.0;
                floatSlider.maximum = 10000000000.0;
                floatSlider.format = "%e";
                Mosaic::slider(ui, "slider float high", &m_dataFloat, floatSlider);
                Mosaic::SliderOptions doubleSlider;
                doubleSlider.minimum = 0.0;
                doubleSlider.maximum = 1.0;
                doubleSlider.precision = 10;
                doubleSlider.format = "%.10f grams";
                Mosaic::slider(ui, "slider double low", &m_dataDouble, doubleSlider);
                doubleSlider.logarithmic = true;
                doubleSlider.format = "%.10f";
                Mosaic::slider(ui, "slider double low log", &m_dataDouble, doubleSlider);
                doubleSlider.minimum = -1000000000000000.0;
                doubleSlider.maximum = 1000000000000000.0;
                doubleSlider.logarithmic = false;
                doubleSlider.format = "%e grams";
                Mosaic::slider(ui, "slider double high", &m_dataDouble, doubleSlider);

                Mosaic::separatorText(ui, "Sliders (reverse)");
                Mosaic::slider(ui, "slider s8 reverse", &m_dataS8, std::numeric_limits<int8_t>::max(), std::numeric_limits<int8_t>::min());
                Mosaic::slider(ui, "slider u8 reverse", &m_dataU8, std::numeric_limits<uint8_t>::max(), uint8_t{0});
                Mosaic::slider(ui, "slider s32 reverse", &m_dataS32, int32_t{50}, int32_t{0});
                Mosaic::slider(ui, "slider u32 reverse", &m_dataU32, uint32_t{50}, uint32_t{0});
                Mosaic::slider(ui, "slider s64 reverse", &m_dataS64, int64_t{50}, int64_t{0});
                Mosaic::slider(ui, "slider u64 reverse", &m_dataU64, uint64_t{50}, uint64_t{0});

                Mosaic::separatorText(ui, "Inputs");
                Mosaic::checkbox(ui, "Show step buttons", &m_dataInputSteps);
                Mosaic::checkbox(ui, "MosaicInputTextFlags_ReadOnly", &m_dataReadOnly);
                Mosaic::checkbox(ui, "MosaicInputTextFlags_ParseEmptyRefVal", &m_dataParseEmptyReference);
                Mosaic::checkbox(ui, "MosaicInputTextFlags_DisplayEmptyRefVal", &m_dataDisplayEmptyReference);
                Mosaic::NumericInputOptions integerInput;
                integerInput.step = m_dataInputSteps ? 1.0 : 0.0;
                integerInput.fastStep = 10.0;
                integerInput.precision = 0;
                integerInput.parseEmptyAsZero = m_dataParseEmptyReference;
                integerInput.displayZeroAsEmpty = m_dataDisplayEmptyReference;
                integerInput.readOnly = m_dataReadOnly;
                Mosaic::inputInt(ui, "input s8", &m_dataS8, integerInput);
                Mosaic::inputInt(ui, "input u8", &m_dataU8, integerInput);
                integerInput.fastStep = 100.0;
                Mosaic::inputInt(ui, "input s16", &m_dataS16, integerInput);
                Mosaic::inputInt(ui, "input u16", &m_dataU16, integerInput);
                Mosaic::inputInt(ui, "input s32", &m_dataS32, integerInput);
                Mosaic::NumericInputOptions hexadecimalInput = integerInput;
                hexadecimalInput.base = Mosaic::NumericBase::Hexadecimal;
                hexadecimalInput.minimumDigits = 4;
                Mosaic::inputInt(ui, "input s32 hex", &m_dataS32, hexadecimalInput);
                Mosaic::inputInt(ui, "input u32", &m_dataU32, integerInput);
                hexadecimalInput.minimumDigits = 8;
                Mosaic::inputInt(ui, "input u32 hex", &m_dataU32, hexadecimalInput);
                Mosaic::inputInt(ui, "input s64", &m_dataS64, integerInput);
                Mosaic::inputInt(ui, "input u64", &m_dataU64, integerInput);
                Mosaic::NumericInputOptions floatInput;
                floatInput.step = m_dataInputSteps ? 1.0 : 0.0;
                floatInput.fastStep = 100.0;
                floatInput.parseEmptyAsZero = m_dataParseEmptyReference;
                floatInput.displayZeroAsEmpty = m_dataDisplayEmptyReference;
                floatInput.readOnly = m_dataReadOnly;
                Mosaic::inputFloat(ui, "input float", &m_dataFloat, floatInput);
                Mosaic::inputDouble(ui, "input double", &m_dataDouble, floatInput);
            }
        }
        disabledSections = {};
        {
            auto disabled = Mosaic::treeNode(ui, Mosaic::Key("disable blocks"), "Disable Blocks");

            if(disabled.expanded() == true)
            {
                Mosaic::checkbox(ui, "Disable entire section above", &m_disabledSection);
                auto disabledScope = Mosaic::disabledScope(ui, true);
                Mosaic::button(ui, "Disabled button");
                Mosaic::checkbox(ui, "Disabled checkbox", &m_disabledCheckbox);
                Mosaic::slider(ui, "Disabled slider", &m_disabledSliderFloat, 0.f, 1.f);
            }
        }
        disabledSections = Mosaic::disabledScope(ui, m_disabledSection);
        {
            auto dragDrop = Mosaic::treeNode(ui, Mosaic::Key("drag drop"), "Drag and Drop");

            if(dragDrop.expanded() == true)
            {
                {
                    auto standard = Mosaic::treeNode(ui, Mosaic::Key("standard drag drop"), "Drag and drop in standard widgets");

                    if(standard.expanded() == true)
                    {
                        Mosaic::helpMarker(ui, "You can drag from the color squares.");
                        constexpr Mosaic::TypeId colorType = 0xb064840c928fcfe8ULL;
                        Mosaic::Response color1 = Mosaic::colorEditorRgb(ui, "color 1", &m_dragDropColor1);
                        Mosaic::Response color2 = Mosaic::colorEditorRgba(ui, "color 2", &m_dragDropColor2);
                        auto beginColorDrag = [ui](const Mosaic::Response & response, const Mosaic::Color & color)
                        {
                            const auto * bytes = reinterpret_cast<const std::byte *>(&color);
                            (void)Mosaic::beginDragDropSource(ui, response, colorType, Mosaic::ByteSpan(bytes, sizeof(color)));
                        };

                        if(m_colorNoDragDrop == false)
                        {
                            beginColorDrag(color1, m_dragDropColor1);
                            beginColorDrag(color2, m_dragDropColor2);
                        }

                        auto acceptColor = [ui](const Mosaic::Response & response, Mosaic::Color & color)
                        {
                            Mosaic::DragDropAcceptResult result = Mosaic::acceptDragDropPayload(ui, response, colorType);

                            if(result.delivery == false)
                            {
                                return false;
                            }

                            if(result.payload.data.size() != sizeof(Mosaic::Color))
                            {
                                return false;
                            }

                            std::memcpy(&color, result.payload.data.data(), sizeof(color));

                            return true;
                        };
                        (void)acceptColor(color1, m_dragDropColor1);
                        (void)acceptColor(color2, m_dragDropColor2);
                    }
                }
                {
                    auto copySwap = Mosaic::treeNode(ui, Mosaic::Key("copy swap drag drop"), "Drag and drop to copy/swap items");

                    if(copySwap.expanded() == true)
                    {
                        auto modes = Mosaic::row(ui);

                        if(Mosaic::radioButton(ui, "Copy", m_dragDropMode == 0).clicked() == true)
                        {
                            m_dragDropMode = 0;
                        }

                        if(Mosaic::radioButton(ui, "Move", m_dragDropMode == 1).clicked() == true)
                        {
                            m_dragDropMode = 1;
                        }

                        if(Mosaic::radioButton(ui, "Swap", m_dragDropMode == 2).clicked() == true)
                        {
                            m_dragDropMode = 2;
                        }

                        constexpr Mosaic::TypeId itemType = 0xd783d0eedfc8f660ULL;
                        Mosaic::LayoutOptions gridLayout;
                        gridLayout.width = Mosaic::Dimension::fixed(188.f);
                        gridLayout.gap = 4.f;
                        auto items = Mosaic::grid(ui, 3, gridLayout);
                        for(size_t index = 0; index != m_dragItems.size(); ++index)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::ButtonOptions itemOptions;
                            itemOptions.width = Mosaic::Dimension::fixed(60.f);
                            itemOptions.height = Mosaic::Dimension::fixed(60.f);
                            Mosaic::Response item = Mosaic::button(ui, Mosaic::Key("drag person"), m_dragItems[index], itemOptions);
                            size_t sourceIndex = index;
                            const auto * sourceBytes = reinterpret_cast<const std::byte *>(&sourceIndex);

                            if(Mosaic::beginDragDropSource(ui, item, itemType, Mosaic::ByteSpan(sourceBytes, sizeof(sourceIndex))) == true)
                            {
                                Mosaic::String preview = m_dragDropMode == 0 ? "Copy " : m_dragDropMode == 1 ? "Move " : "Swap ";
                                preview += m_dragItems[index];
                                auto tooltip = Mosaic::tooltip(ui, "Drag source preview", {200.f, 0.f});

                                if(tooltip.visible() == true)
                                {
                                    Mosaic::text(ui, preview);
                                }
                            }

                            Mosaic::DragDropAcceptResult result = Mosaic::acceptDragDropPayload(ui, item, itemType);

                            if(result.delivery == true && result.payload.data.size() == sizeof(sourceIndex))
                            {
                                size_t payloadIndex = 0;
                                std::memcpy(&payloadIndex, result.payload.data.data(), sizeof(payloadIndex));

                                if(payloadIndex >= m_dragItems.size())
                                {
                                    continue;
                                }

                                if(m_dragDropMode == 0)
                                {
                                    m_dragItems[index] = m_dragItems[payloadIndex];
                                }
                                else if(m_dragDropMode == 1 && index != payloadIndex)
                                {
                                    m_dragItems[index] = m_dragItems[payloadIndex];
                                    m_dragItems[payloadIndex].clear();
                                }
                                else if(m_dragDropMode == 2)
                                {
                                    std::swap(m_dragItems[index], m_dragItems[payloadIndex]);
                                }
                            }
                        }
                    }
                }
                {
                    auto reorder = Mosaic::treeNode(ui, Mosaic::Key("simple reorder"), "Drag to reorder items (simple)");

                    if(reorder.expanded() == true)
                    {
                        const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
                        Mosaic::helpMarker(ui, "This example intentionally does not use the drag and drop API. "
                                               "It reorders a held item after the pointer leaves its bounds.");
                        for(size_t index = 0; index != m_dragItems.size(); ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::Response response = Mosaic::selectable(ui, Mosaic::Key("reorder item"), m_dragItems[index], false);

                            if(response.pressed() == true)
                            {
                                m_dragSourceIndex = index;
                                m_reorderDragRemainder = 0.f;
                            }
                        }

                        if(pointer != nullptr && pointer->isDown() == true && m_dragSourceIndex < m_dragItems.size())
                        {
                            m_reorderDragRemainder += pointer->delta.y;
                            float threshold = std::max(1.f, Mosaic::getTheme(ui).metrics.controlHeight * 0.5f);
                            while(std::abs(m_reorderDragRemainder) >= threshold)
                            {
                                std::ptrdiff_t direction = m_reorderDragRemainder < 0.f ? -1 : 1;
                                std::ptrdiff_t next = static_cast<std::ptrdiff_t>(m_dragSourceIndex) + direction;

                                if(next < 0)
                                {
                                    m_reorderDragRemainder = 0.f;
                                    break;
                                }

                                if(next >= static_cast<std::ptrdiff_t>(m_dragItems.size()))
                                {
                                    m_reorderDragRemainder = 0.f;
                                    break;
                                }

                                std::swap(m_dragItems[m_dragSourceIndex], m_dragItems[static_cast<size_t>(next)]);
                                m_dragSourceIndex = static_cast<size_t>(next);
                                m_reorderDragRemainder -= static_cast<float>(direction) * threshold;
                            }
                        }

                        if(pointer != nullptr && pointer->isReleased() == true)
                        {
                            m_dragSourceIndex = std::numeric_limits<size_t>::max();
                            m_reorderDragRemainder = 0.f;
                        }
                    }
                }
                {
                    auto targetTooltip = Mosaic::treeNode(ui, Mosaic::Key("target tooltip"), "Tooltip at target location");

                    if(targetTooltip.expanded() == true)
                    {
                        constexpr Mosaic::TypeId colorType = 0xb064840c928fcfe8ULL;
                        auto updateRejectedTarget = [ui](const Mosaic::Response & target)
                        {
                            Mosaic::DragDropTargetOptions options;
                            options.acceptBeforeDelivery = true;
                            options.suppressSourcePreview = true;
                            Mosaic::DragDropAcceptResult result = Mosaic::acceptDragDropPayload(ui, target, colorType, options);

                            if(result.preview == true)
                            {
                                Mosaic::setItemCursor(ui, target.id, Mosaic::CursorShape::NotAllowed);
                                auto blocked = Mosaic::interactionScope(ui, false);
                                auto tooltip = Mosaic::tooltip(ui, "Rejected drag target", {180.f, 52.f});

                                if(tooltip.visible() == true)
                                {
                                    Mosaic::text(ui, "Cannot drop here!");
                                }
                            }
                        };
                        Mosaic::Response first = Mosaic::button(ui, Mosaic::Key("drop target 0"), "drop here");
                        updateRejectedTarget(first);
                        Mosaic::Response source = Mosaic::colorEditorRgba(ui, "drag me", &m_dragDropColor1);
                        const auto * bytes = reinterpret_cast<const std::byte *>(&m_dragDropColor1);

                        if(Mosaic::beginDragDropSource(ui, source, colorType, Mosaic::ByteSpan(bytes, sizeof(m_dragDropColor1))) == true && Mosaic::dragDropSourcePreviewVisible(ui) == true)
                        {
                            auto preview = Mosaic::tooltip(ui, "Color drag preview", {150.f, 0.f});

                            if(preview.visible() == true)
                            {
                                Mosaic::text(ui, "Color payload");
                                auto previewColor = Mosaic::scope(ui, Mosaic::Key("drag preview color"));
                                Mosaic::colorEditorRgba(ui, "", &m_dragDropColor1);
                            }
                        }

                        Mosaic::Response second = Mosaic::button(ui, Mosaic::Key("drop target 1"), "drop here");
                        updateRejectedTarget(second);
                    }
                }
            }
        }

        {
            auto sliders = Mosaic::treeNode(ui, Mosaic::Key("slider flags"), "Drag/Slider Flags");

            if(sliders.expanded() == true)
            {
                Mosaic::checkbox(ui, "MosaicSliderFlags_AlwaysClamp", &m_sliderAlwaysClamp);
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicSliderFlags_ClampOnInput", &m_sliderClampOnInput);
                    Mosaic::helpMarker(ui, "Clamp manually entered values to the declared minimum and maximum.");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicSliderFlags_ClampZeroRange", &m_sliderClampZeroRange);
                    Mosaic::helpMarker(ui, "Keep clamping active when both ends of a drag range are zero.");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicSliderFlags_Logarithmic", &m_sliderLogarithmic);
                    Mosaic::helpMarker(ui, "Use logarithmic editing for more precision near small values.");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicSliderFlags_NoRoundToFormat", &m_sliderNoRound);
                    Mosaic::helpMarker(ui, "Do not round the stored value to the visible format precision.");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicSliderFlags_NoInput", &m_sliderNoInput);
                    Mosaic::helpMarker(ui, "Disable temporary text input on double-click or keyboard activation.");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicSliderFlags_NoSpeedTweaks", &m_sliderNoSpeedTweaks);
                    Mosaic::helpMarker(ui, "Disable Shift and Alt modifiers that alter drag speed.");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "MosaicSliderFlags_WrapAround", &m_sliderWrapAround);
                    Mosaic::helpMarker(ui, "Wrap drag values from maximum to minimum and back.");
                }
                Mosaic::checkbox(ui, "MosaicSliderFlags_ColorMarkers", &m_sliderColorMarkers);

                Mosaic::SliderOptions dragFlags;
                dragFlags.minimum = 0.0;
                dragFlags.maximum = 1.0;
                dragFlags.step = 0.005;
                dragFlags.logarithmic = m_sliderLogarithmic;
                dragFlags.temporaryInput = m_sliderNoInput == false;
                dragFlags.wrapAround = m_sliderWrapAround;
                dragFlags.clampInput = m_sliderAlwaysClamp || m_sliderClampOnInput;
                dragFlags.clampZeroRange = m_sliderAlwaysClamp || m_sliderClampZeroRange;
                dragFlags.roundToFormat = m_sliderNoRound == false;
                dragFlags.speedTweaks = m_sliderNoSpeedTweaks == false;
                dragFlags.colorMarkers = m_sliderColorMarkers;
                Mosaic::String underlyingDrag = "Underlying float value: ";
                underlyingDrag += Detail::demoFixed(m_flagDrag, 6);
                Mosaic::text(ui, underlyingDrag);
                Mosaic::dragValue(ui, "DragFloat (0 -> 1)", &m_flagDrag, dragFlags);
                dragFlags.maximum = std::numeric_limits<float>::max();
                Mosaic::dragValue(ui, "DragFloat (0 -> +inf)", &m_flagDrag, dragFlags);
                dragFlags.minimum = -std::numeric_limits<float>::max();
                dragFlags.maximum = 1.0;
                Mosaic::dragValue(ui, "DragFloat (-inf -> 1)", &m_flagDrag, dragFlags);
                dragFlags.maximum = std::numeric_limits<float>::max();
                Mosaic::dragValue(ui, "DragFloat (-inf -> +inf)", &m_flagDrag, dragFlags);
                Mosaic::SliderOptions dragIntFlags = dragFlags;
                dragIntFlags.step = 1.0;
                Mosaic::dragValue(ui, "DragInt (0 -> 100)", &m_flagDragInt, int32_t{0}, int32_t{100}, dragIntFlags);
                Mosaic::dragFloatVector(ui, "DragFloat4 (0 -> 1)", Mosaic::FloatSpan(m_flagDrag4), dragFlags);

                Mosaic::SliderOptions sliderFlags;
                sliderFlags.minimum = 0.0;
                sliderFlags.maximum = 1.0;
                sliderFlags.logarithmic = m_sliderLogarithmic;
                sliderFlags.temporaryInput = m_sliderNoInput == false;
                sliderFlags.clampInput = m_sliderAlwaysClamp || m_sliderClampOnInput;
                sliderFlags.clampZeroRange = m_sliderAlwaysClamp || m_sliderClampZeroRange;
                sliderFlags.roundToFormat = m_sliderNoRound == false;
                sliderFlags.speedTweaks = m_sliderNoSpeedTweaks == false;
                sliderFlags.colorMarkers = m_sliderColorMarkers;
                Mosaic::String underlyingSlider = "Underlying float value: ";
                underlyingSlider += Detail::demoFixed(m_flagSlider, 6);
                Mosaic::text(ui, underlyingSlider);
                Mosaic::slider(ui, "SliderFloat (0 -> 1)", &m_flagSlider, sliderFlags);
                Mosaic::slider(ui, "SliderInt (0 -> 100)", &m_flagSliderInt, int32_t{0}, int32_t{100}, sliderFlags);
                Mosaic::sliderFloatVector(ui, "SliderFloat4 (0 -> 1)", Mosaic::FloatSpan(m_flagSlider4), sliderFlags);
            }
        }
        {
            auto fonts = Mosaic::treeNode(ui, Mosaic::Key("fonts"), "Fonts");

            if(fonts.expanded() == true)
            {
                static float customSize = 16.f;
                static float customScale = 1.f;
                Mosaic::text(ui, "Font atlas and configured fonts");
                Mosaic::bulletText(ui, "Default UI font — proportional interface text");
                Mosaic::bulletText(ui, "Monospace font — changing values and code");
                Mosaic::FontCacheMetrics cache;
                if(Mosaic::fontCacheMetrics(ui, &cache) == false)
                {
                    Mosaic::text(ui, "Font cache metrics are unavailable.");
                }
                else
                {
                    Mosaic::String cacheStatus = "Fonts: ";
                    cacheStatus += Detail::demoNumber(cache.fontCount);
                    cacheStatus += ", resolved faces: ";
                    cacheStatus += Detail::demoNumber(cache.resolvedFaceCount);
                    cacheStatus += ", glyphs: ";
                    cacheStatus += Detail::demoNumber(cache.glyphCount);
                    cacheStatus += ", atlas pages: ";
                    cacheStatus += Detail::demoNumber(cache.atlasPageCount);
                    cacheStatus += ", atlas memory: ";
                    cacheStatus += Detail::demoNumber(cache.atlasMemory / 1024);
                    cacheStatus += " KiB";
                    Mosaic::bulletText(ui, cacheStatus);
                }

                Mosaic::separator(ui);
                {
                    Mosaic::Theme proportional = Mosaic::getTheme(ui);
                    proportional.metrics.font = Mosaic::DefaultFont;
                    auto proportionalStyle = Mosaic::styleScope(ui, proportional);
                    Mosaic::text(ui, "DefaultFont: The quick brown fox jumps over the lazy dog");
                }
                {
                    Mosaic::Theme monospace = Mosaic::getTheme(ui);
                    monospace.metrics.font = Mosaic::MonospaceFont;
                    auto monospaceStyle = Mosaic::styleScope(ui, monospace);
                    Mosaic::text(ui, "MonospaceFont: 0123456789  WWWWWW  iiiiii");
                }
                Mosaic::separatorText(ui, "Sizes");
                for(float size : Mosaic::Array<float, 5>{10.f, 13.f, 16.f, 20.f, 28.f})
                {
                    Mosaic::Theme sample = Mosaic::getTheme(ui);
                    sample.metrics.fontSize = size;
                    sample.metrics.lineHeight = std::ceil(size * 1.3f);
                    auto sampleStyle = Mosaic::styleScope(ui, sample);
                    Mosaic::String label = Detail::demoFixed(size, 0);
                    label += " px: The quick brown fox jumps over the lazy dog";
                    Mosaic::text(ui, label);
                }
                Mosaic::separator(ui);
                Mosaic::slider(ui, "custom_size", &customSize, 10.f, 100.f);
                {
                    Mosaic::Theme custom = Mosaic::getTheme(ui);
                    custom.metrics.fontSize = customSize;
                    custom.metrics.lineHeight = std::ceil(customSize * 1.3f);
                    auto style = Mosaic::styleScope(ui, custom);
                    Mosaic::text(ui, "Mosaic::PushFont(nullptr, custom_size);");
                }
                Mosaic::separator(ui);
                Mosaic::slider(ui, "custom_scale", &customScale, 0.5f, 4.f);
                {
                    Mosaic::Theme custom = Mosaic::getTheme(ui);
                    custom.metrics.fontSize *= customScale;
                    custom.metrics.lineHeight *= customScale;
                    auto style = Mosaic::styleScope(ui, custom);
                    Mosaic::text(ui, "Mosaic::PushFont(nullptr, style.FontSizeBase * custom_scale);");
                }
                Mosaic::text(ui, "The application FontProvider owns shaping, glyph metrics and atlas textures.");
                HelloDemo::drawFontCacheInspector(ui);
            }
        }
        {
            auto images = Mosaic::treeNode(ui, Mosaic::Key("images"), "Images");

            if(images.expanded() == true)
            {
                Mosaic::TextOptions explanation;
                explanation.wordWrap = true;
                explanation.layout.width = Mosaic::SizeRule::Fill;
                Mosaic::text(ui,
                             "Below we display the live font texture because it is the only texture owned by this "
                             "catalog. TextureHandle can carry renderer-defined texture identifiers supplied by the "
                             "application. Hover the texture for the interactive zoomed view below.",
                             explanation);
                Mosaic::FontAtlasPage atlas;
                bool hasAtlas = Mosaic::fontAtlasPage(ui, 0, &atlas);

                if(hasAtlas == false)
                {
                    Mosaic::text(ui, "The font atlas has no texture page yet. Submit text once and reopen this section.");
                }

                if(hasAtlas == true)
                {
                    Mosaic::String atlasSize = Detail::demoNumber(static_cast<uint32_t>(atlas.size.x));
                    atlasSize += "x";
                    atlasSize += Detail::demoNumber(static_cast<uint32_t>(atlas.size.y));
                    atlasSize += atlas.mask ? " alpha atlas" : " RGBA atlas";
                    Mosaic::text(ui, atlasSize);
                    Mosaic::separatorText(ui, "Image()/ImageWithBg() function");
                    {
                        Mosaic::ImageOptions imageOptions;
                        imageOptions.backgroundEnabled = true;
                        imageOptions.background = Mosaic::Color::fromBytes(42, 46, 52);
                        imageOptions.padding = 4.f;
                        Mosaic::Response image = Mosaic::image(ui, atlas.texture, atlas.size, imageOptions);
                        Mosaic::itemTooltip(ui, image, "This is the live glyph atlas texture owned by FontProvider.");
                    }
                    Mosaic::separatorText(ui, "Interactive Image Viewer");
                    drawImageViewerContents(ui, 180.f);
                    Mosaic::separatorText(ui, "Textured Buttons");
                    Mosaic::text(ui, "And now some textured buttons..");
                    {
                        auto buttonRow = Mosaic::row(ui);
                        for(size_t index = 0; index != 8; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::ImageButtonOptions imageOptions;
                            imageOptions.uv = {0.f, 0.f, std::min(1.f, 32.f / std::max(1.f, atlas.size.x)), std::min(1.f, 32.f / std::max(1.f, atlas.size.y))};
                            imageOptions.padding = static_cast<float>(index + 1);
                            imageOptions.backgroundEnabled = true;

                            if(Mosaic::imageButton(ui, Mosaic::Key("texture button"), atlas.texture, {32.f, 32.f}, imageOptions).clicked() == true)
                            {
                                ++m_imagePressedCount;
                            }
                        }
                    }
                    Mosaic::String pressed = "Pressed ";
                    pressed += Detail::demoNumber(m_imagePressedCount);
                    pressed += " times.";
                    Mosaic::text(ui, pressed);
                }
            }
        }
        {
            auto lists = Mosaic::treeNode(ui, Mosaic::Key("list boxes"), "List Boxes");

            if(lists.expanded() == true)
            {
                constexpr Mosaic::Array<Mosaic::StringView, 14> items = {"AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH", "IIII", "JJJJ", "KKKK", "LLLLLLL", "MMMM", "OOOOOOO"};
                Mosaic::checkbox(ui, "Highlight hovered item in second listbox", &m_listHighlight);
                m_listHovered = -1;
                Mosaic::ListBoxOptions firstOptions;
                firstOptions.layout.width = Mosaic::Dimension::fixed(200.f);
                firstOptions.layout.height = Mosaic::Dimension::fixed(118.f);
                {
                    auto list = Mosaic::beginListBox(ui, "listbox 1", firstOptions);
                    float itemExtent = Mosaic::getTheme(ui).metrics.controlHeight;
                    Mosaic::VisibleRange range;
                    (void)Mosaic::beginListClipper(ui, items.size(), itemExtent, &range, list.id());
                    for(size_t index = range.begin; index != range.end; ++index)
                    {
                        auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key("item"), items[index], m_list == static_cast<int>(index));

                        if(m_list == static_cast<int>(index))
                        {
                            bool applyDefaultFocus = Mosaic::navigationFocus(ui) == Mosaic::InvalidId;
                            Mosaic::setItemDefaultFocus(ui, item.id);

                            if(applyDefaultFocus == true)
                            {
                                Mosaic::scrollToItem(ui, list.id(), item.id);
                            }
                        }

                        if(item.hovered() == true)
                        {
                            m_listHovered = static_cast<int>(index);
                        }

                        if(item.clicked() == true)
                        {
                            m_list = static_cast<int>(index);
                        }
                    }
                    Mosaic::endListClipper(ui, range, items.size(), itemExtent);
                }
                Mosaic::helpMarker(ui, "Here we are sharing selection state between both boxes.");
                Mosaic::text(ui, "Full-width:");
                Mosaic::ListBoxOptions secondOptions;
                secondOptions.layout.width = Mosaic::SizeRule::Fill;
                secondOptions.layout.height = Mosaic::Dimension::fixed(108.f);
                {
                    auto listScope = Mosaic::scope(ui, Mosaic::Key("listbox 2"));
                    auto list = Mosaic::beginListBox(ui, {}, secondOptions);
                    float itemExtent = Mosaic::getTheme(ui).metrics.controlHeight;
                    Mosaic::VisibleRange range;
                    (void)Mosaic::beginListClipper(ui, items.size(), itemExtent, &range, list.id());
                    for(size_t index = range.begin; index != range.end; ++index)
                    {
                        auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                        bool selected = m_list == static_cast<int>(index) || (m_listHighlight && m_listHovered == static_cast<int>(index));
                        Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key("item"), items[index], selected);

                        if(m_list == static_cast<int>(index))
                        {
                            bool applyDefaultFocus = Mosaic::navigationFocus(ui) == Mosaic::InvalidId;
                            Mosaic::setItemDefaultFocus(ui, item.id);

                            if(applyDefaultFocus == true)
                            {
                                Mosaic::scrollToItem(ui, list.id(), item.id);
                            }
                        }

                        if(item.clicked() == true)
                        {
                            m_list = static_cast<int>(index);
                        }
                    }
                    Mosaic::endListClipper(ui, range, items.size(), itemExtent);
                }
            }
        }
        {
            auto liveEdit = Mosaic::treeNode(ui, Mosaic::Key("live edit"), "Live Edit Flags");

            if(liveEdit.expanded() == true)
            {
                Mosaic::text(ui, "Select whether to apply keyboard edits to backing variables _while_ typing.");
                Mosaic::checkbox(ui, "Override Live Edit Flags in Demo Window", &m_liveEditOverride);
                {
                    auto optionsDisabled = Mosaic::disabledScope(ui, m_liveEditOverride == false);
                    Mosaic::checkbox(ui, "MosaicItemFlags_LiveEditOnInputText", &m_liveEditText);
                    Mosaic::checkbox(ui, "MosaicItemFlags_LiveEditOnInputScalar", &m_liveEditScalar);
                }
                Mosaic::text(ui, "Try typing '123' and seeing effect on backing value:");
                Mosaic::LiveEditOptions liveEditOptions;

                if(m_liveEditOverride == true)
                {
                    liveEditOptions.text = m_liveEditText;
                    liveEditOptions.scalar = m_liveEditScalar;
                }

                auto liveEditScope = Mosaic::liveEditScope(ui, liveEditOptions);
                Mosaic::inputText(ui, "str", &m_singleLine);
                Mosaic::String textBacking = "Backing value: \"";
                textBacking += m_singleLine;
                textBacking += "\"";
                Mosaic::text(ui, textBacking);
                Mosaic::inputInt(ui, "int", &m_integer, int32_t{0}, int32_t{0});
                Mosaic::String integerBacking = "Backing value: ";
                integerBacking += Detail::demoNumber(m_integer);
                Mosaic::text(ui, integerBacking);
                Mosaic::slider(ui, "float", &m_drag, 0.f, 100.f);
                Mosaic::String floatBacking = "Backing value: ";
                floatBacking += Detail::demoFixed(m_drag, 6);
                Mosaic::text(ui, floatBacking);
            }
        }
        {
            auto multi = Mosaic::treeNode(ui, Mosaic::Key("multi widgets"), "Multi-component Widgets");

            if(multi.expanded() == true)
            {
                Mosaic::checkbox(ui, "MosaicSliderFlags_ColorMarkers", &m_sliderColorMarkers);
                for(size_t components = 2; components <= 4; ++components)
                {
                    Mosaic::String widthLabel = Detail::demoNumber(components);
                    widthLabel += "-wide";
                    Mosaic::separatorText(ui, widthLabel);
                    Mosaic::FloatSpan floatValues(m_vector.data(), components);
                    Mosaic::Int32Span integerValues(m_integerVector.data(), components);
                    Mosaic::String label = "input float";
                    label += Detail::demoNumber(components);
                    Mosaic::inputFloatVector(ui, label, floatValues);
                    label = "input int";
                    label += Detail::demoNumber(components);
                    Mosaic::inputIntVector(ui, label, integerValues);

                    Mosaic::SliderOptions dragOptions;
                    dragOptions.minimum = 0.0;
                    dragOptions.maximum = 1.0;
                    dragOptions.step = 0.01;
                    dragOptions.precision = 3;
                    dragOptions.colorMarkers = m_sliderColorMarkers;
                    label = "drag float";
                    label += Detail::demoNumber(components);
                    Mosaic::dragFloatVector(ui, label, floatValues, dragOptions);
                    label = "drag int";
                    label += Detail::demoNumber(components);
                    dragOptions.minimum = 0.0;
                    dragOptions.maximum = 255.0;
                    dragOptions.step = 1.0;
                    dragOptions.precision = 0;
                    Mosaic::dragIntVector(ui, label, integerValues, int32_t{0}, int32_t{255}, dragOptions);

                    Mosaic::SliderOptions sliderOptions;
                    sliderOptions.minimum = 0.0;
                    sliderOptions.maximum = 1.0;
                    sliderOptions.step = 0.01;
                    sliderOptions.precision = 3;
                    sliderOptions.colorMarkers = m_sliderColorMarkers;
                    label = "slider float";
                    label += Detail::demoNumber(components);
                    Mosaic::sliderFloatVector(ui, label, floatValues, sliderOptions);
                    label = "slider int";
                    label += Detail::demoNumber(components);
                    sliderOptions.minimum = 0.0;
                    sliderOptions.maximum = 255.0;
                    sliderOptions.step = 1.0;
                    sliderOptions.precision = 0;
                    Mosaic::sliderIntVector(ui, label, integerValues, int32_t{0}, int32_t{255}, sliderOptions);
                }
                Mosaic::separatorText(ui, "Ranges");
                {
                    Mosaic::RangeOptions options;
                    options.minimum.step = 0.25;
                    options.minimum.format = "Min: %.1f %%";
                    options.minimum.clampInput = true;
                    options.maximum = options.minimum;
                    options.maximum.format = "Max: %.1f %%";
                    Mosaic::dragRange(ui, "range float", &m_rangeFloatBegin, &m_rangeFloatEnd, 0.f, 100.f, options);
                }
                {
                    Mosaic::RangeOptions options;
                    options.minimum.step = 5.0;
                    options.minimum.format = "Min: %d units";
                    options.minimum.clampInput = true;
                    options.maximum = options.minimum;
                    options.maximum.format = "Max: %d units";
                    Mosaic::dragRange(ui, "range int", &m_rangeIntBegin, &m_rangeIntEnd, int32_t{0}, int32_t{1000}, options);
                }
                {
                    Mosaic::RangeOptions options;
                    options.minimum.step = 5.0;
                    options.minimum.format = "Min: %d units";
                    options.maximum = options.minimum;
                    options.maximum.format = "Max: %d units";
                    Mosaic::dragRange(ui, "range int no bounds", &m_rangeIntBegin, &m_rangeIntEnd, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(), options);
                }
            }
        }
        {
            auto plotting = Mosaic::treeNode(ui, Mosaic::Key("plotting"), "Plotting");

            if(plotting.expanded() == true)
            {
                Mosaic::text(ui, "Need better plotting and graphing? Consider using ImPlot:");
                Mosaic::hyperlink(ui, "https://github.com/epezent/implot", "https://github.com/epezent/implot");
                Mosaic::separator(ui);
                Mosaic::checkbox(ui, "Animate", &m_plotAnimate);
                constexpr Mosaic::Array<float, 7> samples = {0.6f, 0.1f, 1.f, 0.5f, 0.92f, 0.1f, 0.2f};
                Mosaic::plotLines(ui, "Frame Times", samples);
                Mosaic::PlotOptions histogram;
                histogram.height = 80.f;
                Mosaic::plotHistogram(ui, "Histogram", samples, histogram);

                if(m_plotAnimate == true)
                {
                    constexpr float sampleInterval = 1.f / 60.f;
                    m_plotAccumulator += std::min(m_deltaTime, 0.25f);
                    while(m_plotAccumulator >= sampleInterval)
                    {
                        m_plotValues[m_plotOffset] = std::cos(m_plotPhase);
                        m_plotOffset = (m_plotOffset + 1) % m_plotValues.size();
                        m_plotPhase += 0.1f * static_cast<float>(m_plotOffset);
                        m_plotAccumulator -= sampleInterval;
                    }
                }

                float average = 0.f;
                for(float value : m_plotValues)
                {
                    average += value;
                }
                average /= static_cast<float>(m_plotValues.size());
                Mosaic::PlotOptions lineOptions;
                lineOptions.minimum = -1.f;
                lineOptions.maximum = 1.f;
                lineOptions.offset = m_plotOffset;
                Mosaic::String overlay = "avg ";
                overlay += Detail::demoFixed(average, 6);
                lineOptions.overlay = overlay;
                Mosaic::plotLines(ui, "Lines", m_plotValues, lineOptions);
                Mosaic::separatorText(ui, "Functions");
                constexpr Mosaic::Array<Mosaic::StringView, 2> functions = {"Sin", "Saw"};
                Mosaic::ComboOptions comboOptions;
                comboOptions.width = Mosaic::Dimension::fixed(104.f);
                {
                    auto controls = Mosaic::row(ui);
                    Mosaic::comboBox(ui, "func", &m_plotFunction, functions, comboOptions);
                    Mosaic::slider(ui, "Sample count", &m_plotSampleCount, int32_t{1}, int32_t{400});
                }
                auto generatedValue = [](void * userData, size_t index)
                {
                    int function = *static_cast<const int *>(userData);
                    auto returnedValue = function == 0 ? std::sin(static_cast<float>(index) * 0.1f) : ((index & 1U) != 0 ? 1.f : -1.f);

                    return returnedValue;
                };
                size_t generatedCount = static_cast<size_t>(std::max(m_plotSampleCount, int32_t{0}));
                lineOptions.overlay = {};
                lineOptions.offset = 0;
                {
                    auto plotScope = Mosaic::scope(ui, Mosaic::Key("generated line plot"));
                    Mosaic::plotLines(ui, "Lines", generatedCount, generatedValue, &m_plotFunction, lineOptions);
                }
                {
                    auto plotScope = Mosaic::scope(ui, Mosaic::Key("generated histogram"));
                    Mosaic::plotHistogram(ui, "Histogram", generatedCount, generatedValue, &m_plotFunction, lineOptions);
                }
            }
        }

        {
            auto progress = Mosaic::treeNode(ui, Mosaic::Key("progress bars"), "Progress Bars");

            if(progress.expanded() == true)
            {
                m_progressValue += m_progressDirection * 0.4f * m_deltaTime;

                if(m_progressValue >= 1.1f)
                {
                    m_progressValue = 1.1f;
                    m_progressDirection = -1.f;
                }

                if(m_progressValue <= -0.1f)
                {
                    m_progressValue = -0.1f;
                    m_progressDirection = 1.f;
                }

                float value = std::clamp(m_progressValue, 0.f, 1.f);
                {
                    auto progress = Mosaic::row(ui);
                    Mosaic::progressBar(ui, value);
                    Mosaic::text(ui, "Progress Bar");
                }
                Mosaic::String count = Detail::demoNumber(static_cast<int32_t>(value * 1753.f));
                count += "/1753";
                Mosaic::progressBar(ui, value, count);
                {
                    auto progress = Mosaic::row(ui);
                    Mosaic::progressBar(ui, -static_cast<float>(Mosaic::input(ui).timestamp), "Searching..");
                    Mosaic::text(ui, "Indeterminate");
                }
            }
        }
        {
            auto query = Mosaic::treeNode(ui, Mosaic::Key("query status"), "Querying Item Status (Edited/Active/Hovered etc.)");

            if(query.expanded() == true)
            {
                constexpr Mosaic::Array<Mosaic::StringView, 16> itemNames = {"Text", "Button", "Button (w/ repeat)", "Checkbox", "SliderFloat", "InputText", "InputTextMultiline", "InputFloat", "InputFloat3", "ColorEdit4", "Selectable", "MenuItem", "TreeNode", "TreeNode (w/ double-click)", "Combo", "ListBox"};
                Mosaic::comboBox(ui, "Item Type", &m_queryItemType, itemNames);
                Mosaic::helpMarker(ui, "Test how each item kind reports the common interaction states.");
                Mosaic::checkbox(ui, "Item Disabled", &m_queryItemDisabled);
                Mosaic::checkbox(ui, "Override LiveEdit:", &m_liveEditOverride);
                {
                    auto flagsDisabled = Mosaic::disabledScope(ui, m_liveEditOverride == false);
                    auto flags = Mosaic::row(ui);
                    bool liveEditOnInput = m_liveEditText && m_liveEditScalar;
                    if(Mosaic::checkbox(ui, "_LiveEditOnInput", &liveEditOnInput).changed() == true)
                    {
                        m_liveEditText = liveEditOnInput;
                        m_liveEditScalar = liveEditOnInput;
                    }

                    Mosaic::checkbox(ui, "_LiveEditOnInputText", &m_liveEditText);
                    Mosaic::checkbox(ui, "_LiveEditOnInputScalar", &m_liveEditScalar);
                }

                Mosaic::Response response;
                {
                    auto itemDisabled = Mosaic::disabledScope(ui, m_queryItemDisabled);
                    Mosaic::LiveEditOptions liveEditOptions;

                    if(m_liveEditOverride == true)
                    {
                        liveEditOptions.text = m_liveEditText;
                        liveEditOptions.scalar = m_liveEditScalar;
                    }

                    auto liveEditScope = Mosaic::liveEditScope(ui, liveEditOptions);
                    switch(m_queryItemType)
                    {
                    case 0:
                        response = Mosaic::text(ui, "ITEM: Text");
                        break;
                    case 1:
                        response = Mosaic::button(ui, "ITEM: Button");
                        break;
                    case 2:
                    {
                        Mosaic::ButtonOptions repeat;
                        repeat.repeat = true;
                        response = Mosaic::button(ui, Mosaic::Key("query repeat"), "ITEM: Button", repeat);
                        break;
                    }
                    case 3:
                        response = Mosaic::checkbox(ui, "ITEM: Checkbox", &m_queryCheckbox);
                        break;
                    case 4:
                        response = Mosaic::slider(ui, "ITEM: SliderFloat", &m_queryColor.r, 0.f, 1.f);
                        break;
                    case 5:
                        response = Mosaic::inputText(ui, "ITEM: InputText", &m_queryInput);
                        break;
                    case 6:
                    {
                        Mosaic::LayoutOptions inputLayout;
                        inputLayout.width = Mosaic::SizeRule::Fill;
                        inputLayout.height = Mosaic::Dimension::fixed(72.f);
                        response = Mosaic::inputMultiline(ui, "ITEM: InputTextMultiline", &m_queryInput, {}, inputLayout);
                        break;
                    }
                    case 7:
                        response = Mosaic::inputFloat(ui, "ITEM: InputFloat", &m_queryColor.r);
                        break;
                    case 8:
                        response = Mosaic::inputFloat3(ui, "ITEM: InputFloat3", m_vector);
                        break;
                    case 9:
                        response = Mosaic::colorEditorRgba(ui, "ITEM: ColorEdit4", &m_queryColor);
                        break;
                    case 10:
                        response = Mosaic::selectable(ui, "ITEM: Selectable", m_selectableStates[0]);
                        break;
                    case 11:
                        response = Mosaic::menuItem(ui, "ITEM: MenuItem");
                        break;
                    case 12:
                    case 13:
                    {
                        Mosaic::TreeNodeOptions treeOptions;
                        treeOptions.openOnDoubleClick = m_queryItemType == 13;
                        auto item = Mosaic::treeNode(ui, Mosaic::Key("query tree"), m_queryItemType == 12 ? "ITEM: TreeNode" : "ITEM: TreeNode w/ MosaicTreeNodeFlags_OpenOnDoubleClick", treeOptions);
                        (void)Mosaic::itemResponse(ui, item.id(), &response);

                        if(item.expanded() == true)
                        {
                            Mosaic::text(ui, "Tree contents");
                        }

                        break;
                    }
                    case 14:
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 4> items = {"Apple", "Banana", "Cherry", "Kiwi"};
                        response = Mosaic::comboBox(ui, "ITEM: Combo", &m_queryCombo, items);
                        break;
                    }
                    default:
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 4> items = {"Apple", "Banana", "Cherry", "Kiwi"};
                        Mosaic::LayoutOptions listLayout;
                        listLayout.width = Mosaic::Dimension::fixed(220.f);
                        listLayout.height = Mosaic::Dimension::fixed(92.f);
                        response = Mosaic::listBox(ui, "ITEM: ListBox", &m_list, items, listLayout);
                        break;
                    }
                    }
                }
                Mosaic::Rect itemBounds;
                (void)Mosaic::debugBounds(ui, response.id, &itemBounds);
                Mosaic::ItemQueryOptions blockedByPopup;
                blockedByPopup.allowWhenBlockedByPopup = true;
                Mosaic::ItemQueryOptions blockedByActive;
                blockedByActive.allowWhenBlockedByActiveItem = true;
                Mosaic::ItemQueryOptions overlappedByItem;
                overlappedByItem.allowWhenOverlappedByItem = true;
                Mosaic::ItemQueryOptions overlappedByWindow;
                overlappedByWindow.allowWhenOverlappedByWindow = true;
                Mosaic::ItemQueryOptions disabledHover;
                disabledHover.allowWhenDisabled = true;
                Mosaic::ItemQueryOptions rectOnly;
                rectOnly.allowWhenBlockedByPopup = true;
                rectOnly.allowWhenBlockedByActiveItem = true;
                rectOnly.allowWhenOverlappedByItem = true;
                rectOnly.allowWhenOverlappedByWindow = true;
                bool returnValue = false;
                switch(m_queryItemType)
                {
                case 1:
                case 2:
                case 10:
                case 11:
                    returnValue = response.clicked();
                    break;
                case 12:
                case 13:
                    returnValue = Mosaic::treeExpanded(ui, response.id);
                    break;
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                case 14:
                case 15:
                    returnValue = response.changed();
                    break;
                default:
                    break;
                }
                Mosaic::String states = "Return value = ";
                states += returnValue ? "1" : "0";
                states += "\nIsItemFocused() = ";
                states += response.focused() ? "1" : "0";
                states += "\nIsItemHovered() = ";
                states += Mosaic::itemHovered(ui, response.id) ? "1" : "0";
                states += "\nIsItemHovered(_AllowWhenBlockedByPopup) = ";
                states += Mosaic::itemHovered(ui, response.id, blockedByPopup) ? "1" : "0";
                states += "\nIsItemHovered(_AllowWhenBlockedByActiveItem) = ";
                states += Mosaic::itemHovered(ui, response.id, blockedByActive) ? "1" : "0";
                states += "\nIsItemHovered(_AllowWhenOverlappedByItem) = ";
                states += Mosaic::itemHovered(ui, response.id, overlappedByItem) ? "1" : "0";
                states += "\nIsItemHovered(_AllowWhenOverlappedByWindow) = ";
                states += Mosaic::itemHovered(ui, response.id, overlappedByWindow) ? "1" : "0";
                states += "\nIsItemHovered(_AllowWhenDisabled) = ";
                states += Mosaic::itemHovered(ui, response.id, disabledHover) ? "1" : "0";
                states += "\nIsItemHovered(_RectOnly) = ";
                states += Mosaic::itemHovered(ui, response.id, rectOnly) ? "1" : "0";
                states += "\nIsItemActive() = ";
                states += response.active() ? "1" : "0";
                states += "\nIsItemEdited() = ";
                states += response.changed() ? "1" : "0";
                states += "\nIsItemActivated() = ";
                states += response.activated() ? "1" : "0";
                states += "\nIsItemDeactivated() = ";
                states += response.deactivated() ? "1" : "0";
                states += "\nIsItemDeactivatedAfterEdit() = ";
                states += response.deactivatedAfterEdit() ? "1" : "0";
                states += "\nIsItemToggledSelection() = ";
                states += response.toggledSelection() ? "1" : "0";
                states += "\nIsItemToggledOpen() = ";
                states += response.toggledOpen() ? "1" : "0";
                states += "\nIsItemVisible() = ";
                states += Mosaic::itemVisible(ui, response.id) ? "1" : "0";
                states += "\nIsItemDisabled() = ";
                states += response.disabled() ? "1" : "0";
                states += "\nIsItemClicked() = ";
                states += response.clicked() ? "1" : "0";
                states += "\nGetItemRectMin() = (";
                states += Detail::demoFixed(itemBounds.x, 1);
                states += ", ";
                states += Detail::demoFixed(itemBounds.y, 1);
                states += ")\nGetItemRectMax() = (";
                states += Detail::demoFixed(itemBounds.right(), 1);
                states += ", ";
                states += Detail::demoFixed(itemBounds.bottom(), 1);
                states += ")\nGetItemRectSize() = (";
                states += Detail::demoFixed(itemBounds.width, 1);
                states += ", ";
                states += Detail::demoFixed(itemBounds.height, 1);
                states += ")";
                Mosaic::bulletText(ui, states);
                Mosaic::ItemQueryOptions stationary;
                stationary.stationary = true;
                Mosaic::ItemQueryOptions delayShort;
                delayShort.delay = 0.15f;
                Mosaic::ItemQueryOptions delayNormal;
                delayNormal.delay = 0.4f;
                Mosaic::ItemQueryOptions tooltip;
                tooltip.stationary = true;
                tooltip.delay = 0.4f;
                Mosaic::String delayed = "with Hovering Delay or Stationary test:\n";
                delayed += "IsItemHovered() = ";
                delayed += Mosaic::itemHovered(ui, response.id) ? "1" : "0";
                delayed += "\nIsItemHovered(_Stationary) = ";
                delayed += Mosaic::itemHovered(ui, response.id, stationary) ? "1" : "0";
                delayed += "\nIsItemHovered(_DelayShort) = ";
                delayed += Mosaic::itemHovered(ui, response.id, delayShort) ? "1" : "0";
                delayed += "\nIsItemHovered(_DelayNormal) = ";
                delayed += Mosaic::itemHovered(ui, response.id, delayNormal) ? "1" : "0";
                delayed += "\nIsItemHovered(_Tooltip) = ";
                delayed += Mosaic::itemHovered(ui, response.id, tooltip) ? "1" : "0";
                Mosaic::bulletText(ui, delayed);
                Mosaic::TextInputOptions readOnly;
                readOnly.readOnly = true;
                Mosaic::inputText(ui, "unused", &m_filter, readOnly);
                Mosaic::helpMarker(ui, "This field allows tabbing out of the queried widget to observe deactivation.");
            }
        }
        {
            auto query = Mosaic::treeNode(ui, Mosaic::Key("query window status"), "Querying Window Status (Focused/Hovered etc.)");

            if(query.expanded() == true)
            {
                Mosaic::checkbox(ui, "Embed everything inside a child window for testing _RootWindow flag.", &m_queryWindowChild);
                Mosaic::Id rootWindow = Mosaic::currentWindow(ui);
                auto drawWindowQueries = [&](Mosaic::Id queriedScope)
                {
                    Mosaic::String focus = "IsWindowFocused() = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, false) ? "1" : "0";
                    Mosaic::ScopeQueryOptions childScopes;
                    childScopes.includeDescendants = true;
                    Mosaic::ScopeQueryOptions childScopesNoPopup = childScopes;
                    childScopesNoPopup.includePopupHierarchy = false;
                    Mosaic::ScopeQueryOptions childScopesDock = childScopes;
                    childScopesDock.includeDockHierarchy = true;
                    Mosaic::ScopeQueryOptions rootScopes = childScopes;
                    rootScopes.rootScope = true;
                    Mosaic::ScopeQueryOptions rootScopesNoPopup = rootScopes;
                    rootScopesNoPopup.includePopupHierarchy = false;
                    Mosaic::ScopeQueryOptions rootScopesDock = rootScopes;
                    rootScopesDock.includeDockHierarchy = true;
                    Mosaic::ScopeQueryOptions rootScope;
                    rootScope.rootScope = true;
                    Mosaic::ScopeQueryOptions rootScopeNoPopup = rootScope;
                    rootScopeNoPopup.includePopupHierarchy = false;
                    Mosaic::ScopeQueryOptions rootScopeDock = rootScope;
                    rootScopeDock.includeDockHierarchy = true;
                    Mosaic::ScopeQueryOptions anyScope;
                    anyScope.anyScope = true;
                    focus += "\nIsWindowFocused(ChildWindows) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, childScopes) ? "1" : "0";
                    focus += "\nIsWindowFocused(ChildWindows|NoPopupHierarchy) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, childScopesNoPopup) ? "1" : "0";
                    focus += "\nIsWindowFocused(ChildWindows|DockHierarchy) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, childScopesDock) ? "1" : "0";
                    focus += "\nIsWindowFocused(ChildWindows|RootWindow) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, rootScopes) ? "1" : "0";
                    focus += "\nIsWindowFocused(ChildWindows|RootWindow|NoPopupHierarchy) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, rootScopesNoPopup) ? "1" : "0";
                    focus += "\nIsWindowFocused(ChildWindows|RootWindow|DockHierarchy) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, rootScopesDock) ? "1" : "0";
                    focus += "\nIsWindowFocused(RootWindow) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, rootScope) ? "1" : "0";
                    focus += "\nIsWindowFocused(RootWindow|NoPopupHierarchy) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, rootScopeNoPopup) ? "1" : "0";
                    focus += "\nIsWindowFocused(RootWindow|DockHierarchy) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, rootScopeDock) ? "1" : "0";
                    focus += "\nIsWindowFocused(AnyWindow) = ";
                    focus += Mosaic::scopeFocused(ui, queriedScope, anyScope) ? "1" : "0";
                    Mosaic::ScopeQueryOptions blockedPopup;
                    blockedPopup.allowWhenBlockedByPopup = true;
                    Mosaic::ScopeQueryOptions blockedActive;
                    blockedActive.allowWhenBlockedByActiveItem = true;
                    Mosaic::ScopeQueryOptions stationary;
                    stationary.stationary = true;
                    Mosaic::ScopeQueryOptions childScopesBlockedPopup = childScopes;
                    childScopesBlockedPopup.allowWhenBlockedByPopup = true;
                    focus += "\nIsWindowHovered() = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope) ? "1" : "0";
                    focus += "\nIsWindowHovered(AllowWhenBlockedByPopup) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, blockedPopup) ? "1" : "0";
                    focus += "\nIsWindowHovered(AllowWhenBlockedByActiveItem) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, blockedActive) ? "1" : "0";
                    focus += "\nIsWindowHovered(ChildWindows) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, childScopes) ? "1" : "0";
                    focus += "\nIsWindowHovered(ChildWindows|NoPopupHierarchy) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, childScopesNoPopup) ? "1" : "0";
                    focus += "\nIsWindowHovered(ChildWindows|DockHierarchy) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, childScopesDock) ? "1" : "0";
                    focus += "\nIsWindowHovered(ChildWindows|RootWindow) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, rootScopes) ? "1" : "0";
                    focus += "\nIsWindowHovered(ChildWindows|RootWindow|NoPopupHierarchy) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, rootScopesNoPopup) ? "1" : "0";
                    focus += "\nIsWindowHovered(ChildWindows|RootWindow|DockHierarchy) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, rootScopesDock) ? "1" : "0";
                    focus += "\nIsWindowHovered(RootWindow) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, rootScope) ? "1" : "0";
                    focus += "\nIsWindowHovered(RootWindow|NoPopupHierarchy) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, rootScopeNoPopup) ? "1" : "0";
                    focus += "\nIsWindowHovered(RootWindow|DockHierarchy) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, rootScopeDock) ? "1" : "0";
                    focus += "\nIsWindowHovered(ChildWindows|AllowWhenBlockedByPopup) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, childScopesBlockedPopup) ? "1" : "0";
                    focus += "\nIsWindowHovered(AnyWindow) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, anyScope) ? "1" : "0";
                    focus += "\nIsWindowHovered(Stationary) = ";
                    focus += Mosaic::scopeHovered(ui, queriedScope, stationary) ? "1" : "0";
                    Mosaic::bulletText(ui, focus);
                    Mosaic::ScrollOptions testChildOptions;
                    Mosaic::LayoutOptions testChildLayout;
                    testChildLayout.width = Mosaic::SizeRule::Fill;
                    testChildLayout.height = Mosaic::Dimension::fixed(50.f);
                    auto testChild = Mosaic::scrollArea(ui, "query child", testChildOptions, testChildLayout);
                    Mosaic::text(ui, "This is another child window for testing ChildWindows.");
                    Mosaic::String capture = "Pointer owner: ";
                    capture += Detail::demoNumber(Mosaic::capturedPointerOwner(ui));
                    Mosaic::text(ui, capture);
                    Mosaic::checkbox(ui, "Hovered/Active tests after Begin() for title bar testing", &m_queryTitleWindow);
                };

                if(m_queryWindowChild == true)
                {
                    Mosaic::ScrollOptions childOptions;
                    Mosaic::LayoutOptions childLayout;
                    childLayout.width = Mosaic::SizeRule::Fill;
                    childLayout.height = Mosaic::Dimension::fixed(160.f);
                    auto child = Mosaic::scrollArea(ui, "outer_child", childOptions, childLayout);
                    Mosaic::text(ui, "This child contains every query below, including its nested child.");
                    drawWindowQueries(child.id());
                }
                else
                {
                    drawWindowQueries(rootWindow);
                }
            }
        }

        {
            auto selectables = Mosaic::treeNode(ui, Mosaic::Key("selectables"), "Selectables");

            if(selectables.expanded() == true)
            {
                {
                    auto basic = Mosaic::treeNode(ui, Mosaic::Key("selectable basic"), "Basic");

                    if(basic.expanded() == true)
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 4> labels = {"1. I am selectable", "2. I am selectable", "3. I am selectable", "4. I am double clickable"};
                        for(size_t index = 0; index != labels.size(); ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::SelectableOptions options;
                            options.allowDoubleClick = index == 3;
                            Mosaic::Response response = Mosaic::selectable(ui, Mosaic::Key("selectable"), labels[index], m_selectableStates[index], options);

                            if((index == 3 ? response.doubleClicked() == true : response.clicked() == true))
                            {
                                m_selectableStates[index] = !m_selectableStates[index];
                            }
                        }
                        {
                            auto disabled = Mosaic::disabledScope(ui, true);
                            Mosaic::selectable(ui, "Disabled selectable", false);
                        }
                        static bool selectOnNavigation = false;
                        static bool highlighted = true;
                        Mosaic::checkbox(ui, "Select on navigation", &selectOnNavigation);
                        Mosaic::checkbox(ui, "Highlight without selecting", &highlighted);
                        Mosaic::SelectableOptions options;
                        options.selectOnNavigation = selectOnNavigation;
                        options.highlight = highlighted;
                        Mosaic::Response response = Mosaic::selectable(ui, Mosaic::Key("flagged selectable"), "Selectable flags preview", m_selectableStates[7], options);

                        if(response.clicked() == true)
                        {
                            m_selectableStates[7] = !m_selectableStates[7];
                        }
                    }
                }
                {
                    auto sameLine = Mosaic::treeNode(ui, Mosaic::Key("selectable same line"), "Multiple items on the same line");

                    if(sameLine.expanded() == true)
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 3> names = {"main.c", "hello.cpp", "hello.h"};
                        constexpr Mosaic::Array<Mosaic::StringView, 3> links = {"Link 1", "Link 2", "Link 3"};
                        for(size_t rowIndex = 0; rowIndex != names.size(); ++rowIndex)
                        {
                            auto rowScope = Mosaic::scope(ui, Mosaic::Key(rowIndex));
                            auto row = Mosaic::row(ui);
                            Mosaic::SelectableOptions selectableOptions;
                            selectableOptions.allowOverlap = true;

                            if(Mosaic::selectable(ui, Mosaic::Key("source item"), names[rowIndex], m_selectableStates[rowIndex], selectableOptions).clicked() == true)
                            {
                                m_selectableStates[rowIndex] = !m_selectableStates[rowIndex];
                            }

                            Mosaic::smallButton(ui, links[rowIndex]);
                        }
                        Mosaic::spacer(ui, 2.f);
                        for(size_t index = 0; index != 5; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            auto row = Mosaic::row(ui);
                            Mosaic::SelectableOptions selectableOptions;
                            selectableOptions.allowOverlap = true;

                            if(Mosaic::selectable(ui, Mosaic::Key("selectable"), {}, m_singleSelection == static_cast<int>(index), selectableOptions).clicked() == true)
                            {
                                m_singleSelection = static_cast<int>(index);
                            }

                            Mosaic::checkbox(ui, Mosaic::Key("check"), {}, &m_selectableStates[8 + index]);
                            Mosaic::Color marker = {(index & 1U) != 0 ? 1.f : 0.2f, (index & 2U) != 0 ? 1.f : 0.2f, 0.2f, 1.f};
                            {
                                Mosaic::ColorEditOptions colorOptions;
                                colorOptions.labelPlacement = Mosaic::LabelPlacement::Hidden;
                                Mosaic::colorEditorRgb(ui, {}, &marker, colorOptions);
                            }
                            Mosaic::text(ui, "Some label");
                        }
                    }
                }
                {
                    auto inTables = Mosaic::treeNode(ui, Mosaic::Key("selectable table"), "In Tables");

                    if(inTables.expanded() == true)
                    {
                        Mosaic::TableOptions options;
                        options.rowSelection = true;
                        {
                            auto table = Mosaic::table(ui, "split1", 3, options);
                            for(size_t itemIndex = 0; itemIndex != 10; ++itemIndex)
                            {
                                if(itemIndex % 3 == 0)
                                {
                                    Mosaic::tableNextRow(ui);
                                }

                                if(Mosaic::tableSetColumn(ui, static_cast<uint32_t>(itemIndex % 3)) == false)
                                {
                                    continue;
                                }

                                Mosaic::String label = "Item ";
                                label += Detail::demoNumber(itemIndex);
                                Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key(itemIndex), label, m_selectableStates[itemIndex]);

                                if(item.clicked() == true)
                                {
                                    m_selectableStates[itemIndex] = !m_selectableStates[itemIndex];
                                }
                            }
                        }
                        {
                            auto table = Mosaic::table(ui, "split2", 3, options);
                            for(size_t row = 0; row != 10; ++row)
                            {
                                Mosaic::tableNextRow(ui);

                                if(Mosaic::tableSetColumn(ui, 0) == true)
                                {
                                    Mosaic::String label = "Item ";
                                    label += Detail::demoNumber(row);
                                    Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key(row), label, m_selectableStates[row]);

                                    if(item.clicked() == true)
                                    {
                                        m_selectableStates[row] = !m_selectableStates[row];
                                    }
                                }

                                if(Mosaic::tableSetColumn(ui, 1) == true)
                                {
                                    Mosaic::text(ui, "Some other contents");
                                }

                                if(Mosaic::tableSetColumn(ui, 2) == true)
                                {
                                    Mosaic::text(ui, "123456");
                                }
                            }
                        }
                    }
                }
                {
                    auto grid = Mosaic::treeNode(ui, Mosaic::Key("selectable grid"), "Grid");

                    if(grid.expanded() == true)
                    {
                        Mosaic::LayoutOptions layout;
                        layout.width = Mosaic::SizeRule::Fill;
                        auto cells = Mosaic::grid(ui, 4, layout);
                        bool winning = std::all_of(m_selectableStates.begin(), m_selectableStates.end(),
                                                         [](bool selected)
                                                         {
                                                             return selected;
                                                         });
                        for(size_t index = 0; index != m_selectableStates.size(); ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::SelectableOptions options;
                            options.width = Mosaic::Dimension::fixed(54.f);
                            options.height = Mosaic::Dimension::fixed(54.f);

                            if(winning == true)
                            {
                                options.overrideTextAlignment = true;
                                options.textAlignment = {0.5f + 0.5f * std::cos(static_cast<float>(Mosaic::input(ui).timestamp) * 2.f), 0.5f + 0.5f * std::sin(static_cast<float>(Mosaic::input(ui).timestamp) * 3.f)};
                            }

                            if(Mosaic::selectable(ui, Mosaic::Key("grid item"), "Sailor", m_selectableStates[index], options).clicked() == true)
                            {
                                m_selectableStates[index] = !m_selectableStates[index];
                                size_t x = index % 4;
                                size_t y = index / 4;

                                if(x > 0)
                                {
                                    m_selectableStates[index - 1] = !m_selectableStates[index - 1];
                                }

                                if(x < 3)
                                {
                                    m_selectableStates[index + 1] = !m_selectableStates[index + 1];
                                }

                                if(y > 0)
                                {
                                    m_selectableStates[index - 4] = !m_selectableStates[index - 4];
                                }

                                if(y < 3)
                                {
                                    m_selectableStates[index + 4] = !m_selectableStates[index + 4];
                                }
                            }
                        }
                    }
                }

                {
                    auto alignment = Mosaic::treeNode(ui, Mosaic::Key("selectable alignment"), "Alignment");

                    if(alignment.expanded() == true)
                    {
                        Mosaic::helpMarker(ui, "Per-item alignment is demonstrated with a 3 by 3 grid.");
                        auto alignments = Mosaic::grid(ui, 3);
                        for(size_t index = 0; index != 9; ++index)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String label = "(";
                            label += Detail::demoFixed(static_cast<float>(index % 3) / 2.f, 1);
                            label += ",";
                            label += Detail::demoFixed(static_cast<float>(index / 3) / 2.f, 1);
                            label += ")";
                            Mosaic::SelectableOptions options;
                            options.width = Mosaic::Dimension::fixed(72.f);
                            options.height = Mosaic::Dimension::fixed(72.f);
                            options.overrideTextAlignment = true;
                            options.textAlignment = {static_cast<float>(index % 3) / 2.f, static_cast<float>(index / 3) / 2.f};
                            Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key("aligned selectable"), label, m_selectableStates[index], options);

                            if(item.clicked() == true)
                            {
                                m_selectableStates[index] = !m_selectableStates[index];
                            }
                        }
                    }
                }
            }
        }

        {
            auto selection = Mosaic::treeNode(ui, Mosaic::Key("multi select"), "Selection State & Multi-Select");

            if(selection.expanded() == true)
            {
                constexpr Mosaic::Array<Mosaic::StringView, 10> sections = {"Single-Select", "Multi-Select (manual/simplified, without BeginMultiSelect)", "Multi-Select", "Multi-Select (with clipper)", "Multi-Select (with deletion)", "Multi-Select (dual list box)", "Multi-Select (in a table)", "Multi-Select (checkboxes)", "Multi-Select (multiple scopes)", "Multi-Select (tiled assets browser)"};
                for(size_t sectionIndex = 0; sectionIndex != sections.size(); ++sectionIndex)
                {
                    auto sectionScope = Mosaic::scope(ui, Mosaic::Key(sectionIndex));
                    auto example = Mosaic::treeNode(ui, Mosaic::Key("selection example"), sections[sectionIndex]);

                    if(example.expanded() == false)
                    {
                        continue;
                    }

                    size_t itemCount = sectionIndex == 3 ? size_t{1000} : static_cast<size_t>(std::max(int32_t{1}, m_selectionItemCount));
                    Mosaic::IdVector & ordered = m_selectionOrders[sectionIndex];

                    if(sectionIndex == 4 && m_deletionInitialized == false)
                    {
                        ordered.reserve(itemCount);
                        for(size_t index = 0; index != itemCount; ++index)
                        {
                            ordered.push_back(Mosaic::combineId(Mosaic::hashBytes(sections[sectionIndex]), m_deletionNextId++));
                        }
                        m_deletionInitialized = true;
                    }
                    else if(sectionIndex != 4 && ordered.size() != itemCount)
                    {
                        ordered.clear();
                        ordered.reserve(itemCount);
                        for(size_t index = 0; index != itemCount; ++index)
                        {
                            ordered.push_back(Mosaic::combineId(Mosaic::hashBytes(sections[sectionIndex]), index + 1));
                        }
                    }

                    Mosaic::SelectionModel & selectionModel = m_selectionModels[sectionIndex];

                    if(sectionIndex == 0)
                    {
                        for(size_t index = 0; index != 5; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String label = "Object ";
                            label += Detail::demoNumber(index);

                            if(Mosaic::selectable(ui, Mosaic::Key("single selectable"), label, m_singleSelection == static_cast<int>(index)).clicked() == true)
                            {
                                m_singleSelection = static_cast<int>(index);
                            }
                        }
                    }
                    else if(sectionIndex == 1)
                    {
                        Mosaic::text(ui, "Command-click toggles items. Shift-click extends the manually maintained range.");
                        for(size_t index = 0; index != 8; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String label = "Object ";
                            label += Detail::demoNumber(index);

                            if(Mosaic::selectable(ui, Mosaic::Key("manual selectable"), label, selectionModel.selected(ordered[index]) == true).clicked() == true)
                            {
                                const Mosaic::Input & currentInput = Mosaic::input(ui);

                                if(currentInput.modifiers.shift == true)
                                {
                                    selectionModel.selectRange(ordered, ordered[index]);
                                }
                                else
                                {
                                    bool toggle = currentInput.modifiers.primary || currentInput.modifiers.control == true || currentInput.modifiers.super;
                                    selectionModel.select(ordered[index], toggle, toggle);
                                }
                            }
                        }
                    }
                    else if(sectionIndex == 2)
                    {
                        Mosaic::text(ui, "Supported features:");
                        Mosaic::bulletText(ui, "Keyboard navigation (arrows, page up/down, home/end, space).");
                        Mosaic::bulletText(ui, "Ctrl modifier to preserve and toggle selection.");
                        Mosaic::bulletText(ui, "Shift modifier for range selection.");
                        Mosaic::bulletText(ui, "Ctrl+A to select all.");
                        Mosaic::bulletText(ui, "Escape to clear selection.");
                        Mosaic::bulletText(ui, "Click and drag to box-select.");
                        Mosaic::checkbox(ui, "Enable box selection", &m_selectionBox);
                        Mosaic::String status = "Selection: ";
                        status += Detail::demoNumber(selectionModel.size());
                        status += "/";
                        status += Detail::demoNumber(ordered.size());
                        Mosaic::text(ui, status);
                        {
                            Mosaic::LayoutOptions selectionLayout;
                            selectionLayout.width = Mosaic::SizeRule::Fill;
                            selectionLayout.height = Mosaic::Dimension::fixed(220.f);
                            Mosaic::Id selectionSurface = Mosaic::InvalidId;
                            Mosaic::Id requestScope = Mosaic::InvalidId;
                            {
                                auto selectionArea = Mosaic::scrollArea(ui, "multi selection area", Mosaic::ScrollOptions{}, selectionLayout);
                                selectionSurface = selectionArea.id();
                                Mosaic::SelectionOptions selectionOptions;
                                selectionOptions.applyRequests = false;
                                auto multiSelect = Mosaic::multiSelect(ui, Mosaic::Key("multi selection scope"), &selectionModel, ordered, selectionOptions);
                                requestScope = multiSelect.id();
                                for(size_t index = 0; index != ordered.size(); ++index)
                                {
                                    auto item = Mosaic::scope(ui, Mosaic::Key(index));
                                    Mosaic::String label = "Object ";
                                    label += Detail::demoNumber(index);
                                    Mosaic::selectable(ui, Mosaic::Key("multi selectable"), label, ordered[index]);
                                }
                            }
                            Mosaic::applySelectionRequests(&selectionModel, ordered, Mosaic::selectionRequests(ui), requestScope);
                            Mosaic::BoxSelectionOptions boxOptions;
                            boxOptions.enabled = m_selectionBox;
                            Mosaic::boxSelect(ui, selectionSurface, &selectionModel, boxOptions);
                        }
                    }
                    else if(sectionIndex == 3)
                    {
                        Mosaic::text(ui, "Added features:");
                        Mosaic::bulletText(ui, "Using MosaicListClipper.");
                        Mosaic::LayoutOptions listLayout;
                        listLayout.width = Mosaic::SizeRule::Fill;
                        listLayout.height = Mosaic::Dimension::fixed(220.f);
                        auto list = Mosaic::scrollArea(ui, "clipped selection", Mosaic::ScrollOptions{}, listLayout);
                        auto multiSelect = Mosaic::multiSelect(ui, Mosaic::Key("clipped selection scope"), &selectionModel, ordered);
                        float itemHeight = Mosaic::getTheme(ui).metrics.controlHeight;
                        Mosaic::VisibleRange range;
                        (void)Mosaic::beginListClipper(ui, ordered.size(), itemHeight, &range, list.id());
                        for(size_t index = range.begin; index != range.end; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String label = "Object ";
                            label += Detail::demoNumber(index);
                            Mosaic::selectable(ui, Mosaic::Key("clipped item"), label, ordered[index]);
                        }
                        Mosaic::endListClipper(ui, range, ordered.size(), itemHeight);
                    }
                    else if(sectionIndex == 4)
                    {
                        Mosaic::text(ui, "Added features:");
                        Mosaic::bulletText(ui, "Dynamic list with Delete key support.");
                        bool deleteSelected = false;
                        {
                            auto actions = Mosaic::row(ui);

                            if(Mosaic::button(ui, "Add 20 items").clicked() == true)
                            {
                                ordered.reserve(ordered.size() + 20);
                                for(size_t index = 0; index != 20; ++index)
                                {
                                    ordered.push_back(Mosaic::combineId(Mosaic::hashBytes(sections[sectionIndex]), m_deletionNextId++));
                                }
                            }

                            if(Mosaic::button(ui, "Remove 20 items").clicked() == true)
                            {
                                size_t removeCount = std::min(size_t{20}, ordered.size());
                                for(size_t index = 0; index != removeCount; ++index)
                                {
                                    selectionModel.remove(ordered.back());
                                    ordered.pop_back();
                                }
                            }

                            deleteSelected = Mosaic::button(ui, "Delete selected").clicked();
                        }
                        bool selectionFocused = false;
                        for(size_t index = 0; index != ordered.size(); ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String label = "Object ";
                            label += Detail::demoNumber(index);
                            Mosaic::Response response = Mosaic::selectable(ui, Mosaic::Key("deletable item"), label, &selectionModel, ordered[index], ordered);
                            selectionFocused = selectionFocused || response.focused();
                        }

                        if(deleteSelected == true || (selectionFocused == true && Mosaic::input(ui).keyPressed(Mosaic::KeyCode::Delete) == true))
                        {
                            ordered.erase(std::remove_if(ordered.begin(), ordered.end(),
                                                         [&selectionModel](Mosaic::Id id)
                                                         {
                                                             auto returnedValue = selectionModel.selected(id);

                                                             return returnedValue;
                                                         }),
                                          ordered.end());
                            selectionModel.clear();
                        }
                    }
                    else if(sectionIndex == 5)
                    {
                        Mosaic::LayoutOptions layout;
                        layout.width = Mosaic::SizeRule::Fill;
                        auto columns = Mosaic::grid(ui, 3, layout);
                        {
                            auto available = Mosaic::column(ui);
                            Mosaic::text(ui, "Available");
                            Mosaic::LayoutOptions listLayout;
                            listLayout.width = Mosaic::SizeRule::Fill;
                            listLayout.height = Mosaic::Dimension::fixed(190.f);
                            auto list = Mosaic::scrollArea(ui, "available items", Mosaic::ScrollOptions{}, listLayout);
                            for(size_t index = 0; index != ordered.size(); ++index)
                            {
                                if(selectionModel.selected(ordered[index]) == true)
                                {
                                    continue;
                                }

                                auto item = Mosaic::scope(ui, Mosaic::Key(index));
                                Mosaic::String label = "Item ";
                                label += Detail::demoNumber(index);
                                Mosaic::selectable(ui, Mosaic::Key("available"), label, &selectionModel, ordered[index], ordered);
                            }
                        }
                        {
                            auto actions = Mosaic::column(ui);

                            if(Mosaic::button(ui, ">>").clicked() == true)
                            {
                                selectionModel.selectAll(ordered);
                            }

                            if(Mosaic::button(ui, ">").clicked() == true)
                            {
                                auto available = std::find_if(ordered.begin(), ordered.end(),
                                                                    [&selectionModel](Mosaic::Id id)
                                                                    {
                                                                        auto returnedValue = selectionModel.selected(id) == false;

                                                                        return returnedValue;
                                                                    });

                                if(available != ordered.end())
                                {
                                    selectionModel.select(*available, true);
                                }
                            }

                            if(Mosaic::button(ui, "<").clicked() == true && selectionModel.empty() == false)
                            {
                                selectionModel.remove(selectionModel.values().back());
                            }

                            if(Mosaic::button(ui, "<<").clicked() == true)
                            {
                                selectionModel.clear();
                            }
                        }

                        {
                            auto basket = Mosaic::column(ui);
                            Mosaic::text(ui, "Basket");
                            Mosaic::LayoutOptions listLayout;
                            listLayout.width = Mosaic::SizeRule::Fill;
                            listLayout.height = Mosaic::Dimension::fixed(190.f);
                            auto list = Mosaic::scrollArea(ui, "basket items", Mosaic::ScrollOptions{}, listLayout);
                            for(size_t index = 0; index != ordered.size(); ++index)
                            {
                                if(selectionModel.selected(ordered[index]) == false)
                                {
                                    continue;
                                }

                                auto item = Mosaic::scope(ui, Mosaic::Key(index));
                                Mosaic::String label = "Item ";
                                label += Detail::demoNumber(index);
                                Mosaic::selectable(ui, Mosaic::Key("basket"), label, &selectionModel, ordered[index], ordered);
                            }
                        }
                    }
                    else if(sectionIndex == 6)
                    {
                        Mosaic::TableOptions tableOptions;
                        tableOptions.rowSelection = true;
                        tableOptions.scrollVertical = true;
                        auto basketScope = Mosaic::scope(ui, Mosaic::Key("basket"));
                        auto table = Mosaic::table(ui, {}, 2, tableOptions, Detail::fillLayout(220.f));
                        Mosaic::tableSetupColumn(ui, 0, "Object");
                        Mosaic::tableSetupColumn(ui, 1, "Action");
                        Mosaic::tableHeadersRow(ui);
                        for(size_t index = 0; index != ordered.size(); ++index)
                        {
                            Mosaic::tableNextRow(ui, Mosaic::Key(index), &selectionModel, ordered[index], ordered);
                            (void)Mosaic::tableSetColumn(ui, 0);
                            Mosaic::String label = "Object ";
                            label += Detail::demoNumber(index);
                            Mosaic::text(ui, label);
                            (void)Mosaic::tableSetColumn(ui, 1);
                            Mosaic::button(ui, Mosaic::Key(index), "hello");
                        }
                    }
                    else if(sectionIndex == 7)
                    {
                        Mosaic::text(ui, "In a list of checkboxes (not selectable):");
                        Mosaic::bulletText(ui, "Using _NoAutoSelect + _NoAutoClear flags.");
                        Mosaic::bulletText(ui, "Shift+Click to check multiple boxes.");
                        Mosaic::bulletText(ui, "Shift+Keyboard to copy current value to other boxes.");
                        for(size_t index = 0; index != 12; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String label = "Object ";
                            label += Detail::demoNumber(index);
                            Mosaic::Response checkbox = Mosaic::checkbox(ui, label, &m_selectableStates[index]);

                            if(checkbox.clicked() == true)
                            {
                                const Mosaic::Input & currentInput = Mosaic::input(ui);
                                size_t anchor = selectionModel.anchor() == Mosaic::InvalidId ? index : static_cast<size_t>(selectionModel.anchor() - 1);

                                if(currentInput.modifiers.shift == true && anchor < 12)
                                {
                                    size_t first = std::min(anchor, index);
                                    size_t last = std::max(anchor, index);
                                    for(size_t check = first; check <= last; ++check)
                                    {
                                        m_selectableStates[check] = m_selectableStates[index];
                                    }
                                }

                                selectionModel.select(static_cast<Mosaic::Id>(index + 1));
                            }
                        }
                    }
                    else if(sectionIndex == 8)
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 2> scopeNames = {"Selection scope A", "Selection scope B"};
                        for(size_t scopeIndex = 0; scopeIndex != scopeNames.size(); ++scopeIndex)
                        {
                            auto scope = Mosaic::scope(ui, Mosaic::Key(scopeIndex));
                            Mosaic::separatorText(ui, "Selection scope");
                            Mosaic::text(ui, scopeNames[scopeIndex]);
                            Mosaic::SelectionModel * model = scopeIndex == 0 ? &selectionModel : &m_secondarySelection;
                            for(size_t index = 0; index != 8; ++index)
                            {
                                auto item = Mosaic::scope(ui, Mosaic::Key(index));
                                Mosaic::String label = "Object ";
                                label += Detail::demoNumber(index);
                                Mosaic::selectable(ui, Mosaic::Key("scoped item"), label, model, ordered[index], ordered);
                            }
                        }
                    }
                    else
                    {
                        Mosaic::checkbox(ui, "Assets Browser", &m_showAssetsBrowser);
                        Mosaic::text(ui, "(also access from 'Examples->Assets Browser' in menu)");
                    }
                }

                {
                    auto treeSelection = Mosaic::treeNode(ui, Mosaic::Key("selection trees"), "Multi-Select (trees)");

                    if(treeSelection.expanded() == true)
                    {
                        Mosaic::Array<Mosaic::Id, 15> treeOrder = {};
                        for(size_t index = 0; index != treeOrder.size(); ++index)
                        {
                            treeOrder[index] = Mosaic::combineId(Mosaic::hashBytes("selection tree"), index + 1);
                        }
                        Mosaic::text(ui, "Command-click toggles nodes; Shift-click selects a visible range.");
                        for(size_t rootIndex = 0; rootIndex != 3; ++rootIndex)
                        {
                            size_t rootItem = rootIndex * 5;
                            auto rootScope = Mosaic::scope(ui, Mosaic::Key(rootIndex));
                            Mosaic::String rootLabel = "Root ";
                            rootLabel += Detail::demoNumber(rootIndex);
                            Mosaic::TreeNodeOptions rootOptions;
                            rootOptions.defaultExpanded = rootIndex == 0;
                            rootOptions.openOnArrow = true;
                            rootOptions.openOnDoubleClick = true;
                            rootOptions.spanAvailableWidth = true;
                            rootOptions.navigationLeftJumpsToParent = true;
                            rootOptions.lines = Mosaic::TreeLineMode::ToNodes;
                            auto root = Mosaic::treeNode(ui, Mosaic::Key("selected tree root"), rootLabel, &m_treeSelection, treeOrder[rootItem], treeOrder, rootOptions);

                            if(root.expanded() == true)
                            {
                                for(size_t childIndex = 1; childIndex != 5; ++childIndex)
                                {
                                    auto childScope = Mosaic::scope(ui, Mosaic::Key(childIndex));
                                    Mosaic::String childLabel = "Leaf ";
                                    childLabel += Detail::demoNumber(rootIndex);
                                    childLabel += ".";
                                    childLabel += Detail::demoNumber(childIndex - 1);
                                    Mosaic::TreeNodeOptions childOptions;
                                    childOptions.leaf = true;
                                    childOptions.bullet = true;
                                    childOptions.spanAvailableWidth = true;
                                    childOptions.navigationLeftJumpsToParent = true;
                                    childOptions.lines = Mosaic::TreeLineMode::ToNodes;
                                    (void)Mosaic::treeNode(ui, Mosaic::Key("selected tree leaf"), childLabel, &m_treeSelection, treeOrder[rootItem + childIndex], treeOrder, childOptions);
                                }
                            }
                        }
                    }
                }
                {
                    auto advanced = Mosaic::treeNode(ui, Mosaic::Key("selection advanced"), "Multi-Select (advanced)");

                    if(advanced.expanded() == true)
                    {
                        static int widgetType = 0;
                        static bool useClipper = true;
                        static bool useDeletion = true;
                        static bool useDragDrop = true;
                        static bool showInTable = false;
                        static bool showColorButton = true;
                        static bool requestDeletionFromMenu = false;
                        static bool singleSelect = false;
                        static bool selectAll = true;
                        static bool autoSelect = true;
                        static bool autoClear = true;
                        static bool autoClearOnReselect = true;
                        static bool selectOnRightClick = true;
                        static bool clearOnEscape = true;
                        static bool navigationWrapX = false;
                        static int32_t selectionScope = 0;
                        static int32_t selectionPressPolicy = 0;
                        static bool noBoxScroll = false;
                        static bool boxSelectionOneDimensional = true;
                        static bool clearOnClickVoid = false;
                        static bool boxSelectFromSelectedItems = false;
                        auto options = Mosaic::treeNode(ui, Mosaic::Key("advanced selection options"), "Options", true);

                        if(options.expanded() == true)
                        {
                            auto widgetTypes = Mosaic::row(ui);

                            if(Mosaic::radioButton(ui, "Selectables", widgetType == 0).clicked() == true)
                            {
                                widgetType = 0;
                            }

                            if(Mosaic::radioButton(ui, "Tree nodes", widgetType == 1).clicked() == true)
                            {
                                widgetType = 1;
                            }

                            Mosaic::checkbox(ui, "Enable clipper", &useClipper);
                            Mosaic::checkbox(ui, "Enable deletion", &useDeletion);
                            Mosaic::checkbox(ui, "Enable drag & drop", &useDragDrop);
                            Mosaic::checkbox(ui, "Show in a table", &showInTable);
                            Mosaic::checkbox(ui, "Show color button", &showColorButton);
                            Mosaic::checkbox(ui, "Range selection", &m_selectionRange);
                            Mosaic::checkbox(ui, "Box selection", &m_selectionBox);
                            Mosaic::separatorText(ui, "Selection behavior");
                            Mosaic::checkbox(ui, "Single select", &singleSelect);
                            Mosaic::checkbox(ui, "Select all", &selectAll);
                            Mosaic::checkbox(ui, "Auto select", &autoSelect);
                            Mosaic::checkbox(ui, "Auto clear", &autoClear);
                            Mosaic::checkbox(ui, "Auto clear on reselect", &autoClearOnReselect);
                            Mosaic::checkbox(ui, "Select on right click", &selectOnRightClick);
                            Mosaic::separatorText(ui, "Selection timing");
                            {
                                auto timing = Mosaic::row(ui);

                                if(Mosaic::radioButton(ui, "Automatic", selectionPressPolicy == 0).clicked() == true)
                                {
                                    selectionPressPolicy = 0;
                                }

                                if(Mosaic::radioButton(ui, "On press", selectionPressPolicy == 1).clicked() == true)
                                {
                                    selectionPressPolicy = 1;
                                }

                                if(Mosaic::radioButton(ui, "On release", selectionPressPolicy == 2).clicked() == true)
                                {
                                    selectionPressPolicy = 2;
                                }
                            }
                            Mosaic::checkbox(ui, "Clear on click void", &clearOnClickVoid);
                            Mosaic::checkbox(ui, "Box select without auto-scroll", &noBoxScroll);
                            Mosaic::checkbox(ui, "One-dimensional box selection", &boxSelectionOneDimensional);
                            Mosaic::checkbox(ui, "Box select from selected items", &boxSelectFromSelectedItems);
                            Mosaic::checkbox(ui, "Clear on Escape", &clearOnEscape);
                            Mosaic::checkbox(ui, "Navigation wraps on X", &navigationWrapX);
                            Mosaic::separatorText(ui, "Selection scope");
                            {
                                auto scopeChoices = Mosaic::row(ui);

                                if(Mosaic::radioButton(ui, "Scope rectangle", selectionScope == 0).clicked() == true)
                                {
                                    selectionScope = 0;
                                }

                                if(Mosaic::radioButton(ui, "Scope window", selectionScope == 1).clicked() == true)
                                {
                                    selectionScope = 1;
                                }
                            }
                        }

                        Mosaic::text(ui, "Use Command-click to toggle and Shift-click to select a range.");

                        size_t desiredCount = static_cast<size_t>(std::max(int32_t{1}, m_selectionItemCount));
                        while(m_advancedSelectionOrder.size() < desiredCount)
                        {
                            m_advancedSelectionOrder.push_back(Mosaic::combineId(Mosaic::hashBytes("advanced selection item"), m_advancedSelectionNextId++));
                        }
                        while(m_advancedSelectionOrder.size() > desiredCount)
                        {
                            m_advancedSelection.remove(m_advancedSelectionOrder.back());
                            m_advancedSelectionOrder.pop_back();
                        }
                        m_advancedSelectionNodes.assign(m_advancedSelectionOrder.size(), Mosaic::InvalidId);

                        Mosaic::String status = "Selection size: ";
                        status += Detail::demoNumber(m_advancedSelection.size());
                        status += "/";
                        status += Detail::demoNumber(m_advancedSelectionOrder.size());
                        Mosaic::text(ui, status);

                        bool deleteSelected = false;

                        if(useDeletion == true)
                        {
                            auto actions = Mosaic::row(ui);
                            deleteSelected = Mosaic::button(ui, "Delete selected").clicked();

                            if(Mosaic::button(ui, "Add item").clicked() == true)
                            {
                                m_advancedSelectionOrder.push_back(Mosaic::combineId(Mosaic::hashBytes("advanced selection item"), m_advancedSelectionNextId++));
                                m_selectionItemCount = static_cast<int32_t>(m_advancedSelectionOrder.size());
                            }
                        }

                        Mosaic::SelectionOptions selectionOptions;
                        selectionOptions.singleSelect = singleSelect;
                        selectionOptions.selectAll = selectAll;
                        selectionOptions.rangeSelect = m_selectionRange;
                        selectionOptions.autoSelect = autoSelect;
                        selectionOptions.autoClear = autoClear;
                        selectionOptions.autoClearOnReselect = autoClearOnReselect;
                        selectionOptions.selectOnRightClick = selectOnRightClick;
                        selectionOptions.clearOnEscape = clearOnEscape;
                        selectionOptions.navigationWrapX = navigationWrapX;
                        selectionOptions.scope = selectionScope == 0 ? Mosaic::SelectionScope::Rect : Mosaic::SelectionScope::Window;
                        selectionOptions.pressPolicy = selectionPressPolicy == 1 ? Mosaic::SelectionPressPolicy::Press : (selectionPressPolicy == 2 ? Mosaic::SelectionPressPolicy::Release : Mosaic::SelectionPressPolicy::Automatic);

                        constexpr Mosaic::TypeId reorderType = 0x48d06d66d6fca331ULL;
                        const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
                        size_t dropIndex = std::numeric_limits<size_t>::max();
                        Mosaic::DragPayload dropPayload;
                        bool selectionFocused = false;
                        Mosaic::Id selectionSurface = Mosaic::InvalidId;
                        float itemHeight = Mosaic::getTheme(ui).metrics.controlHeight;

                        auto drawSelectionItem = [&](size_t index)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(m_advancedSelectionOrder[index]));
                            auto itemRow = Mosaic::row(ui, Detail::fillLayout());
                            Mosaic::String label = widgetType == 0 ? "Object " : "Node ";
                            label += Detail::demoNumber(index);

                            if(showColorButton == true)
                            {
                                Mosaic::Theme colorTheme = Mosaic::getTheme(ui);
                                float hue = static_cast<float>(index % 12) / 12.f;
                                colorTheme.colors.button = Detail::demoHsv(hue, 0.65f, 0.72f);
                                colorTheme.colors.buttonHovered = Detail::demoHsv(hue, 0.72f, 0.82f);
                                colorTheme.colors.buttonActive = Detail::demoHsv(hue, 0.78f, 0.92f);
                                auto colorStyle = Mosaic::styleScope(ui, colorTheme);
                                Mosaic::ButtonOptions colorButtonOptions;
                                colorButtonOptions.width = Mosaic::Dimension::fixed(itemHeight);
                                Mosaic::button(ui, Mosaic::Key("item color"), " ", colorButtonOptions);
                            }

                            Mosaic::Response response;

                            if(widgetType == 0)
                            {
                                response = Mosaic::selectable(ui, Mosaic::Key("advanced selection row"), label, &m_advancedSelection, m_advancedSelectionOrder[index], m_advancedSelectionOrder, selectionOptions);
                            }
                            else
                            {
                                Mosaic::TreeNodeOptions treeOptions;
                                treeOptions.leaf = index % 5 != 0;
                                treeOptions.bullet = treeOptions.leaf;
                                treeOptions.openOnArrow = true;
                                treeOptions.openOnDoubleClick = true;
                                treeOptions.spanAvailableWidth = true;
                                treeOptions.navigationLeftJumpsToParent = true;
                                treeOptions.lines = Mosaic::TreeLineMode::ToNodes;
                                Mosaic::TreeScope tree = Mosaic::treeNode(ui, Mosaic::Key("advanced selection tree row"), label, &m_advancedSelection, m_advancedSelectionOrder[index], m_advancedSelectionOrder, treeOptions, selectionOptions);
                                (void)Mosaic::itemResponse(ui, tree.id(), &response);
                            }

                            selectionFocused = selectionFocused || response.focused();
                            m_advancedSelectionNodes[index] = response.id;

                            if(pointer != nullptr && response.hovered() == true && pointer->isPressed(Mosaic::PointerButton::Secondary) == true)
                            {
                                Mosaic::PopupOptions popupOptions;
                                popupOptions.owner = response.id;
                                popupOptions.placement = Mosaic::PopupPlacement::Cursor;
                                popupOptions.minimumSize = {168.f, 0.f};
                                Mosaic::openPopup(ui, Mosaic::Key("selection item context"), popupOptions);
                            }

                            Mosaic::PopupOptions popupOptions;
                            popupOptions.owner = response.id;
                            popupOptions.placement = Mosaic::PopupPlacement::Cursor;
                            popupOptions.minimumSize = {168.f, 0.f};
                            auto contextMenu = Mosaic::popup(ui, Mosaic::Key("selection item context"), popupOptions);

                            if(contextMenu.visible() == true)
                            {
                                {
                                    auto deletionDisabled = Mosaic::disabledScope(ui, useDeletion == false || m_advancedSelection.empty() == true);
                                    Mosaic::String deleteLabel = "Delete ";
                                    deleteLabel += Detail::demoNumber(m_advancedSelection.size());
                                    deleteLabel += " item(s)";

                                    if(Mosaic::menuItem(ui, deleteLabel).clicked() == true)
                                    {
                                        requestDeletionFromMenu = true;
                                    }
                                }
                                Mosaic::menuItem(ui, "Close");
                            }

                            if(useDragDrop == true)
                            {
                                Mosaic::Id item = m_advancedSelectionOrder[index];
                                const auto * bytes = reinterpret_cast<const std::byte *>(&item);
                                (void)Mosaic::beginDragDropSource(ui, response, reorderType, Mosaic::ByteSpan(bytes, sizeof(item)));
                                Mosaic::DragDropAcceptResult result = Mosaic::acceptDragDropPayload(ui, response, reorderType);

                                if(result.delivery == true)
                                {
                                    dropIndex = index;
                                    dropPayload = result.payload;
                                }
                            }
                        };

                        Mosaic::VisibleRange visibleItems = {0, m_advancedSelectionOrder.size()};

                        if(showInTable == true)
                        {
                            Mosaic::TableOptions tableOptions;
                            tableOptions.headers = false;
                            tableOptions.rowBackground = true;
                            tableOptions.bordersInnerHorizontal = true;
                            tableOptions.scrollVertical = true;
                            auto table = Mosaic::table(ui, "advanced selection table", 2, tableOptions, Detail::fillLayout(220.f));
                            selectionSurface = table.id();
                            Mosaic::TableColumnOptions objectColumn;
                            objectColumn.sizing = Mosaic::TableSizing::Stretch;
                            objectColumn.widthOrWeight = 0.7f;
                            Mosaic::tableSetupColumn(ui, 0, "Object", objectColumn);
                            Mosaic::TableColumnOptions categoryColumn;
                            categoryColumn.sizing = Mosaic::TableSizing::Stretch;
                            categoryColumn.widthOrWeight = 0.3f;
                            Mosaic::tableSetupColumn(ui, 1, "Category", categoryColumn);

                            if(useClipper == true)
                            {
                                (void)Mosaic::tableVisibleRows(ui, m_advancedSelectionOrder.size(), itemHeight, &visibleItems);
                            }

                            for(size_t index = visibleItems.begin; index != visibleItems.end; ++index)
                            {
                                Mosaic::tableNextRow(ui, Mosaic::Key(index), false);
                                (void)Mosaic::tableSetColumn(ui, 0);
                                drawSelectionItem(index);
                                (void)Mosaic::tableSetColumn(ui, 1);
                                Mosaic::String category = "Category ";
                                category += Detail::demoNumber(index % 6);
                                Mosaic::TextInputOptions readOnly;
                                readOnly.readOnly = true;
                                Mosaic::inputText(ui, "category", &category, readOnly);
                            }
                        }
                        else
                        {
                            Mosaic::LayoutOptions listLayout;
                            listLayout.width = Mosaic::SizeRule::Fill;
                            listLayout.height = Mosaic::Dimension::fixed(220.f);
                            auto list = Mosaic::scrollArea(ui, "advanced selection list", Mosaic::ScrollOptions{}, listLayout);
                            selectionSurface = list.id();

                            if(useClipper == true)
                            {
                                (void)Mosaic::beginListClipper(ui, m_advancedSelectionOrder.size(), itemHeight, &visibleItems, selectionSurface);
                            }

                            for(size_t index = visibleItems.begin; index != visibleItems.end; ++index)
                            {
                                drawSelectionItem(index);
                            }

                            if(useClipper == true)
                            {
                                Mosaic::endListClipper(ui, visibleItems, m_advancedSelectionOrder.size(), itemHeight);
                            }
                        }

                        Mosaic::BoxSelectionOptions boxOptions;
                        boxOptions.enabled = m_selectionBox;
                        boxOptions.clearOnClick = clearOnClickVoid;
                        boxOptions.allowFromSelectedItems = boxSelectFromSelectedItems;
                        boxOptions.noScroll = noBoxScroll;
                        boxOptions.mode = boxSelectionOneDimensional ? Mosaic::BoxSelectionMode::OneDimensional : Mosaic::BoxSelectionMode::TwoDimensional;
                        Mosaic::Id boxSelectionSurface = selectionOptions.scope == Mosaic::SelectionScope::Window ? Mosaic::currentWindow(ui) : selectionSurface;
                        Mosaic::boxSelect(ui, boxSelectionSurface, &m_advancedSelection, boxOptions);

                        if(dropIndex < m_advancedSelectionOrder.size())
                        {
                            Mosaic::Id sourceItem = Mosaic::InvalidId;

                            if(dropPayload.type == reorderType && dropPayload.data.size() == sizeof(sourceItem))
                            {
                                std::memcpy(&sourceItem, dropPayload.data.data(), sizeof(sourceItem));
                            }

                            auto source = std::find(m_advancedSelectionOrder.begin(), m_advancedSelectionOrder.end(), sourceItem);

                            if(source != m_advancedSelectionOrder.end())
                            {
                                size_t sourceIndex = static_cast<size_t>(source - m_advancedSelectionOrder.begin());
                                Mosaic::Id moved = *source;
                                m_advancedSelectionOrder.erase(source);
                                size_t destination = sourceIndex < dropIndex ? dropIndex - 1 : dropIndex;
                                m_advancedSelectionOrder.insert(m_advancedSelectionOrder.begin() + static_cast<std::ptrdiff_t>(destination), moved);
                            }
                        }

                        bool deleteItems = deleteSelected;

                        if(requestDeletionFromMenu == true)
                        {
                            deleteItems = true;
                        }

                        if(selectionFocused == true)
                        {
                            if(Mosaic::input(ui).keyPressed(Mosaic::KeyCode::Delete) == true)
                            {
                                deleteItems = true;
                            }
                        }

                        if(useDeletion == false)
                        {
                            deleteItems = false;
                        }

                        if(deleteItems == true)
                        {
                            size_t firstSelected = m_advancedSelectionOrder.size();
                            Mosaic::IdVector remaining;
                            remaining.reserve(m_advancedSelectionOrder.size());
                            for(size_t index = 0; index != m_advancedSelectionOrder.size(); ++index)
                            {
                                if(m_advancedSelection.selected(m_advancedSelectionOrder[index]) == true)
                                {
                                    firstSelected = std::min(firstSelected, index);
                                }
                                else
                                {
                                    remaining.push_back(m_advancedSelectionOrder[index]);
                                }
                            }
                            Mosaic::Id focusNode = Mosaic::InvalidId;

                            if(remaining.empty() == false)
                            {
                                size_t remainingIndex = std::min(firstSelected, remaining.size() - 1);
                                Mosaic::Id focusItem = remaining[remainingIndex];
                                auto original = std::find(m_advancedSelectionOrder.begin(), m_advancedSelectionOrder.end(), focusItem);

                                if(original != m_advancedSelectionOrder.end())
                                {
                                    focusNode = m_advancedSelectionNodes[static_cast<size_t>(original - m_advancedSelectionOrder.begin())];
                                }
                            }

                            m_advancedSelectionOrder.erase(std::remove_if(m_advancedSelectionOrder.begin(), m_advancedSelectionOrder.end(),
                                                                          [this](Mosaic::Id item)
                                                                          {
                                                                              auto returnedValue = m_advancedSelection.selected(item);

                                                                              return returnedValue;
                                                                          }),
                                                           m_advancedSelectionOrder.end());
                            m_advancedSelection.clear();
                            requestDeletionFromMenu = false;
                            m_selectionItemCount = static_cast<int32_t>(m_advancedSelectionOrder.size());

                            if(focusNode != Mosaic::InvalidId)
                            {
                                Mosaic::focus(ui, focusNode);
                                Mosaic::scrollToItem(ui, selectionSurface, focusNode);
                            }
                        }
                    }
                }
            }
        }
        {
            auto tabs = Mosaic::treeNode(ui, Mosaic::Key("tabs"), "Tabs");

            if(tabs.expanded() == true)
            {
                {
                    auto basic = Mosaic::treeNode(ui, Mosaic::Key("tabs basic"), "Basic");

                    if(basic.expanded() == true)
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 3> items = {"Avocado", "Broccoli", "Cucumber"};
                        auto tabBar = Mosaic::beginTabBar(ui, "basic tabs");
                        for(size_t index = 0; index != items.size(); ++index)
                        {
                            auto tabItem = Mosaic::beginTabItem(ui, Mosaic::Key(index), items[index]);

                            if(tabItem.expanded() == true)
                            {
                                m_basicTab = static_cast<int>(index);
                                Mosaic::String contents = "This is the ";
                                contents += items[index];
                                contents += " tab!\nblah blah blah blah blah";
                                Mosaic::text(ui, contents);
                            }
                        }
                        Mosaic::separator(ui);
                    }
                }
                {
                    auto advanced = Mosaic::treeNode(ui, Mosaic::Key("tabs advanced"), "Advanced & Close Button");

                    if(advanced.expanded() == true)
                    {
                        static bool reorderable = true;
                        static bool autoSelectNew = false;
                        static bool listPopup = false;
                        static bool noMiddleClose = false;
                        static bool selectedOverline = false;
                        static Mosaic::TabFittingPolicy fittingPolicy = Mosaic::TabFittingPolicy::Mixed;
                        Mosaic::checkbox(ui, "MosaicTabBarFlags_Reorderable", &reorderable);
                        Mosaic::checkbox(ui, "MosaicTabBarFlags_AutoSelectNewTabs", &autoSelectNew);
                        Mosaic::checkbox(ui, "MosaicTabBarFlags_TabListPopupButton", &listPopup);
                        Mosaic::checkbox(ui, "MosaicTabBarFlags_NoCloseWithMiddleMouseButton", &noMiddleClose);
                        Mosaic::checkbox(ui, "MosaicTabBarFlags_DrawSelectedOverline", &selectedOverline);
                        Detail::tabFittingPolicyControls(ui, &fittingPolicy);
                        constexpr Mosaic::Array<Mosaic::StringView, 4> labels = {"Artichoke", "Beetroot", "Celery", "Daikon"};
                        Mosaic::text(ui, "Opened:");
                        Mosaic::TabsOptions tabOptions;
                        tabOptions.reorderable = reorderable;
                        tabOptions.autoSelectNewTabs = autoSelectNew;
                        tabOptions.tabListPopupButton = listPopup;
                        tabOptions.closeWithMiddleMouse = noMiddleClose == false;
                        tabOptions.selectedOverline = selectedOverline;
                        tabOptions.fittingPolicy = fittingPolicy;
                        Mosaic::tabs(ui, "closable tabs", &m_closableTab, labels, Mosaic::BoolSpan(m_tabOpen.data(), m_tabOpen.size()), tabOptions);
                        for(size_t index = 0; index != labels.size(); ++index)
                        {
                            Mosaic::checkbox(ui, labels[index], &m_tabOpen[index]);
                        }

                        if(std::any_of(m_tabOpen.begin(), m_tabOpen.end(),
                                       [](bool value)
                                       {
                                           return value;
                                       }))
                        {
                            Mosaic::String contents = "This is the ";
                            size_t active = static_cast<size_t>(std::clamp(m_closableTab, 0, static_cast<int>(labels.size() - 1)));
                            contents += labels[active];
                            contents += " tab!";
                            Mosaic::text(ui, contents);

                            if((m_closableTab & 1) != 0)
                            {
                                Mosaic::text(ui, "I am an odd tab.");
                            }
                        }
                        Mosaic::separatorText(ui, "Per-tab flags");
                        static bool leadingTab = true;
                        static bool trailingTab = true;
                        static bool unsavedTab = true;
                        static bool noTooltipTab = false;
                        {
                            auto flagsRow = Mosaic::row(ui);
                            Mosaic::checkbox(ui, "Leading", &leadingTab);
                            Mosaic::checkbox(ui, "Trailing", &trailingTab);
                            Mosaic::checkbox(ui, "Unsaved", &unsavedTab);
                            Mosaic::checkbox(ui, "No tooltip", &noTooltipTab);
                        }
                        Mosaic::Array<Mosaic::TabItemOptions, 4> itemOptions = {};
                        itemOptions[0].leading = leadingTab;
                        itemOptions[1].unsavedDocument = unsavedTab;
                        itemOptions[2].noTooltip = noTooltipTab;
                        itemOptions[3].trailing = trailingTab;
                        Mosaic::tabs(ui, "tab item flags", &m_inlineTab, labels, Mosaic::BoolSpan(m_tabOpen.data(), m_tabOpen.size()), Mosaic::TabItemOptionsSpan(itemOptions), tabOptions);
                        Mosaic::separatorText(ui, "Externally closed tabs");
                        static Mosaic::Array<bool, 3> externalTabs = {true, true, true};
                        static int externallyClosed = -1;
                        {
                            auto actions = Mosaic::row(ui);
                            for(size_t index = 0; index != externalTabs.size(); ++index)
                            {
                                auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                                auto disabled = Mosaic::disabledScope(ui, externalTabs[index] == false);
                                Mosaic::String action = "Close Document ";
                                action += Detail::demoNumber(index + 1);

                                if(Mosaic::button(ui, action).clicked() == true)
                                {
                                    externalTabs[index] = false;
                                    externallyClosed = static_cast<int>(index);
                                }
                            }
                        }
                        Mosaic::TabsOptions externalOptions;
                        externalOptions.reorderable = true;
                        externalOptions.autoSelectNewTabs = true;
                        {
                            auto externalBar = Mosaic::beginTabBar(ui, "external lifecycle tabs", externalOptions);

                            if(externallyClosed >= 0)
                            {
                                Mosaic::setTabItemClosed(ui, Mosaic::Key(static_cast<size_t>(externallyClosed)));
                                externallyClosed = -1;
                            }

                            for(size_t index = 0; index != externalTabs.size(); ++index)
                            {
                                if(externalTabs[index] == false)
                                {
                                    continue;
                                }

                                Mosaic::String label = "Document ";
                                label += Detail::demoNumber(index + 1);
                                auto item = Mosaic::beginTabItem(ui, Mosaic::Key(index), label, &externalTabs[index]);

                                if(item.expanded() == true)
                                {
                                    Mosaic::text(ui, "The application owns this tab's open state.");
                                }
                            }
                        }

                        if(Mosaic::button(ui, "Reopen all documents").clicked() == true)
                        {
                            externalTabs.fill(true);
                        }

                        Mosaic::separator(ui);
                    }
                }

                {
                    auto buttons = Mosaic::treeNode(ui, Mosaic::Key("tab buttons"), "TabItemButton & Leading/Trailing flags");

                    if(buttons.expanded() == true)
                    {
                        static bool showLeading = true;
                        static bool showTrailing = true;
                        static Mosaic::TabFittingPolicy fittingPolicy = Mosaic::TabFittingPolicy::Shrink;
                        Mosaic::checkbox(ui, "Show Leading TabItemButton()", &showLeading);
                        Mosaic::checkbox(ui, "Show Trailing TabItemButton()", &showTrailing);
                        Detail::tabFittingPolicyControls(ui, &fittingPolicy);

                        struct InlineTabContent
                        {
                            HelloDemo * demo = nullptr;
                            bool leading = false;
                            bool trailing = false;
                        };

                        InlineTabContent content = {this, showLeading, showTrailing};
                        Mosaic::TabsOptions inlineOptions;
                        inlineOptions.reorderable = true;
                        inlineOptions.scrollToFit = true;
                        inlineOptions.fittingPolicy = fittingPolicy;
                        inlineOptions.contentUserData = &content;
                        inlineOptions.leadingContent = [](Mosaic::Context * context, void * userData)
                        {
                            const auto * tabContent = static_cast<const InlineTabContent *>(userData);

                            if(tabContent == nullptr)
                            {
                                return;
                            }

                            if(tabContent->leading == false)
                            {
                                return;
                            }

                            Mosaic::Response help = Mosaic::tabItemButton(context, Mosaic::Key("Leading help"), "?");

                            if(help.clicked() == true)
                            {
                                Mosaic::openPopup(context, Mosaic::Key("tab help popup"));
                            }

                            auto popup = Mosaic::popup(context, Mosaic::Key("tab help popup"));

                            if(popup.visible() == true)
                            {
                                Mosaic::text(context, "This is a leading tab button.");
                            }
                        };
                        inlineOptions.trailingContent = [](Mosaic::Context * context, void * userData)
                        {
                            auto * tabContent = static_cast<InlineTabContent *>(userData);

                            if(tabContent == nullptr)
                            {
                                return;
                            }

                            if(tabContent->trailing == false)
                            {
                                return;
                            }

                            if(tabContent->demo == nullptr)
                            {
                                return;
                            }

                            if(Mosaic::tabItemButton(context, Mosaic::Key("Trailing add"), "+").clicked() == false)
                            {
                                return;
                            }

                            tabContent->demo->m_dynamicTabIds.push_back(tabContent->demo->m_nextDynamicTabId++);
                        };
                        Mosaic::SizeVector closedTabs;
                        {
                            auto tabBar = Mosaic::beginTabBar(ui, "dynamic inline tab bar", inlineOptions);
                            for(size_t index = 0; index != m_dynamicTabIds.size(); ++index)
                            {
                                uint32_t tabId = m_dynamicTabIds[index];
                                Mosaic::String label = "Document ";
                                label += Detail::demoNumber(tabId);
                                bool open = true;
                                auto tabItem = Mosaic::beginTabItem(ui, Mosaic::Key(tabId), label, &open);

                                if(tabItem.expanded() == true)
                                {
                                    Mosaic::String contents = "Contents of ";
                                    contents += label;
                                    Mosaic::text(ui, contents);
                                }

                                if(open == false)
                                {
                                    closedTabs.push_back(index);
                                }
                            }
                        }
                        for(auto closed = closedTabs.rbegin(); closed != closedTabs.rend(); ++closed)
                        {
                            m_dynamicTabIds.erase(m_dynamicTabIds.begin() + static_cast<std::ptrdiff_t>(*closed));
                        }
                        Mosaic::separator(ui);
                    }
                }
            }
        }

        {
            auto text = Mosaic::treeNode(ui, Mosaic::Key("text"), "Text");

            if(text.expanded() == true)
            {
                {
                    auto colorful = Mosaic::treeNode(ui, Mosaic::Key("colorful text"), "Colorful Text");

                    if(colorful.expanded() == true)
                    {
                        constexpr Mosaic::Array<Mosaic::Color, 2> colors = {Mosaic::Color{1.f, 0.f, 1.f, 1.f}, Mosaic::Color{1.f, 1.f, 0.f, 1.f}};
                        constexpr Mosaic::Array<Mosaic::StringView, 2> labels = {"Pink", "Yellow"};
                        for(size_t index = 0; index != labels.size(); ++index)
                        {
                            Mosaic::Theme theme = Mosaic::getTheme(ui);
                            theme.colors.text = colors[index];
                            auto style = Mosaic::styleScope(ui, theme);
                            Mosaic::text(ui, labels[index]);
                        }
                        {
                            Mosaic::Theme disabledTheme = Mosaic::getTheme(ui);
                            disabledTheme.colors.text = disabledTheme.colors.textDisabled;
                            auto disabledStyle = Mosaic::styleScope(ui, disabledTheme);
                            Mosaic::text(ui, "Disabled");
                        }
                        Mosaic::helpMarker(ui, "The disabled text color is stored in the current StyleColors.");
                    }
                }
                {
                    auto fontSize = Mosaic::treeNode(ui, Mosaic::Key("font size"), "Font Size");

                    if(fontSize.expanded() == true)
                    {
                        Mosaic::String mainScale = "style.FontScaleMain = ";
                        mainScale += Detail::demoFixed(1.f, 2);
                        Mosaic::text(ui, mainScale);
                        Mosaic::Viewport viewport;
                        if(Mosaic::currentViewport(ui, &viewport) == false)
                        {
                            return;
                        }

                        float viewportScale = std::max(0.01f, viewport.dpiScale);
                        Mosaic::String dpiScale = "style.FontScaleDpi = ";
                        dpiScale += Detail::demoFixed(viewportScale, 2);
                        Mosaic::text(ui, dpiScale);
                        Mosaic::String globalScale = "global_scale = ~";
                        globalScale += Detail::demoFixed(viewportScale, 2);
                        Mosaic::text(ui, globalScale);
                        Mosaic::String currentSize = "FontSize = ";
                        currentSize += Detail::demoFixed(Mosaic::getTheme(ui).metrics.fontSize, 2);
                        Mosaic::text(ui, currentSize);
                        Mosaic::separator(ui);
                        static float customSize = 16.f;
                        Mosaic::slider(ui, "custom_size", &customSize, 10.f, 100.f);
                        Mosaic::text(ui, "Mosaic::PushFont(nullptr, custom_size);");
                        {
                            Mosaic::Theme customTheme = Mosaic::getTheme(ui);
                            customTheme.metrics.fontSize = customSize;
                            customTheme.metrics.lineHeight = std::ceil(customSize * 1.3f);
                            auto customStyle = Mosaic::styleScope(ui, customTheme);
                            Mosaic::String label = "FontSize = ";
                            label += Detail::demoFixed(customSize, 2);
                            label += " (== ";
                            label += Detail::demoFixed(customSize, 2);
                            label += " * global_scale)";
                            Mosaic::text(ui, label);
                        }
                        Mosaic::separator(ui);
                        static float customScale = 1.f;
                        Mosaic::slider(ui, "custom_scale", &customScale, 0.5f, 4.f);
                        Mosaic::text(ui, "Mosaic::PushFont(nullptr, style.FontSizeBase * custom_scale);");
                        {
                            Mosaic::Theme theme = Mosaic::getTheme(ui);
                            theme.metrics.fontSize *= customScale;
                            theme.metrics.lineHeight *= customScale;
                            auto style = Mosaic::styleScope(ui, theme);
                            Mosaic::String label = "FontSize = ";
                            label += Detail::demoFixed(theme.metrics.fontSize, 2);
                            label += " (== style.FontSizeBase * ";
                            label += Detail::demoFixed(customScale, 2);
                            label += " * global_scale)";
                            Mosaic::text(ui, label);
                        }
                        Mosaic::separator(ui);
                        Mosaic::Theme baseTheme = Mosaic::getTheme(ui);
                        for(uint32_t index = 1; index <= 8; ++index)
                        {
                            float scale = static_cast<float>(index) * 0.5f;
                            Mosaic::Theme theme = baseTheme;
                            theme.metrics.fontSize *= scale;
                            theme.metrics.lineHeight *= scale;
                            auto style = Mosaic::styleScope(ui, theme);
                            Mosaic::String label = "FontSize = ";
                            label += Detail::demoFixed(theme.metrics.fontSize, 2);
                            label += " (== style.FontSizeBase * ";
                            label += Detail::demoFixed(scale, 2);
                            label += " * global_scale)";
                            Mosaic::text(ui, label);
                        }
                    }
                }
                {
                    auto wrapping = Mosaic::treeNode(ui, Mosaic::Key("word wrapping"), "Word Wrapping");

                    if(wrapping.expanded() == true)
                    {
                        Mosaic::text(ui, "This text should automatically wrap on the edge of the window. The current "
                                         "implementation for text wrapping follows simple rules suitable for English "
                                         "and possibly other languages.");
                        Mosaic::slider(ui, "Wrap width", &m_wrapWidth, -20.f, 600.f);
                        Mosaic::LayoutOptions wrapLayout;

                        if(m_wrapWidth > 0.f)
                        {
                            wrapLayout.width = Mosaic::Dimension::fixed(m_wrapWidth);
                        }
                        else
                        {
                            wrapLayout.width = Mosaic::SizeRule::Fill;
                            wrapLayout.padding.right = -m_wrapWidth;
                        }

                        auto wrapped = Mosaic::column(ui, wrapLayout);
                        for(size_t index = 0; index != 2; ++index)
                        {
                            Mosaic::String title = "Test paragraph ";
                            title += Detail::demoNumber(index);
                            title += " (wrap width ";
                            title += Detail::demoFixed(m_wrapWidth, 0);
                            title += "):";
                            Mosaic::text(ui, title);
                            Mosaic::TextOptions textOptions;
                            textOptions.wordWrap = true;
                            textOptions.layout.width = Mosaic::SizeRule::Fill;
                            Mosaic::Response paragraph = Mosaic::text(ui,
                                                                            index == 0 ? "The lazy dog is a good dog. This paragraph should fit within the selected "
                                                                                         "width. Testing a 1 character word. The quick brown fox jumps over the lazy dog."
                                                                                       : "aaaaaaaa bbbbbbbb, c cccccccc,dddddddd. d eeeeeeee   ffffffff. "
                                                                                         "gggggggghhhhhhhh == false",
                                                                            textOptions);
                            Mosaic::Rect paragraphBounds;
                            if(Mosaic::debugBounds(ui, paragraph.id, &paragraphBounds) == false)
                            {
                                continue;
                            }

                            Mosaic::String measured = "Text BB: ";
                            measured += Detail::demoFixed(paragraphBounds.width, 0);
                            measured += " x ";
                            measured += Detail::demoFixed(paragraphBounds.height, 0);
                            Mosaic::text(ui, measured);
                        }
                    }
                }
                {
                    auto utf8 = Mosaic::treeNode(ui, Mosaic::Key("utf8 text"), "UTF-8 Text");

                    if(utf8.expanded() == true)
                    {
                        Mosaic::text(ui, "CJK text will only appear if the font was loaded with the appropriate CJK "
                                         "character ranges. Read docs/FONTS.md for details.");
                        Mosaic::text(ui, "Hiragana: かきくけこ (kakikukeko)");
                        Mosaic::text(ui, "Kanjis: 日本語 (nihongo)");
                        Mosaic::inputText(ui, "UTF-8 input", &m_utf8Input);
                    }
                }
            }
        }
        {
            auto filter = Mosaic::treeNode(ui, Mosaic::Key("text filter"), "Text Filter");

            if(filter.expanded() == true)
            {
                Mosaic::helpMarker(ui, "The demo filter performs simple include and exclude matching on text strings.");
                Mosaic::text(ui, "Filter usage:\n  \"\"         display all lines\n  \"xxx\"      display lines "
                                 "containing \"xxx\"\n  \"xxx,yyy\"  display lines containing \"xxx\" or \"yyy\"\n  "
                                 "\"-xxx\"     hide lines containing \"xxx\"");
                Mosaic::searchField(ui, "Filter", &m_filter);
                constexpr Mosaic::Array<Mosaic::StringView, 8> lines = {"aaa1.c", "bbb1.c", "ccc1.c", "aaa2.cpp", "bbb2.cpp", "ccc2.cpp", "abc.h", "hello, world"};
                for(Mosaic::StringView line : lines)
                {
                    if(Detail::passesTextFilter(line, m_filter) == true)
                    {
                        Mosaic::bulletText(ui, line);
                    }
                }
            }
        }
        {
            auto input = Mosaic::treeNode(ui, Mosaic::Key("text input"), "Text Input");

            if(input.expanded() == true)
            {
                {
                    auto multiline = Mosaic::treeNode(ui, Mosaic::Key("multiline input"), "Multi-line Text Input");

                    if(multiline.expanded() == true)
                    {
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_ReadOnly", &m_multilineReadOnly);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_WordWrap", &m_multilineWordWrap);
                        Mosaic::helpMarker(ui, "Word wrapping is shown by constraining the multiline field to the available width.");
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_AllowTabInput", &m_multilineAllowTab);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_CtrlEnterForNewLine", &m_multilineControlEnter);
                        Mosaic::TextInputOptions options;
                        options.maximumBytes = 16 * 1024;
                        options.readOnly = m_multilineReadOnly;
                        options.allowTabInput = m_multilineAllowTab;
                        options.controlEnterForNewLine = m_multilineControlEnter;
                        options.wordWrap = m_multilineWordWrap;
                        Mosaic::LayoutOptions layout;
                        layout.width = Mosaic::SizeRule::Fill;
                        layout.height = Mosaic::Dimension::fixed(170.f);
                        Mosaic::inputMultiline(ui, "Multiline", &m_multiline, options, layout);
                    }
                }
                {
                    auto filtered = Mosaic::treeNode(ui, Mosaic::Key("filtered input"), "Filtered Text Input");

                    if(filtered.expanded() == true)
                    {
                        Mosaic::inputText(ui, "default", &m_filteredDefault);
                        Mosaic::TextInputOptions decimal;
                        decimal.numeric = true;
                        decimal.characterFilter = Mosaic::TextCharacterFilter::Decimal;
                        Mosaic::inputText(ui, "decimal", &m_filteredDecimal, decimal);
                        Mosaic::TextInputOptions hexadecimal;
                        hexadecimal.characterFilter = Mosaic::TextCharacterFilter::Hexadecimal;
                        Mosaic::inputText(ui, "hexadecimal", &m_filteredHexadecimal, hexadecimal);
                        Mosaic::TextInputOptions uppercase;
                        uppercase.characterFilter = Mosaic::TextCharacterFilter::Uppercase;
                        Mosaic::inputText(ui, "uppercase", &m_filteredUppercase, uppercase);
                        Mosaic::TextInputOptions noBlank;
                        noBlank.characterFilter = Mosaic::TextCharacterFilter::NoBlank;
                        Mosaic::inputText(ui, "no blank", &m_filteredNoBlank, noBlank);
                        Mosaic::TextInputOptions casingSwap;
                        casingSwap.callbackCharacterFilter = true;
                        //////////////////////////////////////////////////////////////////////////
                        casingSwap.callback = +[](Mosaic::TextInputCallbackData & data)
                        {
                            if(data.event != Mosaic::TextInputCallbackEvent::CharacterFilter)
                            {
                                return;
                            }

                            if(data.character >= U'a' && data.character <= U'z')
                            {
                                data.character -= U'a' - U'A';
                            }
                            else if(data.character >= U'A' && data.character <= U'Z')
                            {
                                data.character += U'a' - U'A';
                            }
                        };
                        Mosaic::inputText(ui, "casing swap", &m_filteredCasingSwap, casingSwap);
                        Mosaic::TextInputOptions mosaicOnly;
                        mosaicOnly.callbackCharacterFilter = true;
                        //////////////////////////////////////////////////////////////////////////
                        mosaicOnly.callback = +[](Mosaic::TextInputCallbackData & data)
                        {
                            if(data.event != Mosaic::TextInputCallbackEvent::CharacterFilter)
                            {
                                return;
                            }

                            data.reject = data.character != U'm' && data.character != U'o' && data.character != U's' && data.character != U'a' && data.character != U'i' && data.character != U'c';
                        };
                        Mosaic::inputText(ui, "\"mosaic\"", &m_filteredLowercase, mosaicOnly);
                    }
                }
                {
                    auto password = Mosaic::treeNode(ui, Mosaic::Key("password input"), "Password Input");

                    if(password.expanded() == true)
                    {
                        Mosaic::TextInputOptions options;
                        options.password = true;
                        Mosaic::inputText(ui, "password", &m_password, options);
                        options.hint = "password (w/ hint)";
                        Mosaic::inputText(ui, "password (w/ hint)", &m_password, options);
                        Mosaic::inputText(ui, "password (clear)", &m_password);
                    }
                }
                {
                    auto callbacks = Mosaic::treeNode(ui, Mosaic::Key("callbacks input"), "Completion, History, Edit Callbacks");

                    if(callbacks.expanded() == true)
                    {
                        Mosaic::TextInputOptions completionOptions;
                        completionOptions.callback = Detail::completionInputCallback;
                        completionOptions.callbackCompletion = true;
                        Mosaic::Response completion = Mosaic::inputText(ui, "Completion", &m_completionInput, completionOptions);
                        Mosaic::itemTooltip(ui, completion,
                                            "Press Tab while editing to complete the current word. The Console example "
                                            "demonstrates application command completion.");
                        Mosaic::TextInputOptions historyOptions;
                        historyOptions.callback = Detail::historyInputCallback;
                        historyOptions.callbackUserData = &m_historyCallbackIndex;
                        historyOptions.callbackHistory = true;
                        Mosaic::Response history = Mosaic::inputText(ui, "History", &m_historyInput, historyOptions);
                        Mosaic::itemTooltip(ui, history, "Press Up or Down while editing to navigate application history.");
                        Mosaic::TextInputOptions editOptions;
                        editOptions.callback = Detail::editInputCallback;
                        editOptions.callbackUserData = &m_editCallbackCount;
                        editOptions.callbackEdit = true;
                        Mosaic::inputText(ui, "Edit", &m_editCallbackInput, editOptions);
                        {
                            auto countRow = Mosaic::row(ui);
                            Mosaic::text(ui, "Every edit toggles the first character casing.");
                            Mosaic::text(ui, Detail::demoNumber(m_editCallbackCount));
                        }
                        Mosaic::separator(ui);
                        Mosaic::text(ui, "Console callback example:");
                        Mosaic::inputText(ui, "Command", &m_commandInput);
                        Mosaic::text(ui, "Commands: HELP, HISTORY, CLEAR, CLASSIFY");
                    }
                }
                {
                    auto resize = Mosaic::treeNode(ui, Mosaic::Key("resize callback"), "Resize Callback");

                    if(resize.expanded() == true)
                    {
                        static bool resizeWordWrap = false;
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_WordWrap", &resizeWordWrap);
                        Mosaic::TextInputOptions resizeOptions;
                        resizeOptions.callback = Detail::countInputCallback;
                        resizeOptions.callbackUserData = &m_resizeCallbackCount;
                        resizeOptions.callbackResize = true;
                        resizeOptions.wordWrap = resizeWordWrap;
                        Mosaic::LayoutOptions resizeLayout;
                        resizeLayout.width = Mosaic::SizeRule::Fill;
                        resizeLayout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 16.f);
                        Mosaic::inputMultiline(ui, "Resizable buffer", &m_resizeInput, resizeOptions, resizeLayout);
                        Mosaic::String callbackCount = "Resize callbacks: ";
                        callbackCount += Detail::demoNumber(m_resizeCallbackCount);
                        Mosaic::text(ui, callbackCount);
                        Mosaic::String bufferState = "Size: ";
                        bufferState += Detail::demoNumber(m_resizeInput.size());
                        bufferState += "  Capacity: ";
                        bufferState += Detail::demoNumber(m_resizeInput.capacity());
                        bufferState += "\nData: ";
                        bufferState += Detail::demoPointer(m_resizeInput.data());
                        Mosaic::text(ui, bufferState);
                    }
                }
                {
                    auto eliding = Mosaic::treeNode(ui, Mosaic::Key("eliding"), "Eliding, Alignment");

                    if(eliding.expanded() == true)
                    {
                        static bool elideLeft = true;
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_ElideLeft", &elideLeft);
                        Mosaic::TextInputOptions elideOptions;
                        elideOptions.elideLeft = elideLeft;
                        Mosaic::inputText(ui, "Path", &m_elidePath, elideOptions);
                    }
                }
                {
                    auto misc = Mosaic::treeNode(ui, Mosaic::Key("text input misc"), "Miscellaneous");

                    if(misc.expanded() == true)
                    {
                        static bool enterReturnsTrue = false;
                        static bool noHorizontalScroll = false;
                        static bool overwrite = false;
                        static bool callbackAlways = false;
                        static int32_t callbackCount = 0;
                        Mosaic::checkbox(ui, "Read-only", &m_dataReadOnly);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_EscapeClearsAll", &m_textEscapeClearsAll);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_NoUndoRedo", &m_textDisableUndoRedo);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_EnterReturnsTrue", &enterReturnsTrue);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_NoHorizontalScroll", &noHorizontalScroll);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_AlwaysOverwrite", &overwrite);
                        Mosaic::checkbox(ui, "MosaicInputTextFlags_CallbackAlways", &callbackAlways);
                        Mosaic::TextInputOptions options;
                        options.readOnly = m_dataReadOnly;
                        options.escapeClearsAll = m_textEscapeClearsAll;
                        options.undoRedo = m_textDisableUndoRedo == false;
                        options.enterReturnsTrue = enterReturnsTrue;
                        options.noHorizontalScroll = noHorizontalScroll;
                        options.alwaysOverwrite = overwrite;
                        options.callbackAlways = callbackAlways;
                        options.callbackUserData = &callbackCount;
                        //////////////////////////////////////////////////////////////////////////
                        options.callback = +[](Mosaic::TextInputCallbackData & data)
                        {
                            if(data.event == Mosaic::TextInputCallbackEvent::Always)
                            {
                                ++*static_cast<int32_t *>(data.userData);
                            }
                        };
                        Mosaic::Response field = Mosaic::inputText(ui, "Flags field", &m_singleLine, options);
                        Mosaic::String status = "Enter returned: ";
                        status += field.submitted() ? "true" : "false";
                        status += ", always callbacks: ";
                        status += Detail::demoNumber(callbackCount);
                        Mosaic::text(ui, status);
                        Mosaic::inputText(ui, "UTF-8 input", &m_utf8Input);
                    }
                }
            }
        }

        {
            auto tooltips = Mosaic::treeNode(ui, Mosaic::Key("tooltips"), "Tooltips");

            if(tooltips.expanded() == true)
            {
                auto general = Mosaic::treeNode(ui, Mosaic::Key("tooltip general"), "General");

                if(general.expanded() == true)
                {
                    Mosaic::ButtonOptions tooltipButton;
                    tooltipButton.width = Mosaic::SizeRule::Fill;
                    Mosaic::Response response = Mosaic::button(ui, Mosaic::Key("Basic"), "Basic", tooltipButton);
                    Mosaic::itemTooltip(ui, response, "I am a tooltip", {240.f, 64.f}, 0.25f);
                    Mosaic::Response fancy = Mosaic::button(ui, Mosaic::Key("Fancy"), "Fancy", tooltipButton);
                    Mosaic::ItemTooltipOptions fancyOptions;
                    fancyOptions.maximumSize = {300.f, 104.f};
                    auto tooltip = Mosaic::itemTooltip(ui, fancy, Mosaic::Key("Fancy tooltip"), fancyOptions);

                    if(tooltip.visible() == true)
                    {
                        Mosaic::text(ui, "I am a fancy tooltip");
                        Mosaic::String sine = "Sin(time) = ";
                        sine += Detail::demoFixed(std::sin(Mosaic::input(ui).timestamp), 3);
                        Mosaic::text(ui, sine);
                        Mosaic::plotLines(ui, "Sin(time)", Mosaic::ConstFloatSpan(m_plotValues));
                    }
                }

                auto always = Mosaic::treeNode(ui, Mosaic::Key("tooltip always"), "Always On");

                if(always.expanded() == true)
                {
                    auto choices = Mosaic::row(ui);

                    if(Mosaic::radioButton(ui, "Off", m_tooltipAlways == 0).clicked() == true)
                    {
                        m_tooltipAlways = 0;
                    }

                    if(Mosaic::radioButton(ui, "Always On (Simple)", m_tooltipAlways == 1).clicked() == true)
                    {
                        m_tooltipAlways = 1;
                    }

                    if(Mosaic::radioButton(ui, "Always On (Advanced)", m_tooltipAlways == 2).clicked() == true)
                    {
                        m_tooltipAlways = 2;
                    }

                    if(m_tooltipAlways != 0)
                    {
                        auto tooltip = Mosaic::tooltip(ui, "Always following tooltip", m_tooltipAlways == 1 ? Mosaic::Vec2{220.f, 54.f} : Mosaic::Vec2{300.f, 100.f});

                        if(tooltip.visible() == true)
                        {
                            Mosaic::text(ui, "I am following you around.");

                            if(m_tooltipAlways == 2)
                            {
                                float progress = 0.5f + 0.5f * std::sin(static_cast<float>(Mosaic::input(ui).timestamp));
                                Mosaic::progressBar(ui, progress, "Advanced contents");
                            }
                        }
                    }
                }

                auto custom = Mosaic::treeNode(ui, Mosaic::Key("tooltip custom"), "Custom");

                if(custom.expanded() == true)
                {
                    Mosaic::Response manual = Mosaic::button(ui, "Manual");
                    Mosaic::itemTooltip(ui, manual, "I am a manually emitted tooltip.", {280.f, 64.f}, 0.25f);
                    Mosaic::Response none = Mosaic::button(ui, "DelayNone");
                    Mosaic::itemTooltip(ui, none, "I am a tooltip with no delay.", {260.f, 64.f}, 0.f);
                    Mosaic::Response shortDelay = Mosaic::button(ui, "DelayShort");
                    Mosaic::ItemTooltipOptions shortOptions;
                    shortOptions.maximumSize = {300.f, 64.f};
                    shortOptions.delay = 0.25f;
                    shortOptions.sharedDelay = false;
                    Mosaic::itemTooltip(ui, shortDelay, "I am a tooltip with a short delay (0.25 sec).", shortOptions);
                    Mosaic::Response longDelay = Mosaic::button(ui, "DelayLong");
                    Mosaic::ItemTooltipOptions longOptions;
                    longOptions.maximumSize = {300.f, 64.f};
                    longOptions.delay = 0.75f;
                    longOptions.sharedDelay = false;
                    Mosaic::itemTooltip(ui, longDelay, "I am a tooltip with a long delay (0.75 sec).", longOptions);
                    Mosaic::Response stationary = Mosaic::button(ui, "Stationary");
                    Mosaic::ItemTooltipOptions stationaryOptions;
                    stationaryOptions.maximumSize = {340.f, 82.f};
                    stationaryOptions.delay = 0.6f;
                    stationaryOptions.stationary = true;
                    Mosaic::itemTooltip(ui, stationary, "I am a tooltip requiring mouse to be stationary before activating.", stationaryOptions);
                    Mosaic::Response disabledResponse;
                    {
                        auto disabled = Mosaic::disabledScope(ui, true);
                        disabledResponse = Mosaic::button(ui, "Disabled item");
                    }
                    Mosaic::itemTooltip(ui, disabledResponse, "I am a tooltip for a disabled item.", {280.f, 64.f}, 0.25f);
                }
            }
        }
        {
            auto trees = Mosaic::treeNode(ui, Mosaic::Key("tree nodes"), "Tree Nodes");

            if(trees.expanded() == true)
            {
                auto basic = Mosaic::treeNode(ui, Mosaic::Key("basic trees"), "Basic Trees");

                if(basic.expanded() == true)
                {
                    for(size_t index = 0; index != 5; ++index)
                    {
                        auto childScope = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::String label = "Child ";
                        label += Detail::demoNumber(index);
                        Mosaic::TreeNodeOptions options;
                        options.defaultExpanded = index == 0;
                        auto node = Mosaic::treeNode(ui, Mosaic::Key("basic child"), label, options);

                        if(node.expanded() == true)
                        {
                            auto contents = Mosaic::row(ui);
                            Mosaic::text(ui, "blah blah");
                            Mosaic::smallButton(ui, "button");
                        }
                    }
                }

                auto hierarchy = Mosaic::treeNode(ui, Mosaic::Key("hierarchy lines"), "Hierarchy Lines");

                if(hierarchy.expanded() == true)
                {
                    static Mosaic::Array<bool, 3> lineFlags = {false, true, false};
                    Mosaic::helpMarker(ui, "Default option for DrawLinesXXX is stored in style.TreeLinesFlags");
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_DrawLinesNone", &lineFlags[0]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_DrawLinesFull", &lineFlags[1]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_DrawLinesToNodes", &lineFlags[2]);
                    Mosaic::TreeNodeOptions hierarchyOptions;
                    hierarchyOptions.defaultExpanded = true;
                    hierarchyOptions.lines = lineFlags[2] ? Mosaic::TreeLineMode::ToNodes : lineFlags[1] ? Mosaic::TreeLineMode::Full : Mosaic::TreeLineMode::None;

                    if(lineFlags[0])
                    {
                        hierarchyOptions.lines = Mosaic::TreeLineMode::None;
                    }

                    auto parent = Mosaic::treeNode(ui, Mosaic::Key("parent"), "Parent", hierarchyOptions);

                    if(parent.expanded() == true)
                    {
                        {
                            auto child = Mosaic::treeNode(ui, Mosaic::Key("child 1"), "Child 1", hierarchyOptions);

                            if(child.expanded() == true)
                            {
                                Mosaic::button(ui, "Button for Child 1");
                            }
                        }
                        {
                            auto child = Mosaic::treeNode(ui, Mosaic::Key("child 2"), "Child 2", hierarchyOptions);

                            if(child.expanded() == true)
                            {
                                Mosaic::button(ui, "Button for Child 2");
                            }
                        }
                        Mosaic::text(ui, "Remaining contents");
                        Mosaic::text(ui, "Remaining contents");
                    }
                }

                auto clipped = Mosaic::treeNode(ui, Mosaic::Key("large trees"), "Clipping Large Trees");

                if(clipped.expanded() == true)
                {
                    Mosaic::text(ui, "- Clipping trees is less straightforward than clipping arrays or grids.");
                    Mosaic::text(ui, "- See Examples > Property Editor for a practical implementation.");
                }

                auto selectableNodes = Mosaic::treeNode(ui, Mosaic::Key("selectable nodes"), "Selectable Nodes");

                if(selectableNodes.expanded() == true)
                {
                    for(size_t index = 0; index != 6; ++index)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::String label = index < 3 ? "Selectable Node " : "Selectable Leaf ";
                        label += Detail::demoNumber(index);
                        Mosaic::TreeNodeOptions options;
                        options.leaf = index >= 3;
                        options.selected = m_treeSelectableStates[index];
                        options.openOnArrow = true;
                        options.openOnDoubleClick = true;
                        options.spanAvailableWidth = true;
                        auto node = Mosaic::treeNode(ui, Mosaic::Key("node"), label, options);
                        Mosaic::Response response;
                        if(Mosaic::itemResponse(ui, node.id(), &response) == false)
                        {
                            continue;
                        }

                        if(response.clicked() == true && response.toggledOpen() == false)
                        {
                            const Mosaic::Modifiers & modifiers = Mosaic::input(ui).modifiers;
                            bool toggle = modifiers.primary || modifiers.control == true || modifiers.super;

                            if(toggle == false)
                            {
                                m_treeSelectableStates.fill(false);
                                m_treeSelectableStates[index] = true;
                            }
                            else
                            {
                                m_treeSelectableStates[index] = !m_treeSelectableStates[index];
                            }
                        }

                        if(node.expanded() == true)
                        {
                            Mosaic::bulletText(ui, "Blah blah");
                            Mosaic::bulletText(ui, "Blah Blah");
                        }
                    }
                }

                auto advanced = Mosaic::treeNode(ui, Mosaic::Key("advanced trees"), "Advanced");

                if(advanced.expanded() == true)
                {
                    static Mosaic::Array<bool, 16> flags = {};
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_OpenOnArrow", &flags[0]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_OpenOnDoubleClick", &flags[1]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_SpanAvailWidth", &flags[2]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_SpanFullWidth", &flags[3]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_SpanLabelWidth", &flags[4]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_SpanAllColumns", &flags[5]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_AllowOverlap", &flags[6]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_Framed", &flags[7]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_FramePadding", &flags[8]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_NavLeftJumpsToParent", &flags[9]);
                    Mosaic::helpMarker(ui, "Default option for DrawLinesXXX is stored in style.TreeLinesFlags");
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_DrawLinesNone", &flags[10]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_DrawLinesFull", &flags[11]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_Leaf", &flags[12]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_Bullet", &flags[13]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_Selected", &flags[14]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_AutoCloseChildNodes", &flags[15]);
                    Mosaic::checkbox(ui, "MosaicTreeNodeFlags_DrawLinesToNodes", &m_treeDrawLinesToNodes);
                    Mosaic::checkbox(ui, "Align label with current X position", &m_treeAlignLabel);
                    Mosaic::checkbox(ui, "Make Tree Nodes as drag & drop sources", &m_treeDragSource);
                    Mosaic::TreeNodeOptions nodeOptions;
                    nodeOptions.openOnArrow = flags[0];
                    nodeOptions.openOnDoubleClick = flags[1];
                    nodeOptions.spanAvailableWidth = flags[2];
                    nodeOptions.spanFullWidth = flags[3];
                    nodeOptions.spanLabelWidth = flags[4];
                    nodeOptions.spanAllColumns = flags[5];
                    nodeOptions.allowOverlap = flags[6];
                    nodeOptions.framed = flags[7];
                    nodeOptions.framePadding = flags[8];
                    nodeOptions.navigationLeftJumpsToParent = flags[9];
                    nodeOptions.leaf = flags[12];
                    nodeOptions.bullet = flags[13];
                    nodeOptions.selected = flags[14];
                    nodeOptions.autoCloseChildNodes = flags[15];
                    nodeOptions.lines = m_treeDrawLinesToNodes ? Mosaic::TreeLineMode::ToNodes : flags[11] ? Mosaic::TreeLineMode::Full : Mosaic::TreeLineMode::None;

                    if(flags[10])
                    {
                        nodeOptions.lines = Mosaic::TreeLineMode::None;
                    }

                    nodeOptions.alignLabelWithCurrentX = m_treeAlignLabel;
                    constexpr Mosaic::TypeId treeDragType = 0x30821f38a013ce41ULL;
                    auto beginTreeDrag = [ui](const Mosaic::Response & response)
                    {
                        Mosaic::Id id = response.id;
                        const auto * bytes = reinterpret_cast<const std::byte *>(&id);
                        (void)Mosaic::beginDragDropSource(ui, response, treeDragType, Mosaic::ByteSpan(bytes, sizeof(id)));
                    };
                    for(size_t index = 0; index != 6; ++index)
                    {
                        auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::String label = index < 3 ? "Selectable Node " : "Selectable Leaf ";
                        label += Detail::demoNumber(index);
                        Mosaic::TreeNodeOptions itemOptions = nodeOptions;
                        itemOptions.leaf = nodeOptions.leaf || index >= 3;
                        itemOptions.selected = nodeOptions.selected || m_treeAdvancedSelectableStates[index];
                        auto node = Mosaic::treeNode(ui, Mosaic::Key(index < 3 ? "advanced node" : "advanced leaf"), label, itemOptions);
                        Mosaic::Response nodeResponse;
                        if(Mosaic::itemResponse(ui, node.id(), &nodeResponse) == false)
                        {
                            continue;
                        }

                        if(m_treeDragSource == true)
                        {
                            beginTreeDrag(nodeResponse);
                            Mosaic::itemTooltip(ui, nodeResponse, "This is a drag and drop source");
                        }

                        if(itemOptions.leaf == true && nodeResponse.clicked() == true)
                        {
                            m_treeAdvancedSelectableStates[index] = !m_treeAdvancedSelectableStates[index];
                        }

                        if(node.expanded() == true)
                        {
                            Mosaic::bulletText(ui, "Blah blah\nBlah Blah");
                            Mosaic::button(ui, "Button");
                        }
                    }
                }
            }
        }

        {
            auto vertical = Mosaic::treeNode(ui, Mosaic::Key("vertical sliders"), "Vertical Sliders");

            if(vertical.expanded() == true)
            {
                auto sliderRow = Mosaic::row(ui);
                {
                    auto item = Mosaic::scope(ui, Mosaic::Key("integer"));
                    Mosaic::SliderOptions options;
                    options.showValueOnTrack = true;
                    Mosaic::verticalSlider(ui, {}, &m_verticalInteger, int32_t{0}, int32_t{5}, {18.f, 160.f}, options);
                }
                for(size_t index = 0; index != 7; ++index)
                {
                    auto item = Mosaic::scope(ui, Mosaic::Key(index));
                    Mosaic::Theme theme = Mosaic::getTheme(ui);
                    float hue = static_cast<float>(index) / 7.f;
                    theme.colors.frame = Detail::demoHsv(hue, 0.5f, 0.5f);
                    theme.colors.frameHovered = Detail::demoHsv(hue, 0.6f, 0.5f);
                    theme.colors.frameActive = Detail::demoHsv(hue, 0.7f, 0.5f);
                    theme.colors.sliderGrab = Detail::demoHsv(hue, 0.9f, 0.9f);
                    auto colors = Mosaic::styleScope(ui, theme);
                    Mosaic::SliderOptions options;
                    options.precision = 3;
                    options.showValueOnTrack = false;
                    options.showValueTooltip = true;
                    options.valueTooltipDelay = 0.f;
                    Mosaic::verticalSlider(ui, {}, &m_verticalSliders[index], 0.f, 1.f, {18.f, 160.f}, options);
                }
                for(size_t column = 0; column != 4; ++column)
                {
                    auto columnScope = Mosaic::scope(ui, Mosaic::Key(column));
                    auto group = Mosaic::column(ui);
                    for(size_t row = 0; row != 3; ++row)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(row));
                        Mosaic::SliderOptions options;
                        options.precision = 3;
                        options.showValueOnTrack = false;
                        options.showValueTooltip = true;
                        options.valueTooltipDelay = 0.f;
                        Mosaic::verticalSlider(ui, {}, &m_verticalSmallSliders[column], 0.f, 1.f, {18.f, 50.f}, options);
                    }
                }
                for(size_t index = 0; index != 4; ++index)
                {
                    auto item = Mosaic::scope(ui, Mosaic::Key(index));
                    Mosaic::Theme theme = Mosaic::getTheme(ui);
                    theme.metrics.grabMinimumSize = 40.f;
                    auto grabStyle = Mosaic::styleScope(ui, theme);
                    Mosaic::SliderOptions options;
                    options.precision = 2;
                    options.showValueOnTrack = true;
                    options.format = "%.2f\nsec";
                    Mosaic::verticalSlider(ui, {}, &m_verticalSliders[index], 0.f, 1.f, {40.f, 160.f}, options);
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawLayout(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Layout & Scrolling");

        if(section.expanded() == false)
        {
            return;
        }

        const Mosaic::Theme & layoutTheme = Mosaic::getTheme(ui);
        Mosaic::Rect layoutWindow;
        if(Mosaic::windowBounds(ui, Mosaic::currentWindow(ui), &layoutWindow) == false)
        {
            return;
        }

        float availableWidth = std::max(120.f, layoutWindow.width - layoutTheme.metrics.padding * 2.f - layoutTheme.metrics.scrollbarWidth - layoutTheme.metrics.indent * 2.f);
        {
            auto children = Mosaic::treeNode(ui, Mosaic::Key("child windows"), "Child windows");

            if(children.expanded() == true)
            {
                Mosaic::separatorText(ui, "Child windows");
                Mosaic::helpMarker(ui, "Child windows create independent scrolling and clipping regions inside a host window.");
                Mosaic::checkbox(ui, "Disable Mouse Wheel", &m_childDisableWheel);
                Mosaic::checkbox(ui, "Disable Menu", &m_childDisableMenu);
                Mosaic::LayoutOptions rowLayout;
                rowLayout.width = Mosaic::SizeRule::Fill;
                rowLayout.height = Mosaic::Dimension::fixed(260.f);
                {
                    auto row = Mosaic::row(ui, rowLayout);
                    {
                        Mosaic::ScrollOptions options;
                        options.axes = Mosaic::ScrollAxes::Both;

                        if(m_childDisableWheel == true)
                        {
                            options.wheelStep = 0.f;
                        }

                        auto child = Mosaic::scrollArea(ui, "ChildL", options, Detail::fillLayout());
                        for(size_t index = 0; index != 100; ++index)
                        {
                            Mosaic::String text = index < 10 ? "000" : index < 100 ? "00" : "0";
                            text += Detail::demoNumber(index);
                            text += ": scrollable region";
                            Mosaic::text(ui, text);
                        }
                    }
                    {
                        Mosaic::Theme childTheme = Mosaic::getTheme(ui);
                        childTheme.metrics.childCornerRadius = 5.f;
                        auto childStyle = Mosaic::styleScope(ui, childTheme);
                        Mosaic::ScrollOptions options;
                        options.axes = Mosaic::ScrollAxes::Vertical;
                        options.background = true;
                        options.framed = true;

                        if(m_childDisableWheel == true)
                        {
                            options.wheelStep = 0.f;
                        }

                        auto child = Mosaic::scrollArea(ui, "ChildR", options, Detail::fillLayout());

                        if(m_childDisableMenu == false)
                        {
                            {
                                auto bar = Mosaic::menuBar(ui);
                                {
                                    auto menu = Mosaic::menu(ui, "Menu");

                                    if(menu.expanded() == true)
                                    {
                                        HelloDemo::drawExampleMenuFile(ui);
                                    }
                                }
                            }
                        }

                        auto childTable = Mosaic::table(ui, "split", 2);
                        for(size_t index = 0; index != 100; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));

                            if(index % 2 == 0)
                            {
                                Mosaic::tableNextRow(ui);
                            }

                            if(Mosaic::tableSetColumn(ui, static_cast<uint32_t>(index % 2)) == true)
                            {
                                Mosaic::ButtonOptions buttonOptions;
                                buttonOptions.width = Mosaic::SizeRule::Fill;
                                Mosaic::button(ui, Mosaic::Key("child button"), Detail::demoNumber(index), buttonOptions);
                            }
                        }
                    }
                }

                Mosaic::separatorText(ui, "Manual-resize");
                Mosaic::helpMarker(ui, "Drag the bottom border to resize. Double-click it to auto-fit the child height to its contents.");
                Mosaic::LayoutOptions resizableLayout;
                resizableLayout.width = Mosaic::SizeRule::Fill;
                resizableLayout.height = Mosaic::Dimension::fixed(136.f);
                {
                    Mosaic::ScrollOptions manualOptions;
                    manualOptions.resizeY = true;
                    manualOptions.framed = true;
                    manualOptions.minimumSize = {120.f, 72.f};
                    manualOptions.maximumSize = {1200.f, 320.f};
                    auto manual = Mosaic::scrollArea(ui, "ResizableChild", manualOptions, resizableLayout);
                    for(size_t index = 0; index != 10; ++index)
                    {
                        Mosaic::String line = "Line 000";
                        line += Detail::demoNumber(index);
                        Mosaic::text(ui, line);
                    }
                }

                Mosaic::separatorText(ui, "Auto-resize with constraints");
                Mosaic::dragValue(ui, "Lines Count", &m_childLineCount, int32_t{0}, int32_t{100});
                Mosaic::dragValue(ui, "Max Height (in Lines)", &m_childMaxLines, int32_t{1}, int32_t{30});
                Mosaic::LayoutOptions constrainedLayout;
                constrainedLayout.width = Mosaic::SizeRule::Fill;
                {
                    float lineHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                    Mosaic::ScrollOptions constrainedOptions;
                    constrainedOptions.autoResizeY = true;
                    constrainedOptions.minimumSize = {120.f, lineHeight * 4.f};
                    constrainedOptions.maximumSize = {1200.f, lineHeight * static_cast<float>(m_childMaxLines) + 10.f};
                    auto constrained = Mosaic::scrollArea(ui, "ConstrainedChild", constrainedOptions, constrainedLayout);
                    for(int32_t index = 0; index < m_childLineCount; ++index)
                    {
                        Mosaic::String line = "Line 000";
                        line += Detail::demoNumber(index);
                        Mosaic::text(ui, line);
                    }
                }

                Mosaic::separatorText(ui, "Misc/Advanced");
                Mosaic::dragValue(ui, "Offset X", &m_childOffsetX, int32_t{-1000}, int32_t{1000});
                Mosaic::checkbox(ui, "Override ChildBg color", &m_childOverrideBackground);
                Mosaic::checkbox(ui, "MosaicChildFlags_Borders", &m_childBorders);
                Mosaic::checkbox(ui, "MosaicChildFlags_AlwaysUseWindowPadding", &m_childAlwaysPadding);
                Mosaic::checkbox(ui, "MosaicChildFlags_ResizeX", &m_childResizeX);
                Mosaic::checkbox(ui, "MosaicChildFlags_ResizeY", &m_childResizeY);
                Mosaic::checkbox(ui, "MosaicChildFlags_FrameStyle", &m_childFrameStyle);
                Mosaic::helpMarker(ui, "FrameStyle uses framed-widget colors, borders and padding for the child.");
                Mosaic::Theme childTheme = Mosaic::getTheme(ui);

                if(m_childOverrideBackground == true && m_childFrameStyle == false)
                {
                    childTheme.colors.background = Mosaic::Color{0.32f, 0.07f, 0.08f, 1.f};
                }

                auto childStyle = Mosaic::styleScope(ui, childTheme);
                Mosaic::LayoutOptions advancedLayout;
                advancedLayout.width = Mosaic::Dimension::fixed(200.f);
                advancedLayout.height = Mosaic::Dimension::fixed(100.f);

                if(m_childAlwaysPadding == true)
                {
                    advancedLayout.padding = Mosaic::EdgeInsets(Mosaic::getTheme(ui).metrics.padding);
                }

                advancedLayout.offset.x = static_cast<float>(m_childOffsetX);
                Mosaic::ScrollOptions advancedScroll;
                advancedScroll.axes = Mosaic::ScrollAxes::Both;
                advancedScroll.background = m_childOverrideBackground || m_childFrameStyle;
                advancedScroll.framed = m_childBorders || m_childFrameStyle;
                advancedScroll.frameStyle = m_childFrameStyle;
                advancedScroll.resizeX = m_childResizeX;
                advancedScroll.resizeY = m_childResizeY;
                advancedScroll.minimumSize = {96.f, 64.f};
                advancedScroll.maximumSize = {560.f, 320.f};
                Mosaic::Id advancedChildId = Mosaic::InvalidId;
                {
                    auto advancedChild = Mosaic::scrollArea(ui, "Red", advancedScroll, advancedLayout);
                    advancedChildId = advancedChild.id();
                    for(size_t index = 0; index != 50; ++index)
                    {
                        Mosaic::String value = "Some test ";
                        value += Detail::demoNumber(index);
                        Mosaic::text(ui, value);
                    }
                }
                Mosaic::Rect childBounds;
                if(Mosaic::debugBounds(ui, advancedChildId, &childBounds) == false)
                {
                    return;
                }

                Mosaic::Response childResponse;
                if(Mosaic::itemResponse(ui, advancedChildId, &childResponse) == false)
                {
                    return;
                }

                Mosaic::String childRect = "Hovered: ";
                childRect += childResponse.hovered() == true ? "1\nRect of child window is: (" : "0\nRect of child window is: (";
                childRect += Detail::demoFixed(childBounds.x, 0);
                childRect += ",";
                childRect += Detail::demoFixed(childBounds.y, 0);
                childRect += ") (";
                childRect += Detail::demoFixed(childBounds.right(), 0);
                childRect += ",";
                childRect += Detail::demoFixed(childBounds.bottom(), 0);
                childRect += ")";
                Mosaic::text(ui, childRect);
            }
        }
        {
            auto widths = Mosaic::treeNode(ui, Mosaic::Key("widgets width"), "Widgets Width");

            if(widths.expanded() == true)
            {
                Mosaic::checkbox(ui, "Show indented items", &m_widthShowIndented);
                auto widthSample = [this, ui](Mosaic::StringView description, float width, Mosaic::StringView suffix)
                {
                    Mosaic::text(ui, description);
                    Mosaic::helpMarker(ui, suffix);
                    Mosaic::setNextItemWidth(ui, width);
                    Mosaic::dragValue(ui, "float", &m_widthFloat);

                    if(m_widthShowIndented == true)
                    {
                        Mosaic::LayoutOptions indented;
                        indented.padding.left = Mosaic::getTheme(ui).metrics.indent;
                        auto indentScope = Mosaic::column(ui, indented);
                        Mosaic::setNextItemWidth(ui, width);
                        Mosaic::dragValue(ui, "float (indented)", &m_widthFloat);
                    }
                };
                widthSample("SetNextItemWidth/PushItemWidth(100)", 100.f, "Fixed width.");
                widthSample("SetNextItemWidth/PushItemWidth(-100)", -100.f, "Align to right edge minus 100.");
                widthSample("SetNextItemWidth/PushItemWidth(GetContentRegionAvail().x * 0.5f)", availableWidth * 0.5f, "Half of available width.");
                widthSample("SetNextItemWidth/PushItemWidth(-GetContentRegionAvail().x * 0.5f)", -availableWidth * 0.5f, "Align to right edge minus half.");
                widthSample("SetNextItemWidth/PushItemWidth(-Min(GetContentRegionAvail().x * 0.40f, GetFontSize() * 12))", -std::min(availableWidth * 0.4f, layoutTheme.metrics.fontSize * 12.f), "Bound a right-aligned field by a font-relative width.");
                widthSample("SetNextItemWidth/PushItemWidth(-FLT_MIN)", -std::numeric_limits<float>::min(), "Align to right edge.");
                Mosaic::text(ui, "PushItemWidth applies to consecutive widgets:");
                Mosaic::pushItemWidth(ui, 100.f);
                Mosaic::dragValue(ui, "first", &m_widthFloat);
                Mosaic::dragValue(ui, "second", &m_widthFloat);
                Mosaic::dragValue(ui, "third", &m_widthFloat);
                Mosaic::popItemWidth(ui);
            }
        }

        {
            auto horizontal = Mosaic::treeNode(ui, Mosaic::Key("horizontal layout"), "Basic Horizontal Layout");

            if(horizontal.expanded() == true)
            {
                Mosaic::text(ui, "Use row() to keep adding items to the right of the preceding item.");
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::text(ui, "Two items: Hello");
                    Mosaic::Theme yellow = Mosaic::getTheme(ui);
                    yellow.colors.text = Mosaic::Color{1.f, 1.f, 0.f, 1.f};
                    auto color = Mosaic::styleScope(ui, yellow);
                    Mosaic::text(ui, "Sailor");
                }
                {
                    Mosaic::LayoutOptions spacedLayout;
                    spacedLayout.gap = 20.f;
                    auto row = Mosaic::row(ui, spacedLayout);
                    Mosaic::text(ui, "More spacing: Hello");
                    Mosaic::text(ui, "Sailor");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::text(ui, "Normal buttons");
                    Mosaic::button(ui, "Banana");
                    Mosaic::button(ui, "Apple");
                    Mosaic::button(ui, "Corniflower");
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::text(ui, "Small buttons");
                    Mosaic::smallButton(ui, "Like this one");
                    Mosaic::text(ui, "can fit within a text block.");
                }
                {
                    Mosaic::TableOptions positionOptions;
                    positionOptions.headers = false;
                    positionOptions.resizable = false;
                    positionOptions.reorderable = false;
                    positionOptions.hideable = false;
                    positionOptions.saveSettings = false;
                    positionOptions.padInnerHorizontal = false;
                    positionOptions.extendHostHorizontal = true;
                    auto aligned = Mosaic::table(ui, "Positioned items", 3, positionOptions);
                    Mosaic::TableColumnOptions firstColumn;
                    firstColumn.sizing = Mosaic::TableSizing::Fixed;
                    firstColumn.widthOrWeight = 150.f;
                    Mosaic::TableColumnOptions secondColumn = firstColumn;
                    Mosaic::TableColumnOptions thirdColumn;
                    thirdColumn.sizing = Mosaic::TableSizing::Stretch;
                    Mosaic::tableSetupColumn(ui, 0, {}, firstColumn);
                    Mosaic::tableSetupColumn(ui, 1, {}, secondColumn);
                    Mosaic::tableSetupColumn(ui, 2, {}, thirdColumn);
                    Mosaic::tableNextRow(ui);

                    if(Mosaic::tableSetColumn(ui, 0) == true)
                    {
                        Mosaic::text(ui, "Aligned");
                    }

                    if(Mosaic::tableSetColumn(ui, 1) == true)
                    {
                        Mosaic::text(ui, "x=150");
                    }

                    if(Mosaic::tableSetColumn(ui, 2) == true)
                    {
                        Mosaic::text(ui, "x=300");
                    }

                    Mosaic::tableNextRow(ui);

                    if(Mosaic::tableSetColumn(ui, 0) == true)
                    {
                        Mosaic::text(ui, "Aligned");
                    }

                    if(Mosaic::tableSetColumn(ui, 1) == true)
                    {
                        Mosaic::button(ui, "x=150");
                    }

                    if(Mosaic::tableSetColumn(ui, 2) == true)
                    {
                        Mosaic::button(ui, "x=300");
                    }
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "My", &m_richTextChecks[0]);
                    Mosaic::checkbox(ui, "Tailor", &m_richTextChecks[1]);
                    Mosaic::checkbox(ui, "Is", &m_richTextChecks[2]);
                    Mosaic::checkbox(ui, "Rich", &m_richTextChecks[3]);
                }
                {
                    constexpr Mosaic::Array<Mosaic::StringView, 4> items = {"AAAA", "BBBB", "CCCC", "DDDD"};
                    auto row = Mosaic::row(ui);
                    Mosaic::ComboOptions compact;
                    compact.width = Mosaic::Dimension::fixed(90.f);
                    Mosaic::comboBox(ui, "Combo", &m_layoutCombo, items, compact);
                    Mosaic::slider(ui, "X", &m_vector[0], 0.f, 5.f);
                    Mosaic::slider(ui, "Y", &m_vector[1], 0.f, 5.f);
                    Mosaic::slider(ui, "Z", &m_vector[2], 0.f, 5.f);
                }
                Mosaic::text(ui, "Lists:");
                {
                    constexpr Mosaic::Array<Mosaic::StringView, 4> items = {"AAAA", "BBBB", "CCCC", "DDDD"};
                    auto row = Mosaic::row(ui);
                    for(size_t index = 0; index != 4; ++index)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::LayoutOptions listLayout;
                        listLayout.width = Mosaic::Dimension::fixed(92.f);
                        listLayout.height = Mosaic::Dimension::fixed(92.f);
                        Mosaic::listBox(ui, {}, &m_integerVector[index], items, listLayout);
                    }
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::ButtonOptions square;
                    square.width = Mosaic::Dimension::fixed(40.f);
                    Mosaic::button(ui, Mosaic::Key("square a"), "A", square);
                    Mosaic::spacer(ui, 40.f);
                    Mosaic::button(ui, Mosaic::Key("square b"), "B", square);
                }
                Mosaic::text(ui, "Manual wrapping:");
                constexpr size_t itemCount = 20;
                constexpr float itemWidth = 40.f;
                float itemStep = itemWidth + layoutTheme.metrics.itemSpacing.x;
                size_t itemsPerRow = std::max(size_t{1}, static_cast<size_t>((availableWidth + layoutTheme.metrics.itemSpacing.x) / itemStep));
                for(size_t begin = 0; begin < itemCount; begin += itemsPerRow)
                {
                    auto wrapRow = Mosaic::row(ui);
                    size_t end = std::min(itemCount, begin + itemsPerRow);
                    for(size_t index = begin; index != end; ++index)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::ButtonOptions box;
                        box.width = Mosaic::Dimension::fixed(itemWidth);
                        Mosaic::button(ui, Mosaic::Key("box"), "Box", box);
                    }
                }
            }
        }
        {
            auto groups = Mosaic::treeNode(ui, Mosaic::Key("groups"), "Groups");

            if(groups.expanded() == true)
            {
                Mosaic::helpMarker(ui, "A nested row or column bundles its contents as one layout item.");
                auto row = Mosaic::row(ui);
                {
                    auto group = Mosaic::column(ui);
                    {
                        auto top = Mosaic::row(ui);
                        Mosaic::button(ui, "AAA");
                        Mosaic::button(ui, "BBB");
                        auto middle = Mosaic::column(ui);
                        Mosaic::button(ui, "CCC");
                        Mosaic::button(ui, "DDD");
                    }
                    Mosaic::button(ui, "EEE");
                    constexpr Mosaic::Array<float, 5> values = {0.5f, 0.2f, 0.8f, 0.6f, 0.25f};
                    Mosaic::PlotOptions histogram;
                    histogram.width = Mosaic::Dimension::fixed(260.f);
                    histogram.height = 72.f;
                    Mosaic::plotHistogram(ui, {}, values, histogram);
                    auto actions = Mosaic::row(ui);
                    Mosaic::button(ui, "ACTION");
                    Mosaic::button(ui, "REACTION");
                }
                Mosaic::button(ui, "LEVERAGE\nBUZZWORD");
                constexpr Mosaic::Array<Mosaic::StringView, 2> listItems = {"Selected", "Not Selected"};
                Mosaic::LayoutOptions listLayout;
                listLayout.width = Mosaic::Dimension::fixed(130.f);
                listLayout.height = Mosaic::Dimension::fixed(110.f);
                Mosaic::listBox(ui, "List", &m_list, listItems, listLayout);
            }
        }
        {
            auto baseline = Mosaic::treeNode(ui, Mosaic::Key("baseline"), "Text Baseline Alignment");

            if(baseline.expanded() == true)
            {
                Mosaic::LayoutOptions unadjustedRow;
                unadjustedRow.crossAxisAlignment = Mosaic::CrossAxisAlignment::Start;
                Mosaic::LayoutOptions baselineRow;
                baselineRow.crossAxisAlignment = Mosaic::CrossAxisAlignment::Baseline;
                Mosaic::bulletText(ui, "Text baseline:");
                Mosaic::helpMarker(ui, "Text and framed widgets share the line baseline while retaining their own padding.");
                {
                    auto row = Mosaic::row(ui, unadjustedRow);
                    Mosaic::text(ui, "KO Blahblah");
                    Mosaic::button(ui, "Some framed item");
                    Mosaic::helpMarker(ui, "Unadjusted baseline sample.");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::text(ui, "OK Blahblah");
                    Mosaic::button(ui, Mosaic::Key("some framed item 2"), "Some framed item");
                    Mosaic::helpMarker(ui, "Mosaic rows align their children to one baseline.");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::button(ui, Mosaic::Key("test 1"), "TEST");
                    Mosaic::text(ui, "TEST");
                    Mosaic::button(ui, Mosaic::Key("test 2"), "TEST");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::text(ui, "Text aligned to framed item");
                    Mosaic::button(ui, Mosaic::Key("item 1"), "Item");
                    Mosaic::text(ui, "Item");
                    Mosaic::button(ui, Mosaic::Key("item 2"), "Item");
                    Mosaic::button(ui, Mosaic::Key("item 3"), "Item");
                }
                Mosaic::spacer(ui, 2.f);
                Mosaic::bulletText(ui, "Multi-line text:");
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::text(ui, "One\nTwo\nThree");
                    Mosaic::text(ui, "Hello\nWorld");
                    Mosaic::text(ui, "Banana");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::button(ui, Mosaic::Key("hop 1"), "HOP");
                    Mosaic::text(ui, "Banana");
                    Mosaic::text(ui, "Hello\nWorld");
                    Mosaic::text(ui, "Banana");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::button(ui, Mosaic::Key("hop 2"), "HOP");
                    Mosaic::text(ui, "Hello\nWorld");
                    Mosaic::text(ui, "Banana");
                }
                Mosaic::spacer(ui, 2.f);
                Mosaic::bulletText(ui, "Misc items:");
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::ButtonOptions large;
                    large.width = Mosaic::Dimension::fixed(80.f);
                    large.height = Mosaic::Dimension::fixed(80.f);
                    Mosaic::button(ui, Mosaic::Key("80x80"), "80x80", large);
                    large.width = Mosaic::Dimension::fixed(50.f);
                    large.height = Mosaic::Dimension::fixed(50.f);
                    Mosaic::button(ui, Mosaic::Key("50x50"), "50x50", large);
                    Mosaic::button(ui, "Button()");
                    Mosaic::smallButton(ui, "SmallButton()");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::button(ui, Mosaic::Key("baseline button 1"), "Button");
                    auto firstNode = Mosaic::treeNode(ui, Mosaic::Key("baseline node 1"), "Node");

                    if(firstNode.expanded() == true)
                    {
                        for(size_t index = 0; index != 6; ++index)
                        {
                            Mosaic::String item = "Item ";
                            item += Detail::demoNumber(index);
                            item += "..";
                            Mosaic::bulletText(ui, item);
                        }
                    }
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::button(ui, Mosaic::Key("baseline button 2"), "Button");
                    (void)Mosaic::treeNode(ui, Mosaic::Key("baseline node 2"), "Node");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    auto node = Mosaic::treeNode(ui, Mosaic::Key("baseline node 3"), "Node");
                    Mosaic::button(ui, Mosaic::Key("baseline button 3"), "Button");

                    if(node.expanded() == true)
                    {
                        for(size_t index = 0; index != 6; ++index)
                        {
                            Mosaic::String item = "Item ";
                            item += Detail::demoNumber(index);
                            item += "..";
                            Mosaic::bulletText(ui, item);
                        }
                    }
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::button(ui, Mosaic::Key("baseline button 4"), "Button");
                    Mosaic::bulletText(ui, "Bullet text");
                }
                {
                    auto row = Mosaic::row(ui, baselineRow);
                    Mosaic::bulletText(ui, "Node");
                    Mosaic::button(ui, Mosaic::Key("baseline button 5"), "Button");
                }
            }
        }
        {
            auto scrolling = Mosaic::treeNode(ui, Mosaic::Key("scrolling"), "Scrolling");

            if(scrolling.expanded() == true)
            {
                Mosaic::helpMarker(ui, "Use scrollTo(), scrollToItem() and scrollOffset() to control a scrolling region.");
                Mosaic::checkbox(ui, "Decoration", &m_scrollDecoration);
                bool applyScrollOffset = false;
                bool applyScrollPosition = false;
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::Response trackItem = Mosaic::dragValue(ui, {}, &m_scrollTrackItem, int32_t{0}, int32_t{99});

                    if(trackItem.changed() == true)
                    {
                        m_scrollTrack = true;
                    }

                    Mosaic::checkbox(ui, "Track", &m_scrollTrack);
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::Response offset = Mosaic::dragValue(ui, {}, &m_scrollOffsetRequest);

                    if(offset.changed() == true || Mosaic::button(ui, "Scroll Offset").clicked() == true)
                    {
                        m_scrollTrack = false;
                        applyScrollOffset = true;
                    }
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::Response position = Mosaic::dragValue(ui, {}, &m_scrollPositionRequest);

                    if(position.changed() == true || Mosaic::button(ui, "Scroll To Pos").clicked() == true)
                    {
                        m_scrollTrack = false;
                        applyScrollPosition = true;
                    }
                }

                constexpr Mosaic::Array<Mosaic::StringView, 5> anchors = {"Top", "25%", "Center", "75%", "Bottom"};
                {
                    Mosaic::LayoutOptions gridLayout;
                    gridLayout.width = Mosaic::SizeRule::Fill;
                    auto verticalGrid = Mosaic::grid(ui, 5, gridLayout);
                    for(size_t columnIndex = 0; columnIndex != anchors.size(); ++columnIndex)
                    {
                        auto columnScope = Mosaic::scope(ui, Mosaic::Key(columnIndex));
                        Mosaic::LayoutOptions columnLayout;
                        columnLayout.width = Mosaic::SizeRule::Fill;
                        auto column = Mosaic::column(ui, columnLayout);
                        Mosaic::text(ui, anchors[columnIndex]);
                        Mosaic::LayoutOptions childLayout;
                        childLayout.width = Mosaic::SizeRule::Fill;
                        childLayout.height = Mosaic::Dimension::fixed(200.f);
                        childLayout.padding = Mosaic::EdgeInsets{8.f, 2.f};
                        childLayout.gap = 2.f;
                        Mosaic::Id childId = Mosaic::InvalidId;
                        {
                            Mosaic::ScrollOptions options;
                            options.axes = Mosaic::ScrollAxes::Vertical;
                            options.framed = true;
                            options.visibility = m_scrollDecoration ? Mosaic::ScrollbarVisibility::Always : Mosaic::ScrollbarVisibility::Automatic;
                            auto child = Mosaic::scrollArea(ui, "vertical child", options, childLayout);
                            childId = child.id();
                            float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                            Mosaic::VisibleRange range;
                            (void)Mosaic::beginListClipper(ui, 100, itemHeight, &range, childId);
                            for(size_t itemIndex = range.begin; itemIndex != range.end; ++itemIndex)
                            {
                                auto item = Mosaic::scope(ui, Mosaic::Key(itemIndex));
                                Mosaic::String itemLabel = "Item ";
                                itemLabel += Detail::demoNumber(itemIndex);

                                if(m_scrollTrack == true && itemIndex == static_cast<size_t>(m_scrollTrackItem))
                                {
                                    Mosaic::Theme trackedTheme = Mosaic::getTheme(ui);
                                    trackedTheme.colors.text = Mosaic::Color{1.f, 1.f, 0.f, 1.f};
                                    auto tracked = Mosaic::styleScope(ui, trackedTheme);
                                    Mosaic::text(ui, itemLabel);
                                }
                                else
                                {
                                    Mosaic::text(ui, itemLabel);
                                }
                            }
                            Mosaic::endListClipper(ui, range, 100, itemHeight);
                        }

                        if(m_scrollTrack == true)
                        {
                            float anchor = static_cast<float>(columnIndex) / 4.f;
                            float itemStride = Mosaic::getTheme(ui).metrics.lineHeight + 2.f;
                            Mosaic::scrollToPosition(ui, childId, {0.f, static_cast<float>(m_scrollTrackItem) * itemStride + anchor * Mosaic::getTheme(ui).metrics.lineHeight}, {0.f, anchor});
                        }
                        else if(applyScrollOffset == true)
                        {
                            Mosaic::Vec2 range;
                            if(Mosaic::scrollRange(ui, childId, &range) == true)
                            {
                                Mosaic::scrollTo(ui, childId, std::clamp(m_scrollOffsetRequest, 0.f, range.y));
                            }
                        }
                        else if(applyScrollPosition == true)
                        {
                            float anchor = static_cast<float>(columnIndex) / 4.f;
                            Mosaic::scrollToPosition(ui, childId, {0.f, m_scrollPositionRequest}, {0.f, anchor});
                        }

                        Mosaic::Vec2 offset;
                        Mosaic::Vec2 range;
                        (void)Mosaic::scrollOffset(ui, childId, &offset);
                        (void)Mosaic::scrollRange(ui, childId, &range);
                        Mosaic::String status = Detail::demoFixed(offset.y, 0);
                        status += "/";
                        status += Detail::demoFixed(range.y, 0);
                        Mosaic::text(ui, status);
                    }
                }

                Mosaic::spacer(ui, 2.f);
                Mosaic::helpMarker(ui, "Horizontal scrolling uses the same API and independent X offset.");
                constexpr Mosaic::Array<Mosaic::StringView, 5> horizontalAnchors = {"Left", "25%", "Center", "75%", "Right"};
                for(size_t childIndex = 0; childIndex != horizontalAnchors.size(); ++childIndex)
                {
                    auto childScope = Mosaic::scope(ui, Mosaic::Key(childIndex));
                    Mosaic::LayoutOptions rowLayout;
                    rowLayout.width = Mosaic::SizeRule::Fill;
                    auto row = Mosaic::row(ui, rowLayout);
                    Mosaic::LayoutOptions childLayout;
                    childLayout.width = Mosaic::SizeRule::Fill;
                    childLayout.height = Mosaic::Dimension::fixed(48.f);
                    childLayout.padding = Mosaic::EdgeInsets{8.f, 4.f};
                    Mosaic::Id childId = Mosaic::InvalidId;
                    {
                        Mosaic::ScrollOptions options;
                        options.axes = Mosaic::ScrollAxes::Horizontal;
                        options.framed = true;
                        auto child = Mosaic::scrollArea(ui, "horizontal child", options, childLayout);
                        childId = child.id();
                        auto itemRow = Mosaic::row(ui);
                        Mosaic::ListClipperOptions clipperOptions;
                        clipperOptions.orientation = Mosaic::Orientation::Horizontal;
                        Mosaic::VisibleRange visibleItems;
                        (void)Mosaic::beginListClipper(ui, 100, 58.f, &visibleItems, childId, clipperOptions);
                        for(size_t itemIndex = visibleItems.begin; itemIndex != visibleItems.end; ++itemIndex)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(itemIndex));
                            Mosaic::String label = "Item ";
                            label += Detail::demoNumber(itemIndex);
                            Mosaic::TextOptions itemOptions;
                            itemOptions.layout.width = Mosaic::Dimension::fixed(58.f);

                            if(m_scrollTrack == true && itemIndex == static_cast<size_t>(m_scrollTrackItem))
                            {
                                Mosaic::Theme trackedTheme = Mosaic::getTheme(ui);
                                trackedTheme.colors.text = Mosaic::Color{1.f, 1.f, 0.f, 1.f};
                                auto tracked = Mosaic::styleScope(ui, trackedTheme);
                                Mosaic::text(ui, label, itemOptions);
                            }
                            else
                            {
                                Mosaic::text(ui, label, itemOptions);
                            }
                        }
                        Mosaic::endListClipper(ui, visibleItems, 100, 58.f, clipperOptions);
                    }

                    if(m_scrollTrack == true)
                    {
                        float anchor = static_cast<float>(childIndex) / 4.f;
                        float itemStride = 58.f + Mosaic::getTheme(ui).metrics.itemSpacing.x;
                        Mosaic::scrollToPosition(ui, childId, {static_cast<float>(m_scrollTrackItem) * itemStride + anchor * 58.f, 0.f}, {anchor, 0.f});
                    }
                    else if(applyScrollOffset == true)
                    {
                        Mosaic::Vec2 range;
                        if(Mosaic::scrollRange(ui, childId, &range) == true)
                        {
                            Mosaic::scrollTo(ui, childId, {std::clamp(m_scrollOffsetRequest, 0.f, range.x), 0.f});
                        }
                    }
                    else if(applyScrollPosition == true)
                    {
                        float anchor = static_cast<float>(childIndex) / 4.f;
                        Mosaic::scrollToPosition(ui, childId, {m_scrollPositionRequest, 0.f}, {anchor, 0.f});
                    }

                    Mosaic::LayoutOptions statusLayout;
                    statusLayout.width = Mosaic::Dimension::fixed(72.f);
                    auto statusColumn = Mosaic::column(ui, statusLayout);
                    Mosaic::Vec2 offset;
                    Mosaic::Vec2 range;
                    (void)Mosaic::scrollOffset(ui, childId, &offset);
                    (void)Mosaic::scrollRange(ui, childId, &range);
                    Mosaic::String status = Mosaic::String(horizontalAnchors[childIndex]);
                    status += "\n";
                    status += Detail::demoFixed(offset.x, 0);
                    status += "/";
                    status += Detail::demoFixed(range.x, 0);
                    Mosaic::text(ui, status);
                }

                Mosaic::SliderOptions linesOptions;
                linesOptions.width = Mosaic::SizeRule::Fill;
                linesOptions.minimum = 1.0;
                linesOptions.maximum = 15.0;
                linesOptions.precision = 0;
                linesOptions.labelPlacement = Mosaic::LabelPlacement::After;
                Mosaic::slider(ui, "Lines", &m_scrollLines, int32_t{1}, int32_t{15}, linesOptions);
                Mosaic::LayoutOptions timelineLayout;
                timelineLayout.width = Mosaic::SizeRule::Fill;
                timelineLayout.height = Mosaic::Dimension::fixed(205.f);
                timelineLayout.padding = Mosaic::EdgeInsets{8.f};
                Mosaic::Array<Mosaic::Theme, 20> timelineButtonThemes = {};
                for(size_t index = 0; index != timelineButtonThemes.size(); ++index)
                {
                    timelineButtonThemes[index] = Mosaic::getTheme(ui);
                    timelineButtonThemes[index].colors.button = Detail::demoHsv(static_cast<float>(index) * 0.05f, 0.6f, 0.6f);
                }
                Mosaic::Id timelineId = Mosaic::InvalidId;
                {
                    Mosaic::ScrollOptions timelineOptions;
                    timelineOptions.axes = Mosaic::ScrollAxes::Horizontal;
                    timelineOptions.framed = true;
                    auto timeline = Mosaic::scrollArea(ui, "scrolling", timelineOptions, timelineLayout);
                    timelineId = timeline.id();
                    for(int32_t lineIndex = 0; lineIndex < m_scrollLines; ++lineIndex)
                    {
                        auto lineScope = Mosaic::scope(ui, Mosaic::Key(lineIndex));
                        auto line = Mosaic::row(ui);
                        int32_t count = 10 + ((lineIndex & 1) != 0 ? lineIndex * 9 : lineIndex * 3);
                        for(int32_t index = 0; index < count; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            const Mosaic::Theme & colorTheme = timelineButtonThemes[static_cast<size_t>(index) % timelineButtonThemes.size()];
                            auto color = Mosaic::styleScope(ui, colorTheme);
                            Mosaic::String label = index % 15 == 0 ? "FizzBuzz" : index % 3 == 0 ? "Fizz" : index % 5 == 0 ? "Buzz" : Detail::demoNumber(index);
                            float baseWidth = Mosaic::getTheme(ui).metrics.fontSize * 3.f;
                            Mosaic::ButtonOptions buttonOptions;
                            buttonOptions.width = Mosaic::Dimension::fixed(baseWidth + std::sin(static_cast<float>(lineIndex + index)) * baseWidth * 0.5f);
                            Mosaic::button(ui, Mosaic::Key("timeline button"), label, buttonOptions);
                        }
                    }
                }
                {
                    auto controls = Mosaic::row(ui);

                    if(Mosaic::button(ui, "<<").active() == true)
                    {
                        Mosaic::Vec2 offset;
                        if(Mosaic::scrollOffset(ui, timelineId, &offset) == true)
                        {
                            offset.x -= m_deltaTime * 1000.f;
                            Mosaic::scrollTo(ui, timelineId, offset);
                        }
                    }

                    Mosaic::text(ui, "Scroll from code");

                    if(Mosaic::button(ui, ">>").active() == true)
                    {
                        Mosaic::Vec2 offset;
                        if(Mosaic::scrollOffset(ui, timelineId, &offset) == true)
                        {
                            offset.x += m_deltaTime * 1000.f;
                            Mosaic::scrollTo(ui, timelineId, offset);
                        }
                    }

                    Mosaic::Vec2 offset;
                    Mosaic::Vec2 range;
                    (void)Mosaic::scrollOffset(ui, timelineId, &offset);
                    (void)Mosaic::scrollRange(ui, timelineId, &range);
                    Mosaic::String status = Detail::demoFixed(offset.x, 0);
                    status += "/";
                    status += Detail::demoFixed(range.x, 0);
                    Mosaic::text(ui, status);
                }
                Mosaic::checkbox(ui, "Show Horizontal contents size demo window", &m_showHorizontalContentsSizeWindow);
            }
        }
        {
            auto clipping = Mosaic::treeNode(ui, Mosaic::Key("text clipping"), "Text Clipping");

            if(clipping.expanded() == true)
            {
                static Mosaic::Array<float, 2> clippingSize = {100.f, 100.f};
                static Mosaic::Array<float, 2> clippingOffset = {30.f, 30.f};
                Mosaic::vectorEditor(ui, "Size", Mosaic::FloatSpan(clippingSize.data(), clippingSize.size()), 1.f, 200.f);
                Mosaic::text(ui, "(Click and drag to scroll)");
                Mosaic::helpMarker(ui, "Left: the canvas/container clip affects both interaction and rendering.\n\n"
                                       "Center: the Canvas clip stack affects subsequent drawing commands.\n\n"
                                       "Right: textClipped() limits only that text command without changing "
                                       "the surrounding canvas clip.");

                auto drawClippingSample = [ui](Mosaic::StringView identity, uint8_t mode)
                {
                    auto sampleScope = Mosaic::scope(ui, Mosaic::Key(identity));
                    Mosaic::LayoutOptions canvasLayout;
                    canvasLayout.width = Mosaic::Dimension::fixed(clippingSize[0]);
                    canvasLayout.height = Mosaic::Dimension::fixed(clippingSize[1]);
                    auto canvas = Mosaic::canvas(ui, "clip canvas", canvasLayout);
                    const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();

                    if(pointer != nullptr && pointer->isDown() == true && Mosaic::capturedPointerOwner(ui) == canvas.id())
                    {
                        clippingOffset[0] += pointer->delta.x;
                        clippingOffset[1] += pointer->delta.y;
                    }

                    Mosaic::Rect canvasBounds;
                    if(canvas.contentRect(&canvasBounds) == false)
                    {
                        return;
                    }

                    Mosaic::Rect clipBounds = {0.f, 0.f, canvasBounds.width, canvasBounds.height};
                    const Mosaic::Theme & theme = Mosaic::getTheme(ui);
                    canvas.rect(clipBounds, theme.colors.frameHovered);
                    Mosaic::Vec2 position = {clipBounds.x + clippingOffset[0], clipBounds.y + clippingOffset[1]};
                    constexpr Mosaic::StringView sample = "Line 1 hello\nLine 2 clip me!";
                    Mosaic::Vec2 textSize;

                    if(mode == 0)
                    {
                        (void)canvas.text(position, sample, theme.colors.text, &textSize);
                    }
                    else if(mode == 1)
                    {
                        canvas.pushClip(clipBounds);
                        (void)canvas.text(position, sample, theme.colors.text, &textSize);
                        canvas.popClip();
                    }
                    else
                    {
                        (void)canvas.textClipped(position, sample, theme.colors.text, clipBounds, &textSize);
                    }

                    Mosaic::Array<Mosaic::Vec2, 5> border = {{{clipBounds.x, clipBounds.y}, {clipBounds.right(), clipBounds.y}, {clipBounds.right(), clipBounds.bottom()}, {clipBounds.x, clipBounds.bottom()}, {clipBounds.x, clipBounds.y}}};
                    canvas.polyline(Mosaic::Vec2Span(border), 1.f, theme.colors.borderStrong);
                };
                {
                    auto samples = Mosaic::row(ui);
                    drawClippingSample("container clip", 0);
                    drawClippingSample("draw clip", 1);
                    drawClippingSample("fine clip", 2);
                }
            }
        }

        {
            auto overlap = Mosaic::treeNode(ui, Mosaic::Key("overlap mode"), "Overlap Mode");

            if(overlap.expanded() == true)
            {
                Mosaic::helpMarker(ui, "Hit-testing is performed in submission order. AllowOverlap permits a later "
                                       "item to share an earlier item's visual bounds.");
                Mosaic::checkbox(ui, "Enable AllowOverlap", &m_overlapEnabled);
                {
                    Mosaic::LayoutOptions layout;
                    layout.width = Mosaic::Dimension::fixed(130.f);
                    layout.height = Mosaic::Dimension::fixed(130.f);
                    auto layered = Mosaic::overlay(ui, layout);
                    Mosaic::ButtonOptions firstButton;
                    firstButton.width = Mosaic::Dimension::fixed(80.f);
                    firstButton.height = Mosaic::Dimension::fixed(80.f);
                    firstButton.allowOverlap = m_overlapEnabled;
                    Mosaic::button(ui, Mosaic::Key("overlap button 1"), "Button 1", firstButton);
                    Mosaic::LayoutOptions secondPosition;
                    secondPosition.absoluteRect = {50.f, 50.f, 80.f, 80.f};
                    auto positioned = Mosaic::absolute(ui, secondPosition);
                    Mosaic::ButtonOptions secondButton;
                    secondButton.width = Mosaic::Dimension::fixed(80.f);
                    secondButton.height = Mosaic::Dimension::fixed(80.f);
                    Mosaic::button(ui, Mosaic::Key("overlap button 2"), "Button 2", secondButton);
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::SelectableOptions selectableOptions;
                    selectableOptions.allowOverlap = m_overlapEnabled;
                    Mosaic::selectable(ui, Mosaic::Key("overlap selectable"), "Some Selectable", false, selectableOptions);
                    Mosaic::smallButton(ui, "++");
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawPopups(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Popups & Modal windows");

        if(section.expanded() == false)
        {
            return;
        }

        {
            auto popups = Mosaic::treeNode(ui, Mosaic::Key("popups"), "Popups");

            if(popups.expanded() == true)
            {
                Mosaic::text(ui, "When a popup is active it blocks interaction behind it. Clicking outside or pressing "
                                 "Escape closes a non-modal popup.");
                constexpr Mosaic::Array<Mosaic::StringView, 5> fish = {"Bream", "Haddock", "Mackerel", "Pollock", "Tilefish"};
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::Response button = Mosaic::button(ui, "Select..");
                    Mosaic::text(ui, m_popupSelectedFish < 0 ? "<None>" : fish[static_cast<size_t>(m_popupSelectedFish)]);
                    Mosaic::PopupOptions options;
                    options.owner = button.id;
                    (void)Mosaic::debugBounds(ui, button.id, &options.anchor);
                    options.placement = Mosaic::PopupPlacement::Below;
                    options.minimumSize = {160.f, 0.f};

                    if(button.clicked() == true)
                    {
                        Mosaic::openPopup(ui, Mosaic::Key("my_select_popup"), options);
                    }

                    auto popup = Mosaic::popup(ui, Mosaic::Key("my_select_popup"), options);

                    if(popup.visible() == true)
                    {
                        Mosaic::separatorText(ui, "Aquarium");
                        for(size_t index = 0; index != fish.size(); ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));

                            if(Mosaic::selectable(ui, Mosaic::Key("fish"), fish[index], m_popupSelectedFish == static_cast<int>(index)).clicked() == true)
                            {
                                m_popupSelectedFish = static_cast<int>(index);
                            }
                        }
                    }
                }
                {
                    Mosaic::Response button = Mosaic::button(ui, "Toggle..");
                    Mosaic::PopupOptions options;
                    options.owner = button.id;
                    (void)Mosaic::debugBounds(ui, button.id, &options.anchor);
                    options.placement = Mosaic::PopupPlacement::Below;
                    options.closeOnSelection = false;

                    if(button.clicked() == true)
                    {
                        Mosaic::openPopup(ui, Mosaic::Key("my_toggle_popup"), options);
                    }

                    auto popup = Mosaic::popup(ui, Mosaic::Key("my_toggle_popup"), options);

                    if(popup.visible() == true)
                    {
                        for(size_t index = 0; index != fish.size(); ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::MenuItemOptions itemOptions;
                            itemOptions.checked = &m_popupToggles[index];
                            itemOptions.closeOnActivate = false;
                            Mosaic::menuItem(ui, fish[index], itemOptions);
                        }
                        {
                            auto subMenu = Mosaic::menu(ui, "Sub-menu");

                            if(subMenu.expanded() == true)
                            {
                                Mosaic::menuItem(ui, "Click me");
                            }
                        }
                        Mosaic::separator(ui);
                        Mosaic::Response tooltipText = Mosaic::text(ui, "Tooltip here");
                        Mosaic::itemTooltip(ui, tooltipText, "I am a tooltip over a popup");
                        Mosaic::Response stackedButton = Mosaic::button(ui, "Stacked Popup");
                        Mosaic::PopupOptions stackedOptions;
                        stackedOptions.owner = stackedButton.id;
                        (void)Mosaic::debugBounds(ui, stackedButton.id, &stackedOptions.anchor);
                        stackedOptions.placement = Mosaic::PopupPlacement::Right;
                        stackedOptions.closeOnSelection = false;

                        if(stackedButton.clicked() == true)
                        {
                            Mosaic::openPopup(ui, Mosaic::Key("another popup"), stackedOptions);
                        }

                        auto stacked = Mosaic::popup(ui, Mosaic::Key("another popup"), stackedOptions);

                        if(stacked.visible() == true)
                        {
                            for(size_t index = 0; index != fish.size(); ++index)
                            {
                                auto item = Mosaic::scope(ui, Mosaic::Key(index));
                                Mosaic::MenuItemOptions itemOptions;
                                itemOptions.checked = &m_popupToggles[index];
                                itemOptions.closeOnActivate = false;
                                Mosaic::menuItem(ui, fish[index], itemOptions);
                            }
                            auto subMenu = Mosaic::menu(ui, "Sub-menu");

                            if(subMenu.expanded() == true)
                            {
                                Mosaic::menuItem(ui, "Click me");
                                Mosaic::Response nestedButton = Mosaic::button(ui, "Stacked Popup");
                                Mosaic::PopupOptions nestedOptions;
                                nestedOptions.owner = nestedButton.id;
                                (void)Mosaic::debugBounds(ui, nestedButton.id, &nestedOptions.anchor);
                                nestedOptions.placement = Mosaic::PopupPlacement::Right;
                                nestedOptions.closeOnSelection = false;

                                if(nestedButton.clicked() == true)
                                {
                                    Mosaic::openPopup(ui, Mosaic::Key("last popup"), nestedOptions);
                                }

                                auto nested = Mosaic::popup(ui, Mosaic::Key("last popup"), nestedOptions);

                                if(nested.visible() == true)
                                {
                                    Mosaic::text(ui, "I am the last one here.");
                                }
                            }
                        }
                    }
                }
                {
                    Mosaic::Response button = Mosaic::button(ui, "With a menu..");
                    Mosaic::PopupOptions options;
                    options.owner = button.id;
                    (void)Mosaic::debugBounds(ui, button.id, &options.anchor);
                    options.placement = Mosaic::PopupPlacement::Below;
                    options.minimumSize = {300.f, 0.f};
                    options.closeOnSelection = false;

                    if(button.clicked() == true)
                    {
                        Mosaic::openPopup(ui, Mosaic::Key("my_file_popup"), options);
                    }

                    auto popup = Mosaic::popup(ui, Mosaic::Key("my_file_popup"), options);

                    if(popup.visible() == true)
                    {
                        {
                            auto bar = Mosaic::menuBar(ui);
                            {
                                auto file = Mosaic::menu(ui, "File");

                                if(file.expanded() == true)
                                {
                                    HelloDemo::drawExampleMenuFile(ui);
                                }
                            }
                            {
                                auto edit = Mosaic::menu(ui, "Edit");

                                if(edit.expanded() == true)
                                {
                                    Mosaic::menuItem(ui, "Dummy");
                                }
                            }
                        }
                        Mosaic::text(ui, "Hello from popup!");
                        Mosaic::button(ui, "This is a dummy button..");
                    }
                }
            }
        }
        {
            auto contexts = Mosaic::treeNode(ui, Mosaic::Key("context menus"), "Context menus");

            if(contexts.expanded() == true)
            {
                Mosaic::helpMarker(ui, "Right-click an item to open the popup associated with its stable ID.");
                constexpr Mosaic::Array<Mosaic::StringView, 5> labels = {"Label1", "Label2", "Label3", "Label4", "Label5"};
                for(size_t index = 0; index != labels.size(); ++index)
                {
                    auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                    Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key("context label"), labels[index], m_contextSelected == static_cast<int>(index));
                    Mosaic::itemTooltip(ui, item, "Right-click to open popup");
                    Mosaic::PopupOptions options;
                    options.owner = item.id;
                    (void)Mosaic::debugBounds(ui, item.id, &options.anchor);
                    options.placement = Mosaic::PopupPlacement::Cursor;

                    if(Detail::secondaryClicked(ui, item) == true)
                    {
                        m_contextSelected = static_cast<int>(index);
                        Mosaic::openPopup(ui, Mosaic::Key("item context"), options);
                    }

                    auto popup = Mosaic::popup(ui, Mosaic::Key("item context"), options);

                    if(popup.visible() == true)
                    {
                        Mosaic::String message = "This is a popup for \"";
                        message += labels[index];
                        message += "\"!";
                        Mosaic::text(ui, message);

                        if(Mosaic::button(ui, "Close").clicked() == true)
                        {
                            Mosaic::closeCurrentPopup(ui);
                        }
                    }
                }
                Mosaic::helpMarker(ui, "Text elements can use an explicit popup identifier.");
                Mosaic::String valueLabel = "Value = ";
                valueLabel += Detail::demoFixed(m_contextValue, 3);
                valueLabel += " <-- (1) right-click this text";
                Mosaic::Response valueText = Mosaic::text(ui, valueLabel);
                Mosaic::Response secondText = Mosaic::text(ui, "(2) Or right-click this text");
                Mosaic::Response valueButton = Mosaic::button(ui, "(3) Or click this button");
                Mosaic::PopupOptions valueOptions;
                valueOptions.owner = valueText.id;
                (void)Mosaic::debugBounds(ui, valueText.id, &valueOptions.anchor);
                valueOptions.placement = Mosaic::PopupPlacement::Cursor;
                valueOptions.closeOnSelection = false;

                if(Detail::secondaryClicked(ui, valueText) == true || Detail::secondaryClicked(ui, secondText) == true || valueButton.clicked() == true)
                {
                    valueOptions.owner = valueButton.clicked() ? valueButton.id : Detail::secondaryClicked(ui, secondText) ? secondText.id : valueText.id;
                    (void)Mosaic::debugBounds(ui, valueOptions.owner, &valueOptions.anchor);
                    Mosaic::openPopup(ui, Mosaic::Key("my popup"), valueOptions);
                }

                auto valuePopup = Mosaic::popup(ui, Mosaic::Key("my popup"), valueOptions);

                if(valuePopup.visible() == true)
                {
                    if(Mosaic::selectable(ui, "Set to zero").clicked() == true)
                    {
                        m_contextValue = 0.f;
                    }

                    if(Mosaic::selectable(ui, "Set to PI").clicked() == true)
                    {
                        m_contextValue = 3.1415f;
                    }

                    Mosaic::dragValue(ui, {}, &m_contextValue);
                }

                Mosaic::String stableButtonLabel = "Button: ";
                stableButtonLabel += m_contextLabel;
                Mosaic::Response stableButton = Mosaic::button(ui, Mosaic::Key("stable context button"), stableButtonLabel);
                Mosaic::PopupOptions stableOptions;
                stableOptions.owner = stableButton.id;
                (void)Mosaic::debugBounds(ui, stableButton.id, &stableOptions.anchor);
                stableOptions.placement = Mosaic::PopupPlacement::Cursor;
                stableOptions.closeOnSelection = false;

                if(Detail::secondaryClicked(ui, stableButton) == true)
                {
                    Mosaic::openPopup(ui, Mosaic::Key("stable popup"), stableOptions);
                }

                auto stablePopup = Mosaic::popup(ui, Mosaic::Key("stable popup"), stableOptions);

                if(stablePopup.visible() == true)
                {
                    Mosaic::text(ui, "Edit name:");
                    Mosaic::inputText(ui, {}, &m_contextLabel);

                    if(Mosaic::button(ui, "Close").clicked() == true)
                    {
                        Mosaic::closeCurrentPopup(ui);
                    }
                }
            }
        }
        {
            auto modals = Mosaic::treeNode(ui, Mosaic::Key("modals"), "Modals");

            if(modals.expanded() == true)
            {
                Mosaic::text(ui, "Modal windows block interaction outside and cannot be dismissed by clicking the backdrop.");

                if(Mosaic::button(ui, "Delete..").clicked() == true)
                {
                    m_modalOpen = true;
                }

                if(Mosaic::button(ui, "Stacked modals..").clicked() == true)
                {
                    m_stackedModalFirst = true;
                }
            }
        }

        if(m_modalOpen == true)
        {
            auto modal = Mosaic::modal(ui, "Delete?", &m_modalOpen, {420.f, 180.f});

            if(modal.visible() == true)
            {
                Mosaic::text(ui, "All those beautiful files will be deleted.\nThis operation cannot be undone!");
                Mosaic::separator(ui);
                Mosaic::checkbox(ui, "Don't ask me next time", &m_modalDontAsk);
                auto row = Mosaic::row(ui);
                Mosaic::ButtonOptions actionOptions;
                actionOptions.width = Mosaic::Dimension::fixed(120.f);
                Mosaic::Response confirm = Mosaic::button(ui, Mosaic::Key("confirm deletion"), "OK", actionOptions);
                Mosaic::setItemDefaultFocus(ui, confirm.id);

                if(confirm.clicked() == true)
                {
                    m_modalOpen = false;
                }

                if(Mosaic::button(ui, Mosaic::Key("cancel deletion"), "Cancel", actionOptions).clicked() == true)
                {
                    m_modalOpen = false;
                }
            }
        }

        if(m_stackedModalFirst == true)
        {
            auto modal = Mosaic::modal(ui, "Stacked 1", &m_stackedModalFirst, {440.f, 250.f});

            if(modal.visible() == true)
            {
                {
                    auto bar = Mosaic::menuBar(ui);
                    {
                        auto file = Mosaic::menu(ui, "File");

                        if(file.expanded() == true)
                        {
                            Mosaic::menuItem(ui, "Some menu item");
                        }
                    }
                }
                Mosaic::text(ui, "Hello from Stacked The First\nUsing the modal backdrop behind it.");
                constexpr Mosaic::Array<Mosaic::StringView, 5> values = {"aaaa", "bbbb", "cccc", "dddd", "eeee"};
                Mosaic::comboBox(ui, "Combo", &m_modalCombo, values);
                Mosaic::colorEditorRgba(ui, "Color", &m_modalColor);

                if(Mosaic::button(ui, "Add another modal..").clicked() == true)
                {
                    m_stackedModalSecond = true;
                }

                if(Mosaic::button(ui, "Close").clicked() == true)
                {
                    m_stackedModalFirst = false;
                }
            }
        }

        if(m_stackedModalSecond == true)
        {
            auto modal = Mosaic::modal(ui, "Stacked 2", &m_stackedModalSecond, {360.f, 160.f});

            if(modal.visible() == true)
            {
                Mosaic::text(ui, "Hello from Stacked The Second!");
                Mosaic::colorEditorRgba(ui, "Color", &m_modalColor);

                if(Mosaic::button(ui, "Close").clicked() == true)
                {
                    m_stackedModalSecond = false;
                }
            }
        }

        {
            auto menus = Mosaic::treeNode(ui, Mosaic::Key("menus regular"), "Menus inside a regular window");

            if(menus.expanded() == true)
            {
                Mosaic::text(ui, "Below we are adding menu items to a regular window. It is unusual but supported.");
                Mosaic::separator(ui);
                Mosaic::MenuItemOptions shortcut;
                shortcut.shortcut = "Ctrl+M";
                Mosaic::menuItem(ui, "Menu item", shortcut);
                {
                    auto menu = Mosaic::menu(ui, "Menu inside a regular window");

                    if(menu.expanded() == true)
                    {
                        HelloDemo::drawExampleMenuFile(ui);
                    }
                }
                Mosaic::separator(ui);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawTables(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Tables & Columns");

        if(section.expanded() == false)
        {
            return;
        }

        Detail::tableOpenAction = -1;
        static bool disableIndent = false;
        {
            auto actions = Mosaic::row(ui);

            if(Mosaic::button(ui, "Expand all").clicked() == true)
            {
                Detail::tableOpenAction = 1;
            }

            if(Mosaic::button(ui, "Collapse all").clicked() == true)
            {
                Detail::tableOpenAction = 0;
            }

            Mosaic::checkbox(ui, "Disable tree indentation", &disableIndent);
            Mosaic::helpMarker(ui, "Disable the indenting of tree nodes so demo tables can use the full window width.");
        }
        Mosaic::separator(ui);
        Mosaic::Theme tableSectionTheme = Mosaic::getTheme(ui);

        if(disableIndent == true)
        {
            tableSectionTheme.metrics.indent = 0.f;
        }

        auto tableSectionStyle = Mosaic::styleScope(ui, tableSectionTheme);
        Mosaic::TableOptions basicOptions;
        basicOptions.resizable = false;
        basicOptions.reorderable = false;
        basicOptions.hideable = false;
        Detail::drawTableTree(ui, Mosaic::Key("table basic"), "Basic", basicOptions);
        Detail::drawTableTree(ui, Mosaic::Key("table borders"), "Borders, background", basicOptions);

        Mosaic::TableOptions stretchOptions;
        stretchOptions.reorderable = false;
        stretchOptions.hideable = false;
        Detail::drawTableTree(ui, Mosaic::Key("table resize"), "Resizable, stretch", stretchOptions);
        Detail::drawTableTree(ui, Mosaic::Key("table fixed"), "Resizable, fixed", stretchOptions, true);
        Detail::drawTableTree(ui, Mosaic::Key("table mixed"), "Resizable, mixed", stretchOptions);

        Mosaic::TableOptions reorderOptions;
        reorderOptions.reorderable = true;
        reorderOptions.hideable = true;
        Detail::drawTableTree(ui, Mosaic::Key("table reorder"), "Reorderable, hideable, with headers", reorderOptions);
        Detail::drawTableTree(ui, Mosaic::Key("table padding"), "Padding", basicOptions);
        Detail::drawTableTree(ui, Mosaic::Key("table sizing"), "Sizing policies", stretchOptions);

        Mosaic::TableOptions verticalOptions = stretchOptions;
        verticalOptions.scrollVertical = true;
        verticalOptions.frozenRows = 1;
        Detail::drawTableTree(ui, Mosaic::Key("table vertical"), "Vertical scrolling, with clipping", verticalOptions);
        Mosaic::TableOptions horizontalOptions = stretchOptions;
        horizontalOptions.scrollHorizontal = true;
        Detail::drawTableTree(ui, Mosaic::Key("table horizontal"), "Horizontal scrolling", horizontalOptions, true);
        Detail::drawTableTree(ui, Mosaic::Key("table flags"), "Columns flags", reorderOptions);
        Detail::drawTableTree(ui, Mosaic::Key("table widths"), "Columns widths", stretchOptions, true);
        Detail::drawTableTree(ui, Mosaic::Key("nested tables"), "Nested tables", basicOptions);
        Detail::drawTableTree(ui, Mosaic::Key("row height"), "Row height", basicOptions);
        Detail::drawTableTree(ui, Mosaic::Key("outer size"), "Outer size", verticalOptions);
        Detail::drawTableTree(ui, Mosaic::Key("background color"), "Background color", basicOptions);
        Detail::drawTableTree(ui, Mosaic::Key("tree view"), "Tree view", basicOptions);
        Detail::drawTableTree(ui, Mosaic::Key("item width"), "Item width", stretchOptions);
        Detail::drawTableTree(ui, Mosaic::Key("custom headers"), "Custom headers", reorderOptions);
        Detail::drawTableTree(ui, Mosaic::Key("angled headers"), "Angled headers", reorderOptions);
        Detail::drawTableTree(ui, Mosaic::Key("table contexts"), "Context menus", reorderOptions);
        Detail::drawTableTree(ui, Mosaic::Key("synced instances"), "Synced instances", stretchOptions);
        Mosaic::TableOptions sortingOptions = reorderOptions;
        sortingOptions.sortable = true;
        sortingOptions.multiSort = true;
        Detail::drawTableTree(ui, Mosaic::Key("sorting"), "Sorting", sortingOptions);
        auto advanced = Mosaic::treeNode(ui, Mosaic::Key("table advanced"), "Advanced");

        if(Detail::tableOpenAction != -1)
        {
            Mosaic::setTreeExpanded(ui, advanced.id(), Detail::tableOpenAction != 0);
        }

        if(advanced.expanded() == true)
        {
            static Mosaic::Array<bool, 24> flags = {true, true, true, true, false, false, true, true, true, true, true, true, true, true, false, false, false, false, false, false, false, false, false, false};
            static bool showHeaders = true;
            static bool showWrappedText = false;
            static bool outerSizeEnabled = true;
            static bool showDebugDetails = false;
            static int32_t freezeColumns = 1;
            static int32_t freezeRows = 1;
            static int32_t itemCount = 24;
            static int contentsType = 5;
            static int sizingPolicy = 3;
            static Mosaic::Array<float, 2> outerSize = {0.f, 248.f};
            static float innerWidth = 0.f;
            static float rowMinimumHeight = 0.f;
            auto optionsTree = Mosaic::treeNode(ui, Mosaic::Key("advanced options"), "Options");

            if(optionsTree.expanded() == true)
            {
                Mosaic::TreeNodeOptions groupOptions;
                groupOptions.defaultExpanded = true;
                auto features = Mosaic::treeNode(ui, Mosaic::Key("advanced features"), "Features:", groupOptions);

                if(features.expanded() == true)
                {
                    Mosaic::checkbox(ui, "MosaicTableFlags_Resizable", &flags[0]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_Reorderable", &flags[1]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_Hideable", &flags[2]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_Sortable", &flags[3]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoSavedSettings", &flags[4]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_ContextMenuInBody", &flags[5]);
                }

                auto decorations = Mosaic::treeNode(ui, Mosaic::Key("advanced decorations"), "Decorations:", groupOptions);

                if(decorations.expanded() == true)
                {
                    Mosaic::checkbox(ui, "MosaicTableFlags_RowBg", &flags[6]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_BordersV", &flags[7]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_BordersOuterV", &flags[8]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_BordersInnerV", &flags[9]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_BordersH", &flags[10]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_BordersOuterH", &flags[11]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_BordersInnerH", &flags[12]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoBordersInBody", &flags[13]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoBordersInBodyUntilResize", &flags[14]);
                }

                auto sizing = Mosaic::treeNode(ui, Mosaic::Key("advanced sizing"), "Sizing:", groupOptions);

                if(sizing.expanded() == true)
                {
                    constexpr Mosaic::Array<Mosaic::StringView, 4> policies = {"SizingFixedFit", "SizingFixedSame", "SizingStretchProp", "SizingStretchSame"};
                    Mosaic::comboBox(ui, "Policy", &sizingPolicy, policies);
                    Mosaic::helpMarker(ui, "The selected sizing policy is applied to every column in the table below.");
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoHostExtendX", &flags[15]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoHostExtendY", &flags[16]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoKeepColumnsVisible", &flags[17]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_PreciseWidths", &flags[18]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoClip", &flags[19]);
                }

                auto padding = Mosaic::treeNode(ui, Mosaic::Key("advanced padding"), "Padding:", groupOptions);

                if(padding.expanded() == true)
                {
                    Mosaic::checkbox(ui, "MosaicTableFlags_PadOuterX", &flags[20]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoPadOuterX", &flags[21]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_NoPadInnerX", &flags[22]);
                }

                auto scrolling = Mosaic::treeNode(ui, Mosaic::Key("advanced scrolling"), "Scrolling:", groupOptions);

                if(scrolling.expanded() == true)
                {
                    {
                        auto row = Mosaic::row(ui);
                        Mosaic::checkbox(ui, "MosaicTableFlags_ScrollX", &m_tableAdvancedFlags[2]);
                        Mosaic::dragValue(ui, "freeze_cols", &freezeColumns, int32_t{0}, int32_t{9});
                    }
                    {
                        auto row = Mosaic::row(ui);
                        Mosaic::checkbox(ui, "MosaicTableFlags_ScrollY", &m_tableAdvancedFlags[5]);
                        Mosaic::dragValue(ui, "freeze_rows", &freezeRows, int32_t{0}, int32_t{9});
                    }
                }

                auto sorting = Mosaic::treeNode(ui, Mosaic::Key("advanced sorting"), "Sorting:", groupOptions);

                if(sorting.expanded() == true)
                {
                    Mosaic::checkbox(ui, "MosaicTableFlags_SortMulti", &m_tableAdvancedFlags[0]);
                    Mosaic::checkbox(ui, "MosaicTableFlags_SortTristate", &flags[23]);
                }

                auto headers = Mosaic::treeNode(ui, Mosaic::Key("advanced headers"), "Headers:", groupOptions);

                if(headers.expanded() == true)
                {
                    Mosaic::checkbox(ui, "show_headers", &showHeaders);
                    Mosaic::checkbox(ui, "MosaicTableFlags_HighlightHoveredColumn", &m_tableAdvancedFlags[1]);
                    Mosaic::checkbox(ui, "MosaicTableColumnFlags_AngledHeader", &m_tableAdvancedFlags[3]);
                }

                auto other = Mosaic::treeNode(ui, Mosaic::Key("advanced other"), "Other:", groupOptions);

                if(other.expanded() == true)
                {
                    Mosaic::checkbox(ui, "show_wrapped_text", &showWrappedText);
                    {
                        auto row = Mosaic::row(ui);
                        Mosaic::vectorEditor(ui, "outer_size", Mosaic::FloatSpan(outerSize.data(), outerSize.size()), 0.f, 1000.f);
                        Mosaic::checkbox(ui, "outer_size", &outerSizeEnabled);
                    }
                    Mosaic::dragValue(ui, "inner_width (when ScrollX active)", &innerWidth);
                    Mosaic::dragValue(ui, "row_min_height", &rowMinimumHeight);
                    Mosaic::dragValue(ui, "items_count", &itemCount, int32_t{0}, int32_t{9999});
                    constexpr Mosaic::Array<Mosaic::StringView, 6> contentTypes = {"Text", "Button", "SmallButton", "FillButton", "Selectable", "Selectable (span row)"};
                    Mosaic::comboBox(ui, "items_type (first column)", &contentsType, contentTypes);
                }
            }

            Mosaic::TableOptions options;
            options.headers = showHeaders;
            options.resizable = flags[0];
            options.reorderable = flags[1];
            options.hideable = flags[2];
            options.sortable = flags[3];
            options.multiSort = m_tableAdvancedFlags[0];
            options.rowSelection = true;
            options.rowBackground = flags[6];
            options.bordersInnerVertical = flags[7] || flags[9];
            options.bordersOuterVertical = flags[7] || flags[8];
            options.bordersInnerHorizontal = flags[10] || flags[12];
            options.bordersOuterHorizontal = flags[10] || flags[11];
            options.clipCells = flags[19] == false;
            options.preciseWidths = flags[18];
            options.highlightHoveredColumn = m_tableAdvancedFlags[1];
            options.padOuterHorizontal = flags[20] && flags[21] == false;
            options.padInnerHorizontal = flags[22] == false;
            options.saveSettings = flags[4] == false;
            options.contextMenuInBody = flags[5];
            options.bordersInBody = flags[13] == false;
            options.bordersInBodyUntilResize = flags[14];
            options.extendHostHorizontal = flags[15] == false;
            options.extendHostVertical = flags[16] == false;
            options.keepColumnsVisible = flags[17] == false;
            options.sortTristate = flags[23];
            options.scrollHorizontal = m_tableAdvancedFlags[2];
            options.scrollVertical = m_tableAdvancedFlags[5];
            options.innerWidth = options.scrollHorizontal ? std::max(0.f, innerWidth) : 0.f;
            options.rowMinimumHeight = std::max(0.f, rowMinimumHeight);
            options.frozenRows = static_cast<uint32_t>(freezeRows);
            options.frozenColumns = static_cast<uint32_t>(freezeColumns);
            constexpr Mosaic::Id advancedTableSettingsId = 0xa6c33ec2cd769dedULL;
            options.settingsId = advancedTableSettingsId;

            if(Mosaic::button(ui, "Reset table settings").clicked() == true)
            {
                Mosaic::resetTableSettings(ui, advancedTableSettingsId);
            }

            Mosaic::Id advancedTableId = Mosaic::InvalidId;
            Mosaic::VisibleRange visibleRows;
            size_t advancedItemCount = 0;
            {
                Mosaic::LayoutOptions advancedTableLayout;
                advancedTableLayout.width = outerSizeEnabled && outerSize[0] > 0.f ? Mosaic::Dimension::fixed(outerSize[0]) : Mosaic::SizeRule::Fill;
                advancedTableLayout.height = outerSizeEnabled && outerSize[1] > 0.f ? Mosaic::Dimension::fixed(outerSize[1]) : Mosaic::SizeRule::Content;
                auto table = Mosaic::table(ui, "Advanced table", 6, options, advancedTableLayout);
                advancedTableId = table.id();
                Mosaic::TableColumnOptions idColumn;
                Detail::applyTableSizing(idColumn, sizingPolicy, 64.f, 0.7f);
                idColumn.defaultSort = true;
                Mosaic::tableSetupColumn(ui, 0, "ID", idColumn);
                Mosaic::TableColumnOptions nameColumn;
                Detail::applyTableSizing(nameColumn, sizingPolicy, 132.f, 1.8f);
                nameColumn.angledHeader = m_tableAdvancedFlags[3];
                Mosaic::tableSetupColumn(ui, 1, "Name", nameColumn);
                Mosaic::TableColumnOptions actionColumn;
                Detail::applyTableSizing(actionColumn, sizingPolicy, 104.f, 1.1f);
                actionColumn.sortable = false;
                Mosaic::tableSetupColumn(ui, 2, "Action", actionColumn);
                Mosaic::TableColumnOptions quantityColumn;
                Detail::applyTableSizing(quantityColumn, sizingPolicy, 88.f, 1.f);
                quantityColumn.preferredSort = Mosaic::SortDirection::Descending;
                Mosaic::tableSetupColumn(ui, 3, "Quantity", quantityColumn);
                Mosaic::TableColumnOptions descriptionColumn;
                Detail::applyTableSizing(descriptionColumn, sizingPolicy, 180.f, 2.f);
                Mosaic::tableSetupColumn(ui, 4, "Description", descriptionColumn);
                Mosaic::TableColumnOptions hiddenColumn;
                Detail::applyTableSizing(hiddenColumn, sizingPolicy, 80.f, 1.f);
                hiddenColumn.visible = false;
                hiddenColumn.sortable = false;
                Mosaic::tableSetupColumn(ui, 5, "Hidden", hiddenColumn);
                Mosaic::tableAngledHeadersRow(ui);
                Mosaic::tableHeadersRow(ui);

                Mosaic::TableSortState sortState;
                (void)Mosaic::tableSortState(ui, table.id(), &sortState);
                Mosaic::TableSortSpecSpan specs = sortState.specifications;
                bool sortChanged = sortState.dirty;
                advancedItemCount = static_cast<size_t>(std::max(itemCount, int32_t{0}));

                if(m_advancedTableQuantities.size() != advancedItemCount)
                {
                    size_t previousSize = m_advancedTableQuantities.size();
                    m_advancedTableQuantities.resize(advancedItemCount);
                    for(size_t index = previousSize; index != advancedItemCount; ++index)
                    {
                        size_t type = index % 5;
                        m_advancedTableQuantities[index] = type == 3 ? 10 : type == 4 ? 20 : 0;
                    }
                    m_advancedTableDataDirty = true;
                }

                if(sortChanged == true || m_advancedTableDataDirty == true || m_advancedTableOrder.size() != advancedItemCount)
                {
                    m_advancedTableOrder.resize(advancedItemCount);
                    std::iota(m_advancedTableOrder.begin(), m_advancedTableOrder.end(), size_t{0});
                    for(size_t specIndex = specs.size(); specIndex > 0; --specIndex)
                    {
                        const Mosaic::TableSortSpec & spec = specs[specIndex - 1];
                        std::stable_sort(m_advancedTableOrder.begin(), m_advancedTableOrder.end(),
                                         [this, &spec](size_t first, size_t second)
                                         {
                                             int comparison = 0;

                                             if(spec.column == 0)
                                             {
                                                 comparison = first < second ? -1 : first > second ? 1 : 0;
                                             }
                                             else if(spec.column == 1 || spec.column == 4)
                                             {
                                                 constexpr Mosaic::Array<Mosaic::StringView, 5> names = {"Banana", "Apple", "Cherry", "Watermelon", "Grapefruit"};
                                                 comparison = names[first % names.size()].compare(names[second % names.size()]);
                                             }
                                             else if(spec.column == 3)
                                             {
                                                 int32_t firstValue = m_advancedTableQuantities[first];
                                                 int32_t secondValue = m_advancedTableQuantities[second];
                                                 comparison = firstValue < secondValue ? -1 : firstValue > secondValue ? 1 : 0;
                                             }

                                             if(spec.direction == Mosaic::SortDirection::None)
                                             {
                                                 return false;
                                             }

                                             if(comparison == 0)
                                             {
                                                 return false;
                                             }

                                             return spec.direction == Mosaic::SortDirection::Ascending ? comparison < 0 : comparison > 0;
                                         });
                    }
                    m_advancedTableDataDirty = false;
                    Mosaic::tableSortSpecsHandled(ui, table.id());
                    m_tableOrderedIds.clear();
                    m_tableOrderedIds.reserve(m_advancedTableOrder.size());
                    for(size_t row : m_advancedTableOrder)
                    {
                        m_tableOrderedIds.push_back(Detail::demoRowId(row));
                    }
                }
                visibleRows = {0, m_advancedTableOrder.size()};

                if(options.scrollVertical == true)
                {
                    (void)Mosaic::tableVisibleRows(ui, m_advancedTableOrder.size(), Mosaic::getTheme(ui).metrics.controlHeight, &visibleRows);
                }

                for(size_t rowIndex = visibleRows.begin; rowIndex != visibleRows.end; ++rowIndex)
                {
                    size_t row = m_advancedTableOrder[rowIndex];

                    if(contentsType == 5)
                    {
                        Mosaic::tableNextRow(ui, Mosaic::Key(row), &m_tableSelection, Detail::demoRowId(row), m_tableOrderedIds);
                    }
                    else
                    {
                        Mosaic::tableNextRow(ui);
                    }

                    if(Mosaic::tableSetColumn(ui, 0) == true)
                    {
                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), 0)));
                        Mosaic::String value = Detail::demoNumber(row + 1000);

                        if(contentsType == 0)
                        {
                            Mosaic::text(ui, value);
                        }
                        else if(contentsType == 1)
                        {
                            Mosaic::button(ui, Mosaic::Key("button"), value);
                        }
                        else if(contentsType == 2)
                        {
                            Mosaic::Theme compactTheme = Mosaic::getTheme(ui);
                            compactTheme.metrics.framePadding = {3.f, 1.f};
                            compactTheme.metrics.controlHeight = compactTheme.metrics.lineHeight + 2.f;
                            auto compactStyle = Mosaic::styleScope(ui, compactTheme);
                            Mosaic::button(ui, Mosaic::Key("small button"), value);
                        }
                        else if(contentsType == 3)
                        {
                            Mosaic::ButtonOptions buttonOptions;
                            buttonOptions.width = Mosaic::SizeRule::Fill;
                            Mosaic::button(ui, Mosaic::Key("fill button"), value, buttonOptions);
                        }
                        else if(contentsType == 4)
                        {
                            Mosaic::selectable(ui, Mosaic::Key("selectable"), value, &m_tableSelection, Detail::demoRowId(row), m_tableOrderedIds);
                        }
                        else
                        {
                            Mosaic::text(ui, value);
                        }
                    }

                    if(Mosaic::tableSetColumn(ui, 1) == true)
                    {
                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), 1)));
                        constexpr Mosaic::Array<Mosaic::StringView, 5> names = {"Banana", "Apple", "Cherry", "Watermelon", "Grapefruit"};
                        Mosaic::text(ui, names[row % names.size()]);
                    }

                    if(Mosaic::tableSetColumn(ui, 2) == true)
                    {
                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), 2)));
                        auto actions = Mosaic::row(ui);
                        Mosaic::Theme compactTheme = Mosaic::getTheme(ui);
                        compactTheme.metrics.framePadding = {3.f, 1.f};
                        compactTheme.metrics.controlHeight = compactTheme.metrics.lineHeight + 2.f;
                        auto compactStyle = Mosaic::styleScope(ui, compactTheme);

                        if(Mosaic::button(ui, Mosaic::Key("chop"), "Chop").clicked() == true)
                        {
                            ++m_advancedTableQuantities[row];
                            m_advancedTableDataDirty = true;
                        }

                        if(Mosaic::button(ui, Mosaic::Key("eat"), "Eat").clicked() == true)
                        {
                            --m_advancedTableQuantities[row];
                            m_advancedTableDataDirty = true;
                        }
                    }

                    if(Mosaic::tableSetColumn(ui, 3) == true)
                    {
                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), 3)));
                        Mosaic::text(ui, Detail::demoNumber(m_advancedTableQuantities[row]));
                    }

                    if(Mosaic::tableSetColumn(ui, 4) == true)
                    {
                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), 4)));
                        Mosaic::TextOptions textOptions;
                        textOptions.wordWrap = showWrappedText;
                        textOptions.layout.width = Mosaic::SizeRule::Fill;
                        Mosaic::text(ui, "Lorem ipsum dolor sit amet", textOptions);
                    }

                    if(Mosaic::tableSetColumn(ui, 5) == true)
                    {
                        auto cellScope = Mosaic::scope(ui, Mosaic::Key(Mosaic::combineId(static_cast<Mosaic::Id>(row), 5)));
                        Mosaic::text(ui, "1234");
                    }
                }
            }

            Mosaic::checkbox(ui, "Debug details", &showDebugDetails);

            if(showDebugDetails == true)
            {
                Mosaic::Vec2 current;
                Mosaic::Vec2 maximum;
                (void)Mosaic::scrollOffset(ui, advancedTableId, &current);
                (void)Mosaic::scrollRange(ui, advancedTableId, &maximum);
                Mosaic::String details = "Rows: ";
                details += Detail::demoNumber(advancedItemCount);
                details += ", visible: ";
                details += Detail::demoNumber(visibleRows.end - visibleRows.begin);
                details += ", scroll: (";
                details += Detail::demoFixed(current.x, 0);
                details += "/";
                details += Detail::demoFixed(maximum.x, 0);
                details += ") (";
                details += Detail::demoFixed(current.y, 0);
                details += "/";
                details += Detail::demoFixed(maximum.y, 0);
                details += ")";
                Mosaic::text(ui, details);
            }
        }

        auto legacy = Mosaic::treeNode(ui, Mosaic::Key("legacy columns"), "Legacy Columns API");

        if(Detail::tableOpenAction != -1)
        {
            Mosaic::setTreeExpanded(ui, legacy.id(), Detail::tableOpenAction != 0);
        }

        Mosaic::itemTooltip(ui, Mosaic::Response{legacy.id()}, "Columns() is an old API. Prefer the more flexible table API.");

        if(legacy.expanded() == true)
        {
            {
                auto basic = Mosaic::treeNode(ui, Mosaic::Key("legacy basic"), "Basic");

                if(basic.expanded() == true)
                {
                    Mosaic::text(ui, "Without border:");
                    {
                        Mosaic::ColumnsOptions options;
                        options.border = false;
                        auto columns = Mosaic::columns(ui, "legacy basic borderless", 3, options);
                        for(size_t index = 0; index != 14; ++index)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String label = "Item ";
                            label += Detail::demoNumber(index);
                            Mosaic::selectable(ui, label);
                            (void)Mosaic::nextColumn(ui);
                        }
                    }
                    Mosaic::separator(ui);
                    Mosaic::text(ui, "With border:");
                    auto columns = Mosaic::columns(ui, "legacy bordered", 4);
                    for(Mosaic::StringView heading : Mosaic::Array<Mosaic::StringView, 4>{"ID", "Name", "Path", "Hovered"})
                    {
                        Mosaic::text(ui, heading);
                        (void)Mosaic::nextColumn(ui);
                    }
                    constexpr Mosaic::Array<Mosaic::StringView, 3> names = {"One", "Two", "Three"};
                    constexpr Mosaic::Array<Mosaic::StringView, 3> paths = {"/path/one", "/path/two", "/path/three"};
                    for(size_t index = 0; index != 3; ++index)
                    {
                        Mosaic::selectable(ui, Detail::demoNumber(index), m_list == static_cast<int>(index));
                        (void)Mosaic::nextColumn(ui);
                        Mosaic::text(ui, names[index]);
                        (void)Mosaic::nextColumn(ui);
                        Mosaic::text(ui, paths[index]);
                        (void)Mosaic::nextColumn(ui);
                        Mosaic::text(ui, "0");
                        (void)Mosaic::nextColumn(ui);
                    }
                }
            }
            {
                auto borders = Mosaic::treeNode(ui, Mosaic::Key("legacy borders"), "Borders");

                if(borders.expanded() == true)
                {
                    {
                        auto row = Mosaic::row(ui);
                        Mosaic::dragValue(ui, {}, &m_legacyColumnCount, int32_t{2}, int32_t{10});
                        Mosaic::checkbox(ui, "horizontal", &m_legacyHorizontalBorders);
                        Mosaic::checkbox(ui, "vertical", &m_legacyVerticalBorders);
                    }
                    Mosaic::ColumnsOptions columnOptions;
                    columnOptions.border = m_legacyVerticalBorders;
                    columnOptions.horizontalBorders = m_legacyHorizontalBorders;
                    auto columns = Mosaic::columns(ui, "legacy configurable borders", static_cast<uint32_t>(m_legacyColumnCount), columnOptions);
                    for(int32_t row = 0; row != 3; ++row)
                    {
                        for(int32_t column = 0; column != m_legacyColumnCount; ++column)
                        {
                            auto item = Mosaic::scope(ui, Mosaic::Key(row * m_legacyColumnCount + column));
                            Mosaic::String label(3, static_cast<char>('a' + column % 26));

                            if(row == 0)
                            {
                                Mosaic::text(ui, label);
                            }
                            else if(row == 1)
                            {
                                Mosaic::text(ui, "Long text that is likely to clip");
                            }
                            else
                            {
                                Mosaic::button(ui, "Button");
                            }

                            (void)Mosaic::nextColumn(ui);
                        }
                    }
                }
            }
            {
                auto mixed = Mosaic::treeNode(ui, Mosaic::Key("legacy mixed"), "Mixed items");

                if(mixed.expanded() == true)
                {
                    {
                        auto columns = Mosaic::columns(ui, "legacy mixed columns", 3);
                        for(size_t index = 0; index != 3; ++index)
                        {
                            auto cell = Mosaic::column(ui);
                            Mosaic::text(ui, index == 0 ? "Hello" : index == 1 ? "Mosaic" : "Sailor");
                            Mosaic::button(ui, index == 0 ? "Banana" : index == 1 ? "Apple" : "Corniflower");
                            Mosaic::inputFloat(ui, index == 1 ? "red" : "blue", &m_legacyColumnFloat);
                            (void)Mosaic::nextColumn(ui);
                        }
                    }
                    {
                        auto category = Mosaic::collapsingHeader(ui, "Category A");

                        if(category.expanded() == true)
                        {
                            Mosaic::text(ui, "Blah blah blah");
                        }
                    }
                    {
                        auto category = Mosaic::collapsingHeader(ui, "Category B");

                        if(category.expanded() == true)
                        {
                            Mosaic::text(ui, "Blah blah blah");
                        }
                    }
                    {
                        auto category = Mosaic::collapsingHeader(ui, "Category C");

                        if(category.expanded() == true)
                        {
                            Mosaic::text(ui, "Blah blah blah");
                        }
                    }
                }
            }
            {
                auto wrapping = Mosaic::treeNode(ui, Mosaic::Key("legacy wrapping"), "Word-wrapping");

                if(wrapping.expanded() == true)
                {
                    auto columns = Mosaic::columns(ui, "legacy wrapping columns", 2);
                    Mosaic::text(ui, "The quick brown fox jumps over the lazy dog.\nHello Left");
                    (void)Mosaic::nextColumn(ui);
                    Mosaic::text(ui, "The quick brown fox jumps over the lazy dog.\nHello Right");
                }
            }
            {
                auto horizontal = Mosaic::treeNode(ui, Mosaic::Key("legacy horizontal"), "Horizontal Scrolling");

                if(horizontal.expanded() == true)
                {
                    Mosaic::LayoutOptions layout;
                    layout.width = Mosaic::SizeRule::Fill;
                    layout.height = Mosaic::Dimension::fixed(260.f);
                    Mosaic::ScrollOptions scrollOptions;
                    scrollOptions.axes = Mosaic::ScrollAxes::Both;
                    auto scrolling = Mosaic::scrollArea(ui, "legacy columns scroll", scrollOptions, layout);
                    Mosaic::LayoutOptions columnsLayout;
                    columnsLayout.width = Mosaic::Dimension::fixed(1500.f);
                    auto columns = Mosaic::columns(ui, "legacy scrolling", 10, {}, columnsLayout);
                    for(uint32_t column = 0; column != 10; ++column)
                    {
                        Mosaic::setColumnWidth(ui, static_cast<int32_t>(column), 150.f);
                    }
                    for(uint32_t column = 0; column != 10; ++column)
                    {
                        Mosaic::text(ui, Detail::demoNumber(column));
                        (void)Mosaic::nextColumn(ui);
                    }
                    float rowHeight = Mosaic::getTheme(ui).metrics.controlHeight;
                    Mosaic::VisibleRange rows;
                    (void)Mosaic::beginListClipper(ui, 100, rowHeight, &rows, scrolling.id());
                    for(size_t rowIndex = rows.begin; rowIndex != rows.end; ++rowIndex)
                    {
                        for(uint32_t column = 0; column != 10; ++column)
                        {
                            Mosaic::String cell = "Line ";
                            cell += Detail::demoNumber(rowIndex);
                            cell += " Column ";
                            cell += Detail::demoNumber(column);
                            cell += "...";
                            Mosaic::text(ui, cell);
                            (void)Mosaic::nextColumn(ui);
                        }
                    }
                    Mosaic::endListClipper(ui, rows, 100, rowHeight);
                }
            }
            {
                auto tree = Mosaic::treeNode(ui, Mosaic::Key("legacy tree"), "Tree");

                if(tree.expanded() == true)
                {
                    auto columns = Mosaic::columns(ui, "legacy tree table", 2);
                    for(size_t first = 0; first != 3; ++first)
                    {
                        Mosaic::String label = "Node";
                        label += Detail::demoNumber(first);
                        auto node = Mosaic::treeNode(ui, Mosaic::Key(first), first == 0 ? "Tree in column" : label);
                        (void)Mosaic::nextColumn(ui);
                        Mosaic::text(ui, "Node contents");
                        (void)Mosaic::nextColumn(ui);

                        if(node.expanded() == true)
                        {
                            Mosaic::text(ui, "Tree in column\nThe quick brown fox jumps over the lazy dog");
                            (void)Mosaic::nextColumn(ui);
                            Mosaic::text(ui, "Child row");
                            (void)Mosaic::nextColumn(ui);
                        }
                    }
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawInputs(Mosaic::Context * ui)
    {
        auto section = Mosaic::collapsingHeader(ui, "Inputs & Focus");

        if(section.expanded() == false)
        {
            return;
        }

        {
            auto inputs = Mosaic::treeNode(ui, Mosaic::Key("inputs"), "Inputs");

            if(inputs.expanded() == true)
            {
                Mosaic::helpMarker(ui, "This is a simplified view. Detailed routing information belongs in the Metrics "
                                       "and Debug Log tools.");
                const Mosaic::Input & input = Mosaic::input(ui);
                Mosaic::String mouse = "Mouse pos: ";

                if(input.pointers.empty() == false)
                {
                    const Mosaic::PointerState & pointer = input.pointers.front();
                    mouse += Detail::demoFixed(pointer.position.x, 1);
                    mouse += ", ";
                    mouse += Detail::demoFixed(pointer.position.y, 1);
                }
                else
                {
                    mouse += "<invalid>";
                }

                Mosaic::text(ui, mouse);
                Mosaic::String delta = "Mouse delta: ";

                if(input.pointers.empty() == false)
                {
                    const Mosaic::PointerState & pointer = input.pointers.front();
                    delta += Detail::demoFixed(pointer.delta.x, 1);
                    delta += ", ";
                    delta += Detail::demoFixed(pointer.delta.y, 1);
                }
                else
                {
                    delta += "0, 0";
                }

                Mosaic::text(ui, delta);
                Mosaic::String down = "Mouse down:";

                if(input.pointers.empty() == false)
                {
                    const Mosaic::PointerState & pointer = input.pointers.front();
                    for(uint8_t button = 0; button != 5; ++button)
                    {
                        if((pointer.down & (1U << button)) == 0)
                        {
                            continue;
                        }

                        down += " b";
                        down += Detail::demoNumber(button);
                        down += " (";
                        down += Detail::demoFixed(Mosaic::pointerDownDuration(ui, static_cast<Mosaic::PointerButton>(button)), 2);
                        down += " s)";
                    }
                }

                Mosaic::text(ui, down);
                Mosaic::String wheel = "Mouse wheel: ";
                wheel += Detail::demoFixed(input.wheel.x, 1);
                wheel += ", ";
                wheel += Detail::demoFixed(input.wheel.y, 1);
                Mosaic::text(ui, wheel);
                Mosaic::String clicked = "Mouse clicked count:";

                if(input.pointers.empty() == false)
                {
                    const Mosaic::PointerState & pointer = input.pointers.front();
                    for(uint8_t button = 0; button != 5; ++button)
                    {
                        uint8_t count = pointer.buttonClickCount(static_cast<Mosaic::PointerButton>(button));

                        if(count == 0)
                        {
                            continue;
                        }

                        clicked += " b";
                        clicked += Detail::demoNumber(button);
                        clicked += ": ";
                        clicked += Detail::demoNumber(count);
                    }
                }

                Mosaic::text(ui, clicked);
                Mosaic::String keys = "Keys down:";
                for(uint16_t value = static_cast<uint16_t>(Mosaic::KeyCode::Tab); value < static_cast<uint16_t>(Mosaic::KeyCode::Count); ++value)
                {
                    Mosaic::KeyCode key = static_cast<Mosaic::KeyCode>(value);

                    if(Mosaic::keyDown(ui, key) == false)
                    {
                        continue;
                    }

                    keys += " \"";
                    keys += Detail::keyName(key);
                    keys += "\" ";
                    keys += Detail::demoFixed(Mosaic::keyDownDuration(ui, key), 2);
                    keys += "s";
                }
                Mosaic::text(ui, keys);
                Mosaic::String modifiers = "Keys mods: ";

                if(input.modifiers.control == true)
                {
                    modifiers += "CTRL ";
                }

                if(input.modifiers.shift == true)
                {
                    modifiers += "SHIFT ";
                }

                if(input.modifiers.alt == true)
                {
                    modifiers += "ALT ";
                }

                if(input.modifiers.super == true)
                {
                    modifiers += "SUPER ";
                }

                Mosaic::text(ui, modifiers);
                Mosaic::String characters = "Chars queue:";
                for(const Mosaic::String & value : input.text)
                {
                    for(size_t offset = 0; offset < value.size();)
                    {
                        char32_t codepoint = U'\0';
                        size_t length = Detail::decodeUtf8(value, offset, codepoint);

                        if(length == 0)
                        {
                            break;
                        }

                        characters += " '";
                        characters.append(value.data() + offset, length);
                        characters += "' (";
                        characters += Detail::demoCodepoint(codepoint);
                        characters += ")";
                        offset += length;
                    }
                }
                Mosaic::text(ui, characters);
            }
        }
        {
            auto outputs = Mosaic::treeNode(ui, Mosaic::Key("outputs"), "Outputs");

            if(outputs.expanded() == true)
            {
                const Mosaic::Frame & frame = Mosaic::getFrame(ui);
                Mosaic::text(ui, frame.inputCapture.pointer ? "io.WantCaptureMouse: true" : "io.WantCaptureMouse: false");
                Mosaic::text(ui, frame.inputCapture.pointerUnlessPopupClose ? "io.WantCaptureMouseUnlessPopupClose: true" : "io.WantCaptureMouseUnlessPopupClose: false");
                Mosaic::text(ui, frame.inputCapture.keyboard ? "io.WantCaptureKeyboard: true" : "io.WantCaptureKeyboard: false");
                Mosaic::text(ui, frame.inputCapture.text ? "io.WantTextInput: true" : "io.WantTextInput: false");
                Mosaic::text(ui, "io.WantSetMousePos: false");
                Mosaic::Configuration configuration;
                if(Mosaic::getConfiguration(ui, &configuration) == false)
                {
                    return;
                }

                Mosaic::String navigation = "io.NavActive: ";
                navigation += configuration.keyboardNavigation ? "true" : "false";
                navigation += ", io.NavVisible: ";
                navigation += Mosaic::navigationFocus(ui) != Mosaic::InvalidId ? "true" : "false";
                Mosaic::text(ui, navigation);
                auto capture = Mosaic::treeNode(ui, Mosaic::Key("capture override"), "WantCapture override");

                if(capture.expanded() == true)
                {
                    Mosaic::helpMarker(ui, "Hover the colored canvas to override the next frame capture outputs. -1 "
                                           "keeps automatic routing.");
                    Mosaic::slider(ui, "SetNextFrameWantCaptureMouse() on hover", &m_captureOverridePointer, int32_t{-1}, int32_t{1});
                    Mosaic::slider(ui, "SetNextFrameWantCaptureKeyboard() on hover", &m_captureOverrideKeyboard, int32_t{-1}, int32_t{1});
                    Mosaic::LayoutOptions canvasLayout;
                    canvasLayout.width = Mosaic::Dimension::fixed(128.f);
                    canvasLayout.height = Mosaic::Dimension::fixed(96.f);
                    auto panel = Mosaic::canvas(ui, "panel", canvasLayout);
                    panel.rect({0.f, 0.f, 128.f, 96.f}, Mosaic::Color{0.7f, 0.1f, 0.7f, 1.f});
                    const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
                    Mosaic::Rect panelBounds;
                    bool hasPanelBounds = Mosaic::debugBounds(ui, panel.id(), &panelBounds);
                    bool hovered = pointer != nullptr && hasPanelBounds == true && panelBounds.contains(pointer->position) == true;

                    if(hovered == true)
                    {
                        Mosaic::InputCaptureOverride overrideValue;
                        overrideValue.pointer = static_cast<int8_t>(m_captureOverridePointer);
                        overrideValue.keyboard = static_cast<int8_t>(m_captureOverrideKeyboard);
                        Mosaic::setInputCaptureOverride(ui, overrideValue);
                    }
                }
            }
        }
        {
            auto shortcuts = Mosaic::treeNode(ui, Mosaic::Key("shortcuts"), "Shortcuts");

            if(shortcuts.expanded() == true)
            {
                Mosaic::checkbox(ui, "MosaicInputFlags_Repeat", &m_shortcutRepeat);
                Mosaic::checkbox(ui, "MosaicInputFlags_Tooltip", &m_shortcutTooltip);

                if(Mosaic::radioButton(ui, "MosaicInputFlags_RouteActive", m_shortcutRoute == 0).clicked() == true)
                {
                    m_shortcutRoute = 0;
                }

                if(Mosaic::radioButton(ui, "MosaicInputFlags_RouteFocused (default)", m_shortcutRoute == 1).clicked() == true)
                {
                    m_shortcutRoute = 1;
                }

                {
                    auto routeFocused = Mosaic::disabledScope(ui, m_shortcutRoute != 1);
                    Mosaic::checkbox(ui, Mosaic::Key("route over active 0"), "MosaicInputFlags_RouteOverActive", &m_shortcutOverActive);
                }

                if(Mosaic::radioButton(ui, "MosaicInputFlags_RouteGlobal", m_shortcutRoute == 2).clicked() == true)
                {
                    m_shortcutRoute = 2;
                }

                {
                    auto routeGlobal = Mosaic::disabledScope(ui, m_shortcutRoute != 2);
                    Mosaic::checkbox(ui, "MosaicInputFlags_RouteOverFocused", &m_shortcutOverFocused);
                    Mosaic::checkbox(ui, "MosaicInputFlags_RouteOverActive", &m_shortcutOverActive);
                    Mosaic::checkbox(ui, "MosaicInputFlags_RouteUnlessBgFocused", &m_shortcutUnlessBackground);
                }

                if(Mosaic::radioButton(ui, "MosaicInputFlags_RouteAlways", m_shortcutRoute == 3).clicked() == true)
                {
                    m_shortcutRoute = 3;
                }

                Mosaic::ShortcutOptions routeOptions;
                routeOptions.route = m_shortcutRoute == 0 ? Mosaic::ShortcutRoute::Active : m_shortcutRoute == 1 ? Mosaic::ShortcutRoute::Focused : m_shortcutRoute == 2 ? Mosaic::ShortcutRoute::Global : Mosaic::ShortcutRoute::Always;
                routeOptions.repeat = m_shortcutRepeat;
                routeOptions.tooltip = m_shortcutTooltip;
                routeOptions.routeOverActive = m_shortcutOverActive;
                routeOptions.routeOverFocused = m_shortcutOverFocused;
                routeOptions.routeUnlessBackgroundFocused = m_shortcutUnlessBackground;
                Mosaic::separatorText(ui, "Using SetNextItemShortcut()");
                Mosaic::text(ui, "Ctrl+S");
                Mosaic::setNextItemShortcut(ui, Mosaic::Shortcut::primary(Mosaic::KeyCode::S), routeOptions);
                bool savePressed = Mosaic::button(ui, "Save").clicked();
                Mosaic::text(ui, savePressed ? "Save shortcut: PRESSED" : "Save shortcut: ...");
                Mosaic::text(ui, "Alt+F");
                Mosaic::Shortcut factorShortcut;
                factorShortcut.key = Mosaic::KeyCode::F;
                factorShortcut.modifiers.alt = true;
                Mosaic::setNextItemShortcut(ui, factorShortcut, routeOptions);
                Mosaic::slider(ui, "Factor", &m_shortcutFactor, 0.f, 1.f);

                Mosaic::separatorText(ui, "Using Shortcut()");
                Mosaic::text(ui, "Ctrl+A");
                Mosaic::Shortcut selectAll = Mosaic::Shortcut::primary(Mosaic::KeyCode::A);
                bool rootShortcut = Mosaic::shortcut(ui, selectAll, routeOptions);
                Mosaic::String rootStatus = "IsWindowFocused: ";
                rootStatus += Mosaic::windowFocused(ui, Mosaic::currentWindow(ui)) ? "1" : "0";
                rootStatus += rootShortcut ? ", Shortcut: PRESSED" : ", Shortcut: ...";
                Mosaic::text(ui, rootStatus);
                Mosaic::LayoutOptions windowLayout;
                windowLayout.width = Mosaic::SizeRule::Fill;
                windowLayout.height = Mosaic::Dimension::fixed(238.f);
                {
                    auto windowA = Mosaic::scrollArea(ui, "WindowA", Mosaic::ScrollOptions{}, windowLayout);
                    Mosaic::text(ui, "Press Ctrl+A and see who receives it!");
                    Mosaic::separator(ui);
                    Mosaic::text(ui, "(in WindowA)");
                    bool windowShortcut = Mosaic::shortcut(ui, selectAll, routeOptions);
                    Mosaic::String windowStatus = "IsWindowFocused: ";
                    windowStatus += Mosaic::scopeFocused(ui, windowA.id()) ? "1" : "0";
                    windowStatus += windowShortcut ? ", Shortcut: PRESSED" : ", Shortcut: ...";
                    Mosaic::text(ui, windowStatus);
                    Mosaic::LayoutOptions childLayout;
                    childLayout.width = Mosaic::SizeRule::Fill;
                    childLayout.height = Mosaic::Dimension::fixed(64.f);
                    {
                        auto childD = Mosaic::scrollArea(ui, "ChildD", Mosaic::ScrollOptions{}, childLayout);
                        Mosaic::String childStatus = "(in ChildD: not using same Shortcut)\nIsWindowFocused: ";
                        childStatus += Mosaic::scopeFocused(ui, childD.id()) ? "1" : "0";
                        Mosaic::text(ui, childStatus);
                    }
                    {
                        auto childE = Mosaic::scrollArea(ui, "ChildE", Mosaic::ScrollOptions{}, childLayout);
                        bool childShortcut = Mosaic::shortcut(ui, selectAll, routeOptions);
                        Mosaic::String childStatus = "(in ChildE: using same Shortcut)\nIsWindowFocused: ";
                        childStatus += Mosaic::scopeFocused(ui, childE.id()) ? "1" : "0";
                        childStatus += childShortcut ? ", Shortcut: PRESSED" : ", Shortcut: ...";
                        Mosaic::text(ui, childStatus);
                    }
                    Mosaic::Response popupButton = Mosaic::button(ui, "Open Popup");
                    Mosaic::PopupOptions popupOptions;
                    popupOptions.owner = popupButton.id;
                    (void)Mosaic::debugBounds(ui, popupButton.id, &popupOptions.anchor);
                    popupOptions.placement = Mosaic::PopupPlacement::Below;

                    if(popupButton.clicked() == true)
                    {
                        Mosaic::openPopup(ui, Mosaic::Key("PopupF"), popupOptions);
                    }

                    auto popup = Mosaic::popup(ui, Mosaic::Key("PopupF"), popupOptions);

                    if(popup.visible() == true)
                    {
                        Mosaic::text(ui, "(in PopupF)");
                        bool popupShortcut = Mosaic::shortcut(ui, selectAll, routeOptions);
                        Mosaic::String popupStatus = "IsWindowFocused: ";
                        popupStatus += Mosaic::scopeFocused(ui, popup.id()) ? "1" : "0";
                        popupStatus += popupShortcut ? ", Shortcut: PRESSED" : ", Shortcut: ...";
                        Mosaic::text(ui, popupStatus);
                    }
                }
            }
        }
        {
            auto cursors = Mosaic::treeNode(ui, Mosaic::Key("mouse cursors"), "Mouse Cursors");

            if(cursors.expanded() == true)
            {
                Mosaic::String currentCursor = "Current mouse cursor: ";
                currentCursor += Detail::cursorShapeName(Mosaic::cursorShape(ui));
                Mosaic::text(ui, currentCursor);
                {
                    auto disabled = Mosaic::disabledScope(ui, true);
                    Mosaic::checkbox(ui, "io.BackendFlags: HasMouseCursors", &m_backendFlags[0]);
                }
                Mosaic::text(ui, "Hover to see mouse cursors:");
                Mosaic::helpMarker(ui, "The platform adapter applies CursorShape requested by the hovered item.");
                constexpr Mosaic::Array<Mosaic::StringView, 11> labels = {"Arrow", "TextInput", "ResizeAll", "ResizeNS", "ResizeEW", "ResizeNESW", "ResizeNWSE", "Hand", "Wait", "Progress", "NotAllowed"};
                constexpr Mosaic::Array<Mosaic::CursorShape, 11> shapes = {Mosaic::CursorShape::Arrow, Mosaic::CursorShape::Text, Mosaic::CursorShape::ResizeAll, Mosaic::CursorShape::ResizeVertical, Mosaic::CursorShape::ResizeHorizontal, Mosaic::CursorShape::ResizeDiagonalNesw, Mosaic::CursorShape::ResizeDiagonalNwse, Mosaic::CursorShape::Hand, Mosaic::CursorShape::Wait, Mosaic::CursorShape::Progress, Mosaic::CursorShape::NotAllowed};
                for(size_t index = 0; index != labels.size(); ++index)
                {
                    auto item = Mosaic::scope(ui, Mosaic::Key(index));
                    Mosaic::String label = "Mouse cursor ";
                    label += Detail::demoNumber(index);
                    label += ": ";
                    label += labels[index];
                    Mosaic::Response response = Mosaic::selectable(ui, label);
                    Mosaic::setItemCursor(ui, response.id, shapes[index]);
                }
            }
        }
        {
            auto tabbing = Mosaic::treeNode(ui, Mosaic::Key("tabbing"), "Tabbing");

            if(tabbing.expanded() == true)
            {
                Mosaic::text(ui, "Use Tab/Shift+Tab to cycle through keyboard editable fields.");
                Mosaic::inputText(ui, "1", &m_tabbingInputs[0]);
                Mosaic::inputText(ui, "2", &m_tabbingInputs[1]);
                Mosaic::inputText(ui, "3", &m_tabbingInputs[2]);
                {
                    Mosaic::TextInputOptions options;
                    options.tabStop = false;
                    Mosaic::inputText(ui, "4 (tab skip)", &m_tabbingInputs[3], options);
                }
                Mosaic::helpMarker(ui, "This catalog marks the field as a tab-skip example.");
                Mosaic::inputText(ui, "5", &m_tabbingInputs[4]);
            }
        }
        {
            auto focus = Mosaic::treeNode(ui, Mosaic::Key("focus code"), "Focus from code");

            if(focus.expanded() == true)
            {
                bool focusFirst = false;
                bool focusSecond = false;
                bool focusThird = false;
                bool focusX = false;
                bool focusY = false;
                bool focusZ = false;
                {
                    auto controls = Mosaic::row(ui);
                    focusFirst = Mosaic::button(ui, "Focus on 1").clicked();
                    focusSecond = Mosaic::button(ui, "Focus on 2").clicked();
                    focusThird = Mosaic::button(ui, "Focus on 3").clicked();
                }

                if(focusFirst == true)
                {
                    Mosaic::focusNextItem(ui, 0);
                }

                if(focusSecond == true)
                {
                    Mosaic::focusNextItem(ui, 1);
                }

                if(focusThird == true)
                {
                    Mosaic::focusNextItem(ui, 2);
                }

                Mosaic::Response first = Mosaic::inputText(ui, "1", &m_focusInputs[0]);
                Mosaic::Response second = Mosaic::inputText(ui, "2", &m_focusInputs[1]);
                Mosaic::TextInputOptions tabSkip;
                tabSkip.tabStop = false;
                Mosaic::Response third = Mosaic::inputText(ui, "3 (tab skip)", &m_focusInputs[2], tabSkip);
                Mosaic::helpMarker(ui, "The third field is shown as the programmatic tab-skip target.");
                Mosaic::String focused = "Item with focus: ";
                focused += first.focused() ? "1" : second.focused() ? "2" : third.focused() ? "3" : "<none>";
                Mosaic::text(ui, focused);
                {
                    auto controls = Mosaic::row(ui);
                    focusX = Mosaic::button(ui, "Focus on X").clicked();
                    focusY = Mosaic::button(ui, "Focus on Y").clicked();
                    focusZ = Mosaic::button(ui, "Focus on Z").clicked();
                }

                if(focusX == true)
                {
                    Mosaic::focusNextItem(ui, 0);
                }

                if(focusY == true)
                {
                    Mosaic::focusNextItem(ui, 1);
                }

                if(focusZ == true)
                {
                    Mosaic::focusNextItem(ui, 2);
                }

                {
                    auto vector = Mosaic::scope(ui, Mosaic::Key("Focus Float3"));
                    auto components = Mosaic::grid(ui, 3);
                    Mosaic::SliderOptions componentOptions;
                    componentOptions.minimum = 0.0;
                    componentOptions.maximum = 1.0;
                    componentOptions.dragSpeed = 0.002;
                    componentOptions.precision = 3;
                    {
                        auto component = Mosaic::scope(ui, Mosaic::Key(0));
                        Mosaic::dragValue(ui, "X", &m_vector[0], componentOptions);
                    }
                    {
                        auto component = Mosaic::scope(ui, Mosaic::Key(1));
                        Mosaic::dragValue(ui, "Y", &m_vector[1], componentOptions);
                    }
                    {
                        auto component = Mosaic::scope(ui, Mosaic::Key(2));
                        Mosaic::dragValue(ui, "Z", &m_vector[2], componentOptions);
                    }
                }
                Mosaic::text(ui, "Cursor and selection are preserved when refocusing the last used text item.");
            }
        }
        {
            auto dragging = Mosaic::treeNode(ui, Mosaic::Key("dragging"), "Dragging");

            if(dragging.expanded() == true)
            {
                Mosaic::text(ui, "PointerState exposes drag delta for any captured widget.");
                const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
                for(uint8_t button = 0; button != 3; ++button)
                {
                    Mosaic::String state = "IsMouseDragging(";
                    state += Detail::demoNumber(button);
                    state += "):\n  w/ default threshold: ";
                    bool down = pointer != nullptr && (pointer->down & (1U << button)) != 0;
                    float distance = pointer == nullptr ? 0.f : std::hypot(pointer->position.x - pointer->pressPosition(static_cast<Mosaic::PointerButton>(button)).x, pointer->position.y - pointer->pressPosition(static_cast<Mosaic::PointerButton>(button)).y);
                    state += down && distance > Mosaic::getTheme(ui).behavior.dragThreshold ? "1" : "0";
                    state += ",\n  w/ zero threshold: ";
                    state += down && distance > 0.f ? "1" : "0";
                    state += ",\n  w/ large threshold: ";
                    state += down && distance > 20.f ? "1" : "0";
                    Mosaic::text(ui, state);
                }
                Mosaic::Response dragButton = Mosaic::button(ui, "Drag Me");
                Mosaic::String dragDelta = "GetMouseDragDelta(0):\n  w/ default threshold: (";
                Mosaic::Vec2 delta = pointer == nullptr ? Mosaic::Vec2{} : pointer->position - pointer->pressPosition();
                float distance = std::hypot(delta.x, delta.y);
                Mosaic::Vec2 defaultDelta = distance > Mosaic::getTheme(ui).behavior.dragThreshold ? delta : Mosaic::Vec2{};
                dragDelta += Detail::demoFixed(defaultDelta.x, 1);
                dragDelta += ", ";
                dragDelta += Detail::demoFixed(defaultDelta.y, 1);
                dragDelta += ")\n  w/ zero threshold: (";
                dragDelta += Detail::demoFixed(delta.x, 1);
                dragDelta += ", ";
                dragDelta += Detail::demoFixed(delta.y, 1);
                dragDelta += ")\nio.MouseDelta: (";
                dragDelta += Detail::demoFixed(pointer == nullptr ? 0.f : pointer->delta.x, 1);
                dragDelta += ", ";
                dragDelta += Detail::demoFixed(pointer == nullptr ? 0.f : pointer->delta.y, 1);
                dragDelta += ")";
                Mosaic::text(ui, dragDelta);

                if(dragButton.active() == true && pointer != nullptr)
                {
                    Mosaic::LayoutOptions overlayLayout;
                    overlayLayout.width = Mosaic::Dimension::fixed(1.f);
                    overlayLayout.height = Mosaic::Dimension::fixed(1.f);
                    Mosaic::Canvas overlay = Mosaic::canvas(ui, "Dragging foreground", overlayLayout);
                    overlay.setLayer(Mosaic::CanvasLayer::Foreground);
                    Mosaic::Rect origin;
                    if(Mosaic::debugBounds(ui, overlay.id(), &origin) == false)
                    {
                        return;
                    }

                    Mosaic::Vec2 canvasOrigin = {origin.x, origin.y};
                    overlay.line(pointer->pressPosition() - canvasOrigin, pointer->position - canvasOrigin, 4.f, Mosaic::getTheme(ui).colors.button);
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawDemoContents(Mosaic::Context * ui)
    {
        HelloDemo::drawHelp(ui);
        HelloDemo::drawConfiguration(ui);
        HelloDemo::drawWindowOptions(ui);
        HelloDemo::drawWidgets(ui);
        HelloDemo::drawLayout(ui);
        HelloDemo::drawPopups(ui);
        HelloDemo::drawTables(ui);
        HelloDemo::drawInputs(ui);
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawDemoWindow(Mosaic::Context * ui)
    {
        if(m_showDemoWindow == false)
        {
            return;
        }

        Mosaic::WindowOptions options;
        options.open = m_noClose ? nullptr : &m_showDemoWindow;
        options.collapsed = m_noCollapse ? nullptr : &m_demoCollapsed;
        options.initialBounds = {650.f, 20.f, 550.f, 680.f};
        options.minimumSize = {420.f, 320.f};
        options.movable = m_noMove == false;
        options.resizable = m_noResize == false;
        options.navigation = m_noNavigation == false;
        options.bringToFront = m_noBringToFront == false;
        options.dockable = m_noDocking == false;
        options.titleBar = m_noTitleBar == false;
        options.background = m_noBackground == false;
        options.unsavedDocument = m_unsavedDocument;
        options.scrollable = true;
        options.scroll.visibility = m_noScrollbar ? Mosaic::ScrollbarVisibility::Hidden : Mosaic::ScrollbarVisibility::Automatic;
        options.collapsePlacement = Mosaic::WindowCollapsePlacement::Left;
        auto window = Mosaic::window(ui, Mosaic::Key("Mosaic Demo"), "Mosaic Demo", options);

        if(window.visible() == false)
        {
            return;
        }

        if(m_noMenu == false)
        {
            HelloDemo::drawMenuBar(ui);
        }

        Mosaic::text(ui, "Mosaic says hello! (0.1.0)");
        Mosaic::Rect bounds;
        if(Mosaic::windowBounds(ui, window.id(), &bounds) == false)
        {
            return;
        }

        const Mosaic::Theme & theme = Mosaic::getTheme(ui);
        float contentWidth = std::max(1.f, bounds.width - theme.metrics.padding * 2.f - theme.metrics.scrollbarWidth);
        float labelWidth = std::min(theme.metrics.fontSize * 12.f, contentWidth * 0.4f);
        Mosaic::pushItemWidth(ui, -labelWidth);
        HelloDemo::drawDemoContents(ui);
        Mosaic::popItemWidth(ui);
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawHelloWindow(Mosaic::Context * ui)
    {
        Mosaic::WindowOptions options;
        options.collapsed = &m_helloCollapsed;
        options.initialBounds = {60.f, 60.f, 370.f, 180.f};
        options.minimumSize = {300.f, 160.f};
        options.dockGroup = 1;
        options.fitContentHeight = true;
        options.collapsePlacement = Mosaic::WindowCollapsePlacement::Left;
        auto window = Mosaic::window(ui, "Hello, world!", options);

        if(window.visible() == false)
        {
            return;
        }

        auto body = Mosaic::column(ui);
        Mosaic::text(ui, "This is some useful text.");
        Mosaic::checkbox(ui, "Demo Window", &m_showDemoWindow);
        Mosaic::checkbox(ui, "Another Window", &m_showAnotherWindow);
        Mosaic::SliderOptions sliderOptions;
        sliderOptions.minimum = 0.0;
        sliderOptions.maximum = 1.0;
        sliderOptions.labelPlacement = Mosaic::LabelPlacement::After;
        Mosaic::slider(ui, "float", &m_helloFloat, sliderOptions);
        Mosaic::ColorEditOptions colorOptions;
        colorOptions.labelPlacement = Mosaic::LabelPlacement::After;
        colorOptions.showInputs = true;
        Mosaic::colorEditorRgb(ui, "clear color", &m_clearColor, colorOptions);
        {
            auto row = Mosaic::row(ui);

            if(Mosaic::button(ui, "Button").clicked() == true)
            {
                ++m_helloButtonClicks;
            }

            Mosaic::String counter = "counter = ";
            counter += Detail::demoNumber(m_helloButtonClicks);
            Mosaic::text(ui, counter);
        }
        double frameMilliseconds = static_cast<double>(m_deltaTime) * 1000.0;
        double framesPerSecond = m_deltaTime > 0.f ? 1.0 / m_deltaTime : 0.0;
        Mosaic::String performance = "Application average ";
        performance += Detail::demoFixed(frameMilliseconds, 3);
        performance += " ms/frame (";
        performance += Detail::demoFixed(framesPerSecond, 1);
        performance += " FPS)";
        Mosaic::text(ui, performance);
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawAnotherWindow(Mosaic::Context * ui)
    {
        if(m_showAnotherWindow == false)
        {
            return;
        }

        Mosaic::WindowOptions options;
        options.open = &m_showAnotherWindow;
        options.collapsed = &m_anotherCollapsed;
        options.initialBounds = {60.f, 300.f, 310.f, 110.f};
        options.dockGroup = 1;
        auto window = Mosaic::window(ui, "Another Window", options);

        if(window.visible() == false)
        {
            return;
        }

        Mosaic::text(ui, "Hello from another window!");

        if(Mosaic::button(ui, "Close Me").clicked() == true)
        {
            m_showAnotherWindow = false;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawCustomRendering(Mosaic::Context * ui)
    {
        if(m_showCustomRendering == false)
        {
            return;
        }

        Mosaic::WindowOptions options;
        options.open = &m_showCustomRendering;
        options.collapsed = &m_customCollapsed;
        options.initialBounds = {100.f, 150.f, 760.f, 600.f};
        options.dockable = false;
        auto window = Mosaic::window(ui, "Example: Custom rendering", options);

        if(window.visible() == false)
        {
            return;
        }

        constexpr Mosaic::Array<Mosaic::StringView, 4> tabs = {"Primitives", "Canvas", "BG/FG draw lists", "Draw Channels"};
        Mosaic::tabs(ui, "Custom rendering tabs", &m_customTab, tabs);

        if(m_customTab == 0)
        {
            Mosaic::text(ui, "Gradients");
            Mosaic::LayoutOptions gradientLayout;
            gradientLayout.width = Mosaic::SizeRule::Fill;
            gradientLayout.height = Mosaic::Dimension::fixed(21.f);
            {
                Mosaic::Canvas gradient = Mosaic::canvas(ui, "Black to white", gradientLayout);
                Mosaic::Rect bounds;
                if(gradient.contentRect(&bounds) == true)
                {
                    gradient.gradient(bounds, {0.f, 0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 1.f});
                }
            }
            {
                Mosaic::Canvas gradient = Mosaic::canvas(ui, "Green to red", gradientLayout);
                Mosaic::Rect bounds;
                if(gradient.contentRect(&bounds) == true)
                {
                    gradient.gradient(bounds, {0.f, 1.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {0.f, 1.f, 0.f, 1.f});
                }
            }

            Mosaic::text(ui, "All primitives");
            Mosaic::SliderOptions sizeOptions;
            sizeOptions.minimum = 2.0;
            sizeOptions.maximum = 100.0;
            sizeOptions.step = 0.2;
            Mosaic::dragValue(ui, "Size", &m_customPrimitiveSize, sizeOptions);
            Mosaic::SliderOptions thicknessOptions;
            thicknessOptions.minimum = 1.0;
            thicknessOptions.maximum = 8.0;
            thicknessOptions.step = 0.05;
            Mosaic::dragValue(ui, "Thickness", &m_customThickness, thicknessOptions);
            Mosaic::slider(ui, "N-gon sides", &m_customNgonSides, int32_t{3}, int32_t{12});
            {
                auto row = Mosaic::row(ui);
                Mosaic::checkbox(ui, Mosaic::Key("circle segment override enabled"), {}, &m_customCircleSegmentsOverride);
                Mosaic::slider(ui, "Circle segments override", &m_customCircleSegments, int32_t{3}, int32_t{40});
            }
            {
                auto row = Mosaic::row(ui);
                Mosaic::checkbox(ui, Mosaic::Key("curve segment override enabled"), {}, &m_customCurveSegmentsOverride);
                Mosaic::slider(ui, "Curves segments override", &m_customCurveSegments, int32_t{3}, int32_t{40});
            }
            Mosaic::colorEditorRgba(ui, "Color", &m_customColor);

            Mosaic::LayoutOptions primitiveLayout;
            primitiveLayout.width = Mosaic::SizeRule::Fill;
            primitiveLayout.height = Mosaic::Dimension::fixed(340.f);
            Mosaic::Canvas canvas = Mosaic::canvas(ui, "All primitives canvas", primitiveLayout);
            Mosaic::Rect bounds;
            if(canvas.contentRect(&bounds) == false)
            {
                return;
            }

            canvas.rect(bounds, Mosaic::Color::fromBytes(20, 20, 20));
            float size = std::clamp(m_customPrimitiveSize, 10.f, 64.f);
            float spacing = 12.f;
            for(int32_t row = 0; row != 3; ++row)
            {
                float y = 12.f + static_cast<float>(row) * (size + spacing);
                float x = 12.f;
                float thickness = row == 0 ? 1.f : m_customThickness;
                int32_t circleSides = m_customCircleSegmentsOverride ? m_customCircleSegments : 20;
                Mosaic::Vec2 circleCenter = {x + size * 0.5f, y + size * 0.5f};

                if(row == 1)
                {
                    canvas.circleFilled(circleCenter, size * 0.5f, m_customColor, static_cast<uint32_t>(circleSides));
                }
                else
                {
                    canvas.circle(circleCenter, size * 0.5f, thickness, m_customColor, static_cast<uint32_t>(circleSides));
                }

                x += size + spacing;

                if(row == 1)
                {
                    canvas.rect({x, y, size, size}, m_customColor);
                }
                else
                {
                    Mosaic::Vec2Quad square = {{{x, y}, {x + size, y}, {x + size, y + size}, {x, y + size}}};
                    canvas.polyline(square, thickness, m_customColor, true);
                }

                x += size + spacing;
                canvas.roundedRect({x, y, size, size}, size * 0.2f, m_customColor);
                x += size + spacing;
                Mosaic::Vec2 triangleFirst = {x + size * 0.5f, y};
                Mosaic::Vec2 triangleSecond = {x + size, y + size};
                Mosaic::Vec2 triangleThird = {x, y + size};

                if(row == 1)
                {
                    canvas.triangleFilled(triangleFirst, triangleSecond, triangleThird, m_customColor);
                }
                else
                {
                    canvas.triangle(triangleFirst, triangleSecond, triangleThird, thickness, m_customColor);
                }

                x += size + spacing;
                canvas.line({x, y}, {x + size, y + size}, thickness, m_customColor);
                x += size + spacing;
                int32_t curveSegments = m_customCurveSegmentsOverride ? m_customCurveSegments : 20;
                canvas.bezierCubic({x, y + size * 0.7f}, {x + size * 0.25f, y}, {x + size * 0.5f, y + size}, {x + size, y + size * 0.25f}, thickness, m_customColor, static_cast<uint32_t>(curveSegments));
                x += size + spacing;
                constexpr float polygonRotation = -1.57079632679f;

                if(row == 1)
                {
                    canvas.regularPolygonFilled({x + size * 0.5f, y + size * 0.5f}, size * 0.5f, static_cast<uint32_t>(m_customNgonSides), polygonRotation, m_customColor);
                }
                else
                {
                    canvas.regularPolygon({x + size * 0.5f, y + size * 0.5f}, size * 0.5f, static_cast<uint32_t>(m_customNgonSides), polygonRotation, thickness, m_customColor);
                }

                if(x + size >= bounds.width)
                {
                    break;
                }
            }
            {
                float y = 12.f + 3.f * (size + spacing);
                float x = 12.f;
                int32_t curveSegments = m_customCurveSegmentsOverride ? m_customCurveSegments : 20;
                canvas.ellipse({x + size * 0.5f, y + size * 0.5f}, {size * 0.5f, size * 0.3f}, 0.35f, m_customThickness, m_customColor, static_cast<uint32_t>(curveSegments));
                x += size + spacing;
                canvas.ellipseFilled({x + size * 0.5f, y + size * 0.5f}, {size * 0.5f, size * 0.3f}, -0.35f, m_customColor, static_cast<uint32_t>(curveSegments));
                x += size + spacing;
                canvas.bezierQuadratic({x, y + size}, {x + size * 0.5f, y - size * 0.2f}, {x + size, y + size}, m_customThickness, m_customColor, static_cast<uint32_t>(curveSegments));
                x += size + spacing;
                Mosaic::Array<Mosaic::Vec2, 6> concave = {{{x, y + size * 0.2f}, {x + size, y + size * 0.2f}, {x + size * 0.62f, y + size * 0.5f}, {x + size, y + size * 0.8f}, {x, y + size * 0.8f}, {x + size * 0.38f, y + size * 0.5f}}};
                canvas.concavePolygon(concave, m_customColor);
                x += size + spacing;
                Mosaic::Rect textClip = {x, y, size * 2.f, size};
                canvas.pushClip(textClip);
                Mosaic::Vec2 textSize;
                (void)canvas.text({x, y + size * 0.15f}, "Canvas text is clipped", m_customColor, &textSize);
                canvas.popClip();
                canvas.polyline(Mosaic::Vec2Quad{{{textClip.x, textClip.y}, {textClip.right(), textClip.y}, {textClip.right(), textClip.bottom()}, {textClip.x, textClip.bottom()}}}, 1.f, Mosaic::Color::fromBytes(120, 120, 120), true);
            }
        }
        else if(m_customTab == 1)
        {
            Mosaic::checkbox(ui, "Enable grid", &m_customEnableGrid);
            Mosaic::checkbox(ui, "Enable context menu", &m_customEnableContextMenu);
            Mosaic::text(ui, "Mouse Left: drag to add lines,\nMouse Right: drag to scroll, click for context menu.");
            {
                auto row = Mosaic::row(ui);

                if(Mosaic::button(ui, "Remove one").clicked() == true && m_canvasPoints.size() >= 2)
                {
                    m_canvasPoints.resize(m_canvasPoints.size() - 2);
                }

                if(Mosaic::button(ui, "Remove all").clicked() == true)
                {
                    m_canvasPoints.clear();
                }
            }
            Mosaic::LayoutOptions canvasLayout;
            canvasLayout.width = Mosaic::SizeRule::Fill;
            canvasLayout.height = Mosaic::Dimension::fixed(410.f);
            Mosaic::Id canvasId = Mosaic::InvalidId;
            {
                Mosaic::Canvas canvas = Mosaic::canvas(ui, "canvas", canvasLayout);
                canvasId = canvas.id();
                Mosaic::Rect bounds;
                if(canvas.contentRect(&bounds) == false)
                {
                    return;
                }

                canvas.rect(bounds, Mosaic::Color::fromBytes(50, 50, 50));

                if(m_customEnableGrid == true)
                {
                    float firstX = std::fmod(m_customCanvasPan.x, 64.f);
                    float firstY = std::fmod(m_customCanvasPan.y, 64.f);

                    if(firstX < 0.f)
                    {
                        firstX += 64.f;
                    }

                    if(firstY < 0.f)
                    {
                        firstY += 64.f;
                    }

                    for(float x = firstX; x <= bounds.width; x += 64.f)
                    {
                        canvas.line({x, 0.f}, {x, bounds.height}, 1.f, Mosaic::Color::fromBytes(200, 200, 200, 40));
                    }
                    for(float y = firstY; y <= bounds.height; y += 64.f)
                    {
                        canvas.line({0.f, y}, {bounds.width, y}, 1.f, Mosaic::Color::fromBytes(200, 200, 200, 40));
                    }
                }

                const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
                Mosaic::Rect canvasBounds;
                bool hasCanvasBounds = Mosaic::debugBounds(ui, canvas.id(), &canvasBounds);
                bool hovered = pointer != nullptr && hasCanvasBounds == true && canvasBounds.contains(pointer->position) == true;

                if(pointer != nullptr && hovered == true && pointer->isPressed() == true)
                {
                    Mosaic::Vec2 local;
                    if(canvas.localPointerPosition(&local) == true)
                    {
                        local = local - m_customCanvasPan;
                        m_canvasPoints.push_back(local);
                        m_canvasPoints.push_back(local);
                        m_customAddingLine = true;
                    }
                }

                if(pointer != nullptr && m_customAddingLine == true && m_canvasPoints.empty() == false)
                {
                    Mosaic::Vec2 local;
                    if(canvas.localPointerPosition(&local) == true)
                    {
                        m_canvasPoints.back() = local - m_customCanvasPan;
                    }

                    if(pointer->isReleased() == true || pointer->isDown() == false)
                    {
                        m_customAddingLine = false;
                    }
                }

                if(pointer != nullptr && hovered == true && pointer->isPressed(Mosaic::PointerButton::Secondary) == true)
                {
                    m_customCanvasPanning = true;
                    m_customCanvasPanStart = m_customCanvasPan;
                }

                if(pointer != nullptr && m_customCanvasPanning == true)
                {
                    if(pointer->isDown(Mosaic::PointerButton::Secondary) == true)
                    {
                        m_customCanvasPan = m_customCanvasPanStart + (pointer->position - pointer->pressPosition(Mosaic::PointerButton::Secondary));
                    }
                    else
                    {
                        Mosaic::Vec2 drag = pointer->position - pointer->pressPosition(Mosaic::PointerButton::Secondary);

                        if(m_customEnableContextMenu == true && hovered == true && std::abs(drag.x) < 3.f && std::abs(drag.y) < 3.f)
                        {
                            Mosaic::PopupOptions popupOptions;
                            popupOptions.owner = canvas.id();
                            popupOptions.placement = Mosaic::PopupPlacement::Cursor;
                            Mosaic::openPopup(ui, Mosaic::Key("canvas context menu"), popupOptions);
                        }

                        m_customCanvasPanning = false;
                    }
                }

                for(size_t index = 0; index + 1 < m_canvasPoints.size(); index += 2)
                {
                    canvas.line(m_canvasPoints[index] + m_customCanvasPan, m_canvasPoints[index + 1] + m_customCanvasPan, 2.f, Mosaic::Color::fromBytes(255, 255, 0));
                }
            }
            Mosaic::PopupOptions popupOptions;
            popupOptions.owner = canvasId;
            popupOptions.placement = Mosaic::PopupPlacement::Cursor;
            auto context = Mosaic::popup(ui, Mosaic::Key("canvas context menu"), popupOptions);

            if(context.visible() == true)
            {
                if(Mosaic::menuItem(ui, "Remove one").clicked() == true && m_canvasPoints.size() >= 2)
                {
                    m_canvasPoints.resize(m_canvasPoints.size() - 2);
                    Mosaic::closeCurrentPopup(ui);
                }

                if(Mosaic::menuItem(ui, "Remove all").clicked() == true)
                {
                    m_canvasPoints.clear();
                    Mosaic::closeCurrentPopup(ui);
                }

                if(Mosaic::menuItem(ui, "Reset view").clicked() == true)
                {
                    m_customCanvasPan = {};
                    Mosaic::closeCurrentPopup(ui);
                }
            }
        }
        else if(m_customTab == 2)
        {
            Mosaic::checkbox(ui, "Draw in Background draw list", &m_customDrawBackground);
            Mosaic::helpMarker(ui, "The background draw list is rendered below every UI window.");
            Mosaic::checkbox(ui, "Draw in Foreground draw list", &m_customDrawForeground);
            Mosaic::helpMarker(ui, "The foreground draw list is rendered over every UI window.");
            Mosaic::text(ui, "Background and foreground canvases keep local coordinates, but are ordered below or "
                             "above every window.");
            Mosaic::LayoutOptions previewLayout;
            previewLayout.width = Mosaic::SizeRule::Fill;
            previewLayout.height = Mosaic::Dimension::fixed(260.f);
            Mosaic::Canvas preview = Mosaic::canvas(ui, "BG FG preview", previewLayout);
            Mosaic::Rect bounds;
            if(preview.contentRect(&bounds) == false)
            {
                return;
            }

            preview.rect(bounds, Mosaic::Color::fromBytes(22, 24, 28));

            if(m_customDrawBackground == true)
            {
                preview.circle({bounds.width * 0.42f, bounds.height * 0.5f}, 100.f, 14.f, Mosaic::Color::fromBytes(255, 0, 0, 200));
            }

            if(m_customDrawForeground == true)
            {
                preview.circle({bounds.width * 0.58f, bounds.height * 0.5f}, 86.f, 10.f, Mosaic::Color::fromBytes(0, 255, 0, 200));
            }

            Mosaic::LayoutOptions layerLayout;
            layerLayout.width = Mosaic::Dimension::fixed(1.f);
            layerLayout.height = Mosaic::Dimension::fixed(1.f);
            Mosaic::Rect windowScreen;
            if(Mosaic::windowBounds(ui, window.id(), &windowScreen) == false)
            {
                return;
            }

            if(m_customDrawBackground == true)
            {
                Mosaic::Canvas backgroundLayer = Mosaic::canvas(ui, "Global background layer", layerLayout);
                backgroundLayer.setLayer(Mosaic::CanvasLayer::Background);
                Mosaic::Rect origin;
                if(Mosaic::debugBounds(ui, backgroundLayer.id(), &origin) == true)
                {
                    backgroundLayer.circle({windowScreen.x + windowScreen.width * 0.22f - origin.x, windowScreen.y + windowScreen.height * 0.5f - origin.y}, 118.f, 16.f, Mosaic::Color::fromBytes(255, 0, 0, 180));
                }
            }

            if(m_customDrawForeground == true)
            {
                Mosaic::Canvas foregroundLayer = Mosaic::canvas(ui, "Global foreground layer", layerLayout);
                foregroundLayer.setLayer(Mosaic::CanvasLayer::Foreground);
                Mosaic::Rect origin;
                if(Mosaic::debugBounds(ui, foregroundLayer.id(), &origin) == true)
                {
                    foregroundLayer.circle({windowScreen.x + windowScreen.width * 0.78f - origin.x, windowScreen.y + windowScreen.height * 0.5f - origin.y}, 96.f, 12.f, Mosaic::Color::fromBytes(0, 255, 0, 200));
                }
            }
        }
        else
        {
            Mosaic::text(ui, "Blue shape is drawn first: appears in back");
            Mosaic::text(ui, "Red shape is drawn after: appears in front");
            Mosaic::LayoutOptions firstLayout;
            firstLayout.width = Mosaic::SizeRule::Fill;
            firstLayout.height = Mosaic::Dimension::fixed(100.f);
            {
                Mosaic::Canvas preview = Mosaic::canvas(ui, "Ordered draw", firstLayout);
                preview.rect({10.f, 10.f, 50.f, 50.f}, Mosaic::Color::fromBytes(0, 0, 255));
                preview.rect({35.f, 35.f, 50.f, 50.f}, Mosaic::Color::fromBytes(255, 0, 0));
            }
            Mosaic::separator(ui);
            Mosaic::text(ui, "Blue shape is drawn first, into channel 1: appears in front");
            Mosaic::text(ui, "Red shape is drawn after, into channel 0: appears in back");
            {
                Mosaic::Canvas preview = Mosaic::canvas(ui, "Reordered draw", firstLayout);
                preview.setChannel(1);
                preview.rect({10.f, 10.f, 50.f, 50.f}, Mosaic::Color::fromBytes(0, 0, 255));
                preview.setChannel(0);
                preview.rect({35.f, 35.f, 50.f, 50.f}, Mosaic::Color::fromBytes(255, 0, 0));
            }
            Mosaic::text(ui, "After reordering, contents of channel 0 appears below channel 1.");
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::draw(Mosaic::Context * ui, float deltaTime)
    {
        Mosaic::FrameCaptureOptions captureOptions;
        captureOptions.debug = m_showMetrics || m_showIdStack == true || m_showItemPicker;
        captureOptions.metrics = m_showMetrics;
        Mosaic::setFrameCaptureOptions(ui, captureOptions);

        if(m_initialized == false)
        {
            m_configurationFlags[6] = true;
            m_configurationFlags[7] = true;
            m_configurationFlags[9] = true;
            m_configurationFlags[11] = true;
            m_configurationFlags[26] = true;
            m_initialized = true;
        }

        Mosaic::Configuration configuration;
        configuration.pointerInput = m_configurationFlags[0] == false;
        configuration.cursorChanges = m_configurationFlags[1] == false;
        configuration.keyboardInput = m_configurationFlags[2] == false;
        configuration.keyboardNavigation = m_navKeyboard;
        configuration.navigationCapturesKeyboard = m_configurationFlags[6];
        configuration.navigationCursorVisibleAuto = m_configurationFlags[9];
        configuration.navigationCursorVisibleAlways = m_configurationFlags[10];
        configuration.escapeClearsItemFocus = m_configurationFlags[7];
        configuration.escapeClearsWindowFocus = m_configurationFlags[8];
        configuration.dockingEnabled = m_configurationFlags[11];
        configuration.dockingNoSplit = m_configurationFlags[12];
        configuration.dockingNoMerge = m_configurationFlags[13];
        configuration.dockingWithShift = m_configurationFlags[14];
        configuration.windowResizeFromEdges = m_configurationSecondary[4];
        configuration.windowMoveFromTitleBarOnly = m_configurationSecondary[5];
        configuration.scrollbarScrollByPage = m_configurationFlags[24];
        configuration.inputTextCursorBlink = m_configurationSecondary[6];
        configuration.inputTextEnterKeepActive = m_configurationFlags[25];
        configuration.dragClickToInputText = m_configurationSecondary[7];
        configuration.windowCopyContentsWithPrimaryC = m_configurationFlags[23];
        Mosaic::setConfiguration(ui, configuration);
        m_deltaTime = std::max(deltaTime, 0.0001f);

        if(m_themeInitialized == false)
        {
            m_demoTheme = Detail::mosaicTheme(Mosaic::getTheme(ui));
            m_themeInitialized = true;
        }

        m_demoTheme.behavior.hoverEnabled = m_hoverEnabled;
        m_demoTheme.behavior.animationsEnabled = m_animationsEnabled;
        auto style = Mosaic::styleScope(ui, m_demoTheme);

        HelloDemo::drawDemoWindow(ui);
        HelloDemo::drawHelloWindow(ui);
        HelloDemo::drawAnotherWindow(ui);
        HelloDemo::drawExampleWindows(ui);

        if(m_showSimpleLayout == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showSimpleLayout;
            options.collapsed = &m_layoutCollapsed;
            options.initialBounds = {180.f, 180.f, 520.f, 360.f};
            options.dockable = false;
            auto layoutWindow = Mosaic::window(ui, "Example: Simple layout", options);

            if(layoutWindow.visible() == true)
            {
                Mosaic::text(ui, "Simple layout");
                constexpr Mosaic::Array<Mosaic::StringView, 8> objects = {"MyObject 0", "MyObject 1", "MyObject 2", "MyObject 3", "MyObject 4", "MyObject 5", "MyObject 6", "MyObject 7"};
                Mosaic::SplitOptions splitOptions;
                splitOptions.minimumFirst = 120.f;
                splitOptions.minimumSecond = 220.f;
                Mosaic::LayoutOptions splitLayout;
                splitLayout.width = Mosaic::SizeRule::Fill;
                splitLayout.height = Mosaic::Dimension::fixed(270.f);
                auto split = Mosaic::split(ui, "Simple layout split", Mosaic::Orientation::Horizontal, &m_splitRatio, splitOptions, splitLayout);
                Mosaic::listBox(ui, "Objects", &m_list, objects, Detail::fillLayout());
                {
                    auto details = Mosaic::column(ui, Detail::fillLayout());
                    constexpr Mosaic::Array<Mosaic::StringView, 2> tabs = {"Description", "Details"};
                    Mosaic::tabs(ui, "Object tabs", &m_objectTab, tabs);
                    Mosaic::text(ui, "Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
                    auto actions = Mosaic::row(ui);
                    Mosaic::button(ui, "Revert");
                    Mosaic::button(ui, "Save");
                }
            }
        }

        HelloDemo::drawCustomRendering(ui);

        if(m_queryTitleWindow == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_queryTitleWindow;
            options.initialBounds = {420.f, 260.f, 390.f, 150.f};
            options.dockable = false;
            auto window = Mosaic::window(ui, "Title bar Hovered/Active tests", options);
            Mosaic::Response titleResponse;
            (void)Mosaic::itemResponse(ui, window.id(), &titleResponse);
            const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
            Mosaic::PopupOptions popupOptions;
            popupOptions.owner = window.id();
            popupOptions.placement = Mosaic::PopupPlacement::Cursor;

            if(pointer != nullptr)
            {
                popupOptions.anchor = {pointer->position.x, pointer->position.y, 1.f, 1.f};
            }

            if(titleResponse.hovered() == true && pointer != nullptr && pointer->isPressed(Mosaic::PointerButton::Secondary) == true)
            {
                Mosaic::openPopup(ui, Mosaic::Key("Title bar context"), popupOptions);
            }

            if(window.visible() == true)
            {
                auto popup = Mosaic::popup(ui, Mosaic::Key("Title bar context"), popupOptions);

                if(popup.visible() == true && Mosaic::menuItem(ui, "Close").clicked() == true)
                {
                    m_queryTitleWindow = false;
                    Mosaic::closeCurrentPopup(ui);
                }

                Mosaic::String status = "IsItemHovered() after begin = ";
                status += titleResponse.hovered() ? "1" : "0";
                status += " (== is title bar hovered)\nIsItemActive() after begin = ";
                status += titleResponse.active() ? "1" : "0";
                status += " (== is window being clicked/moved)";
                Mosaic::text(ui, status);
            }
        }

        HelloDemo::drawToolWindows(ui);
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace MosaicExample
