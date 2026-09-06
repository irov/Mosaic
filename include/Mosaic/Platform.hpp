#pragma once

#include "Mosaic/Input.hpp"

namespace Mosaic
{
    struct Monitor
    {
        uint64_t id = 0;
        Rect bounds;
        Rect workArea;
        float dpiScale = 1.f;
        bool primary = false;
    };

    using MonitorVector = Vector<Monitor>;
    using MonitorSpan = Span<const Monitor>;

    struct NativeWindowDescription
    {
        String title;
        Rect bounds;
        bool decorated = true;
        bool resizable = true;
        bool alwaysOnTop = false;
    };

    using NativeSurfaceHandle = void *;

    enum class NativeSurfaceKind : uint8_t
    {
        Scene,
        Design,
        Motion,
        Custom
    };

    struct NativeSurfaceDescription
    {
        uint64_t id = 0;
        NativeSurfaceKind kind = NativeSurfaceKind::Scene;
        NativeSurfaceHandle parent = nullptr;
        Rect bounds;
        uint64_t applicationTag = 0;
        bool visible = true;
        bool focusable = true;
        bool forwardInput = true;
    };

    using NativeSurfaceInputCallback = void (*)(NativeSurfaceHandle surface, const Input & input, void * userData);

    class PlatformAdapter
    {
    public:
        virtual ~PlatformAdapter() = default;

        [[nodiscard]] virtual bool getClipboardText(String * const _out) = 0;
        virtual void setClipboardText(StringView text) = 0;
        [[nodiscard]] virtual bool writeConsole(StringView text) = 0;
        [[nodiscard]] virtual bool readFile(StringView path, ByteVector * const _out) = 0;
        [[nodiscard]] virtual bool writeFile(StringView path, ByteSpan data) = 0;
        [[nodiscard]] virtual bool userDataPath(StringView application, StringView filename, String * const _out) = 0;
        [[nodiscard]] virtual double monotonicTime() const noexcept = 0;
        virtual void setCursor(CursorShape cursor) = 0;
        virtual void setImeCandidateRect(const Rect & screenRect) = 0;

        virtual void openUrl(StringView url)
        {
            (void)url;
        }

        [[nodiscard]] virtual bool supportsScreenColorPicker() const noexcept
        {
            return false;
        }

        virtual void beginScreenColorPick(Id request)
        {
            (void)request;
        }

        [[nodiscard]] virtual bool consumeScreenColorPick(Id request, Color * const _out)
        {
            (void)request;
            (void)_out;

            return false;
        }

        [[nodiscard]] virtual void * createWindow(const NativeWindowDescription & description) = 0;
        virtual void destroyWindow(void * nativeHandle) = 0;
        virtual void showWindow(void * nativeHandle, bool visible) = 0;
        virtual void setWindowBounds(void * nativeHandle, const Rect & bounds) = 0;

        [[nodiscard]] virtual bool createNativeSurface(const NativeSurfaceDescription & description, NativeSurfaceHandle * const _out)
        {
            (void)description;
            (void)_out;

            return false;
        }

        virtual bool destroyNativeSurface(NativeSurfaceHandle surface)
        {
            (void)surface;

            return false;
        }

        virtual bool showNativeSurface(NativeSurfaceHandle surface, bool visible)
        {
            (void)surface;
            (void)visible;

            return false;
        }

        virtual bool setNativeSurfaceBounds(NativeSurfaceHandle surface, const Rect & bounds)
        {
            (void)surface;
            (void)bounds;

            return false;
        }

        [[nodiscard]] virtual bool nativeSurfaceBounds(NativeSurfaceHandle surface, Rect * const _out) const
        {
            (void)surface;
            (void)_out;

            return false;
        }

        [[nodiscard]] virtual bool nativeSurfaceScreenBounds(NativeSurfaceHandle surface, Rect * const _out) const
        {
            (void)surface;
            (void)_out;

            return false;
        }

        [[nodiscard]] virtual bool nativeSurfaceDpiScale(NativeSurfaceHandle surface, float * const _out) const
        {
            (void)surface;
            (void)_out;

            return false;
        }

        virtual bool focusNativeSurface(NativeSurfaceHandle surface)
        {
            (void)surface;

            return false;
        }

        [[nodiscard]] virtual bool nativeSurfaceFocused(NativeSurfaceHandle surface, bool * const _out) const
        {
            (void)surface;
            (void)_out;

            return false;
        }

        virtual bool setNativeSurfaceParent(NativeSurfaceHandle surface, NativeSurfaceHandle parent)
        {
            (void)surface;
            (void)parent;

            return false;
        }

        [[nodiscard]] virtual bool nativeSurfaceRenderHandle(NativeSurfaceHandle surface, NativeSurfaceHandle * const _out) const
        {
            (void)surface;
            (void)_out;

            return false;
        }

        virtual bool setNativeSurfaceInputCallback(NativeSurfaceHandle surface, NativeSurfaceInputCallback callback, void * userData)
        {
            (void)surface;
            (void)callback;
            (void)userData;

            return false;
        }

        [[nodiscard]] virtual MonitorSpan monitors() const noexcept = 0;
        virtual void publishAccessibilityTree(SemanticNodeSpan semantics) = 0;
    };

    class NullPlatformAdapter final : public PlatformAdapter
    {
    public:
        [[nodiscard]] bool getClipboardText(String * const _out) override;
        void setClipboardText(StringView text) override;
        [[nodiscard]] bool writeConsole(StringView text) override;
        [[nodiscard]] bool readFile(StringView path, ByteVector * const _out) override;
        [[nodiscard]] bool writeFile(StringView path, ByteSpan data) override;
        [[nodiscard]] bool userDataPath(StringView application, StringView filename, String * const _out) override;
        [[nodiscard]] double monotonicTime() const noexcept override;
        void setCursor(CursorShape cursor) override;
        void setImeCandidateRect(const Rect & screenRect) override;
        [[nodiscard]] void * createWindow(const NativeWindowDescription & description) override;
        void destroyWindow(void * nativeHandle) override;
        void showWindow(void * nativeHandle, bool visible) override;
        void setWindowBounds(void * nativeHandle, const Rect & bounds) override;
        [[nodiscard]] MonitorSpan monitors() const noexcept override;
        void publishAccessibilityTree(SemanticNodeSpan semantics) override;

    private:
        String m_clipboard;
    };
} // namespace Mosaic
