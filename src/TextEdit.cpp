#include "TextEdit.hpp"
#include "Interaction.hpp"
#include "Popup.hpp"
#include "Utility.hpp"
#include "Window.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace Mosaic
{
    namespace Detail
    {
        enum class TextContextAction : uint8_t
        {
            None,
            Undo,
            Redo,
            Cut,
            Copy,
            Paste,
            Delete,
            SelectAll
        };
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] String filterText(StringView value, TextCharacterFilter filter)
        {
            if(filter == TextCharacterFilter::None)
            {
                auto returnedValue = String(value.data(), value.size());

                return returnedValue;
            }

            String filtered;
            filtered.reserve(value.size());
            for(char character : value)
            {
                unsigned char byte = static_cast<unsigned char>(character);

                if(byte >= 0x80U)
                {
                    bool acceptsNonAscii = true;

                    if(filter == TextCharacterFilter::Decimal)
                    {
                        acceptsNonAscii = false;
                    }

                    if(filter == TextCharacterFilter::Scientific)
                    {
                        acceptsNonAscii = false;
                    }

                    if(filter == TextCharacterFilter::Hexadecimal)
                    {
                        acceptsNonAscii = false;
                    }

                    if(filter == TextCharacterFilter::LowercaseOnly)
                    {
                        acceptsNonAscii = false;
                    }

                    if(acceptsNonAscii == true)
                    {
                        filtered.push_back(character);
                    }

                    continue;
                }

                switch(filter)
                {
                case TextCharacterFilter::Decimal:
                {
                    bool accepted = std::isdigit(byte) != 0;
                    switch(character)
                    {
                    case '+':
                    case '-':
                    case '.':
                    case '*':
                    case '/':
                    {
                        accepted = true;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                    }

                    if(accepted == true)
                    {
                        filtered.push_back(character);
                    }

                    break;
                }
                case TextCharacterFilter::Scientific:
                {
                    bool accepted = std::isdigit(byte) != 0;
                    switch(character)
                    {
                    case '+':
                    case '-':
                    case '.':
                    case '*':
                    case '/':
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

                    if(accepted == true)
                    {
                        filtered.push_back(character);
                    }

                    break;
                }
                case TextCharacterFilter::Hexadecimal:
                    if(std::isxdigit(byte) != 0)
                    {
                        filtered.push_back(static_cast<char>(std::toupper(byte)));
                    }

                    break;
                case TextCharacterFilter::Uppercase:
                    filtered.push_back(static_cast<char>(std::toupper(byte)));
                    break;
                case TextCharacterFilter::NoBlank:
                    if(std::isspace(byte) == 0)
                    {
                        filtered.push_back(character);
                    }

                    break;
                case TextCharacterFilter::CasingSwap:
                    if(std::islower(byte) != 0)
                    {
                        filtered.push_back(static_cast<char>(std::toupper(byte)));
                    }
                    else if(std::isupper(byte) != 0)
                    {
                        filtered.push_back(static_cast<char>(std::tolower(byte)));
                    }
                    else
                    {
                        filtered.push_back(character);
                    }

                    break;
                case TextCharacterFilter::LowercaseOnly:
                    if(std::islower(byte) != 0)
                    {
                        filtered.push_back(character);
                    }

                    break;
                case TextCharacterFilter::None:
                    filtered.push_back(character);
                    break;
                }
            }

            return filtered;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t decodeUtf8Character(StringView value, size_t offset, char32_t & character) noexcept
        {
            unsigned char first = static_cast<unsigned char>(value[offset]);

            if(first < 0x80U)
            {
                character = first;

                return 1;
            }

            size_t length = 0;
            char32_t codepoint = 0;

            if((first & 0xe0U) == 0xc0U)
            {
                length = 2;
                codepoint = first & 0x1fU;
            }
            else if((first & 0xf0U) == 0xe0U)
            {
                length = 3;
                codepoint = first & 0x0fU;
            }
            else if((first & 0xf8U) == 0xf0U)
            {
                length = 4;
                codepoint = first & 0x07U;
            }
            else
            {
                character = first;

                return 1;
            }

            if(offset + length > value.size())
            {
                character = first;

                return 1;
            }

            for(size_t index = 1; index != length; ++index)
            {
                unsigned char continuation = static_cast<unsigned char>(value[offset + index]);

                if((continuation & 0xc0U) != 0x80U)
                {
                    character = first;

                    return 1;
                }

                codepoint = static_cast<char32_t>((codepoint << 6U) | (continuation & 0x3fU));
            }
            character = codepoint;

            return length;
        }
        //////////////////////////////////////////////////////////////////////////
        void appendUtf8Character(String & value, char32_t character)
        {
            if(character <= 0x7fU)
            {
                value.push_back(static_cast<char>(character));
            }
            else if(character <= 0x7ffU)
            {
                value.push_back(static_cast<char>(0xc0U | (character >> 6U)));
                value.push_back(static_cast<char>(0x80U | (character & 0x3fU)));
            }
            else if(character <= 0xffffU)
            {
                value.push_back(static_cast<char>(0xe0U | (character >> 12U)));
                value.push_back(static_cast<char>(0x80U | ((character >> 6U) & 0x3fU)));
                value.push_back(static_cast<char>(0x80U | (character & 0x3fU)));
            }
            else if(character <= 0x10ffffU)
            {
                value.push_back(static_cast<char>(0xf0U | (character >> 18U)));
                value.push_back(static_cast<char>(0x80U | ((character >> 12U) & 0x3fU)));
                value.push_back(static_cast<char>(0x80U | ((character >> 6U) & 0x3fU)));
                value.push_back(static_cast<char>(0x80U | (character & 0x3fU)));
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] String filterTextWithCallback(StringView value, const TextInputOptions & options, const TextEditorState & state)
        {
            if(options.callbackCharacterFilter == false)
            {
                auto returnedValue = String(value);

                return returnedValue;
            }

            if(options.callback == nullptr)
            {
                auto returnedValue = String(value);

                return returnedValue;
            }

            String filtered;
            filtered.reserve(value.size());
            for(size_t offset = 0; offset < value.size();)
            {
                char32_t character = U'\0';
                size_t length = Detail::decodeUtf8Character(value, offset, character);
                TextInputCallbackData data;
                data.event = TextInputCallbackEvent::CharacterFilter;
                data.cursor = state.cursor;
                data.anchor = state.anchor;
                data.userData = options.callbackUserData;
                data.character = character;
                options.callback(data);

                if(data.reject == false && data.character != U'\0')
                {
                    Detail::appendUtf8Character(filtered, data.character);
                }

                offset += length;
            }

            return filtered;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t previousUtf8Boundary(StringView value, size_t offset) noexcept
        {
            if(offset == 0)
            {
                return 0;
            }

            --offset;
            while(offset != 0 && (static_cast<unsigned char>(value[offset]) & 0xc0U) == 0x80U)
            {
                --offset;
            }

            return offset;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] String elideTextLeft(Context * ui, StringView value, const Theme & style, float width)
        {
            constexpr StringView ellipsis = "\xe2\x80\xa6";

            if(value.empty() == true)
            {
                auto returnedValue = String(value);

                return returnedValue;
            }

            if(ui->estimateText(value, style).x <= width)
            {
                auto returnedValue = String(value);

                return returnedValue;
            }

            float ellipsisWidth = ui->estimateText(ellipsis, style).x;

            if(width <= ellipsisWidth)
            {
                auto returnedValue = String(ellipsis);

                return returnedValue;
            }

            size_t suffixBegin = value.size();
            size_t bestBegin = suffixBegin;
            String candidate;
            while(suffixBegin != 0)
            {
                suffixBegin = Detail::previousUtf8Boundary(value, suffixBegin);
                candidate.assign(ellipsis.data(), ellipsis.size());
                candidate.append(value.data() + suffixBegin, value.size() - suffixBegin);

                if(ui->estimateText(candidate, style).x > width)
                {
                    break;
                }

                bestBegin = suffixBegin;
            }

            String result(ellipsis);
            result.append(value.data() + bestBegin, value.size() - bestBegin);

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void replaceSelection(String & value, size_t & cursor, size_t & anchor, StringView replacement, size_t maximumBytes)
        {
            size_t begin = std::min(cursor, anchor);
            size_t end = std::max(cursor, anchor);
            size_t replacementSize = replacement.size();

            if(maximumBytes != 0 && value.size() - (end - begin) + replacementSize > maximumBytes)
            {
                replacementSize = maximumBytes > value.size() - (end - begin) ? maximumBytes - (value.size() - (end - begin)) : 0;
                while(replacementSize > 0 && replacementSize < replacement.size() && (static_cast<unsigned char>(replacement[replacementSize]) & 0xc0U) == 0x80U)
                {
                    --replacementSize;
                }
            }

            value.replace(begin, end - begin, replacement.substr(0, replacementSize));
            cursor = begin + replacementSize;
            anchor = cursor;
        }
        //////////////////////////////////////////////////////////////////////////
        void replaceSelectionOrOverwrite(const Context::Node & node, String & value, size_t & cursor, size_t & anchor, StringView replacement, const TextInputOptions & options)
        {
            if(options.alwaysOverwrite == true && cursor == anchor && cursor < value.size() && replacement.empty() == false)
            {
                size_t replacementOffset = 0;
                size_t overwriteEnd = cursor;
                while(replacementOffset < replacement.size() && overwriteEnd < value.size())
                {
                    char32_t character = U'\0';
                    replacementOffset += Detail::decodeUtf8Character(replacement, replacementOffset, character);
                    overwriteEnd = Detail::nextTextPosition(node, value, overwriteEnd, options.password);
                }
                anchor = overwriteEnd;
            }

            Detail::replaceSelection(value, cursor, anchor, replacement, options.maximumBytes);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] TextUndoRecord & undoRecord(TextEditorState & state, size_t position) noexcept
        {
            TextUndoRecord & returnedValue = state.undo[(state.undoBegin + position) % state.undo.size()];

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] const TextUndoRecord & undoRecord(const TextEditorState & state, size_t position) noexcept
        {
            const TextUndoRecord & returnedValue = state.undo[(state.undoBegin + position) % state.undo.size()];

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        void clearUndo(TextEditorState & state, StringView value, double timestamp)
        {
            state.undoValue.assign(value.data(), value.size());
            state.undoBegin = 0;
            state.undoCount = 0;
            state.undoPosition = 0;
            state.lastEditTimestamp = timestamp;
        }
        //////////////////////////////////////////////////////////////////////////
        void recordUndo(TextEditorState & state, const String & value, double timestamp, bool coalesce)
        {
            if(state.undoValue == value)
            {
                return;
            }

            size_t prefix = 0;
            while(prefix != state.undoValue.size() && prefix != value.size() && state.undoValue[prefix] == value[prefix])
            {
                ++prefix;
            }
            size_t suffix = 0;
            while(suffix < state.undoValue.size() - prefix && suffix < value.size() - prefix && state.undoValue[state.undoValue.size() - suffix - 1] == value[value.size() - suffix - 1])
            {
                ++suffix;
            }

            TextUndoRecord current;
            current.offset = prefix;
            current.removed.assign(state.undoValue.data() + prefix, state.undoValue.size() - prefix - suffix);
            current.inserted.assign(value.data() + prefix, value.size() - prefix - suffix);

            bool mayCoalesce = coalesce && state.undoPosition > 0 && state.undoPosition == state.undoCount && timestamp - state.lastEditTimestamp <= 0.6;

            if(mayCoalesce == true)
            {
                TextUndoRecord & previous = Detail::undoRecord(state, state.undoPosition - 1);

                if(previous.removed.empty() == true && current.removed.empty() == true && current.offset == previous.offset + previous.inserted.size())
                {
                    previous.inserted += current.inserted;
                    state.undoValue = value;
                    state.lastEditTimestamp = timestamp;

                    return;
                }

                if(previous.inserted.empty() == true && current.inserted.empty() == true && current.offset + current.removed.size() == previous.offset)
                {
                    previous.offset = current.offset;
                    previous.removed.insert(0, current.removed);
                    state.undoValue = value;
                    state.lastEditTimestamp = timestamp;

                    return;
                }

                if(previous.inserted.empty() == true && current.inserted.empty() == true && current.offset == previous.offset)
                {
                    previous.removed += current.removed;
                    state.undoValue = value;
                    state.lastEditTimestamp = timestamp;

                    return;
                }
            }

            state.undoCount = state.undoPosition;
            constexpr size_t maximumUndo = 128;

            if(state.undoCount == maximumUndo)
            {
                state.undoBegin = (state.undoBegin + 1) % maximumUndo;
                --state.undoCount;
                --state.undoPosition;
            }

            size_t physical = (state.undoBegin + state.undoCount) % maximumUndo;

            if(physical == state.undo.size())
            {
                state.undo.push_back(std::move(current));
            }
            else
            {
                state.undo[physical] = std::move(current);
            }

            ++state.undoCount;
            state.undoPosition = state.undoCount;
            state.undoValue = value;
            state.lastEditTimestamp = timestamp;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool applyUndo(TextEditorState & state, String & value)
        {
            if(state.undoPosition == 0)
            {
                return false;
            }

            const TextUndoRecord & record = Detail::undoRecord(state, state.undoPosition - 1);

            if(record.offset > value.size())
            {
                Detail::clearUndo(state, value, state.lastEditTimestamp);

                return false;
            }

            if(record.inserted.size() > value.size() - record.offset)
            {
                Detail::clearUndo(state, value, state.lastEditTimestamp);

                return false;
            }

            value.replace(record.offset, record.inserted.size(), record.removed);
            --state.undoPosition;
            state.undoValue = value;
            state.cursor = state.anchor = record.offset + record.removed.size();

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool applyRedo(TextEditorState & state, String & value)
        {
            if(state.undoPosition >= state.undoCount)
            {
                return false;
            }

            const TextUndoRecord & record = Detail::undoRecord(state, state.undoPosition);

            if(record.offset > value.size())
            {
                Detail::clearUndo(state, value, state.lastEditTimestamp);

                return false;
            }

            if(record.removed.size() > value.size() - record.offset)
            {
                Detail::clearUndo(state, value, state.lastEditTimestamp);

                return false;
            }

            value.replace(record.offset, record.removed.size(), record.inserted);
            ++state.undoPosition;
            state.undoValue = value;
            state.cursor = state.anchor = record.offset + record.inserted.size();

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool invokeTextInputCallback(const TextInputOptions & options, TextInputCallbackEvent event, String & value, TextEditorState & state, size_t previousCapacity = 0)
        {
            if(options.callback == nullptr)
            {
                return false;
            }

            size_t previousSize = value.size();
            Id previousHash = Mosaic::hashBytes(value);
            TextInputCallbackData data;
            data.event = event;
            data.value = &value;
            data.capacity = value.capacity();
            data.previousCapacity = event == TextInputCallbackEvent::Resize ? previousCapacity : data.capacity;
            data.cursor = state.cursor;
            data.anchor = state.anchor;
            data.userData = options.callbackUserData;
            options.callback(data);
            state.cursor = std::min(data.cursor, value.size());
            state.anchor = std::min(data.anchor, value.size());
            auto returnedValue = data.changed || value.size() != previousSize || Mosaic::hashBytes(value) != previousHash;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool contextMenuPointerHit(const Context * ui, const Context::Node & node, const PointerState & pointer) noexcept
        {
            const Context::Persistent * persistentState = ui->findState(node.id);
            auto returnedValue = persistentState != nullptr && node.disabled == false && node.inputBlocked == false && (Detail::inputLayerBlocked(ui, node) == false || ui->popupReplacementAllowed == true) && (ui->captured == InvalidId || ui->captured == node.id) && persistentState->lastBounds.empty() == false && persistentState->lastClip.empty() == false && persistentState->lastBounds.contains(pointer.position) && persistentState->lastClip.contains(pointer.position);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        void openContextMenu(Context * ui, Response & response, size_t node, const Vec2 & position)
        {
            constexpr StringView popupName = "Context menu";

            if(Mosaic::isPopupOpen(ui, Key(popupName), response.id) == true)
            {
                Mosaic::closeCurrentPopup(ui);
            }

            ui->focused = response.id;
            Detail::setFlag(response, 2);
            PopupOptions options;
            options.owner = response.id;
            options.anchor = {position.x, position.y, 1.f, 1.f};
            options.placement = PopupPlacement::Cursor;
            options.minimumSize = {ui->currentStyle->metrics.minimumPopupWidth, 0.f};
            options.maximumSize = {420.f, 520.f};
            Mosaic::openPopup(ui, Key(popupName), options);
            ui->popupReplacementAllowed = false;
            (void)node;
        }
        //////////////////////////////////////////////////////////////////////////
        bool contextMenuOpen(const Context * ui, Id owner) noexcept
        {
            constexpr StringView popupName = "Context menu";
            Id id = Detail::popupId(ui, Key(popupName), owner);
            const Context::PopupState * state = Detail::findPopup(ui, id);

            return state != nullptr && state->open;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Response contextMenuItem(Context * ui, const Key & key, StringView label, bool enabled, const SourceLocation & location)
        {
            auto disabled = Mosaic::disabledScope(ui, enabled == false, location);
            LayoutOptions layout;
            layout.width = SizeRule::Fill;
            layout.height = Dimension::fixed(ui->currentStyle->metrics.controlHeight);
            size_t node = ui->addNode(Detail::NodeKind::Button, key, label, layout, location, SemanticRole::Button, true);
            Context::Node & itemNode = ui->nodes[node];
            Theme & style = ui->mutableStyle(itemNode);
            style.colors.panel = style.colors.background;
            style.colors.border = Color{0.f, 0.f, 0.f, 0.f};
            style.colors.borderStrong = Color{0.f, 0.f, 0.f, 0.f};
            style.metrics.buttonTextAlignment.x = 0.f;
            style.metrics.cornerRadius = 3.f;
            auto returnedValue = ui->interact(node);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ValueContextAction valueContextMenu(Context * ui, Id owner, StringView currentValue, bool canCopy, bool canPaste, const SourceLocation & location)
        {
            if(Detail::contextMenuOpen(ui, owner) == false)
            {
                return Detail::ValueContextAction::None;
            }

            Theme valueTheme = *ui->currentStyle;
            valueTheme.colors.text.a *= 0.7f;
            valueTheme.metrics.font = MonospaceFont;
            Vec2 valueSize = ui->estimateText(currentValue, valueTheme);
            float copyWidth = ui->estimateText("Copy Value", *ui->currentStyle).x + ui->currentStyle->metrics.padding * 2.f;
            float pasteWidth = ui->estimateText("Paste Value", *ui->currentStyle).x + ui->currentStyle->metrics.padding * 2.f;
            constexpr float menuGap = 3.f;
            float menuWidth = std::ceil(std::max(ui->currentStyle->metrics.minimumPopupWidth, std::max(valueSize.x, std::max(copyWidth, pasteWidth)) + ui->currentStyle->metrics.padding * 2.f));
            Detail::ValueContextAction action = Detail::ValueContextAction::None;
            PopupOptions popupOptions;
            popupOptions.owner = owner;
            popupOptions.minimumSize = {menuWidth, 0.f};
            popupOptions.maximumSize = {menuWidth, 520.f};
            {
                auto popupScope = Mosaic::popup(ui, Key("Context menu"), popupOptions, location);

                if(popupScope.visible() == true)
                {
                    LayoutOptions menuLayout;
                    menuLayout.width = SizeRule::Fill;
                    menuLayout.height = SizeRule::Content;
                    menuLayout.gap = menuGap;
                    auto menuColumn = Mosaic::column(ui, menuLayout, location);
                    {
                        auto valueStyle = Mosaic::styleScope(ui, valueTheme, location);
                        Mosaic::text(ui, currentValue, location);
                    }
                    Mosaic::separator(ui, location);

                    if(Detail::contextMenuItem(ui, Key(0U), "Copy Value", canCopy, location).clicked() == true)
                    {
                        action = Detail::ValueContextAction::Copy;
                    }

                    if(Detail::contextMenuItem(ui, Key(1U), "Paste Value", canPaste, location).clicked() == true)
                    {
                        action = Detail::ValueContextAction::Paste;
                    }
                }
            }

            if(action != Detail::ValueContextAction::None)
            {
                Mosaic::closeCurrentPopup(ui);
            }

            return action;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] TextContextAction textContextMenu(Context * ui, Id owner, StringView value, const TextEditorState & state, const TextInputOptions & options, const SourceLocation & location)
        {
            if(Detail::contextMenuOpen(ui, owner) == false)
            {
                return Detail::TextContextAction::None;
            }

            size_t selectionBegin = std::min(state.cursor, state.anchor);
            size_t selectionEnd = std::max(state.cursor, state.anchor);
            bool hasSelection = selectionBegin != selectionEnd;
            String clipboard;
            bool hasClipboard = ui->platform->getClipboardText(&clipboard);
            bool canUndo = options.undoRedo && options.readOnly == false && state.undoPosition > 0;
            bool canRedo = options.undoRedo && options.readOnly == false && state.undoPosition < state.undoCount;
            bool canCut = options.readOnly == false && options.password == false && hasSelection;
            bool canCopy = options.password == false && hasSelection;
            bool decimalNumeric = options.numeric && options.characterFilter != TextCharacterFilter::Hexadecimal;
            bool canPaste = options.readOnly == false && hasClipboard == true && clipboard.empty() == false && (decimalNumeric == false || Detail::numericText(clipboard));
            bool canDelete = options.readOnly == false && hasSelection;

            constexpr Detail::TextContextLabels actionLabels = {"Undo", "Redo", "Cut", "Copy", "Paste", "Delete", "Select All"};
            float actionWidth = 0.f;
            for(StringView actionLabel : actionLabels)
            {
                actionWidth = std::max(actionWidth, ui->estimateText(actionLabel, *ui->currentStyle).x + ui->currentStyle->metrics.padding * 2.f);
            }
            constexpr float menuGap = 3.f;
            float menuWidth = std::ceil(std::max(ui->currentStyle->metrics.minimumPopupWidth, actionWidth + ui->currentStyle->metrics.padding * 2.f));
            Detail::TextContextAction action = Detail::TextContextAction::None;
            PopupOptions popupOptions;
            popupOptions.owner = owner;
            popupOptions.minimumSize = {menuWidth, 0.f};
            popupOptions.maximumSize = {menuWidth, 520.f};
            {
                auto popupScope = Mosaic::popup(ui, Key("Context menu"), popupOptions, location);

                if(popupScope.visible() == true)
                {
                    LayoutOptions menuLayout;
                    menuLayout.width = SizeRule::Fill;
                    menuLayout.height = SizeRule::Content;
                    menuLayout.gap = menuGap;
                    auto menuColumn = Mosaic::column(ui, menuLayout, location);

                    if(Detail::contextMenuItem(ui, Key(0U), "Undo", canUndo, location).clicked() == true)
                    {
                        action = Detail::TextContextAction::Undo;
                    }

                    if(Detail::contextMenuItem(ui, Key(1U), "Redo", canRedo, location).clicked() == true)
                    {
                        action = Detail::TextContextAction::Redo;
                    }

                    Mosaic::separator(ui, location);

                    if(Detail::contextMenuItem(ui, Key(2U), "Cut", canCut, location).clicked() == true)
                    {
                        action = Detail::TextContextAction::Cut;
                    }

                    if(Detail::contextMenuItem(ui, Key(3U), "Copy", canCopy, location).clicked() == true)
                    {
                        action = Detail::TextContextAction::Copy;
                    }

                    if(Detail::contextMenuItem(ui, Key(4U), "Paste", canPaste, location).clicked() == true)
                    {
                        action = Detail::TextContextAction::Paste;
                    }

                    if(Detail::contextMenuItem(ui, Key(5U), "Delete", canDelete, location).clicked() == true)
                    {
                        action = Detail::TextContextAction::Delete;
                    }

                    Mosaic::separator(ui, location);

                    if(Detail::contextMenuItem(ui, Key(6U), "Select All", value.empty() == false, location).clicked() == true)
                    {
                        action = Detail::TextContextAction::SelectAll;
                    }
                }
            }

            if(action != Detail::TextContextAction::None)
            {
                Mosaic::closeCurrentPopup(ui);
            }

            return action;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Response inputText(Context * ui, StringView label, String * value, const TextInputOptions & options, const SourceLocation & location)
    {
        if(value == nullptr)
        {
            size_t nullNode = ui->addNode(Detail::NodeKind::InputText, {}, {}, {}, location, SemanticRole::TextField, options.tabStop);
            ui->nodeSemanticName(ui->nodes[nullNode]).assign(label);
            ui->nodeSemanticDescription(ui->nodes[nullNode]).assign(options.validationMessage);
            auto returnedValue = ui->interact(nullNode, false);

            return returnedValue;
        }

        size_t node = ui->addNode(Detail::NodeKind::InputText, {}, label, {}, location, SemanticRole::TextField, options.tabStop);
        ui->nodeSemanticName(ui->nodes[node]).assign(label);
        ui->nodeSemanticDescription(ui->nodes[node]).assign(options.validationMessage);
        ui->nodes[node].readOnly = options.readOnly;
        ui->nodes[node].validation = options.validation;

        if(options.numeric == true)
        {
            ui->mutableStyle(ui->nodes[node]).metrics.font = MonospaceFont;
            ui->estimateNodeText(ui->nodes[node], *value);
        }

        Response response = ui->interact(node, false);
        Context::Persistent & persistentState = ui->state(ui->nodes[node]);
        persistentState.acceptsTabInput = options.allowTabInput || (options.callbackCompletion == true && options.callback != nullptr);
        Detail::TextEditorState & editorState = ui->textEditorState(response.id);
        String * backingValue = value;
        bool liveEdit = options.numeric ? ui->currentLiveEditScalar : ui->currentLiveEditText;

        if(liveEdit == false && persistentState.editing == false)
        {
            editorState.editValue = *backingValue;
        }

        if(liveEdit == false)
        {
            value = &editorState.editValue;
        }

        editorState.cursor = std::min(editorState.cursor, value->size());
        editorState.anchor = std::min(editorState.anchor, value->size());

        const PointerState * pointer = ui->input.primaryPointer();
        bool contextRequested = pointer != nullptr && pointer->isPressed(PointerButton::Secondary) && Detail::contextMenuPointerHit(ui, ui->nodes[node], *pointer);

        if(contextRequested == true)
        {
            Detail::openContextMenu(ui, response, node, pointer->position);
        }

        if(response.focused() == true || ui->captured == response.id || response.released() == true || contextRequested == true)
        {
            ui->prepareText(ui->nodes[node], *value);
        }

        bool committedByFocusLoss = false;

        if(ui->focused != response.id && persistentState.editing == true && Detail::contextMenuOpen(ui, response.id) == false)
        {
            persistentState.editing = false;
            committedByFocusLoss = true;
        }

        bool beganEditing = false;

        if(response.focused() == true && persistentState.editing == false)
        {
            beganEditing = true;
            persistentState.editing = true;
            editorState.editOriginal = *value;
            editorState.cursor = options.selectAllOnFocus ? value->size() : std::min(editorState.cursor, value->size());
            editorState.anchor = options.selectAllOnFocus ? 0 : editorState.cursor;

            if(options.undoRedo == true)
            {
                Detail::clearUndo(editorState, *value, ui->input.timestamp);
            }

            Detail::setFlag(response, 7);
            Detail::setFlag(response, 14);
            response.flags &= ~(1U << 15U);
            ui->frame.events.push_back({EventType::BeginEdit, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        if(contextRequested == true)
        {
            size_t position = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pointer->position);
            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);

            if(selectionBegin == selectionEnd || position < selectionBegin || position > selectionEnd)
            {
                editorState.cursor = position;
                editorState.anchor = position;
            }

            editorState.selectingText = false;
        }

        if(pointer != nullptr && (ui->captured == response.id || response.released() == true))
        {
            if(response.pressed() == true)
            {
                Vec2 pressPosition = pointer->pressPosition();

                if(pointer->buttonClickCount() >= 2)
                {
                    size_t position = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pressPosition);

                    if(pointer->buttonClickCount() >= 3)
                    {
                        Detail::selectLine(ui->nodes[node], *value, position, editorState.cursor, editorState.anchor);
                    }
                    else
                    {
                        Detail::selectWord(*value, position, editorState.cursor, editorState.anchor);
                    }

                    editorState.selectingText = false;
                }
                else if(beganEditing == false || options.selectAllOnFocus == false)
                {
                    size_t anchor = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pressPosition);

                    if(ui->input.modifiers.shift == false)
                    {
                        editorState.anchor = anchor;
                    }

                    editorState.cursor = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pointer->position);
                    editorState.selectingText = true;
                }
            }
            else if(editorState.selectingText == true && (pointer->isDown() == true || response.released() == true))
            {
                Detail::autoScrollTextSelection(ui->nodes[node], editorState, persistentState.lastBounds, *pointer, ui->input.deltaTime);
                editorState.cursor = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pointer->position);
            }

            if(response.released() == true)
            {
                editorState.selectingText = false;
            }
        }

        size_t editBeginCapacity = value->capacity();
        bool changed = false;
        bool committed = committedByFocusLoss;
        bool canceled = false;
        auto insertText = [&](StringView inserted)
        {
            if(options.readOnly == true)
            {
                return;
            }

            bool validateNumeric = options.characterFilter == TextCharacterFilter::Decimal;

            if(options.numeric == true)
            {
                if(options.characterFilter != TextCharacterFilter::Hexadecimal)
                {
                    validateNumeric = true;
                }
            }

            if(validateNumeric == true)
            {
                if(Detail::numericText(inserted) == false)
                {
                    return;
                }
            }

            String filtered = Detail::filterText(inserted, options.characterFilter);
            filtered = Detail::filterTextWithCallback(filtered, options, editorState);

            if(filtered.empty() == true && inserted.empty() == false)
            {
                return;
            }

            Detail::replaceSelectionOrOverwrite(ui->nodes[node], *value, editorState.cursor, editorState.anchor, filtered, options);
            changed = true;
        };

        if(response.focused() == true)
        {
            if(ui->input.modifiers.primary == false && ui->input.modifiers.control == false && ui->input.modifiers.super == false)
            {
                for(const String & textEvent : ui->input.text)
                {
                    insertText(textEvent);
                }
            }

            for(const ImeEvent & event : ui->input.ime)
            {
                switch(event.type)
                {
                case ImeEventType::Start:
                    editorState.composition.clear();
                    editorState.compositionBegin = std::min(editorState.cursor, editorState.anchor);
                    editorState.compositionEnd = editorState.compositionBegin;
                    editorState.compositionSelectionBegin = 0;
                    editorState.compositionSelectionEnd = 0;
                    break;
                case ImeEventType::Update:
                    editorState.composition = event.text;
                    editorState.compositionBegin = std::min(editorState.cursor, editorState.anchor);
                    editorState.compositionEnd = editorState.compositionBegin + event.text.size();
                    editorState.compositionSelectionBegin = std::min(event.selectionBegin, event.text.size());
                    editorState.compositionSelectionEnd = std::min(event.selectionEnd, event.text.size());
                    break;
                case ImeEventType::Commit:
                    insertText(event.text);
                    editorState.composition.clear();
                    editorState.compositionBegin = editorState.cursor;
                    editorState.compositionEnd = editorState.cursor;
                    editorState.compositionSelectionBegin = 0;
                    editorState.compositionSelectionEnd = 0;
                    break;
                case ImeEventType::Cancel:
                    editorState.composition.clear();
                    editorState.compositionEnd = editorState.compositionBegin;
                    editorState.compositionSelectionBegin = 0;
                    editorState.compositionSelectionEnd = 0;
                    break;
                }
            }

            for(const KeyEvent & event : ui->input.keyboard)
            {
                if(event.pressed == false)
                {
                    continue;
                }

                bool primary = event.modifiers.primary || event.modifiers.control == true || event.modifiers.super;
                size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
                size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
                bool redoShortcut = primary;

                if(event.key != KeyCode::Y)
                {
                    if(event.key == KeyCode::Z)
                    {
                        redoShortcut = event.modifiers.shift;
                    }
                    else
                    {
                        redoShortcut = false;
                    }
                }

                if(options.undoRedo == false)
                {
                    redoShortcut = false;
                }

                if(options.readOnly == true)
                {
                    redoShortcut = false;
                }

                if(primary == true && event.key == KeyCode::A)
                {
                    editorState.anchor = 0;
                    editorState.cursor = value->size();
                }
                else if(primary == true && event.key == KeyCode::C && options.password == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
                    }
                }
                else if(primary == true && event.key == KeyCode::X && options.readOnly == false && options.password == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
                        Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
                        changed = true;
                    }
                }
                else if(primary == true && event.key == KeyCode::V)
                {
                    String clipboard;
                    if(ui->platform->getClipboardText(&clipboard) == true)
                    {
                        insertText(clipboard);
                    }
                }
                else if(primary == true && event.key == KeyCode::Z && event.modifiers.shift == false && options.undoRedo == true && options.readOnly == false)
                {
                    if(Detail::applyUndo(editorState, *value) == true)
                    {
                        changed = true;
                    }
                }
                else if(redoShortcut == true)
                {
                    if(Detail::applyRedo(editorState, *value) == true)
                    {
                        changed = true;
                    }
                }
                else if(event.key == KeyCode::Left)
                {
                    editorState.cursor = event.modifiers.shift == false && selectionBegin != selectionEnd ? selectionBegin : (primary ? Detail::previousWord(*value, editorState.cursor) : Detail::previousTextPosition(ui->nodes[node], *value, editorState.cursor, options.password));

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::Right)
                {
                    editorState.cursor = event.modifiers.shift == false && selectionBegin != selectionEnd ? selectionEnd : (primary ? Detail::nextWord(*value, editorState.cursor) : Detail::nextTextPosition(ui->nodes[node], *value, editorState.cursor, options.password));

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::Home)
                {
                    editorState.cursor = 0;

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::End)
                {
                    editorState.cursor = value->size();

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::Tab && options.callbackCompletion == true)
                {
                    changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Completion, *value, editorState) || changed;
                }
                else if((event.key == KeyCode::Up || event.key == KeyCode::Down) && options.callbackHistory == true)
                {
                    TextInputCallbackEvent callbackEvent = event.key == KeyCode::Up ? TextInputCallbackEvent::HistoryPrevious : TextInputCallbackEvent::HistoryNext;
                    changed = Detail::invokeTextInputCallback(options, callbackEvent, *value, editorState) || changed;
                }
                else if(event.key == KeyCode::Backspace && options.readOnly == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
                        changed = true;
                    }
                    else if(editorState.cursor > 0)
                    {
                        size_t previous = primary ? Detail::previousWord(*value, editorState.cursor) : Detail::previousTextPosition(ui->nodes[node], *value, editorState.cursor, options.password);
                        value->erase(previous, editorState.cursor - previous);
                        editorState.cursor = editorState.anchor = previous;
                        changed = true;
                    }
                }
                else if(event.key == KeyCode::Delete && options.readOnly == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
                        changed = true;
                    }
                    else if(editorState.cursor < value->size())
                    {
                        size_t next = primary ? Detail::nextWord(*value, editorState.cursor) : Detail::nextTextPosition(ui->nodes[node], *value, editorState.cursor, options.password);
                        value->erase(editorState.cursor, next - editorState.cursor);
                        changed = true;
                    }
                }
                else if(event.key == KeyCode::Enter)
                {
                    committed = true;
                    Detail::setFlag(response, 18, options.enterReturnsTrue);
                    persistentState.editing = ui->configuration.inputTextEnterKeepActive;

                    if(persistentState.editing == false)
                    {
                        ui->focused = InvalidId;
                    }
                }
                else if(event.key == KeyCode::Escape)
                {
                    String replacement = options.escapeClearsAll ? String{} : editorState.editOriginal;

                    if(*value != replacement)
                    {
                        *value = replacement;
                        changed = true;
                    }

                    editorState.cursor = editorState.anchor = value->size();
                    persistentState.editing = false;
                    canceled = true;
                    ui->focused = InvalidId;
                }
            }
        }

        Detail::TextContextAction contextAction = Detail::textContextMenu(ui, response.id, *value, editorState, options, location);
        switch(contextAction)
        {
        case Detail::TextContextAction::Undo:
            changed = Detail::applyUndo(editorState, *value) || changed;
            break;
        case Detail::TextContextAction::Redo:
            changed = Detail::applyRedo(editorState, *value) || changed;
            break;
        case Detail::TextContextAction::Cut:
        {
            if(options.password == true)
            {
                break;
            }

            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
            ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
            Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
            changed = true;
            break;
        }
        case Detail::TextContextAction::Copy:
        {
            if(options.password == true)
            {
                break;
            }

            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
            ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
            break;
        }
        case Detail::TextContextAction::Paste:
        {
            String clipboard;
            if(ui->platform->getClipboardText(&clipboard) == true)
            {
                insertText(clipboard);
            }

            break;
        }
        case Detail::TextContextAction::Delete:
            Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
            changed = true;
            break;
        case Detail::TextContextAction::SelectAll:
            editorState.anchor = 0;
            editorState.cursor = value->size();
            break;
        case Detail::TextContextAction::None:
            break;
        }

        if(contextAction != Detail::TextContextAction::None || Detail::contextMenuOpen(ui, response.id) == true)
        {
            ui->focused = response.id;
            Detail::setFlag(response, 2);
        }

        if(committed == true && canceled == false && liveEdit == false && *backingValue != *value)
        {
            *backingValue = *value;
            changed = true;
        }

        if(changed == true && options.callbackResize == true && value->capacity() != editBeginCapacity)
        {
            changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Resize, *value, editorState, editBeginCapacity) || changed;
        }

        if(changed == true && options.callbackEdit == true)
        {
            changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Edit, *value, editorState) || changed;
        }

        if(response.focused() == true && options.callbackAlways == true)
        {
            changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Always, *value, editorState) || changed;
        }

        if(changed == true)
        {
            Detail::setFlag(response, 6);
            Detail::setFlag(response, 7, persistentState.editing);

            if(options.undoRedo == true)
            {
                Detail::recordUndo(editorState, *value, ui->input.timestamp);
            }

            ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        if(committed == true)
        {
            Detail::setFlag(response, 8);
            Detail::setFlag(response, 15);
            Detail::setFlag(response, 16, *value != editorState.editOriginal);
            ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        if(canceled == true)
        {
            Detail::setFlag(response, 9);
            Detail::setFlag(response, 15);
            ui->frame.events.push_back({EventType::Cancel, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        Context::Node & inputNode = ui->nodes[node];
        inputNode.textEditData().password = options.password;
        inputNode.textEditData().hint = value->empty() && options.hint.empty() == false;
        inputNode.label = inputNode.textEditData().hint ? String(options.hint) : Detail::inputDisplayText(*value, options.password);
        size_t displayCursor = Detail::inputDisplayOffset(*value, editorState.cursor, options.password);
        inputNode.textEditData().cursor = displayCursor;
        inputNode.textEditData().anchor = Detail::inputDisplayOffset(*value, editorState.anchor, options.password);

        if(options.password == false && editorState.composition.empty() == false && inputNode.textEditData().hint == false)
        {
            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
            size_t compositionBegin = Detail::inputDisplayOffset(*value, selectionBegin, false);
            size_t compositionReplaceEnd = Detail::inputDisplayOffset(*value, selectionEnd, false);
            inputNode.label.replace(compositionBegin, compositionReplaceEnd - compositionBegin, editorState.composition);
            inputNode.textEditData().compositionBegin = compositionBegin;
            inputNode.textEditData().compositionEnd = compositionBegin + editorState.composition.size();
            inputNode.textEditData().cursor = inputNode.textEditData().compositionBegin + editorState.compositionSelectionEnd;
            inputNode.textEditData().anchor = inputNode.textEditData().compositionBegin + editorState.compositionSelectionBegin;
        }

        if(ui->focused == response.id || ui->captured == response.id)
        {
            ui->prepareText(inputNode, inputNode.label);
        }
        else
        {
            ui->estimateNodeText(inputNode, inputNode.label);
        }

        Detail::updateTextScroll(inputNode, editorState, persistentState.lastBounds, ui->focused == response.id);

        if(options.noHorizontalScroll == true)
        {
            editorState.textScrollX = 0.f;
            inputNode.textEditData().scrollX = 0.f;
        }

        if(persistentState.editing == true)
        {
            response.flags &= ~((1U << 15U) | (1U << 16U));

            if(beganEditing == false)
            {
                response.flags &= ~(1U << 14U);
            }
        }

        Detail::setFlag(response, 1, persistentState.editing);

        if(options.elideLeft == true && ui->focused != response.id && persistentState.lastBounds.empty() == false)
        {
            float visibleWidth = std::max(0.f, persistentState.lastBounds.width - inputNode.style->metrics.padding * 2.f - inputNode.style->metrics.frameBorderSize * 2.f);
            inputNode.label = Detail::elideTextLeft(ui, inputNode.label, *inputNode.style, visibleWidth);
            ui->estimateNodeText(inputNode, inputNode.label);
            inputNode.textEditData().scrollX = 0.f;
        }

        inputNode.response = response;
        ui->nodeSemanticValue(inputNode) = options.password ? Detail::inputDisplayText(*value, true) : *value;

        if(options.validation != Validation::Normal && options.validationMessage.empty() == false)
        {
            ItemTooltipOptions tooltipOptions;
            tooltipOptions.delay = 0.15f;
            Mosaic::itemTooltip(ui, response, options.validationMessage, tooltipOptions, location);
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
    Response inputMultiline(Context * ui, StringView label, String * value, const TextInputOptions & options, const LayoutOptions & layout, const SourceLocation & location)
    {
        size_t node = ui->addNode(Detail::NodeKind::InputMultiline, {}, label, layout, location, SemanticRole::TextField, options.tabStop);
        ui->nodeSemanticName(ui->nodes[node]).assign(label);
        ui->nodeSemanticDescription(ui->nodes[node]).assign(options.validationMessage);
        ui->nodes[node].readOnly = options.readOnly;
        ui->nodes[node].validation = options.validation;
        ui->nodes[node].textEditData().multiline = true;
        ui->nodes[node].wordWrap = options.wordWrap;
        Context::Persistent & persistentState = ui->state(ui->nodes[node]);

        if(options.wordWrap == true && persistentState.lastBounds.empty() == false)
        {
            ui->nodes[node].valueData().textWrapWidth = std::max(0.f, persistentState.lastBounds.width - ui->nodes[node].style->metrics.padding * 2.f - ui->nodes[node].style->metrics.frameBorderSize * 2.f);
        }

        if(options.numeric == true)
        {
            ui->mutableStyle(ui->nodes[node]).metrics.font = MonospaceFont;
            ui->estimateNodeText(ui->nodes[node], ui->nodes[node].label);
        }

        if(value == nullptr)
        {
            auto returnedValue = ui->interact(node, false);

            return returnedValue;
        }

        Response response = ui->interact(node, false);
        persistentState.acceptsTabInput = options.allowTabInput || (options.callbackCompletion == true && options.callback != nullptr);
        Detail::TextEditorState & editorState = ui->textEditorState(response.id);
        String * backingValue = value;
        bool liveEdit = options.numeric ? ui->currentLiveEditScalar : ui->currentLiveEditText;

        if(liveEdit == false && persistentState.editing == false)
        {
            editorState.editValue = *backingValue;
        }

        if(liveEdit == false)
        {
            value = &editorState.editValue;
        }

        editorState.cursor = std::min(editorState.cursor, value->size());
        editorState.anchor = std::min(editorState.anchor, value->size());

        const PointerState * pointer = ui->input.primaryPointer();
        bool contextRequested = pointer != nullptr && pointer->isPressed(PointerButton::Secondary) && Detail::contextMenuPointerHit(ui, ui->nodes[node], *pointer);

        if(contextRequested == true)
        {
            Detail::openContextMenu(ui, response, node, pointer->position);
        }

        if(response.focused() == true || ui->captured == response.id || response.released() == true || contextRequested == true)
        {
            ui->prepareText(ui->nodes[node], *value);
        }

        bool committed = false;
        bool canceled = false;

        if(ui->focused != response.id && persistentState.editing == true && Detail::contextMenuOpen(ui, response.id) == false)
        {
            persistentState.editing = false;
            committed = true;
        }

        bool beganEditing = false;

        if(response.focused() == true && persistentState.editing == false)
        {
            beganEditing = true;
            persistentState.editing = true;
            editorState.editOriginal = *value;
            editorState.cursor = options.selectAllOnFocus ? value->size() : editorState.cursor;
            editorState.anchor = options.selectAllOnFocus ? 0 : editorState.cursor;

            if(options.undoRedo == true)
            {
                Detail::clearUndo(editorState, *value, ui->input.timestamp);
            }

            Detail::setFlag(response, 7);
            Detail::setFlag(response, 14);
            response.flags &= ~(1U << 15U);
            ui->frame.events.push_back({EventType::BeginEdit, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        if(contextRequested == true)
        {
            size_t position = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pointer->position);
            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);

            if(selectionBegin == selectionEnd || position < selectionBegin || position > selectionEnd)
            {
                editorState.cursor = position;
                editorState.anchor = position;
            }

            editorState.selectingText = false;
        }

        if(pointer != nullptr && (ui->captured == response.id || response.released() == true))
        {
            if(response.pressed() == true)
            {
                Vec2 pressPosition = pointer->pressPosition();

                if(pointer->buttonClickCount() >= 2)
                {
                    size_t position = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pressPosition);

                    if(pointer->buttonClickCount() >= 3)
                    {
                        Detail::selectLine(ui->nodes[node], *value, position, editorState.cursor, editorState.anchor);
                    }
                    else
                    {
                        Detail::selectWord(*value, position, editorState.cursor, editorState.anchor);
                    }

                    editorState.selectingText = false;
                }
                else if(beganEditing == false || options.selectAllOnFocus == false)
                {
                    size_t anchor = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pressPosition);

                    if(ui->input.modifiers.shift == false)
                    {
                        editorState.anchor = anchor;
                    }

                    editorState.cursor = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pointer->position);
                    editorState.selectingText = true;
                }
            }
            else if(editorState.selectingText == true && (pointer->isDown() == true || response.released() == true))
            {
                Detail::autoScrollTextSelection(ui->nodes[node], editorState, persistentState.lastBounds, *pointer, ui->input.deltaTime);
                editorState.cursor = Detail::inputPositionAtPointer(ui, ui->nodes[node], *value, options.password, pointer->position);
            }

            if(response.released() == true)
            {
                editorState.selectingText = false;
            }
        }

        size_t editBeginCapacity = value->capacity();
        bool changed = false;
        auto insertText = [&](StringView inserted)
        {
            if(options.readOnly == true)
            {
                return;
            }

            bool validateNumeric = options.characterFilter == TextCharacterFilter::Decimal;

            if(options.numeric == true)
            {
                if(options.characterFilter != TextCharacterFilter::Hexadecimal)
                {
                    validateNumeric = true;
                }
            }

            if(validateNumeric == true)
            {
                if(Detail::numericText(inserted) == false)
                {
                    return;
                }
            }

            String filtered = Detail::filterText(inserted, options.characterFilter);
            filtered = Detail::filterTextWithCallback(filtered, options, editorState);

            if(filtered.empty() == true && inserted.empty() == false)
            {
                return;
            }

            Detail::replaceSelectionOrOverwrite(ui->nodes[node], *value, editorState.cursor, editorState.anchor, filtered, options);
            changed = true;
        };

        if(response.focused() == true)
        {
            if(ui->input.modifiers.primary == false && ui->input.modifiers.control == false && ui->input.modifiers.super == false)
            {
                for(const String & textEvent : ui->input.text)
                {
                    insertText(textEvent);
                }
            }

            for(const ImeEvent & event : ui->input.ime)
            {
                switch(event.type)
                {
                case ImeEventType::Start:
                    editorState.composition.clear();
                    editorState.compositionBegin = std::min(editorState.cursor, editorState.anchor);
                    editorState.compositionEnd = editorState.compositionBegin;
                    editorState.compositionSelectionBegin = 0;
                    editorState.compositionSelectionEnd = 0;
                    break;
                case ImeEventType::Update:
                    editorState.composition = event.text;
                    editorState.compositionBegin = std::min(editorState.cursor, editorState.anchor);
                    editorState.compositionEnd = editorState.compositionBegin + event.text.size();
                    editorState.compositionSelectionBegin = std::min(event.selectionBegin, event.text.size());
                    editorState.compositionSelectionEnd = std::min(event.selectionEnd, event.text.size());
                    break;
                case ImeEventType::Commit:
                    insertText(event.text);
                    editorState.composition.clear();
                    editorState.compositionBegin = editorState.cursor;
                    editorState.compositionEnd = editorState.cursor;
                    editorState.compositionSelectionBegin = 0;
                    editorState.compositionSelectionEnd = 0;
                    break;
                case ImeEventType::Cancel:
                    editorState.composition.clear();
                    editorState.compositionEnd = editorState.compositionBegin;
                    editorState.compositionSelectionBegin = 0;
                    editorState.compositionSelectionEnd = 0;
                    break;
                }
            }

            for(const KeyEvent & event : ui->input.keyboard)
            {
                if(event.pressed == false)
                {
                    continue;
                }

                bool primary = event.modifiers.primary || event.modifiers.control == true || event.modifiers.super;
                size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
                size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
                bool redoShortcut = primary;

                if(event.key != KeyCode::Y)
                {
                    if(event.key == KeyCode::Z)
                    {
                        redoShortcut = event.modifiers.shift;
                    }
                    else
                    {
                        redoShortcut = false;
                    }
                }

                if(options.undoRedo == false)
                {
                    redoShortcut = false;
                }

                if(options.readOnly == true)
                {
                    redoShortcut = false;
                }

                if(primary == true && event.key == KeyCode::A)
                {
                    editorState.anchor = 0;
                    editorState.cursor = value->size();
                }
                else if(primary == true && event.key == KeyCode::C && options.password == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
                    }
                }
                else if(primary == true && event.key == KeyCode::X && options.readOnly == false && options.password == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
                        Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
                        changed = true;
                    }
                }
                else if(primary == true && event.key == KeyCode::V)
                {
                    String clipboard;
                    if(ui->platform->getClipboardText(&clipboard) == true)
                    {
                        insertText(clipboard);
                    }
                }
                else if(primary == true && event.key == KeyCode::Z && event.modifiers.shift == false && options.undoRedo == true && options.readOnly == false)
                {
                    if(Detail::applyUndo(editorState, *value) == true)
                    {
                        changed = true;
                    }
                }
                else if(redoShortcut == true)
                {
                    if(Detail::applyRedo(editorState, *value) == true)
                    {
                        changed = true;
                    }
                }
                else if(event.key == KeyCode::Left)
                {
                    editorState.cursor = event.modifiers.shift == false && selectionBegin != selectionEnd ? selectionBegin : (primary ? Detail::previousWord(*value, editorState.cursor) : Detail::previousTextPosition(ui->nodes[node], *value, editorState.cursor, options.password));

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::Right)
                {
                    editorState.cursor = event.modifiers.shift == false && selectionBegin != selectionEnd ? selectionEnd : (primary ? Detail::nextWord(*value, editorState.cursor) : Detail::nextTextPosition(ui->nodes[node], *value, editorState.cursor, options.password));

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if((event.key == KeyCode::Up || event.key == KeyCode::Down) && options.callbackHistory == true)
                {
                    TextInputCallbackEvent callbackEvent = event.key == KeyCode::Up ? TextInputCallbackEvent::HistoryPrevious : TextInputCallbackEvent::HistoryNext;
                    changed = Detail::invokeTextInputCallback(options, callbackEvent, *value, editorState) || changed;
                }
                else if(event.key == KeyCode::Up || event.key == KeyCode::Down || event.key == KeyCode::PageUp || event.key == KeyCode::PageDown)
                {
                    int lines = event.key == KeyCode::Up || event.key == KeyCode::PageUp ? -1 : 1;

                    if(event.key == KeyCode::PageUp || event.key == KeyCode::PageDown)
                    {
                        int pageLines = static_cast<int>(std::max(1.f, persistentState.lastBounds.height / std::max(1.f, ui->nodes[node].style->metrics.lineHeight)));
                        lines *= pageLines;
                    }

                    editorState.cursor = Detail::moveVertical(ui->nodes[node], *value, editorState.cursor, lines);

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::Home)
                {
                    editorState.cursor = Detail::visualLineBoundary(ui->nodes[node], *value, editorState.cursor, false);

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::End)
                {
                    editorState.cursor = Detail::visualLineBoundary(ui->nodes[node], *value, editorState.cursor, true);

                    if(event.modifiers.shift == false)
                    {
                        editorState.anchor = editorState.cursor;
                    }
                }
                else if(event.key == KeyCode::Backspace && options.readOnly == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
                        changed = true;
                    }
                    else if(editorState.cursor > 0)
                    {
                        size_t previous = primary ? Detail::previousWord(*value, editorState.cursor) : Detail::previousTextPosition(ui->nodes[node], *value, editorState.cursor, options.password);
                        value->erase(previous, editorState.cursor - previous);
                        editorState.cursor = editorState.anchor = previous;
                        changed = true;
                    }
                }
                else if(event.key == KeyCode::Delete && options.readOnly == false)
                {
                    if(selectionBegin != selectionEnd)
                    {
                        Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
                        changed = true;
                    }
                    else if(editorState.cursor < value->size())
                    {
                        size_t next = primary ? Detail::nextWord(*value, editorState.cursor) : Detail::nextTextPosition(ui->nodes[node], *value, editorState.cursor, options.password);
                        value->erase(editorState.cursor, next - editorState.cursor);
                        changed = true;
                    }
                }
                else if(event.key == KeyCode::Tab && options.callbackCompletion == true)
                {
                    changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Completion, *value, editorState) || changed;
                }
                else if(event.key == KeyCode::Tab && options.allowTabInput == true)
                {
                    insertText("\t");
                }
                else if(event.key == KeyCode::Enter)
                {
                    bool insertNewLine = options.controlEnterForNewLine ? event.modifiers.control : event.modifiers.control == false;

                    if(insertNewLine == true)
                    {
                        insertText("\n");
                    }
                    else
                    {
                        Detail::setFlag(response, 18, options.enterReturnsTrue);
                        persistentState.editing = ui->configuration.inputTextEnterKeepActive;

                        if(persistentState.editing == false)
                        {
                            ui->focused = InvalidId;
                        }

                        committed = true;
                    }
                }
                else if(event.key == KeyCode::Escape)
                {
                    String replacement = options.escapeClearsAll ? String{} : editorState.editOriginal;

                    if(*value != replacement)
                    {
                        *value = replacement;
                        changed = true;
                    }

                    editorState.cursor = editorState.anchor = value->size();
                    persistentState.editing = false;
                    ui->focused = InvalidId;
                    canceled = true;
                }
            }
        }

        Detail::TextContextAction contextAction = Detail::textContextMenu(ui, response.id, *value, editorState, options, location);
        switch(contextAction)
        {
        case Detail::TextContextAction::Undo:
            changed = Detail::applyUndo(editorState, *value) || changed;
            break;
        case Detail::TextContextAction::Redo:
            changed = Detail::applyRedo(editorState, *value) || changed;
            break;
        case Detail::TextContextAction::Cut:
        {
            if(options.password == true)
            {
                break;
            }

            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
            ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
            Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
            changed = true;
            break;
        }
        case Detail::TextContextAction::Copy:
        {
            if(options.password == true)
            {
                break;
            }

            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
            ui->platform->setClipboardText(StringView(*value).substr(selectionBegin, selectionEnd - selectionBegin));
            break;
        }
        case Detail::TextContextAction::Paste:
        {
            String clipboard;
            if(ui->platform->getClipboardText(&clipboard) == true)
            {
                insertText(clipboard);
            }

            break;
        }
        case Detail::TextContextAction::Delete:
            Detail::replaceSelection(*value, editorState.cursor, editorState.anchor, {}, options.maximumBytes);
            changed = true;
            break;
        case Detail::TextContextAction::SelectAll:
            editorState.anchor = 0;
            editorState.cursor = value->size();
            break;
        case Detail::TextContextAction::None:
            break;
        }

        if(contextAction != Detail::TextContextAction::None || Detail::contextMenuOpen(ui, response.id) == true)
        {
            ui->focused = response.id;
            Detail::setFlag(response, 2);
        }

        if(committed == true && canceled == false && liveEdit == false && *backingValue != *value)
        {
            *backingValue = *value;
            changed = true;
        }

        if(changed == true && options.callbackResize == true && value->capacity() != editBeginCapacity)
        {
            changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Resize, *value, editorState, editBeginCapacity) || changed;
        }

        if(changed == true && options.callbackEdit == true)
        {
            changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Edit, *value, editorState) || changed;
        }

        if(response.focused() == true && options.callbackAlways == true)
        {
            changed = Detail::invokeTextInputCallback(options, TextInputCallbackEvent::Always, *value, editorState) || changed;
        }

        if(changed == true)
        {
            Detail::setFlag(response, 6);
            Detail::setFlag(response, 7, persistentState.editing);

            if(options.undoRedo == true)
            {
                Detail::recordUndo(editorState, *value, ui->input.timestamp);
            }

            ui->frame.events.push_back({EventType::Change, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        if(committed == true)
        {
            Detail::setFlag(response, 8);
            Detail::setFlag(response, 15);
            Detail::setFlag(response, 16, *value != editorState.editOriginal);
            ui->frame.events.push_back({EventType::Commit, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        if(canceled == true)
        {
            Detail::setFlag(response, 9);
            Detail::setFlag(response, 15);
            ui->frame.events.push_back({EventType::Cancel, response.id, ui->nodePath(node), ui->nodes[node].debugData().file, ui->nodes[node].debugData().line, ui->input.timestamp});
        }

        Context::Node & inputNode = ui->nodes[node];
        inputNode.textEditData().password = options.password;
        inputNode.textEditData().hint = value->empty() && options.hint.empty() == false;
        inputNode.label = inputNode.textEditData().hint ? String(options.hint) : Detail::inputDisplayText(*value, options.password);
        size_t displayCursor = Detail::inputDisplayOffset(*value, editorState.cursor, options.password);
        inputNode.textEditData().cursor = displayCursor;
        inputNode.textEditData().anchor = Detail::inputDisplayOffset(*value, editorState.anchor, options.password);

        if(options.password == false && editorState.composition.empty() == false && inputNode.textEditData().hint == false)
        {
            size_t selectionBegin = std::min(editorState.cursor, editorState.anchor);
            size_t selectionEnd = std::max(editorState.cursor, editorState.anchor);
            size_t compositionBegin = Detail::inputDisplayOffset(*value, selectionBegin, false);
            size_t compositionReplaceEnd = Detail::inputDisplayOffset(*value, selectionEnd, false);
            inputNode.label.replace(compositionBegin, compositionReplaceEnd - compositionBegin, editorState.composition);
            inputNode.textEditData().compositionBegin = compositionBegin;
            inputNode.textEditData().compositionEnd = compositionBegin + editorState.composition.size();
            inputNode.textEditData().cursor = inputNode.textEditData().compositionBegin + editorState.compositionSelectionEnd;
            inputNode.textEditData().anchor = inputNode.textEditData().compositionBegin + editorState.compositionSelectionBegin;
        }

        if(ui->focused == response.id || ui->captured == response.id)
        {
            ui->prepareText(inputNode, inputNode.label);
        }
        else
        {
            ui->estimateNodeText(inputNode, inputNode.label);
        }

        Detail::updateTextScroll(inputNode, editorState, persistentState.lastBounds, ui->focused == response.id);

        if(options.noHorizontalScroll == true)
        {
            editorState.textScrollX = 0.f;
            inputNode.textEditData().scrollX = 0.f;
        }

        if(persistentState.editing == true)
        {
            response.flags &= ~((1U << 15U) | (1U << 16U));

            if(beganEditing == false)
            {
                response.flags &= ~(1U << 14U);
            }
        }

        Detail::setFlag(response, 1, persistentState.editing);
        inputNode.response = response;
        ui->nodeSemanticValue(inputNode) = options.password ? Detail::inputDisplayText(*value, true) : *value;

        if(options.validation != Validation::Normal && options.validationMessage.empty() == false)
        {
            ItemTooltipOptions tooltipOptions;
            tooltipOptions.delay = 0.15f;
            Mosaic::itemTooltip(ui, response, options.validationMessage, tooltipOptions, location);
        }

        return response;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
