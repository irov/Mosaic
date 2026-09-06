#include "MacOSPlatformAdapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#import <QuartzCore/CAMetalLayer.h>

@interface MosaicMacOSNativeSurfaceView : NSView
{
@public
    Mosaic::NativeSurfaceInputCallback inputCallback;
    void * inputUserData;
    __unsafe_unretained NSView * rootView;
    Mosaic::Vec2 previousPointer;
    uint8_t pointerButtons;
    NSTrackingArea * trackingArea;
}
@end

static Mosaic::Modifiers MosaicMacOSModifiers(NSEventModifierFlags flags)
{
    Mosaic::Modifiers result;
    result.shift = (flags & NSEventModifierFlagShift) != 0;
    result.control = (flags & NSEventModifierFlagControl) != 0;
    result.alt = (flags & NSEventModifierFlagOption) != 0;
    result.super = (flags & NSEventModifierFlagCommand) != 0;
    result.primary = result.super;

    return result;
}

@implementation MosaicMacOSNativeSurfaceView

+ (Class)layerClass
{
    return CAMetalLayer.class;
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];

    if(trackingArea != nil)
    {
        [self removeTrackingArea:trackingArea];
    }

    NSTrackingAreaOptions options = NSTrackingActiveInKeyWindow | NSTrackingMouseMoved | NSTrackingInVisibleRect | NSTrackingMouseEnteredAndExited;
    trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:options owner:self userInfo:nil];
    [self addTrackingArea:trackingArea];
}

- (BOOL)becomeFirstResponder
{
    BOOL result = [super becomeFirstResponder];

    if(result == YES && inputCallback != nullptr)
    {
        Mosaic::Input input;
        input.windowFocused = true;
        input.modifiers = MosaicMacOSModifiers(NSEvent.modifierFlags);
        inputCallback((__bridge void *)self, input, inputUserData);
    }

    return result;
}

- (BOOL)resignFirstResponder
{
    BOOL result = [super resignFirstResponder];

    if(result == YES && inputCallback != nullptr)
    {
        Mosaic::Input input;
        input.windowFocused = false;
        input.modifiers = MosaicMacOSModifiers(NSEvent.modifierFlags);
        inputCallback((__bridge void *)self, input, inputUserData);
    }

    return result;
}

- (void)forwardPointerEvent:(NSEvent *)event pressed:(uint8_t)pressed released:(uint8_t)released
{
    if(inputCallback == nullptr)
    {
        return;
    }

    NSPoint localPoint = [self convertPoint:event.locationInWindow fromView:nil];
    Mosaic::Vec2 position = {static_cast<float>(localPoint.x), static_cast<float>(localPoint.y)};

    pointerButtons |= pressed;
    pointerButtons &= static_cast<uint8_t>(~released);
    Mosaic::Input input;
    Mosaic::PointerState pointer;
    pointer.id = 1;
    pointer.type = Mosaic::PointerType::Mouse;
    pointer.position = position;
    pointer.delta = position - previousPointer;
    pointer.down = pointerButtons;
    pointer.pressed = pressed;
    pointer.released = released;
    input.pointers.push_back(pointer);
    input.modifiers = MosaicMacOSModifiers(event.modifierFlags);
    input.timestamp = event.timestamp;
    previousPointer = position;
    inputCallback((__bridge void *)self, input, inputUserData);
}

- (void)mouseDown:(NSEvent *)event
{
    [self.window makeFirstResponder:self];
    [self forwardPointerEvent:event pressed:1U << static_cast<unsigned>(Mosaic::PointerButton::Primary) released:0];
}

- (void)mouseUp:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:0 released:1U << static_cast<unsigned>(Mosaic::PointerButton::Primary)];
}

- (void)mouseDragged:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:0 released:0];
}

- (void)mouseMoved:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:0 released:0];
}

- (void)rightMouseDown:(NSEvent *)event
{
    [self.window makeFirstResponder:self];
    [self forwardPointerEvent:event pressed:1U << static_cast<unsigned>(Mosaic::PointerButton::Secondary) released:0];
}

- (void)rightMouseUp:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:0 released:1U << static_cast<unsigned>(Mosaic::PointerButton::Secondary)];
}

- (void)rightMouseDragged:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:0 released:0];
}

- (void)otherMouseDown:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:1U << static_cast<unsigned>(Mosaic::PointerButton::Middle) released:0];
}

- (void)otherMouseUp:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:0 released:1U << static_cast<unsigned>(Mosaic::PointerButton::Middle)];
}

- (void)otherMouseDragged:(NSEvent *)event
{
    [self forwardPointerEvent:event pressed:0 released:0];
}

- (void)scrollWheel:(NSEvent *)event
{
    if(inputCallback == nullptr)
    {
        return;
    }

    Mosaic::Input input;
    (void)Mosaic::MacOSPlatformAdapter::wheelDelta(event, &input.wheel);
    NSPoint localPoint = [self convertPoint:event.locationInWindow fromView:nil];
    Mosaic::PointerState pointer;
    pointer.id = 1;
    pointer.type = Mosaic::PointerType::Mouse;
    pointer.position = {static_cast<float>(localPoint.x), static_cast<float>(localPoint.y)};
    pointer.delta = pointer.position - previousPointer;
    pointer.down = pointerButtons;
    input.pointers.push_back(pointer);
    input.modifiers = MosaicMacOSModifiers(event.modifierFlags);
    input.timestamp = event.timestamp;
    previousPointer = pointer.position;
    inputCallback((__bridge void *)self, input, inputUserData);
}

- (void)keyDown:(NSEvent *)event
{
    if(inputCallback == nullptr)
    {
        return;
    }

    Mosaic::Input input;
    input.modifiers = MosaicMacOSModifiers(event.modifierFlags);
    Mosaic::KeyEvent key;
    key.key = Mosaic::MacOSPlatformAdapter::keyCode(event);
    key.modifiers = input.modifiers;
    key.pressed = true;
    key.repeat = event.isARepeat;
    input.keyboard.push_back(key);

    if(event.characters.length != 0)
    {
        const char * text = event.characters.UTF8String;

        if(text != nullptr)
        {
            input.text.emplace_back(text);
        }
    }

    input.timestamp = event.timestamp;
    inputCallback((__bridge void *)self, input, inputUserData);
}

- (void)keyUp:(NSEvent *)event
{
    if(inputCallback == nullptr)
    {
        return;
    }

    Mosaic::Input input;
    input.modifiers = MosaicMacOSModifiers(event.modifierFlags);
    Mosaic::KeyEvent key;
    key.key = Mosaic::MacOSPlatformAdapter::keyCode(event);
    key.modifiers = input.modifiers;
    key.released = true;
    input.keyboard.push_back(key);
    input.timestamp = event.timestamp;
    inputCallback((__bridge void *)self, input, inputUserData);
}

@end

@interface MosaicMacOSColorSamplerState : NSObject
{
@public
    uint64_t request;
    float red;
    float green;
    float blue;
    float alpha;
    BOOL ready;
    BOOL active;
}
@end

@implementation MosaicMacOSColorSamplerState
@end

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSString * makePlatformString(StringView value) noexcept
        {
            auto returnedValue = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Rect mosaicBounds(const NSRect & frame, const NSRect & desktop) noexcept
        {
            Rect result = {static_cast<float>(frame.origin.x - desktop.origin.x), static_cast<float>(NSMaxY(desktop) - NSMaxY(frame)), static_cast<float>(frame.size.width), static_cast<float>(frame.size.height)};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSRect nativeBounds(const Rect & bounds) noexcept
        {
            NSScreen * screen = NSScreen.mainScreen;
            NSRect desktop = screen == nil ? NSMakeRect(0, 0, bounds.width, bounds.height) : screen.frame;
            auto returnedValue = NSMakeRect(desktop.origin.x + bounds.x, NSMaxY(desktop) - bounds.y - bounds.height, bounds.width, bounds.height);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSRect nativeSurfaceFrame(NSView * root, NSView * parent, const Rect & bounds) noexcept
        {
            if(root == nil || parent == nil)
            {
                return NSMakeRect(bounds.x, bounds.y, bounds.width, bounds.height);
            }

            float rootY = root.isFlipped ? bounds.y : static_cast<float>(root.bounds.size.height) - bounds.y - bounds.height;
            NSRect rootRect = NSMakeRect(bounds.x, rootY, bounds.width, bounds.height);
            NSRect parentRect = [root convertRect:rootRect toView:parent];

            return parentRect;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Rect mosaicSurfaceBounds(NSView * root, NSView * surface) noexcept
        {
            if(root == nil || surface == nil || surface.superview == nil)
            {
                return {};
            }

            NSRect rootRect = [surface.superview convertRect:surface.frame toView:root];
            float y = root.isFlipped ? static_cast<float>(rootRect.origin.y) : static_cast<float>(root.bounds.size.height - NSMaxY(rootRect));
            Rect result = {static_cast<float>(rootRect.origin.x), y, static_cast<float>(rootRect.size.width), static_cast<float>(rootRect.size.height)};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSString * accessibilityRole(SemanticRole role) noexcept
        {
            switch(role)
            {
            case SemanticRole::Window:
            case SemanticRole::Group:
                return NSAccessibilityGroupRole;
            case SemanticRole::Button:
                return NSAccessibilityButtonRole;
            case SemanticRole::Checkbox:
                return NSAccessibilityCheckBoxRole;
            case SemanticRole::Radio:
                return NSAccessibilityRadioButtonRole;
            case SemanticRole::Text:
                return NSAccessibilityStaticTextRole;
            case SemanticRole::TextField:
                return NSAccessibilityTextFieldRole;
            case SemanticRole::Slider:
                return NSAccessibilitySliderRole;
            case SemanticRole::Tree:
                return NSAccessibilityOutlineRole;
            case SemanticRole::TreeItem:
            case SemanticRole::Row:
                return NSAccessibilityRowRole;
            case SemanticRole::Table:
                return NSAccessibilityTableRole;
            case SemanticRole::Cell:
                return NSAccessibilityCellRole;
            case SemanticRole::TabList:
                return NSAccessibilityTabGroupRole;
            case SemanticRole::Menu:
                return NSAccessibilityMenuRole;
            case SemanticRole::MenuItem:
                return NSAccessibilityMenuItemRole;
            case SemanticRole::Image:
                return NSAccessibilityImageRole;
            case SemanticRole::Tab:
                return NSAccessibilityRadioButtonRole;
            case SemanticRole::None:
                return NSAccessibilityGroupRole;
            }

            return NSAccessibilityGroupRole;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool accessibilityFrame(NSView * view, const Rect & bounds, NSRect * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            if(view == nil)
            {
                return false;
            }

            if(view.window == nil)
            {
                return false;
            }

            NSRect local = NSMakeRect(bounds.x, bounds.y, bounds.width, bounds.height);
            NSRect window = [view convertRect:local toView:nil];

            *_out = [view.window convertRectToScreen:window];

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSCursor * symbolCursor(NSString * symbol) noexcept
        {
            NSImage * image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:nil];

            if(image == nil)
            {
                return NSCursor.arrowCursor;
            }

            image.size = NSMakeSize(16.0, 16.0);
            auto returnedValue = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(8.0, 8.0)];

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSCursor * waitCursor() noexcept
        {
            static NSCursor * cursor = Detail::symbolCursor(@"hourglass");

            return cursor;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSCursor * progressCursor() noexcept
        {
            static NSCursor * cursor = Detail::symbolCursor(@"arrow.triangle.2.circlepath");

            return cursor;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    KeyCode MacOSPlatformAdapter::keyCode(NSEvent * event) noexcept
    {
        if(event == nil)
        {
            return KeyCode::Unknown;
        }

        switch(event.keyCode)
        {
        case 48:
            return KeyCode::Tab;
        case 36:
            return KeyCode::Enter;
        case 53:
            return KeyCode::Escape;
        case 49:
            return KeyCode::Space;
        case 51:
            return KeyCode::Backspace;
        case 117:
            return KeyCode::Delete;
        case 123:
            return KeyCode::Left;
        case 124:
            return KeyCode::Right;
        case 125:
            return KeyCode::Down;
        case 126:
            return KeyCode::Up;
        case 115:
            return KeyCode::Home;
        case 119:
            return KeyCode::End;
        case 116:
            return KeyCode::PageUp;
        case 121:
            return KeyCode::PageDown;
        case 0:
            return KeyCode::A;
        case 11:
            return KeyCode::B;
        case 8:
            return KeyCode::C;
        case 2:
            return KeyCode::D;
        case 14:
            return KeyCode::E;
        case 3:
            return KeyCode::F;
        case 5:
            return KeyCode::G;
        case 4:
            return KeyCode::H;
        case 34:
            return KeyCode::I;
        case 38:
            return KeyCode::J;
        case 40:
            return KeyCode::K;
        case 37:
            return KeyCode::L;
        case 46:
            return KeyCode::M;
        case 45:
            return KeyCode::N;
        case 31:
            return KeyCode::O;
        case 35:
            return KeyCode::P;
        case 12:
            return KeyCode::Q;
        case 15:
            return KeyCode::R;
        case 1:
            return KeyCode::S;
        case 17:
            return KeyCode::T;
        case 32:
            return KeyCode::U;
        case 9:
            return KeyCode::V;
        case 13:
            return KeyCode::W;
        case 7:
            return KeyCode::X;
        case 16:
            return KeyCode::Y;
        case 6:
            return KeyCode::Z;
        case 29:
            return KeyCode::D0;
        case 18:
            return KeyCode::D1;
        case 19:
            return KeyCode::D2;
        case 20:
            return KeyCode::D3;
        case 21:
            return KeyCode::D4;
        case 23:
            return KeyCode::D5;
        case 22:
            return KeyCode::D6;
        case 26:
            return KeyCode::D7;
        case 28:
            return KeyCode::D8;
        case 25:
            return KeyCode::D9;
        case 122:
            return KeyCode::F1;
        case 120:
            return KeyCode::F2;
        case 99:
            return KeyCode::F3;
        case 118:
            return KeyCode::F4;
        case 96:
            return KeyCode::F5;
        case 97:
            return KeyCode::F6;
        case 98:
            return KeyCode::F7;
        case 100:
            return KeyCode::F8;
        case 101:
            return KeyCode::F9;
        case 109:
            return KeyCode::F10;
        case 103:
            return KeyCode::F11;
        case 111:
            return KeyCode::F12;
        default:
            return KeyCode::Unknown;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::pointerPosition(NSView * view, NSEvent * event, Vec2 * const _out) noexcept
    {
        if(_out == nullptr)
        {
            return false;
        }

        if(view == nil)
        {
            return false;
        }

        if(view.window == nil)
        {
            return false;
        }

        if(event == nil)
        {
            return false;
        }

        NSPoint windowPosition = [view.window convertPointFromScreen:NSEvent.mouseLocation];
        NSPoint localPosition = [view convertPoint:windowPosition fromView:nil];

        *_out = {static_cast<float>(localPosition.x), static_cast<float>(localPosition.y)};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::wheelDelta(NSEvent * event, Vec2 * const _out) noexcept
    {
        if(_out == nullptr)
        {
            return false;
        }

        if(event == nil)
        {
            return false;
        }

        if(event.phase == NSEventPhaseCancelled)
        {
            return false;
        }

        if(event.hasPreciseScrollingDeltas == true)
        {

            *_out = {static_cast<float>(event.scrollingDeltaX), static_cast<float>(event.scrollingDeltaY)};

            return true;
        }

        auto discreteDelta = [](CGFloat value) noexcept
        {
            float delta = static_cast<float>(value);

            if(delta == 0.f)
            {
                return 0.f;
            }

            auto returnedValue = std::copysign(std::clamp(std::abs(delta) * 4.f, 16.f, 48.f), delta);

            return returnedValue;
        };
        float deltaX = discreteDelta(event.scrollingDeltaX);
        float deltaY = discreteDelta(event.scrollingDeltaY);

        *_out = {deltaX, deltaY};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    MacOSPlatformAdapter::MacOSPlatformAdapter(NSView * view) : m_view(view)
    {
        NSRect desktop = NSScreen.screens.firstObject.frame;
        uint64_t identifier = 1;
        for(NSScreen * screen in NSScreen.screens)
        {
            Rect bounds = Detail::mosaicBounds(screen.frame, desktop);
            Rect workArea = Detail::mosaicBounds(screen.visibleFrame, desktop);
            m_monitors.push_back({identifier++, bounds, workArea, static_cast<float>(screen.backingScaleFactor), screen == NSScreen.mainScreen});
        }

        MosaicMacOSColorSamplerState * state = [[MosaicMacOSColorSamplerState alloc] init];
        m_colorSamplerState = (__bridge_retained void *)state;
    }
    //////////////////////////////////////////////////////////////////////////
    MacOSPlatformAdapter::~MacOSPlatformAdapter()
    {
        MosaicMacOSColorSamplerState * state = (__bridge_transfer MosaicMacOSColorSamplerState *)m_colorSamplerState;
        (void)state;
        m_colorSamplerState = nullptr;
        NSMutableArray<NSAccessibilityElement *> * accessibilityChildren = (__bridge_transfer NSMutableArray<NSAccessibilityElement *> *)m_accessibilityChildren;
        (void)accessibilityChildren;
        m_accessibilityChildren = nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::getClipboardText(String * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        NSString * value = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];

        if(value == nil)
        {
            return false;
        }

        const char * utf8 = value.UTF8String;

        if(utf8 == nullptr)
        {
            return false;
        }

        *_out = String(utf8);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::setClipboardText(StringView text)
    {
        NSPasteboard * pasteboard = NSPasteboard.generalPasteboard;
        [pasteboard clearContents];
        [pasteboard setString:Detail::makePlatformString(text) forType:NSPasteboardTypeString];
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::writeConsole(StringView text)
    {
        if(text.empty() == true)
        {
            return true;
        }

        size_t written = std::fwrite(text.data(), sizeof(char), text.size(), stdout);

        if(written != text.size())
        {
            return false;
        }

        int flushResult = std::fflush(stdout);

        if(flushResult != 0)
        {
            return false;
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::readFile(StringView path, ByteVector * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        NSString * nativePath = Detail::makePlatformString(path);

        if(nativePath == nil)
        {
            return false;
        }

        NSError * error = nil;
        NSData * contents = [NSData dataWithContentsOfFile:nativePath options:NSDataReadingMappedIfSafe error:&error];

        if(contents == nil)
        {
            return false;
        }

        if(error != nil)
        {
            return false;
        }

        ByteVector data(contents.length);

        if(data.empty() == false)
        {
            std::memcpy(data.data(), contents.bytes, data.size());
        }

        *_out = std::move(data);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::writeFile(StringView path, ByteSpan data)
    {
        NSString * nativePath = Detail::makePlatformString(path);

        if(nativePath == nil)
        {
            return false;
        }

        const void * bytes = data.empty() == true ? nullptr : data.data();
        NSData * contents = [NSData dataWithBytes:bytes length:data.size()];
        NSError * error = nil;
        bool written = [contents writeToFile:nativePath options:NSDataWritingAtomic error:&error];

        if(written == false)
        {
            return false;
        }

        if(error != nil)
        {
            return false;
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::userDataPath(StringView application, StringView filename, String * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        NSURL * applicationSupport = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;

        if(applicationSupport == nil)
        {
            return false;
        }

        NSString * applicationName = Detail::makePlatformString(application);
        NSString * fileName = Detail::makePlatformString(filename);

        if(applicationName == nil)
        {
            return false;
        }

        if(fileName == nil)
        {
            return false;
        }

        NSURL * directory = [applicationSupport URLByAppendingPathComponent:applicationName isDirectory:YES];
        NSError * error = nil;
        bool created = [NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error];

        if(created == false)
        {
            return false;
        }

        if(error != nil)
        {
            return false;
        }

        NSURL * file = [directory URLByAppendingPathComponent:fileName];
        const char * path = file.fileSystemRepresentation;

        if(path == nullptr)
        {
            return false;
        }

        *_out = String(path);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    double MacOSPlatformAdapter::monotonicTime() const noexcept
    {
        double result = NSProcessInfo.processInfo.systemUptime;

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::setCursor(CursorShape cursor)
    {
        switch(cursor)
        {
        case CursorShape::Hand:
            [[NSCursor pointingHandCursor] set];
            break;
        case CursorShape::Text:
            [[NSCursor IBeamCursor] set];
            break;
        case CursorShape::ResizeHorizontal:
            [[NSCursor resizeLeftRightCursor] set];
            break;
        case CursorShape::ResizeVertical:
            [[NSCursor resizeUpDownCursor] set];
            break;
        case CursorShape::ResizeDiagonalNesw:
            if(@available(macOS 15.0, *))
            {
                [[NSCursor frameResizeCursorFromPosition:NSCursorFrameResizePositionTopRight inDirections:NSCursorFrameResizeDirectionsAll] set];
            }
            else
            {
                [[NSCursor crosshairCursor] set];
            }

            break;
        case CursorShape::ResizeDiagonalNwse:
            if(@available(macOS 15.0, *))
            {
                [[NSCursor frameResizeCursorFromPosition:NSCursorFrameResizePositionTopLeft inDirections:NSCursorFrameResizeDirectionsAll] set];
            }
            else
            {
                [[NSCursor crosshairCursor] set];
            }

            break;
        case CursorShape::ResizeAll:
            [[NSCursor openHandCursor] set];
            break;
        case CursorShape::Crosshair:
            [[NSCursor crosshairCursor] set];
            break;
        case CursorShape::Wait:
            [Detail::waitCursor() set];
            break;
        case CursorShape::Progress:
            [Detail::progressCursor() set];
            break;
        case CursorShape::NotAllowed:
            [[NSCursor operationNotAllowedCursor] set];
            break;
        case CursorShape::Arrow:
            [[NSCursor arrowCursor] set];
            break;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::openUrl(StringView url)
    {
        NSString * value = Detail::makePlatformString(url);
        NSURL * nativeUrl = [NSURL URLWithString:value];

        if(nativeUrl != nil)
        {
            [NSWorkspace.sharedWorkspace openURL:nativeUrl];
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::setImeCandidateRect(const Rect & screenRect)
    {
        m_imeCandidateRect = screenRect;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::supportsScreenColorPicker() const noexcept
    {
        auto returnedValue = NSClassFromString(@"NSColorSampler") != Nil;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::beginScreenColorPick(Id request)
    {
        MosaicMacOSColorSamplerState * state = (__bridge MosaicMacOSColorSamplerState *)m_colorSamplerState;

        if(state == nil)
        {
            return;
        }

        if(state->active == true)
        {
            return;
        }

        state->request = request;
        state->ready = NO;
        state->active = YES;
        dispatch_async(dispatch_get_main_queue(), ^{
          NSColorSampler * sampler = [[NSColorSampler alloc] init];
          [sampler showSamplerWithSelectionHandler:^(NSColor * selectedColor) {
            state->active = NO;

            if(selectedColor == nil)
            {
                return;
            }

            NSColor * color = [selectedColor colorUsingColorSpace:NSColorSpace.sRGBColorSpace];

            if(color == nil)
            {
                return;
            }

            state->red = static_cast<float>(color.redComponent);
            state->green = static_cast<float>(color.greenComponent);
            state->blue = static_cast<float>(color.blueComponent);
            state->alpha = static_cast<float>(color.alphaComponent);
            state->ready = YES;
          }];
        });
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::consumeScreenColorPick(Id request, Color * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        MosaicMacOSColorSamplerState * state = (__bridge MosaicMacOSColorSamplerState *)m_colorSamplerState;

        if(state == nil)
        {
            return false;
        }

        if(state->ready == false)
        {
            return false;
        }

        if(state->request != request)
        {
            return false;
        }

        Color color = {state->red, state->green, state->blue, state->alpha};
        *_out = color;
        state->ready = NO;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void * MacOSPlatformAdapter::createWindow(const NativeWindowDescription & description)
    {
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable;

        if(description.resizable == true)
        {
            style |= NSWindowStyleMaskResizable;
        }

        if(description.decorated == false)
        {
            style = NSWindowStyleMaskBorderless;
        }

        NSWindow * window = [[NSWindow alloc] initWithContentRect:Detail::nativeBounds(description.bounds) styleMask:style backing:NSBackingStoreBuffered defer:NO];
        window.title = Detail::makePlatformString(description.title);
        window.level = description.alwaysOnTop ? NSFloatingWindowLevel : NSNormalWindowLevel;
        auto returnedValue = (__bridge_retained void *)window;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::destroyWindow(void * nativeHandle)
    {
        if(nativeHandle == nullptr)
        {
            return;
        }

        NSWindow * window = (__bridge_transfer NSWindow *)nativeHandle;
        [window close];
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::showWindow(void * nativeHandle, bool visible)
    {
        NSWindow * window = (__bridge NSWindow *)nativeHandle;

        if(visible == true)
        {
            [window makeKeyAndOrderFront:nil];
        }
        else
        {
            [window orderOut:nil];
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::setWindowBounds(void * nativeHandle, const Rect & bounds)
    {
        NSWindow * window = (__bridge NSWindow *)nativeHandle;
        [window setFrame:Detail::nativeBounds(bounds) display:YES];
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::createNativeSurface(const NativeSurfaceDescription & description, NativeSurfaceHandle * const _out)
    {
        if(_out == nullptr)
        {
            return false;
        }

        NSView * parent = description.parent == nullptr ? m_view : (__bridge NSView *)description.parent;

        if(parent == nil)
        {
            return false;
        }

        MosaicMacOSNativeSurfaceView * view = [[MosaicMacOSNativeSurfaceView alloc] initWithFrame:Detail::nativeSurfaceFrame(m_view, parent, description.bounds)];
        view->inputCallback = nullptr;
        view->inputUserData = nullptr;
        view->rootView = m_view;
        view->previousPointer = {};
        view->pointerButtons = 0;
        view.wantsLayer = YES;
        CAMetalLayer * layer = [CAMetalLayer layer];
        layer.contentsScale = parent.window.backingScaleFactor;
        layer.framebufferOnly = YES;
        view.layer = layer;
        view.hidden = description.visible == false;
        [parent addSubview:view];
        *_out = (__bridge_retained void *)view;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::destroyNativeSurface(NativeSurfaceHandle surface)
    {
        if(surface == nullptr)
        {
            return false;
        }

        MosaicMacOSNativeSurfaceView * view = (__bridge_transfer MosaicMacOSNativeSurfaceView *)surface;
        [view removeFromSuperview];

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::showNativeSurface(NativeSurfaceHandle surface, bool visible)
    {
        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil)
        {
            return false;
        }

        view.hidden = visible == false;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::setNativeSurfaceBounds(NativeSurfaceHandle surface, const Rect & bounds)
    {
        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil || view.superview == nil)
        {
            return false;
        }

        view.frame = Detail::nativeSurfaceFrame(m_view, view.superview, bounds);
        view.layer.contentsScale = view.window.backingScaleFactor;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::nativeSurfaceBounds(NativeSurfaceHandle surface, Rect * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil)
        {
            return false;
        }

        *_out = Detail::mosaicSurfaceBounds(m_view, view);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::nativeSurfaceScreenBounds(NativeSurfaceHandle surface, Rect * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil || view.window == nil)
        {
            return false;
        }

        NSRect screenRect;
        Rect localBounds = {0.f, 0.f, static_cast<float>(view.bounds.size.width), static_cast<float>(view.bounds.size.height)};

        if(Detail::accessibilityFrame(view, localBounds, &screenRect) == false)
        {
            return false;
        }

        NSScreen * screen = NSScreen.screens.firstObject;
        NSRect desktop = screen == nil ? screenRect : screen.frame;
        *_out = Detail::mosaicBounds(screenRect, desktop);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::nativeSurfaceDpiScale(NativeSurfaceHandle surface, float * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil)
        {
            return false;
        }

        CGFloat scale = view.window == nil ? NSScreen.mainScreen.backingScaleFactor : view.window.backingScaleFactor;
        *_out = static_cast<float>(std::max<CGFloat>(1.0, scale));

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::focusNativeSurface(NativeSurfaceHandle surface)
    {
        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil || view.window == nil)
        {
            return false;
        }

        auto returnedValue = [view.window makeFirstResponder:view] == YES;

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::nativeSurfaceFocused(NativeSurfaceHandle surface, bool * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil || view.window == nil)
        {
            return false;
        }

        NSResponder * responder = view.window.firstResponder;
        *_out = responder == view;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::setNativeSurfaceParent(NativeSurfaceHandle surface, NativeSurfaceHandle parent)
    {
        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;
        NSView * parentView = parent == nullptr ? m_view : (__bridge NSView *)parent;

        if(view == nil || parentView == nil)
        {
            return false;
        }

        Rect bounds = Detail::mosaicSurfaceBounds(m_view, view);
        [view removeFromSuperviewWithoutNeedingDisplay];
        view.frame = Detail::nativeSurfaceFrame(m_view, parentView, bounds);
        [parentView addSubview:view];

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::nativeSurfaceRenderHandle(NativeSurfaceHandle surface, NativeSurfaceHandle * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil || view.layer == nil)
        {
            return false;
        }

        *_out = (__bridge void *)view.layer;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool MacOSPlatformAdapter::setNativeSurfaceInputCallback(NativeSurfaceHandle surface, NativeSurfaceInputCallback callback, void * userData)
    {
        MosaicMacOSNativeSurfaceView * view = (__bridge MosaicMacOSNativeSurfaceView *)surface;

        if(view == nil)
        {
            return false;
        }

        view->inputCallback = callback;
        view->inputUserData = userData;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    MonitorSpan MacOSPlatformAdapter::monitors() const noexcept
    {
        return m_monitors;
    }
    //////////////////////////////////////////////////////////////////////////
    void MacOSPlatformAdapter::publishAccessibilityTree(SemanticNodeSpan semantics)
    {
        if(m_view == nil)
        {
            return;
        }

        NSMutableDictionary<NSNumber *, NSAccessibilityElement *> * elements = [[NSMutableDictionary alloc] initWithCapacity:semantics.size()];
        NSMutableDictionary<NSNumber *, NSMutableArray<NSAccessibilityElement *> *> * children = [[NSMutableDictionary alloc] initWithCapacity:semantics.size()];
        NSMutableArray<NSAccessibilityElement *> * roots = [[NSMutableArray alloc] initWithCapacity:semantics.size()];

        for(const SemanticNode & semantic : semantics)
        {
            NSAccessibilityElement * element = [[NSAccessibilityElement alloc] init];
            element.accessibilityRole = Detail::accessibilityRole(semantic.role);
            element.accessibilityLabel = Detail::makePlatformString(semantic.name);

            if(semantic.description.empty() == false)
            {
                element.accessibilityHelp = Detail::makePlatformString(semantic.description);
            }

            if(semantic.value.empty() == false)
            {
                element.accessibilityValue = Detail::makePlatformString(semantic.value);
            }
            else if(semantic.role == SemanticRole::Checkbox || semantic.role == SemanticRole::Radio || semantic.role == SemanticRole::Tab)
            {
                element.accessibilityValue = @(semantic.checked || semantic.selected == true);
            }

            NSRect accessibilityFrame;
            if(Detail::accessibilityFrame(m_view, semantic.bounds, &accessibilityFrame) == false)
            {
                continue;
            }

            element.accessibilityFrame = accessibilityFrame;
            element.accessibilityEnabled = semantic.disabled == false;
            element.accessibilityFocused = semantic.focused;
            element.accessibilitySelected = semantic.selected;
            element.accessibilityExpanded = semantic.expanded;
            element.accessibilityIdentifier = [NSString stringWithFormat:@"Mosaic.%llu", static_cast<unsigned long long>(semantic.id)];
            elements[@(semantic.id)] = element;
            children[@(semantic.id)] = [[NSMutableArray alloc] init];
        }

        for(const SemanticNode & semantic : semantics)
        {
            NSAccessibilityElement * element = elements[@(semantic.id)];
            NSAccessibilityElement * parent = elements[@(semantic.parentId)];

            if(parent == nil)
            {
                element.accessibilityParent = m_view;
                [roots addObject:element];
            }
            else
            {
                element.accessibilityParent = parent;
                [children[@(semantic.parentId)] addObject:element];
            }
        }
        for(const SemanticNode & semantic : semantics)
        {
            NSAccessibilityElement * element = elements[@(semantic.id)];
            NSArray<NSAccessibilityElement *> * elementChildren = children[@(semantic.id)];

            if(elementChildren.count != 0)
            {
                element.accessibilityChildren = elementChildren;
            }
        }

        NSMutableArray<NSAccessibilityElement *> * previous = (__bridge_transfer NSMutableArray<NSAccessibilityElement *> *)m_accessibilityChildren;
        (void)previous;
        m_accessibilityChildren = (__bridge_retained void *)roots;
        m_view.accessibilityRole = NSAccessibilityGroupRole;
        m_view.accessibilityLabel = @"Mosaic UI";
        m_view.accessibilityChildren = roots;
    }
    //////////////////////////////////////////////////////////////////////////
    const Rect & MacOSPlatformAdapter::imeCandidateRect() const noexcept
    {
        return m_imeCandidateRect;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
