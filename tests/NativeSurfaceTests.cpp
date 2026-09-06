#include <Mosaic/Mosaic.hpp>

#include <cstdio>

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

    class SurfacePlatform final : public Mosaic::PlatformAdapter
    {
    public:
        size_t created = 0;
        size_t destroyed = 0;

        bool getClipboardText(Mosaic::String *) override { return false; }
        void setClipboardText(Mosaic::StringView) override {}
        bool writeConsole(Mosaic::StringView) override { return false; }
        bool readFile(Mosaic::StringView, Mosaic::ByteVector *) override { return false; }
        bool writeFile(Mosaic::StringView, Mosaic::ByteSpan) override { return false; }
        bool userDataPath(Mosaic::StringView, Mosaic::StringView, Mosaic::String *) override { return false; }
        double monotonicTime() const noexcept override { return 0; }
        void setCursor(Mosaic::CursorShape) override {}
        void setImeCandidateRect(const Mosaic::Rect &) override {}
        void * createWindow(const Mosaic::NativeWindowDescription &) override { return nullptr; }
        void destroyWindow(void *) override {}
        void showWindow(void *, bool) override {}
        void setWindowBounds(void *, const Mosaic::Rect &) override {}
        Mosaic::MonitorSpan monitors() const noexcept override { return {}; }
        void publishAccessibilityTree(Mosaic::SemanticNodeSpan) override {}

        bool createNativeSurface(const Mosaic::NativeSurfaceDescription &, Mosaic::NativeSurfaceHandle * output) override
        {
            *output = this;
            ++created;
            return true;
        }

        bool destroyNativeSurface(Mosaic::NativeSurfaceHandle surface) override
        {
            check(surface == this, "native surfaces are destroyed by their creating platform");
            ++destroyed;
            return true;
        }

        bool showNativeSurface(Mosaic::NativeSurfaceHandle, bool) override { return true; }
        bool setNativeSurfaceBounds(Mosaic::NativeSurfaceHandle, const Mosaic::Rect &) override { return true; }
    };

    void platformOwnership()
    {
        SurfacePlatform first;
        SurfacePlatform second;
        Mosaic::Context * ui = Mosaic::newContext(&first);
        auto submit = [&]()
        {
            Mosaic::beginFrame(ui, {});
            {
                Mosaic::NativeSurfaceOptions options;
                options.forwardInput = false;
                auto surface = Mosaic::nativeSurface(ui, Mosaic::Key("surface"), "Surface", options, {});
            }
            (void)Mosaic::endFrame(ui);
        };
        submit();
        check(first.created == 1, "initial platform creates the surface");
        Mosaic::setPlatformAdapter(ui, &first);
        check(first.destroyed == 0, "setting the same platform preserves native surfaces");
        Mosaic::setPlatformAdapter(ui, &second);
        check(first.destroyed == 1, "changing platform releases old native surfaces immediately");
        submit();
        check(second.created == 1, "new platform recreates submitted native surfaces");
        Mosaic::deleteContext(ui);
        check(second.destroyed == 1, "context deletion releases the new platform's native surfaces");
    }
}

int main()
{
    platformOwnership();
    return failures == 0 ? 0 : 1;
}
