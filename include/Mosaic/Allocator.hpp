#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <stdint.h>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Mosaic
{
    class Allocator
    {
    public:
        virtual ~Allocator() = default;

        [[nodiscard]] virtual void * allocate(size_t size, size_t alignment) noexcept = 0;
        virtual void deallocate(void * memory, size_t size, size_t alignment) noexcept = 0;

        // Host allocators keep their application-owned lifetime. Internal allocator
        // wrappers override these hooks to survive containers copied out of a context.
        virtual void retain() noexcept {}
        virtual void release() noexcept {}
    };

    [[nodiscard]] Allocator & defaultAllocator() noexcept;
    void setDefaultAllocator(Allocator * allocator) noexcept;

    namespace Detail
    {
        Allocator * setConstructionAllocator(Allocator * allocator) noexcept;

        class AllocatorReference
        {
        public:
            explicit AllocatorReference(Allocator & allocator = defaultAllocator()) noexcept : m_allocator(&allocator)
            {
                m_allocator->retain();
            }

            AllocatorReference(const AllocatorReference & other) noexcept : AllocatorReference(*other.m_allocator) {}

            AllocatorReference & operator=(const AllocatorReference & other) noexcept
            {
                other.m_allocator->retain();
                m_allocator->release();
                m_allocator = other.m_allocator;
                return *this;
            }

            ~AllocatorReference() { m_allocator->release(); }

            [[nodiscard]] Allocator * get() const noexcept { return m_allocator; }
            [[nodiscard]] Allocator * operator->() const noexcept { return m_allocator; }

        private:
            Allocator * m_allocator;
        };

        class ConstructionAllocatorScope
        {
        public:
            explicit ConstructionAllocatorScope(Allocator * allocator) noexcept : m_previous(setConstructionAllocator(allocator)) {}
            ~ConstructionAllocatorScope() { setConstructionAllocator(m_previous); }
            ConstructionAllocatorScope(const ConstructionAllocatorScope &) = delete;
            ConstructionAllocatorScope & operator=(const ConstructionAllocatorScope &) = delete;

        private:
            Allocator * m_previous;
        };
    }

    template<class T> class StlAllocator
    {
    public:
        using value_type = T;
        using propagate_on_container_copy_assignment = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_swap = std::true_type;
        using is_always_equal = std::false_type;

        template<class U> friend class StlAllocator;

        StlAllocator() noexcept = default;

        explicit StlAllocator(Allocator & allocator) noexcept : m_allocator(allocator)
        {
        }

        template<class U> StlAllocator(const StlAllocator<U> & other) noexcept : m_allocator(*other.resource())
        {
        }

        template<class U, class... Args> void construct(U * memory, Args &&... args)
        {
            Detail::ConstructionAllocatorScope scope(resource());
            ::new(static_cast<void *>(memory)) U(std::forward<Args>(args)...);
        }

        [[nodiscard]] T * allocate(size_t count)
        {
            if(count > std::numeric_limits<size_t>::max() / sizeof(T))
            {
                std::terminate();
            }

            void * memory = m_allocator->allocate(count * sizeof(T), alignof(T));

            if(memory == nullptr)
            {
                std::terminate();
            }

            auto returnedValue = static_cast<T *>(memory);

            return returnedValue;
        }

        void deallocate(T * memory, size_t count) noexcept
        {
            m_allocator->deallocate(memory, count * sizeof(T), alignof(T));
        }

        [[nodiscard]] Allocator * resource() const noexcept
        {
            return m_allocator.get();
        }

        template<class U> [[nodiscard]] bool operator==(const StlAllocator<U> & other) const noexcept
        {
            auto returnedValue = resource() == other.resource();

            return returnedValue;
        }

    private:
        Detail::AllocatorReference m_allocator;
    };

    template<class T> using Vector = std::vector<T, StlAllocator<T>>;

    using String = std::basic_string<char, std::char_traits<char>, StlAllocator<char>>;
    using StringView = std::string_view;

    template<class T, size_t Size> using Array = std::array<T, Size>;

    template<class T, size_t Extent = std::dynamic_extent> using Span = std::span<T, Extent>;

    using ByteVector = Vector<std::byte>;
    using ByteSpan = Span<const std::byte>;
    using FloatVector = Vector<float>;
    using FloatSpan = Span<float>;
    using ConstFloatSpan = Span<const float>;
    using Int32Span = Span<int32_t>;
    using StringVector = Vector<String>;
    using StringViewVector = Vector<StringView>;
    using StringViewSpan = Span<const StringView>;

    template<class Key, class Value, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>> using UnorderedMap = std::unordered_map<Key, Value, Hash, Equal, StlAllocator<std::pair<const Key, Value>>>;

    template<class Key, class Hash = std::hash<Key>, class Equal = std::equal_to<Key>> using UnorderedSet = std::unordered_set<Key, Hash, Equal, StlAllocator<Key>>;

    template<class T> class AllocatorDeleter
    {
    public:
        AllocatorDeleter() noexcept = default;

        explicit AllocatorDeleter(Allocator & allocator) noexcept : m_allocator(allocator)
        {
        }

        void operator()(T * value) const noexcept
        {
            if(value == nullptr)
            {
                return;
            }

            value->~T();
            m_allocator->deallocate(value, sizeof(T), alignof(T));
        }

    private:
        Detail::AllocatorReference m_allocator;
    };

    template<class T> using UniquePtr = std::unique_ptr<T, AllocatorDeleter<T>>;

    template<class T, class... Args> [[nodiscard]] UniquePtr<T> makeUnique(Allocator & allocator, Args &&... args)
    {
        void * memory = allocator.allocate(sizeof(T), alignof(T));

        if(memory == nullptr)
        {
            std::terminate();
        }

        // Own the raw allocation before invoking a potentially throwing constructor.
        struct ConstructionGuard
        {
            Allocator & allocator;
            void * memory;
            ~ConstructionGuard()
            {
                if(memory != nullptr)
                {
                    allocator.deallocate(memory, sizeof(T), alignof(T));
                }
            }
        } guard{allocator, memory};
        Detail::ConstructionAllocatorScope scope(&allocator);
        T * value = ::new(memory) T(std::forward<Args>(args)...);
        guard.memory = nullptr;
        auto returnedValue = UniquePtr<T>(value, AllocatorDeleter<T>(allocator));

        return returnedValue;
    }
} // namespace Mosaic
