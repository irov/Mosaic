#pragma once

#include "Mosaic/Allocator.hpp"

#include <algorithm>
#include <cstddef>
#include <stdint.h>
#include <limits>
#include <source_location>
#include <type_traits>

namespace Mosaic
{
    struct SemanticNode;

    using Id = uint64_t;
    using PointerId = uint32_t;
    using TypeId = uint64_t;
    using TextureHandle = uint64_t;
    using FontHandle = uint64_t;
    using RenderTargetHandle = uint64_t;
    using SourceLocation = std::source_location;

    using IdVector = Vector<Id>;
    using IdSpan = Span<const Id>;
    using IdSet = UnorderedSet<Id>;
    using SizeVector = Vector<size_t>;
    using UInt32Vector = Vector<uint32_t>;
    using BoolSpan = Span<bool>;
    using SemanticNodeSpan = Span<const SemanticNode>;

    inline constexpr Id InvalidId = 0;
    inline constexpr Id RootId = 0xcbf29ce484222325ULL;
    inline constexpr FontHandle DefaultFont = 0;
    inline constexpr FontHandle MonospaceFont = 1;

    [[nodiscard]] constexpr Id hashBytes(StringView value) noexcept
    {
        Id hash = RootId;

        for(unsigned char byte : value)
        {
            hash ^= byte;
            hash *= 0x100000001b3ULL;
        }

        return hash == InvalidId ? 1 : hash;
    }

    [[nodiscard]] constexpr Id combineId(Id parent, Id local) noexcept
    {
        Id value = parent;

        for(size_t index = 0; index != sizeof(local); ++index)
        {
            value ^= static_cast<unsigned char>(local >> (index * 8U));
            value *= 0x100000001b3ULL;
        }

        return value == InvalidId ? 1 : value;
    }

    class Key
    {
    public:
        constexpr Key() noexcept = default;

        constexpr Key(StringView value) noexcept : m_value(hashBytes(value)), m_debug(value), m_explicit(true)
        {
        }

        template<class T> requires std::is_integral_v<T>
        constexpr Key(T value) noexcept : m_value(hashIntegral(value)), m_integral(static_cast<uint64_t>(value)), m_hasIntegral(true), m_explicit(true)
        {
        }

        [[nodiscard]] constexpr Id value() const noexcept
        {
            return m_value;
        }

        [[nodiscard]] constexpr StringView debug() const noexcept
        {
            return m_debug;
        }

        [[nodiscard]] constexpr bool hasIntegralDebug() const noexcept
        {
            return m_hasIntegral;
        }

        [[nodiscard]] constexpr uint64_t integralDebug() const noexcept
        {
            return m_integral;
        }

        [[nodiscard]] constexpr bool isExplicit() const noexcept
        {
            return m_explicit;
        }

    private:
        template<class T> [[nodiscard]] static constexpr Id hashIntegral(T input) noexcept
        {
            using U = std::make_unsigned_t<T>;
            U value = static_cast<U>(input);
            Id hash = RootId;

            for(size_t index = 0; index != sizeof(U); ++index)
            {
                hash ^= static_cast<unsigned char>(value >> (index * 8U));
                hash *= 0x100000001b3ULL;
            }

            return hash == InvalidId ? 1 : hash;
        }

    private:
        Id m_value = RootId;
        StringView m_debug;
        uint64_t m_integral = 0;
        bool m_hasIntegral = false;
        bool m_explicit = false;
    };

    struct Vec2
    {
        float x = 0.f;
        float y = 0.f;

        [[nodiscard]] constexpr Vec2 operator+(const Vec2 & other) const noexcept
        {
            return {x + other.x, y + other.y};
        }

        [[nodiscard]] constexpr Vec2 operator-(const Vec2 & other) const noexcept
        {
            return {x - other.x, y - other.y};
        }

        [[nodiscard]] constexpr Vec2 operator*(float scalar) const noexcept
        {
            return {x * scalar, y * scalar};
        }

        [[nodiscard]] constexpr bool operator==(const Vec2 &) const noexcept = default;
    };

    using Vec2Vector = Vector<Vec2>;
    using Vec2Span = Span<const Vec2>;
    using Vec2Triangle = Array<Vec2, 3>;
    using Vec2Quad = Array<Vec2, 4>;

    struct Rect
    {
        float x = 0.f;
        float y = 0.f;
        float width = 0.f;
        float height = 0.f;

        [[nodiscard]] constexpr float right() const noexcept
        {
            return x + width;
        }

        [[nodiscard]] constexpr float bottom() const noexcept
        {
            return y + height;
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return width <= 0.f || height <= 0.f;
        }

        [[nodiscard]] constexpr bool contains(const Vec2 & point) const noexcept
        {
            auto returnedValue = point.x >= x && point.y >= y && point.x < right() && point.y < bottom();

            return returnedValue;
        }

        [[nodiscard]] constexpr Rect inset(float amount) const noexcept
        {
            Rect result = {x + amount, y + amount, std::max(0.f, width - amount * 2.f), std::max(0.f, height - amount * 2.f)};

            return result;
        }

        [[nodiscard]] static constexpr Rect intersection(const Rect & left, const Rect & right) noexcept
        {
            float minimumX = std::max(left.x, right.x);
            float minimumY = std::max(left.y, right.y);
            float maximumX = std::min(left.right(), right.right());
            float maximumY = std::min(left.bottom(), right.bottom());
            Rect result = {minimumX, minimumY, std::max(0.f, maximumX - minimumX), std::max(0.f, maximumY - minimumY)};

            return result;
        }

        [[nodiscard]] constexpr bool operator==(const Rect &) const noexcept = default;
    };

    struct EdgeInsets
    {
        float left = 0.f;
        float top = 0.f;
        float right = 0.f;
        float bottom = 0.f;

        constexpr EdgeInsets() noexcept = default;

        constexpr explicit EdgeInsets(float value) noexcept : left(value), top(value), right(value), bottom(value)
        {
        }

        constexpr EdgeInsets(float horizontal, float vertical) noexcept : left(horizontal), top(vertical), right(horizontal), bottom(vertical)
        {
        }

        [[nodiscard]] constexpr bool operator==(const EdgeInsets &) const noexcept = default;
    };

    struct Color
    {
        float r = 0.f;
        float g = 0.f;
        float b = 0.f;
        float a = 1.f;

        [[nodiscard]] static constexpr Color fromBytes(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) noexcept
        {
            constexpr float scale = 1.f / 255.f;

            return {red * scale, green * scale, blue * scale, alpha * scale};
        }

        [[nodiscard]] constexpr bool operator==(const Color &) const noexcept = default;
    };

    struct ColoredPoint
    {
        Vec2 position;
        Color color;
    };

    using ColoredPointVector = Vector<ColoredPoint>;
    using ColoredPointSpan = Span<const ColoredPoint>;
    using ColorVector = Vector<Color>;
    using ColorSpan = Span<const Color>;

    enum class Orientation : uint8_t
    {
        Horizontal,
        Vertical
    };

    enum class CursorShape : uint8_t
    {
        Arrow,
        Hand,
        Text,
        ResizeHorizontal,
        ResizeVertical,
        ResizeDiagonalNesw,
        ResizeDiagonalNwse,
        ResizeAll,
        Crosshair,
        Wait,
        Progress,
        NotAllowed
    };

    enum class SizeRule : uint8_t
    {
        Auto,
        Content,
        Fixed,
        Fill,
        Percent
    };

    struct Dimension
    {
        SizeRule rule = SizeRule::Content;
        float value = 0.f;

        constexpr Dimension() noexcept = default;

        constexpr Dimension(SizeRule sizeRule) noexcept : rule(sizeRule)
        {
        }

        [[nodiscard]] static constexpr Dimension fixed(float value) noexcept
        {
            return {SizeRule::Fixed, value};
        }

        [[nodiscard]] static constexpr Dimension percent(float value) noexcept
        {
            return {SizeRule::Percent, value};
        }

    private:
        constexpr Dimension(SizeRule sizeRule, float sizeValue) noexcept : rule(sizeRule), value(sizeValue)
        {
        }
    };

    enum class CrossAxisAlignment : uint8_t
    {
        Start,
        Center,
        End,
        Baseline
    };

    struct LayoutOptions
    {
        Dimension width = SizeRule::Content;
        Dimension height = SizeRule::Content;
        Vec2 minimum = {0.f, 0.f};
        Vec2 maximum = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        EdgeInsets padding = {};
        // Relative visual offset applied after normal flow placement. The item keeps
        // its normal flow allocation, which makes this suitable for cursor offsets
        // and small alignment adjustments without switching to absolute layout.
        Vec2 offset;
        float gap = -1.f;
        CrossAxisAlignment crossAxisAlignment = CrossAxisAlignment::Center;
        uint32_t columns = 1;
        Orientation orientation = Orientation::Horizontal;
        float splitRatio = 0.5f;
        Rect absoluteRect = {};
    };

    enum class BlendMode : uint8_t
    {
        Alpha,
        PremultipliedAlpha,
        Additive,
        Multiply,
        Opaque
    };

    enum class Validation : uint8_t
    {
        Normal,
        Warning,
        Error
    };

    enum class SemanticRole : uint8_t
    {
        None,
        Window,
        Group,
        Button,
        Checkbox,
        Radio,
        Text,
        TextField,
        Slider,
        Tree,
        TreeItem,
        Table,
        Row,
        Cell,
        Tab,
        TabList,
        Menu,
        MenuItem,
        Image
    };

    struct VisibleRange
    {
        size_t begin = 0;
        size_t end = 0;
    };

    struct DragPayload
    {
        TypeId type = 0;
        ByteSpan data;
    };
} // namespace Mosaic
