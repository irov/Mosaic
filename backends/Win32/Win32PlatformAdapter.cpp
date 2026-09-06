#include "Win32PlatformAdapter.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <imm.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <algorithm>
#include <limits>
#include <utility>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        inline constexpr wchar_t NativeSurfaceClassName[] = L"MosaicNativeSurface";
        //////////////////////////////////////////////////////////////////////////
        struct Win32NativeSurface
        {
            HWND window = nullptr;
            NativeSurfaceInputCallback callback = nullptr;
            void * userData = nullptr;
            Vec2 previousPointer;
            uint8_t pointerButtons = 0;
            wchar_t pendingHighSurrogate = L'\0';
        };
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool wideString(StringView value, Vector<wchar_t> * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(value.empty() == true)
            {
                _out->assign(1, L'\0');

                return true;
            }

            int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);

            if(required <= 0)
            {
                return false;
            }

            _out->resize(static_cast<size_t>(required) + 1U);
            int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), _out->data(), required);

            if(converted != required)
            {
                return false;
            }

            (*_out)[static_cast<size_t>(required)] = L'\0';

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool utf8String(const wchar_t * value, int length, String * const _out)
        {
            if(value == nullptr)
            {
                return false;
            }

            if(_out == nullptr)
            {
                return false;
            }

            if(length < 0)
            {
                length = static_cast<int>(wcslen(value));
            }

            int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, nullptr, 0, nullptr, nullptr);

            if(required < 0)
            {
                return false;
            }

            _out->resize(static_cast<size_t>(required));

            if(required == 0)
            {
                return true;
            }

            int converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, length, _out->data(), required, nullptr, nullptr);
            bool result = converted == required;

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] double performanceTime() noexcept
        {
            LARGE_INTEGER counter;
            LARGE_INTEGER frequency;
            QueryPerformanceCounter(&counter);
            QueryPerformanceFrequency(&frequency);
            double result = frequency.QuadPart == 0 ? 0.0 : static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] KeyCode keyCode(WPARAM value) noexcept
        {
            if(value >= 'A' && value <= 'Z')
            {
                unsigned offset = static_cast<unsigned>(value - 'A');
                auto returnedValue = static_cast<KeyCode>(static_cast<unsigned>(KeyCode::A) + offset);

                return returnedValue;
            }

            if(value >= '0' && value <= '9')
            {
                unsigned offset = static_cast<unsigned>(value - '0');
                auto returnedValue = static_cast<KeyCode>(static_cast<unsigned>(KeyCode::D0) + offset);

                return returnedValue;
            }

            switch(value)
            {
            case VK_TAB:
                return KeyCode::Tab;
            case VK_RETURN:
                return KeyCode::Enter;
            case VK_ESCAPE:
                return KeyCode::Escape;
            case VK_SPACE:
                return KeyCode::Space;
            case VK_BACK:
                return KeyCode::Backspace;
            case VK_DELETE:
                return KeyCode::Delete;
            case VK_LEFT:
                return KeyCode::Left;
            case VK_RIGHT:
                return KeyCode::Right;
            case VK_UP:
                return KeyCode::Up;
            case VK_DOWN:
                return KeyCode::Down;
            case VK_HOME:
                return KeyCode::Home;
            case VK_END:
                return KeyCode::End;
            case VK_PRIOR:
                return KeyCode::PageUp;
            case VK_NEXT:
                return KeyCode::PageDown;
            default:
                return KeyCode::Unknown;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Modifiers modifiers() noexcept
        {
            Modifiers result;
            result.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            result.control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            result.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            result.primary = result.control;
            result.super = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void submitSurfaceInput(Win32NativeSurface * surface, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if(surface == nullptr)
            {
                return;
            }

            if(surface->callback == nullptr)
            {
                return;
            }

            Input input;
            input.timestamp = Detail::performanceTime();
            input.modifiers = Detail::modifiers();
            PointerState pointer;
            pointer.id = 1;
            pointer.type = PointerType::Mouse;
            POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            if(message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
            {
                ScreenToClient(surface->window, &point);
            }

            pointer.position = {static_cast<float>(point.x), static_cast<float>(point.y)};
            pointer.delta = pointer.position - surface->previousPointer;
            pointer.down = surface->pointerButtons;
            bool pointerEvent = false;

            switch(message)
            {
            case WM_LBUTTONDOWN:
                pointer.pressed = 1U << static_cast<unsigned>(PointerButton::Primary);
                surface->pointerButtons |= pointer.pressed;
                SetFocus(surface->window);
                SetCapture(surface->window);
                pointerEvent = true;
                break;
            case WM_LBUTTONUP:
                pointer.released = 1U << static_cast<unsigned>(PointerButton::Primary);
                surface->pointerButtons &= static_cast<uint8_t>(~pointer.released);

                if(surface->pointerButtons == 0)
                {
                    ReleaseCapture();
                }

                pointerEvent = true;
                break;
            case WM_RBUTTONDOWN:
                pointer.pressed = 1U << static_cast<unsigned>(PointerButton::Secondary);
                surface->pointerButtons |= pointer.pressed;
                SetFocus(surface->window);
                SetCapture(surface->window);
                pointerEvent = true;
                break;
            case WM_RBUTTONUP:
                pointer.released = 1U << static_cast<unsigned>(PointerButton::Secondary);
                surface->pointerButtons &= static_cast<uint8_t>(~pointer.released);

                if(surface->pointerButtons == 0)
                {
                    ReleaseCapture();
                }

                pointerEvent = true;
                break;
            case WM_MBUTTONDOWN:
                pointer.pressed = 1U << static_cast<unsigned>(PointerButton::Middle);
                surface->pointerButtons |= pointer.pressed;
                SetFocus(surface->window);
                SetCapture(surface->window);
                pointerEvent = true;
                break;
            case WM_MBUTTONUP:
                pointer.released = 1U << static_cast<unsigned>(PointerButton::Middle);
                surface->pointerButtons &= static_cast<uint8_t>(~pointer.released);

                if(surface->pointerButtons == 0)
                {
                    ReleaseCapture();
                }

                pointerEvent = true;
                break;
            case WM_CAPTURECHANGED:
            case WM_CANCELMODE:
                surface->pointerButtons = 0;
                break;
            case WM_MOUSEMOVE:
                pointerEvent = true;
                break;
            case WM_MOUSEWHEEL:
                input.wheel.y = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
                pointerEvent = true;
                break;
            case WM_MOUSEHWHEEL:
                input.wheel.x = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
                pointerEvent = true;
                break;
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYUP:
            {
                KeyEvent key;
                key.key = Detail::keyCode(wParam);
                key.modifiers = input.modifiers;
                key.pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
                key.released = key.pressed == false;
                key.repeat = (lParam & (1LL << 30)) != 0;
                input.keyboard.push_back(key);
                break;
            }
            case WM_CHAR:
            {
                wchar_t character = static_cast<wchar_t>(wParam);

                if(character >= 0xD800 && character <= 0xDBFF)
                {
                    surface->pendingHighSurrogate = character;

                    break;
                }

                wchar_t characters[3] = {L'\0', L'\0', L'\0'};
                int count = 1;

                if(character >= 0xDC00 && character <= 0xDFFF && surface->pendingHighSurrogate != L'\0')
                {
                    characters[0] = surface->pendingHighSurrogate;
                    characters[1] = character;
                    count = 2;
                }
                else
                {
                    characters[0] = character;
                }

                surface->pendingHighSurrogate = L'\0';
                String text;

                if(Detail::utf8String(characters, count, &text) == true && text.empty() == false)
                {
                    input.text.emplace_back(std::move(text));
                }

                break;
            }
            case WM_SETFOCUS:
                input.windowFocused = true;
                break;
            case WM_KILLFOCUS:
                input.windowFocused = false;
                surface->pointerButtons = 0;
                break;
            default:
                break;
            }

            pointer.down = surface->pointerButtons;

            if(pointerEvent == true)
            {
                input.pointers.push_back(pointer);
                surface->previousPointer = pointer.position;
            }

            surface->callback(surface, input, surface->userData);
        }
        //////////////////////////////////////////////////////////////////////////
        LRESULT CALLBACK nativeSurfaceProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
        {
            Win32NativeSurface * surface = reinterpret_cast<Win32NativeSurface *>(GetWindowLongPtrW(window, GWLP_USERDATA));

            if(message == WM_NCCREATE)
            {
                CREATESTRUCTW * create = reinterpret_cast<CREATESTRUCTW *>(lParam);
                surface = static_cast<Win32NativeSurface *>(create->lpCreateParams);
                surface->window = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(surface));
            }

            Detail::submitSurfaceInput(surface, message, wParam, lParam);

            if(message == WM_ERASEBKGND)
            {
                return 1;
            }

            if(message == WM_NCDESTROY)
            {
                SetWindowLongPtrW(window, GWLP_USERDATA, 0);

                if(surface != nullptr)
                {
                    surface->window = nullptr;
                }
            }

            return DefWindowProcW(window, message, wParam, lParam);
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool registerSurfaceClass() noexcept
        {
            static bool registered = false;

            if(registered == true)
            {
                return true;
            }

            WNDCLASSEXW windowClass = {};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
            windowClass.lpfnWndProc = Detail::nativeSurfaceProcedure;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            windowClass.lpszClassName = Detail::NativeSurfaceClassName;
            ATOM result = RegisterClassExW(&windowClass);

            if(result == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            {
                return false;
            }

            registered = true;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Win32PlatformAdapter::Win32PlatformAdapter(void * rootWindow)
        : m_rootWindow(rootWindow)
    {
        Win32PlatformAdapter::refreshMonitors();
    }
    //////////////////////////////////////////////////////////////////////////
    Win32PlatformAdapter::~Win32PlatformAdapter() = default;
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::getClipboardText(String * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        if(OpenClipboard(static_cast<HWND>(m_rootWindow)) == FALSE)
        {
            return false;
        }

        HANDLE memory = GetClipboardData(CF_UNICODETEXT);

        if(memory == nullptr)
        {
            CloseClipboard();

            return false;
        }

        const wchar_t * value = static_cast<const wchar_t *>(GlobalLock(memory));

        if(value == nullptr)
        {
            CloseClipboard();

            return false;
        }

        bool result = Detail::utf8String(value, -1, _out);
        GlobalUnlock(memory);
        CloseClipboard();

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::setClipboardText(StringView text)
    {
        Vector<wchar_t> value;

        if(Detail::wideString(text, &value) == false)
        {
            return;
        }

        if(OpenClipboard(static_cast<HWND>(m_rootWindow)) == FALSE)
        {
            return;
        }

        EmptyClipboard();
        size_t size = value.size() * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);

        if(memory == nullptr)
        {
            CloseClipboard();

            return;
        }

        void * destination = GlobalLock(memory);

        if(destination == nullptr)
        {
            GlobalFree(memory);
            CloseClipboard();

            return;
        }

        memcpy(destination, value.data(), size);
        GlobalUnlock(memory);
        SetClipboardData(CF_UNICODETEXT, memory);
        CloseClipboard();
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::writeConsole(StringView text)
    {
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);

        if(output == nullptr || output == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD written = 0;
        BOOL result = WriteFile(output, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);

        return result != FALSE && written == text.size();
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::readFile(StringView path, ByteVector * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        Vector<wchar_t> nativePath;

        if(Detail::wideString(path, &nativePath) == false)
        {
            return false;
        }

        HANDLE file = CreateFileW(nativePath.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if(file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        LARGE_INTEGER size;

        if(GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0 || static_cast<uint64_t>(size.QuadPart) > std::numeric_limits<size_t>::max())
        {
            CloseHandle(file);

            return false;
        }

        _out->resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        BOOL result = ReadFile(file, _out->data(), static_cast<DWORD>(_out->size()), &read, nullptr);
        CloseHandle(file);

        return result != FALSE && read == _out->size();
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::writeFile(StringView path, ByteSpan data)
    {
        Vector<wchar_t> nativePath;

        if(Detail::wideString(path, &nativePath) == false)
        {
            return false;
        }

        HANDLE file = CreateFileW(nativePath.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if(file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        DWORD written = 0;
        BOOL result = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
        CloseHandle(file);

        return result != FALSE && written == data.size();
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::userDataPath(StringView application, StringView filename, String * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        PWSTR folder = nullptr;

        if(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &folder) != S_OK)
        {
            return false;
        }

        String root;
        bool result = Detail::utf8String(folder, -1, &root);
        CoTaskMemFree(folder);

        if(result == false)
        {
            return false;
        }

        _out->assign(root);
        _out->append("/");
        _out->append(application);
        _out->append("/");
        _out->append(filename);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    double Win32PlatformAdapter::monotonicTime() const noexcept
    {
        return Detail::performanceTime();
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::setCursor(CursorShape cursor)
    {
        LPCWSTR name = IDC_ARROW;

        switch(cursor)
        {
        case CursorShape::Text:
            name = IDC_IBEAM;
            break;
        case CursorShape::ResizeHorizontal:
            name = IDC_SIZEWE;
            break;
        case CursorShape::ResizeVertical:
            name = IDC_SIZENS;
            break;
        case CursorShape::ResizeDiagonalNwse:
            name = IDC_SIZENWSE;
            break;
        case CursorShape::ResizeDiagonalNesw:
            name = IDC_SIZENESW;
            break;
        case CursorShape::Hand:
            name = IDC_HAND;
            break;
        default:
            break;
        }

        SetCursor(LoadCursorW(nullptr, name));
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::setImeCandidateRect(const Rect & screenRect)
    {
        m_imeCandidateRect = screenRect;
        HWND window = static_cast<HWND>(m_rootWindow);

        if(window == nullptr)
        {
            return;
        }

        HIMC context = ImmGetContext(window);

        if(context == nullptr)
        {
            return;
        }

        COMPOSITIONFORM form = {};
        form.dwStyle = CFS_POINT;
        form.ptCurrentPos.x = static_cast<LONG>(screenRect.x);
        form.ptCurrentPos.y = static_cast<LONG>(screenRect.y + screenRect.height);
        ImmSetCompositionWindow(context, &form);
        ImmReleaseContext(window, context);
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::openUrl(StringView url)
    {
        Vector<wchar_t> value;

        if(Detail::wideString(url, &value) == true)
        {
            ShellExecuteW(nullptr, L"open", value.data(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void * Win32PlatformAdapter::createWindow(const NativeWindowDescription & description)
    {
        Vector<wchar_t> title;

        if(Detail::wideString(description.title, &title) == false)
        {
            return nullptr;
        }

        DWORD style = description.decorated ? WS_OVERLAPPEDWINDOW : WS_POPUP;

        if(description.resizable == false)
        {
            style &= static_cast<DWORD>(~(WS_THICKFRAME | WS_MAXIMIZEBOX));
        }

        HWND window = CreateWindowExW(description.alwaysOnTop ? WS_EX_TOPMOST : 0, L"STATIC", title.data(), style, static_cast<int>(description.bounds.x), static_cast<int>(description.bounds.y), static_cast<int>(description.bounds.width), static_cast<int>(description.bounds.height), nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

        return window;
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::destroyWindow(void * nativeHandle)
    {
        if(nativeHandle != nullptr)
        {
            DestroyWindow(static_cast<HWND>(nativeHandle));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::showWindow(void * nativeHandle, bool visible)
    {
        if(nativeHandle != nullptr)
        {
            ShowWindow(static_cast<HWND>(nativeHandle), visible ? SW_SHOW : SW_HIDE);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::setWindowBounds(void * nativeHandle, const Rect & bounds)
    {
        if(nativeHandle != nullptr)
        {
            SetWindowPos(static_cast<HWND>(nativeHandle), nullptr, static_cast<int>(bounds.x), static_cast<int>(bounds.y), static_cast<int>(bounds.width), static_cast<int>(bounds.height), SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::createNativeSurface(const NativeSurfaceDescription & description, NativeSurfaceHandle * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        if(Detail::registerSurfaceClass() == false)
        {
            return false;
        }

        HWND parent = description.parent == nullptr ? static_cast<HWND>(m_rootWindow) : static_cast<HWND>(description.parent);

        if(parent == nullptr)
        {
            return false;
        }

        Detail::Win32NativeSurface * surface = new Detail::Win32NativeSurface;
        HWND window = CreateWindowExW(0, Detail::NativeSurfaceClassName, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, static_cast<int>(description.bounds.x), static_cast<int>(description.bounds.y), static_cast<int>(description.bounds.width), static_cast<int>(description.bounds.height), parent, nullptr, GetModuleHandleW(nullptr), surface);

        if(window == nullptr)
        {
            delete surface;

            return false;
        }

        ShowWindow(window, description.visible ? SW_SHOWNA : SW_HIDE);
        *_out = surface;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::destroyNativeSurface(NativeSurfaceHandle surfaceHandle)
    {
        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr)
        {
            return false;
        }

        if(surface->window != nullptr)
        {
            DestroyWindow(surface->window);
        }

        delete surface;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::showNativeSurface(NativeSurfaceHandle surfaceHandle, bool visible)
    {
        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        ShowWindow(surface->window, visible ? SW_SHOWNA : SW_HIDE);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::setNativeSurfaceBounds(NativeSurfaceHandle surfaceHandle, const Rect & bounds)
    {
        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        BOOL result = SetWindowPos(surface->window, nullptr, static_cast<int>(bounds.x), static_cast<int>(bounds.y), static_cast<int>(bounds.width), static_cast<int>(bounds.height), SWP_NOACTIVATE | SWP_NOZORDER);

        return result != FALSE;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::nativeSurfaceBounds(NativeSurfaceHandle surfaceHandle, Rect * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        RECT rectangle;

        if(GetWindowRect(surface->window, &rectangle) == FALSE)
        {
            return false;
        }

        HWND parent = GetParent(surface->window);
        POINT origin = {rectangle.left, rectangle.top};
        ScreenToClient(parent, &origin);
        *_out = {static_cast<float>(origin.x), static_cast<float>(origin.y), static_cast<float>(rectangle.right - rectangle.left), static_cast<float>(rectangle.bottom - rectangle.top)};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::nativeSurfaceScreenBounds(NativeSurfaceHandle surfaceHandle, Rect * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        RECT rectangle;

        if(GetWindowRect(surface->window, &rectangle) == FALSE)
        {
            return false;
        }

        *_out = {static_cast<float>(rectangle.left), static_cast<float>(rectangle.top), static_cast<float>(rectangle.right - rectangle.left), static_cast<float>(rectangle.bottom - rectangle.top)};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::nativeSurfaceDpiScale(NativeSurfaceHandle surfaceHandle, float * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        UINT dpi = GetDpiForWindow(surface->window);
        *_out = std::max(1.f, static_cast<float>(dpi) / 96.f);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::focusNativeSurface(NativeSurfaceHandle surfaceHandle)
    {
        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        HWND result = SetFocus(surface->window);

        return result != nullptr || GetFocus() == surface->window;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::nativeSurfaceFocused(NativeSurfaceHandle surfaceHandle, bool * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        *_out = GetFocus() == surface->window;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::setNativeSurfaceParent(NativeSurfaceHandle surfaceHandle, NativeSurfaceHandle parentHandle)
    {
        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);
        HWND parent = parentHandle == nullptr ? static_cast<HWND>(m_rootWindow) : static_cast<HWND>(parentHandle);

        if(surface == nullptr || surface->window == nullptr || parent == nullptr)
        {
            return false;
        }

        HWND previous = SetParent(surface->window, parent);

        return previous != nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::nativeSurfaceRenderHandle(NativeSurfaceHandle surfaceHandle, NativeSurfaceHandle * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr || surface->window == nullptr)
        {
            return false;
        }

        *_out = surface->window;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Win32PlatformAdapter::setNativeSurfaceInputCallback(NativeSurfaceHandle surfaceHandle, NativeSurfaceInputCallback callback, void * userData)
    {
        Detail::Win32NativeSurface * surface = static_cast<Detail::Win32NativeSurface *>(surfaceHandle);

        if(surface == nullptr)
        {
            return false;
        }

        surface->callback = callback;
        surface->userData = userData;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    MonitorSpan Win32PlatformAdapter::monitors() const noexcept
    {
        return m_monitors;
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::publishAccessibilityTree(SemanticNodeSpan semantics)
    {
        (void)semantics;
    }
    //////////////////////////////////////////////////////////////////////////
    void Win32PlatformAdapter::refreshMonitors()
    {
        m_monitors.clear();
        EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR handle, HDC, LPRECT, LPARAM userData)
        {
            MonitorVector * monitors = reinterpret_cast<MonitorVector *>(userData);
            MONITORINFOEXW info = {};
            info.cbSize = sizeof(info);

            if(GetMonitorInfoW(handle, &info) == FALSE)
            {
                return TRUE;
            }

            Monitor monitor;
            monitor.id = reinterpret_cast<uint64_t>(handle);
            monitor.bounds = {static_cast<float>(info.rcMonitor.left), static_cast<float>(info.rcMonitor.top), static_cast<float>(info.rcMonitor.right - info.rcMonitor.left), static_cast<float>(info.rcMonitor.bottom - info.rcMonitor.top)};
            monitor.workArea = {static_cast<float>(info.rcWork.left), static_cast<float>(info.rcWork.top), static_cast<float>(info.rcWork.right - info.rcWork.left), static_cast<float>(info.rcWork.bottom - info.rcWork.top)};
            monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
            UINT dpi = GetDpiForSystem();
            monitor.dpiScale = std::max(1.f, static_cast<float>(dpi) / 96.f);
            monitors->push_back(monitor);

            return TRUE;
        }, reinterpret_cast<LPARAM>(&m_monitors));
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
