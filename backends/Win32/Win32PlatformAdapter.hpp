#pragma once

#include <Mosaic/Mosaic.hpp>

namespace Mosaic
{
    class Win32PlatformAdapter final : public PlatformAdapter
    {
    public:
        explicit Win32PlatformAdapter(void * rootWindow);
        ~Win32PlatformAdapter() override;

        [[nodiscard]] bool getClipboardText(String * const _out) override;
        void setClipboardText(StringView text) override;
        [[nodiscard]] bool writeConsole(StringView text) override;
        [[nodiscard]] bool readFile(StringView path, ByteVector * const _out) override;
        [[nodiscard]] bool writeFile(StringView path, ByteSpan data) override;
        [[nodiscard]] bool userDataPath(StringView application, StringView filename, String * const _out) override;
        [[nodiscard]] double monotonicTime() const noexcept override;
        void setCursor(CursorShape cursor) override;
        void setImeCandidateRect(const Rect & screenRect) override;
        void openUrl(StringView url) override;
        [[nodiscard]] void * createWindow(const NativeWindowDescription & description) override;
        void destroyWindow(void * nativeHandle) override;
        void showWindow(void * nativeHandle, bool visible) override;
        void setWindowBounds(void * nativeHandle, const Rect & bounds) override;
        [[nodiscard]] bool createNativeSurface(const NativeSurfaceDescription & description, NativeSurfaceHandle * const _out) override;
        bool destroyNativeSurface(NativeSurfaceHandle surface) override;
        bool showNativeSurface(NativeSurfaceHandle surface, bool visible) override;
        bool setNativeSurfaceBounds(NativeSurfaceHandle surface, const Rect & bounds) override;
        [[nodiscard]] bool nativeSurfaceBounds(NativeSurfaceHandle surface, Rect * const _out) const override;
        [[nodiscard]] bool nativeSurfaceScreenBounds(NativeSurfaceHandle surface, Rect * const _out) const override;
        [[nodiscard]] bool nativeSurfaceDpiScale(NativeSurfaceHandle surface, float * const _out) const override;
        bool focusNativeSurface(NativeSurfaceHandle surface) override;
        [[nodiscard]] bool nativeSurfaceFocused(NativeSurfaceHandle surface, bool * const _out) const override;
        bool setNativeSurfaceParent(NativeSurfaceHandle surface, NativeSurfaceHandle parent) override;
        [[nodiscard]] bool nativeSurfaceRenderHandle(NativeSurfaceHandle surface, NativeSurfaceHandle * const _out) const override;
        bool setNativeSurfaceInputCallback(NativeSurfaceHandle surface, NativeSurfaceInputCallback callback, void * userData) override;
        [[nodiscard]] MonitorSpan monitors() const noexcept override;
        void publishAccessibilityTree(SemanticNodeSpan semantics) override;

    private:
        void refreshMonitors();

    private:
        void * m_rootWindow = nullptr;
        MonitorVector m_monitors;
        Rect m_imeCandidateRect;
    };
} // namespace Mosaic
