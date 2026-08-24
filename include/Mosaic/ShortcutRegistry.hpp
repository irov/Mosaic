#pragma once

#include "Mosaic/Input.hpp"

namespace Mosaic
{
    struct Context;

    struct Shortcut
    {
        KeyCode key = KeyCode::Unknown;
        Modifiers modifiers;

        [[nodiscard]] static constexpr Shortcut primary(KeyCode key) noexcept
        {
            Shortcut shortcut;
            shortcut.key = key;
            shortcut.modifiers.primary = true;

            return shortcut;
        }
    };

    enum class ShortcutRoute : uint8_t
    {
        Active,
        Focused,
        Global,
        Always
    };

    struct ShortcutOptions
    {
        ShortcutRoute route = ShortcutRoute::Focused;
        bool repeat = false;
        bool routeOverActive = false;
        bool routeOverFocused = false;
        bool routeUnlessBackgroundFocused = false;
        bool tooltip = false;
    };

    class ShortcutRegistry
    {
    public:
        void bind(StringView command, const Shortcut & shortcut);
        [[nodiscard]] bool triggered(StringView command) const noexcept;

    private:
        friend struct Context;

        struct Binding
        {
            String command;
            Shortcut shortcut;
            bool active = false;
        };

        using BindingVector = Vector<Binding>;
        BindingVector m_bindings;
    };

    [[nodiscard]] bool shortcut(Context * ui, const Shortcut & value, const ShortcutOptions & options = {}) noexcept;
    [[nodiscard]] String shortcutLabel(const Shortcut & value);
    void setNextItemShortcut(Context * ui, const Shortcut & value, const ShortcutOptions & options = {}) noexcept;
} // namespace Mosaic
