#pragma once

#include "Mosaic/Types.hpp"

namespace Mosaic
{
    struct Context;

    class Scope
    {
    public:
        Scope() noexcept = default;
        Scope(const Scope &) = delete;
        Scope & operator=(const Scope &) = delete;
        Scope(Scope && other) noexcept;
        Scope & operator=(Scope && other) noexcept;
        ~Scope();
        Scope(Context * context, uint64_t token, Id id, bool visible) noexcept;

        [[nodiscard]] Id id() const noexcept;
        [[nodiscard]] bool visible() const noexcept;
        explicit operator bool() const noexcept;

    protected:
        void reset() noexcept;

        Context * m_context = nullptr;
        uint64_t m_token = 0;
        Id m_id = InvalidId;
        bool m_visible = false;
    };

    class WindowScope final : public Scope
    {
    public:
        using Scope::Scope;
    };

    class TabBarScope final : public Scope
    {
    public:
        using Scope::Scope;
        TabBarScope(TabBarScope && other) noexcept;
        TabBarScope & operator=(TabBarScope && other) noexcept;
        ~TabBarScope();
    };

    class TreeScope final : public Scope
    {
    public:
        using Scope::Scope;
        TreeScope(WindowScope && other) noexcept;

        [[nodiscard]] bool expanded() const noexcept
        {
            auto returnedValue = visible();

            return returnedValue;
        }
    };
} // namespace Mosaic
