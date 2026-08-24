#include "Mosaic/ShortcutRegistry.hpp"

#include "Context.hpp"
#include "Interaction.hpp"

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] StringView shortcutKeyLabel(KeyCode key) noexcept
        {
            switch(key)
            {
            case KeyCode::Tab:
                return "Tab";
            case KeyCode::Enter:
                return "Enter";
            case KeyCode::Escape:
                return "Escape";
            case KeyCode::Space:
                return "Space";
            case KeyCode::Backspace:
                return "Backspace";
            case KeyCode::Delete:
                return "Delete";
            case KeyCode::Left:
                return "Left";
            case KeyCode::Right:
                return "Right";
            case KeyCode::Up:
                return "Up";
            case KeyCode::Down:
                return "Down";
            case KeyCode::Home:
                return "Home";
            case KeyCode::End:
                return "End";
            case KeyCode::PageUp:
                return "PageUp";
            case KeyCode::PageDown:
                return "PageDown";
            case KeyCode::A:
                return "A";
            case KeyCode::B:
                return "B";
            case KeyCode::C:
                return "C";
            case KeyCode::D:
                return "D";
            case KeyCode::E:
                return "E";
            case KeyCode::F:
                return "F";
            case KeyCode::G:
                return "G";
            case KeyCode::H:
                return "H";
            case KeyCode::I:
                return "I";
            case KeyCode::J:
                return "J";
            case KeyCode::K:
                return "K";
            case KeyCode::L:
                return "L";
            case KeyCode::M:
                return "M";
            case KeyCode::N:
                return "N";
            case KeyCode::O:
                return "O";
            case KeyCode::P:
                return "P";
            case KeyCode::Q:
                return "Q";
            case KeyCode::R:
                return "R";
            case KeyCode::S:
                return "S";
            case KeyCode::T:
                return "T";
            case KeyCode::U:
                return "U";
            case KeyCode::V:
                return "V";
            case KeyCode::W:
                return "W";
            case KeyCode::X:
                return "X";
            case KeyCode::Y:
                return "Y";
            case KeyCode::Z:
                return "Z";
            case KeyCode::D0:
                return "0";
            case KeyCode::D1:
                return "1";
            case KeyCode::D2:
                return "2";
            case KeyCode::D3:
                return "3";
            case KeyCode::D4:
                return "4";
            case KeyCode::D5:
                return "5";
            case KeyCode::D6:
                return "6";
            case KeyCode::D7:
                return "7";
            case KeyCode::D8:
                return "8";
            case KeyCode::D9:
                return "9";
            case KeyCode::F1:
                return "F1";
            case KeyCode::F2:
                return "F2";
            case KeyCode::F3:
                return "F3";
            case KeyCode::F4:
                return "F4";
            case KeyCode::F5:
                return "F5";
            case KeyCode::F6:
                return "F6";
            case KeyCode::F7:
                return "F7";
            case KeyCode::F8:
                return "F8";
            case KeyCode::F9:
                return "F9";
            case KeyCode::F10:
                return "F10";
            case KeyCode::F11:
                return "F11";
            case KeyCode::F12:
                return "F12";
            case KeyCode::Unknown:
            case KeyCode::Count:
                break;
            }

            return "Unknown";
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    String shortcutLabel(const Shortcut & value)
    {
        String result;

        if(value.modifiers.primary == true)
        {
            result += "Primary+";
        }

        if(value.modifiers.control == true)
        {
            result += "Ctrl+";
        }

        if(value.modifiers.super == true)
        {
            result += "Super+";
        }

        if(value.modifiers.alt == true)
        {
            result += "Alt+";
        }

        if(value.modifiers.shift == true)
        {
            result += "Shift+";
        }

        result += Detail::shortcutKeyLabel(value.key);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void ShortcutRegistry::bind(StringView command, const Shortcut & shortcut)
    {
        for(Binding & binding : m_bindings)
        {
            if(binding.command == command)
            {
                binding.shortcut = shortcut;

                return;
            }
        }
        m_bindings.push_back({String(command), shortcut, false});
    }
    //////////////////////////////////////////////////////////////////////////
    bool ShortcutRegistry::triggered(StringView command) const noexcept
    {
        for(const Binding & binding : m_bindings)
        {
            if(binding.command == command)
            {
                return binding.active;
            }
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool shortcut(Context * ui, const Shortcut & value, const ShortcutOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        if(ui->activeFrame == false)
        {
            return false;
        }

        auto returnedValue = Detail::shortcutTriggered(ui, ui->currentParent, value, options);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void setNextItemShortcut(Context * ui, const Shortcut & value, const ShortcutOptions & options) noexcept
    {
        if(ui == nullptr)
        {
            return;
        }

        if(ui->activeFrame == false)
        {
            return;
        }

        ui->nextItemShortcut = value;
        ui->nextItemShortcutOptions = options;
        ui->nextItemShortcutPending = true;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
