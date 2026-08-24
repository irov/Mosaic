#pragma once

#include "Mosaic/Types.hpp"

#include <stdint.h>

namespace Mosaic
{
    enum class PointerType : uint8_t
    {
        Mouse,
        Touch,
        Pen
    };

    enum class PointerButton : uint8_t
    {
        Primary = 0,
        Secondary = 1,
        Middle = 2,
        Auxiliary1 = 3,
        Auxiliary2 = 4
    };

    struct PointerState
    {
        using PositionArray = Array<Vec2, 5>;

        PointerId id = 0;
        PointerType type = PointerType::Mouse;
        Vec2 position;
        Vec2 delta;
        PositionArray pressPositions = {};
        float pressure = 0.f;
        uint8_t down = 0;
        uint8_t pressed = 0;
        uint8_t released = 0;
        uint8_t pressPositionValid = 0;
        Array<uint8_t, 5> clickCounts = {};
        // Compatibility value for adapters that only submit one click count.
        uint8_t clickCount = 0;

        [[nodiscard]] constexpr bool isDown(PointerButton button = PointerButton::Primary) const noexcept
        {
            auto returnedValue = (down & (1U << static_cast<unsigned>(button))) != 0;

            return returnedValue;
        }

        [[nodiscard]] constexpr bool isPressed(PointerButton button = PointerButton::Primary) const noexcept
        {
            auto returnedValue = (pressed & (1U << static_cast<unsigned>(button))) != 0;

            return returnedValue;
        }

        [[nodiscard]] constexpr bool isReleased(PointerButton button = PointerButton::Primary) const noexcept
        {
            auto returnedValue = (released & (1U << static_cast<unsigned>(button))) != 0;

            return returnedValue;
        }

        [[nodiscard]] constexpr Vec2 pressPosition(PointerButton button = PointerButton::Primary) const noexcept
        {
            unsigned index = static_cast<unsigned>(button);
            auto returnedValue = (pressPositionValid & (1U << index)) != 0 ? pressPositions[index] : position;

            return returnedValue;
        }

        [[nodiscard]] constexpr uint8_t buttonClickCount(PointerButton button = PointerButton::Primary) const noexcept
        {
            unsigned index = static_cast<unsigned>(button);
            auto returnedValue = clickCounts[index] != 0 ? clickCounts[index] : (isPressed(button) ? clickCount : 0);

            return returnedValue;
        }
    };

    using PointerStateVector = Vector<PointerState>;

    enum class KeyCode : uint16_t
    {
        Unknown,
        Tab,
        Enter,
        Escape,
        Space,
        Backspace,
        Delete,
        Left,
        Right,
        Up,
        Down,
        Home,
        End,
        PageUp,
        PageDown,
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        D0,
        D1,
        D2,
        D3,
        D4,
        D5,
        D6,
        D7,
        D8,
        D9,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        Count
    };

    struct Modifiers
    {
        bool shift = false;
        bool control = false;
        bool alt = false;
        bool super = false;
        bool primary = false;
    };

    struct KeyEvent
    {
        KeyCode key = KeyCode::Unknown;
        bool pressed = false;
        bool released = false;
        bool repeat = false;
        Modifiers modifiers;
    };

    using KeyEventVector = Vector<KeyEvent>;

    enum class ImeEventType : uint8_t
    {
        Start,
        Update,
        Commit,
        Cancel
    };

    struct ImeEvent
    {
        ImeEventType type = ImeEventType::Update;
        String text;
        size_t selectionBegin = 0;
        size_t selectionEnd = 0;
    };

    using ImeEventVector = Vector<ImeEvent>;

    struct Input
    {
        PointerStateVector pointers;
        KeyEventVector keyboard;
        StringVector text;
        ImeEventVector ime;
        Vec2 wheel;
        Modifiers modifiers;
        bool windowFocused = true;
        float deltaTime = 1.f / 60.f;
        double timestamp = 0.0;

        [[nodiscard]] const PointerState * primaryPointer() const noexcept
        {
            auto returnedValue = pointers.empty() == true ? nullptr : &pointers.front();

            return returnedValue;
        }

        [[nodiscard]] bool keyPressed(KeyCode key) const noexcept
        {
            for(const KeyEvent & event : keyboard)
            {
                if(event.key == key && event.pressed == true)
                {
                    return true;
                }
            }

            return false;
        }
    };

    struct Viewport
    {
        uint64_t id = 1;
        Rect bounds = {0.f, 0.f, 1280.f, 720.f};
        Rect workArea = {0.f, 0.f, 1280.f, 720.f};
        float dpiScale = 1.f;
        bool focused = true;
        void * nativeHandle = nullptr;
        RenderTargetHandle renderTarget = 0;
    };

    struct InputCapture
    {
        bool pointer = false;
        bool pointerUnlessPopupClose = false;
        bool keyboard = false;
        bool text = false;
    };

    struct InputCaptureOverride
    {
        // -1 keeps the value calculated by Mosaic, 0 clears it, 1 forces it.
        int8_t pointer = -1;
        int8_t keyboard = -1;
        int8_t text = -1;
    };
} // namespace Mosaic
