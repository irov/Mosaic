#pragma once

#include "Mosaic/Allocator.hpp"
#include "Mosaic/Types.hpp"

#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace Mosaic
{
    namespace Detail
    {
        class FrameArena
        {
        public:
            FrameArena() = default;
            FrameArena(const FrameArena &) = delete;
            FrameArena & operator=(const FrameArena &) = delete;
            ~FrameArena();

            void setAllocator(Allocator * allocator) noexcept;
            void reset() noexcept;
            [[nodiscard]] void * allocate(size_t size, size_t alignment) noexcept;
            [[nodiscard]] size_t reservedMemory() const noexcept;
            [[nodiscard]] size_t usedMemory() const noexcept;

            template<class T, class... Args>
            [[nodiscard]] T * make(Args &&... args) noexcept
            {
                static_assert(std::is_nothrow_constructible_v<T, Args...>);
                static_assert(std::is_trivially_destructible_v<T>);
                void * memory = allocate(sizeof(T), alignof(T));

                if(memory == nullptr)
                {
                    return nullptr;
                }

                T * value = ::new(memory) T(std::forward<Args>(args)...);

                return value;
            }

        private:
            struct Block
            {
                AllocatorReference allocator;
                std::byte * memory = nullptr;
                size_t capacity = 0;
                size_t used = 0;
                size_t alignment = alignof(std::max_align_t);
            };

            using BlockVector = Vector<Block>;

            AllocatorReference m_allocator;
            BlockVector m_blocks;
            size_t m_block = 0;
        };
    } // namespace Detail
} // namespace Mosaic
