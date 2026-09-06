#include <Mosaic/Mosaic.hpp>
#include "FrameArena.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace
{
    int failures = 0;

    void check(bool condition, const char * message)
    {
        if(condition == false)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
            ++failures;
        }
    }

    class CountingAllocator final : public Mosaic::Allocator
    {
    public:
        Mosaic::Allocator * backing = &Mosaic::defaultAllocator();
        size_t live = 0;

        void * allocate(size_t size, size_t alignment) noexcept override
        {
            if(size > 8U * 1024U * 1024U)
            {
                return nullptr;
            }
            void * memory = backing->allocate(size, alignment);
            live += memory != nullptr;
            return memory;
        }

        void deallocate(void * memory, size_t size, size_t alignment) noexcept override
        {
            if(memory != nullptr)
            {
                --live;
            }
            backing->deallocate(memory, size, alignment);
        }
    };

    void arenaOwnership()
    {
        CountingAllocator first;
        CountingAllocator second;
        {
            Mosaic::Detail::FrameArena arena;
            arena.setAllocator(&first);
            check(arena.allocate(32, 32) != nullptr, "arena allocation succeeds");
            arena.setAllocator(&second);
            check(arena.allocate(2U * 1024U * 1024U, 64) != nullptr, "arena can change allocator");
        }
        check(first.live == 0 && second.live == 0, "arena frees each block through its owning allocator");

        CountingAllocator allocator;
        {
            Mosaic::Detail::FrameArena arena;
            arena.setAllocator(&allocator);
            check(arena.allocate(std::numeric_limits<size_t>::max(), 16) == nullptr, "oversized arena request cannot wrap to a small allocation");
            check(arena.allocate(32, 3) == nullptr, "arena rejects invalid alignment");
        }
        check(allocator.live == 0, "arena releases allocations");
    }

    void contextOwnership()
    {
        Mosaic::Allocator * original = &Mosaic::defaultAllocator();
        CountingAllocator allocator;
        Mosaic::ContextOptions options;
        options.allocator = &allocator;
        Mosaic::Context * first = Mosaic::newContext(options);
        Mosaic::Context * second = Mosaic::newContext(options);
        Mosaic::String applicationValue;
        Mosaic::Frame savedFrame;
        Mosaic::beginFrame(first, {});
        check(&Mosaic::defaultAllocator() == original, "beginFrame preserves the application allocator");
        applicationValue = Mosaic::String(128, 'a');
        Mosaic::text(first, "First context");
        Mosaic::beginFrame(second, {});
        Mosaic::text(second, "Second context");
        savedFrame = Mosaic::endFrame(first);
        Mosaic::deleteContext(first);
        (void)Mosaic::endFrame(second);
        Mosaic::deleteContext(second);
        check(&Mosaic::defaultAllocator() == original, "interleaved contexts restore the application allocator");
        applicationValue.append(128, 'b');
        savedFrame.diagnostics.emplace_back(128, 'c');
        savedFrame = {};
        applicationValue = {};
        check(allocator.live == 0, "copied frame storage releases its allocator after context destruction");
    }

    void allocatorConstruction()
    {
        CountingAllocator allocator;
        Mosaic::Allocator * original = &Mosaic::defaultAllocator();
        {
            Mosaic::Vector<Mosaic::String> values{Mosaic::StlAllocator<Mosaic::String>(allocator)};
            values.emplace_back(128, 'x');
            check(values.front().get_allocator().resource() == &allocator, "nested containers inherit their owner's allocator");
            auto value = Mosaic::makeUnique<Mosaic::String>(allocator, 128, 'y');
            check(value->get_allocator().resource() == &allocator, "makeUnique routes nested storage to the requested allocator");
            check(&Mosaic::defaultAllocator() == original, "construction restores the application allocator");
        }
        check(allocator.live == 0, "nested allocations are released");
#if defined(__cpp_exceptions)
        struct ThrowingValue
        {
            ThrowingValue() { throw std::runtime_error("constructor failure"); }
        };
        try
        {
            (void)Mosaic::makeUnique<ThrowingValue>(allocator);
            check(false, "constructor exception propagates");
        }
        catch(const std::runtime_error &)
        {
        }
        check(allocator.live == 0, "makeUnique releases storage when construction throws");
        check(&Mosaic::defaultAllocator() == original, "constructor exception restores the application allocator");
#endif
    }

    void dockingInvariants()
    {
        Mosaic::DockModel model;
        check(model.dock(1, model.root()), "dock initial window");
        check(model.dock(2, model.root(), Mosaic::DockPlacement::Right), "split root");
        check(model.dock(3, model.nodeForWindow(1), Mosaic::DockPlacement::Bottom), "split nested leaf");
        check(model.dock(4, model.root()), "dock to an arbitrarily nested split");
        check(model.validate(), "nested docking stays valid");
        check(model.dock(2, model.root()), "moving to a split does not lose the existing window");
        check(model.nodeForWindow(2) != 0, "moved window remains in the model");
        check(model.dock(5, model.root(), Mosaic::DockPlacement::Left, std::numeric_limits<float>::quiet_NaN()) == false, "dock rejects NaN ratio");
        check(model.validate(), "invalid split ratio leaves model valid");

        Mosaic::DockNode invalid;
        invalid.id = 42;
        invalid.parent = 42;
        check(model.restore({&invalid, 1}, invalid.id) == false, "restore rejects a parented root");
        check(model.nodeForWindow(1) != 0, "failed restore preserves existing layout");

        Mosaic::DockModel central;
        check(central.dock(10, central.root()), "populate central node");
        check(central.dock(11, central.root(), Mosaic::DockPlacement::Right), "create central sibling");
        check(central.dock(12, central.nodeForWindow(11), Mosaic::DockPlacement::Bottom), "split central sibling");
        check(central.undock(10), "remove central window");
        const Mosaic::DockNode * newCentral = central.node(central.centralNode());
        check(newCentral != nullptr && newCentral->type == Mosaic::DockNodeType::Tabs, "central node remains a tab leaf after collapse");

        Mosaic::DockModel empty;
        check(empty.dock(20, empty.root(), Mosaic::DockPlacement::Right), "dock beside an empty central area");
        check(empty.validate(), "side docking into an empty model stays valid");

        Mosaic::DockNodeVector saved(model.nodes().begin(), model.nodes().end());
        const Mosaic::DockNodeId root = model.root();
        check(model.restore(model.nodes(), root), "restore accepts an aliased snapshot of itself");
        check(model.restore(saved, root), "restore accepts a valid snapshot");
        saved.front().id = 0;
        check(model.restore(saved, root) == false, "restore rejects a zero node ID");
        check(model.nodeForWindow(1) != 0, "invalid node ID preserves the current model");

        Mosaic::DockNode exhausted;
        exhausted.id = std::numeric_limits<Mosaic::DockNodeId>::max() - 1;
        check(empty.restore({&exhausted, 1}, exhausted.id), "restore accepts the last representable node ID");
        check(empty.dock(21, empty.root(), Mosaic::DockPlacement::Right) == false, "dock rejects exhausted node IDs without wrapping");
        check(empty.validate(), "node ID exhaustion preserves a valid model");
    }

    void dockingSequences()
    {
        Mosaic::DockModel model;
        Mosaic::IdSet expected;
        uint32_t random = 12345;
        auto next = [&]()
        {
            random = random * 1664525U + 1013904223U;
            return random;
        };
        for(size_t iteration = 0; iteration != 2000; ++iteration)
        {
            const Mosaic::Id window = 1 + next() % 24;
            if(next() % 5 == 0)
            {
                check(model.undock(window) == (expected.erase(window) != 0), "undock agrees with window membership");
            }
            else
            {
                const Mosaic::DockNodeId target = model.nodes()[next() % model.nodes().size()].id;
                const auto placement = static_cast<Mosaic::DockPlacement>(next() % 5);
                check(model.dock(window, target, placement), "valid docking sequence succeeds");
                expected.insert(window);
            }
            check(model.validate(), "docking sequence preserves tree invariants");
            const Mosaic::DockNode * central = model.node(model.centralNode());
            check(central != nullptr && central->central && central->type == Mosaic::DockNodeType::Tabs, "docking sequence preserves a central tab node");
            const auto layout = model.layout({0, 0, 800, 600});
            check(layout.size() == expected.size(), "docking sequence neither loses nor duplicates windows");
            for(Mosaic::Id id : expected)
            {
                check(model.nodeForWindow(id) != 0, "docking sequence retains every docked window");
            }
        }
    }

    void cancelTextEditing(bool multiline)
    {
        Mosaic::Context * ui = Mosaic::newContext();
        Mosaic::String value = "original";
        auto submit = [&]()
        {
            return multiline ? Mosaic::inputMultiline(ui, "Value", &value) : Mosaic::inputText(ui, "Value", &value);
        };
        Mosaic::beginFrame(ui, {});
        Mosaic::focusNextItem(ui);
        check(submit().focused(), "text editor takes focus");
        (void)Mosaic::endFrame(ui);
        Mosaic::Input typed;
        typed.text.emplace_back("changed");
        Mosaic::beginFrame(ui, typed);
        (void)submit();
        (void)Mosaic::endFrame(ui);
        check(value != "original", "text editor accepts changes");
        Mosaic::Input escape;
        escape.keyboard.push_back({.key = Mosaic::KeyCode::Escape, .pressed = true});
        Mosaic::beginFrame(ui, escape);
        const Mosaic::Response response = submit();
        (void)Mosaic::endFrame(ui);
        check(value == "original", "Escape restores the original edited value");
        check(response.canceled() && response.committed() == false, "Escape cancels rather than committing on focus loss");
        Mosaic::deleteContext(ui);
    }


}

int main()
{
    arenaOwnership();
    allocatorConstruction();
    dockingInvariants();
    dockingSequences();
    contextOwnership();
    cancelTextEditing(false);
    cancelTextEditing(true);
    if(failures != 0)
    {
        return 1;
    }
    std::puts("Mosaic core regressions passed");
    return 0;
}
