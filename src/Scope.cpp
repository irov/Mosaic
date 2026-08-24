#include "Mosaic/Scope.hpp"
#include "ContextDetail.hpp"

#include <utility>

namespace Mosaic
{
    //////////////////////////////////////////////////////////////////////////
    TabBarScope::TabBarScope(TabBarScope && other) noexcept : Scope(std::move(other))
    {
    }
    //////////////////////////////////////////////////////////////////////////
    TabBarScope & TabBarScope::operator=(TabBarScope && other) noexcept
    {
        if(this != &other)
        {
            if(m_context != nullptr)
            {
                Detail::finishTabBar(m_context, m_id);
            }

            Scope::operator=(std::move(other));
        }

        return *this;
    }
    //////////////////////////////////////////////////////////////////////////
    TabBarScope::~TabBarScope()
    {
        if(m_context != nullptr)
        {
            Detail::finishTabBar(m_context, m_id);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    TreeScope::TreeScope(WindowScope && other) noexcept : Scope(std::move(other))
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Scope::Scope(Context * context, uint64_t token, Id id, bool visible) noexcept : m_context(context), m_token(token), m_id(id), m_visible(visible)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Scope::Scope(Scope && other) noexcept : m_context(std::exchange(other.m_context, nullptr)), m_token(std::exchange(other.m_token, 0)), m_id(std::exchange(other.m_id, InvalidId)), m_visible(std::exchange(other.m_visible, false))
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Scope & Scope::operator=(Scope && other) noexcept
    {
        if(this != &other)
        {
            reset();
            m_context = std::exchange(other.m_context, nullptr);
            m_token = std::exchange(other.m_token, 0);
            m_id = std::exchange(other.m_id, InvalidId);
            m_visible = std::exchange(other.m_visible, false);
        }

        return *this;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope::~Scope()
    {
        reset();
    }
    //////////////////////////////////////////////////////////////////////////
    Id Scope::id() const noexcept
    {
        return m_id;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Scope::visible() const noexcept
    {
        return m_visible;
    }
    //////////////////////////////////////////////////////////////////////////
    Scope::operator bool() const noexcept
    {
        auto returnedValue = visible();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void Scope::reset() noexcept
    {
        if(m_context != nullptr)
        {
            Detail::closeScope(m_context, m_token);
            m_context = nullptr;
            m_token = 0;
            m_id = InvalidId;
            m_visible = false;
        }
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
