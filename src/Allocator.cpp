#include "Mosaic/Allocator.hpp"

#include <atomic>

namespace Mosaic
{
    namespace Detail
    {
        class SystemAllocator final : public Allocator
        {
        public:
            //////////////////////////////////////////////////////////////////////////
            [[nodiscard]] void * allocate(size_t size, size_t alignment) noexcept override
            {
                if(size == 0)
                {
                    size = 1;
                }

                auto returnedValue = ::operator new(size, std::align_val_t(alignment), std::nothrow);

                return returnedValue;
            }
            //////////////////////////////////////////////////////////////////////////
            void deallocate(void * memory, size_t, size_t alignment) noexcept override
            {
                ::operator delete(memory, std::align_val_t(alignment));
            }
        };

        SystemAllocator g_systemAllocator;
        std::atomic<Allocator *> g_defaultAllocator = &g_systemAllocator;
        thread_local Allocator * g_constructionAllocator = nullptr;
        //////////////////////////////////////////////////////////////////////////
        Allocator * setConstructionAllocator(Allocator * allocator) noexcept
        {
            Allocator * previous = g_constructionAllocator;
            g_constructionAllocator = allocator;

            return previous;
        }
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Allocator & defaultAllocator() noexcept
    {
        Allocator * allocator = Detail::g_constructionAllocator;

        if(allocator == nullptr)
        {
            allocator = Detail::g_defaultAllocator.load(std::memory_order_acquire);
        }

        Allocator & returnedValue = *allocator;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void setDefaultAllocator(Allocator * allocator) noexcept
    {
        Detail::g_defaultAllocator.store(allocator == nullptr ? &Detail::g_systemAllocator : allocator, std::memory_order_release);
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
