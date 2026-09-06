#include "FrameArena.hpp"

#include <algorithm>
#include <cstdint>

namespace Mosaic
{
    namespace Detail
    {
        namespace FrameArenaDetail
        {
            constexpr size_t BlockSize = 1024U * 1024U;

            [[nodiscard]] size_t alignedOffset(const std::byte * memory, size_t offset, size_t alignment) noexcept
            {
                uintptr_t address = reinterpret_cast<uintptr_t>(memory + offset);
                uintptr_t mask = static_cast<uintptr_t>(alignment - 1U);
                uintptr_t aligned = (address + mask) & ~mask;
                size_t returnedValue = static_cast<size_t>(aligned - reinterpret_cast<uintptr_t>(memory));

                return returnedValue;
            }
        } // namespace FrameArenaDetail

        FrameArena::~FrameArena()
        {
            for(const Block & block : m_blocks)
            {
                block.allocator->deallocate(block.memory, block.capacity, block.alignment);
            }
        }

        void FrameArena::setAllocator(Allocator * allocator) noexcept
        {
            m_allocator = AllocatorReference(allocator == nullptr ? defaultAllocator() : *allocator);
        }

        void FrameArena::reset() noexcept
        {
            for(Block & block : m_blocks)
            {
                block.used = 0;
            }

            m_block = 0;
        }

        void * FrameArena::allocate(size_t size, size_t alignment) noexcept
        {
            if(size == 0)
            {
                size = 1;
            }

            if(alignment == 0 || (alignment & (alignment - 1U)) != 0)
            {
                return nullptr;
            }

            for(size_t index = m_block; index != m_blocks.size(); ++index)
            {
                Block & block = m_blocks[index];

                if(block.alignment < alignment)
                {
                    continue;
                }

                size_t offset = FrameArenaDetail::alignedOffset(block.memory, block.used, alignment);

                if(offset <= block.capacity && size <= block.capacity - offset)
                {
                    block.used = offset + size;
                    m_block = index;

                    return block.memory + offset;
                }
            }

            size_t blockAlignment = std::max(alignment, alignof(std::max_align_t));
            // The allocator already aligns the block base; extra padding can overflow.
            size_t capacity = std::max(FrameArenaDetail::BlockSize, size);
            void * memory = m_allocator->allocate(capacity, blockAlignment);

            if(memory == nullptr)
            {
                return nullptr;
            }

            Block block;
            block.allocator = m_allocator;
            block.memory = static_cast<std::byte *>(memory);
            block.capacity = capacity;
            block.used = 0;
            block.alignment = blockAlignment;
            m_blocks.push_back(block);
            m_block = m_blocks.size() - 1U;
            Block & added = m_blocks.back();
            size_t offset = FrameArenaDetail::alignedOffset(added.memory, 0, alignment);
            added.used = offset + size;

            return added.memory + offset;
        }

        size_t FrameArena::reservedMemory() const noexcept
        {
            size_t result = 0;

            for(const Block & block : m_blocks)
            {
                result += block.capacity;
            }

            return result;
        }

        size_t FrameArena::usedMemory() const noexcept
        {
            size_t result = 0;

            for(const Block & block : m_blocks)
            {
                result += block.used;
            }

            return result;
        }
    } // namespace Detail
} // namespace Mosaic
