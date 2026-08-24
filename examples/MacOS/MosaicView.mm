#include "MosaicView.hpp"

#include "CoreTextFontProvider.hpp"
#include "EditorPersistence.hpp"
#include "FakeEditor.hpp"
#include "MacOSPlatformAdapter.hpp"
#include "MetalRendererAdapter.hpp"
#include "TextInputAdapter.hpp"

#include <mutex>

#import <Cocoa/Cocoa.h>

namespace MosaicExample
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Modifiers modifiers(NSEventModifierFlags flags) noexcept
        {
            Mosaic::Modifiers result;
            result.shift = (flags & NSEventModifierFlagShift) != 0;
            result.control = (flags & NSEventModifierFlagControl) != 0;
            result.alt = (flags & NSEventModifierFlagOption) != 0;
            result.super = (flags & NSEventModifierFlagCommand) != 0;
            result.primary = result.super;

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] uint8_t pointerMask(NSInteger button) noexcept
        {
            NSInteger clamped = std::clamp<NSInteger>(button, 0, 4);
            auto returnedValue = static_cast<uint8_t>(1U << static_cast<unsigned>(clamped));

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool string(id value, Mosaic::String * const _out)
        {
            if(_out == nullptr)
            {
                return false;
            }

            NSString * native = [value isKindOfClass:NSAttributedString.class] ? static_cast<NSAttributedString *>(value).string : static_cast<NSString *>(value);

            if(native == nil)
            {
                return false;
            }

            const char * utf8 = native.UTF8String;

            if(utf8 == nullptr)
            {
                return false;
            }

            *_out = Mosaic::String(utf8);

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::TextureHandle createCheckerTexture(Mosaic::RendererAdapter & renderer)
        {
            constexpr uint32_t size = 64;
            Mosaic::ByteVector pixels(size * size * 4);
            for(uint32_t y = 0; y != size; ++y)
            {
                for(uint32_t x = 0; x != size; ++x)
                {
                    bool bright = ((x / 8) + (y / 8)) % 2 == 0;
                    size_t offset = (static_cast<size_t>(y) * size + x) * 4;
                    pixels[offset] = bright ? std::byte{0x58} : std::byte{0x24};
                    pixels[offset + 1] = bright ? std::byte{0x7f} : std::byte{0x32};
                    pixels[offset + 2] = bright ? std::byte{0x8a} : std::byte{0x36};
                    pixels[offset + 3] = std::byte{0xff};
                }
            }
            auto returnedValue = renderer.createTexture(size, size, pixels);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
} // namespace MosaicExample

@interface MosaicView ()
- (void)handleScrollWheelEvent:(NSEvent *)event;
@end

@implementation MosaicView
{
    Mosaic::MetalRendererAdapter * _renderer;
    Mosaic::CoreTextFontProvider * _fontProvider;
    Mosaic::MacOSPlatformAdapter * _platform;
    MosaicExample::EditorPersistence * _persistence;
    MosaicExample::FakeEditor * _editor;
    Mosaic::Context * _context;
    Mosaic::Input _input;
    Mosaic::GraphicsBridge _graphicsBridge;
    Mosaic::RenderMesh _mesh;
    NSTrackingArea * _trackingArea;
    NSMutableAttributedString * _markedText;
    NSTimeInterval _previousFrameTime;
    std::mutex _wheelMutex;
    Mosaic::Vec2 _pendingWheel;
    Mosaic::Vec2 _pendingWheelPosition;
    bool _pendingWheelPositionValid;
    id _scrollWheelMonitor;
}
//////////////////////////////////////////////////////////////////////////
- (instancetype)initWithFrame:(NSRect)frameRect
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    self = [super initWithFrame:frameRect device:device];

    if(self == nil)
    {
        return nil;
    }

    if(device == nil)
    {
        return nil;
    }

    self.delegate = self;
    self.paused = NO;
    self.enableSetNeedsDisplay = NO;
    self.preferredFramesPerSecond = 60;
    self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.sampleCount = [device supportsTextureSampleCount:4] ? 4 : 1;
    self.clearColor = MTLClearColorMake(0.031, 0.043, 0.055, 1.0);

    _renderer = new Mosaic::MetalRendererAdapter(device, self);
    _fontProvider = new Mosaic::CoreTextFontProvider(*_renderer);
    _platform = new Mosaic::MacOSPlatformAdapter(self);
    Mosaic::String persistencePath;
    if(_platform->userDataPath("Mosaic", "FakeEditor.layout", &persistencePath) == false)
    {
        return nil;
    }

    _persistence = new MosaicExample::EditorPersistence(*_platform, std::move(persistencePath));
    _context = Mosaic::newContext(_platform, _fontProvider);
    _editor = new MosaicExample::FakeEditor(MosaicExample::Detail::createCheckerTexture(*_renderer), *_persistence);
    _editor->configure(_context);

    // One persistent mouse pointer is enough for this desktop sample. Edge flags are cleared
    // after each frame while the down mask survives until AppKit sends mouseUp.
    Mosaic::PointerState & pointer = _input.pointers.emplace_back();
    pointer.id = 0;
    pointer.type = Mosaic::PointerType::Mouse;
    _markedText = [[NSMutableAttributedString alloc] init];
    _previousFrameTime = _platform->monotonicTime();
    __weak MosaicView * weakSelf = self;
    _scrollWheelMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                                                handler:^NSEvent *(NSEvent * event) {
                                                                  MosaicView * view = weakSelf;

                                                                  if(view == nil)
                                                                  {
                                                                      return event;
                                                                  }

                                                                  Mosaic::Vec2 position;
                                                                  if(Mosaic::MacOSPlatformAdapter::pointerPosition(view, event, &position) == false)
                                                                  {
                                                                      return event;
                                                                  }

                                                                  if(NSPointInRect(NSMakePoint(position.x, position.y), view.bounds) == false)
                                                                  {
                                                                      return event;
                                                                  }

                                                                  [view handleScrollWheelEvent:event];

                                                                  return nil;
                                                                }];
    (void)_persistence->load();
    (void)Mosaic::deserialize(_context, *_persistence);

    return self;
}
//////////////////////////////////////////////////////////////////////////
- (void)dealloc
{
    if(_scrollWheelMonitor != nil)
    {
        [NSEvent removeMonitor:_scrollWheelMonitor];
        _scrollWheelMonitor = nil;
    }

    Mosaic::deleteContext(_context);
    delete _editor;
    delete _persistence;
    delete _platform;
    delete _fontProvider;
    delete _renderer;
}
//////////////////////////////////////////////////////////////////////////
- (BOOL)isFlipped
{
    return YES;
}
//////////////////////////////////////////////////////////////////////////
- (BOOL)acceptsFirstResponder
{
    return YES;
}
//////////////////////////////////////////////////////////////////////////
- (void)updateTrackingAreas
{
    [super updateTrackingAreas];

    if(_trackingArea != nil)
    {
        [self removeTrackingArea:_trackingArea];
    }

    _trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect owner:self userInfo:nil];
    [self addTrackingArea:_trackingArea];
}
//////////////////////////////////////////////////////////////////////////
- (void)updatePointer:(NSEvent *)event
{
    Mosaic::PointerState & pointer = _input.pointers.front();
    Mosaic::Vec2 position;
    if(Mosaic::MacOSPlatformAdapter::pointerPosition(self, event, &position) == false)
    {
        return;
    }

    if(pointer.position.x > -99999.f && pointer.position.y > -99999.f)
    {
        pointer.delta = pointer.delta + (position - pointer.position);
    }

    pointer.position = position;
    switch(event.type)
    {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
        pointer.pressure = event.pressure;
        break;
    default:
        break;
    }
}
//////////////////////////////////////////////////////////////////////////
- (void)pressButton:(NSEvent *)event
{
    [self.window makeFirstResponder:self];
    [self updatePointer:event];
    Mosaic::PointerState & pointer = _input.pointers.front();
    uint8_t mask = MosaicExample::Detail::pointerMask(event.buttonNumber);
    size_t button = static_cast<size_t>(std::clamp<NSInteger>(event.buttonNumber, 0, 4));
    pointer.pressPositions[button] = pointer.position;
    pointer.pressPositionValid = static_cast<uint8_t>(pointer.pressPositionValid | mask);
    pointer.down = static_cast<uint8_t>(pointer.down | mask);
    pointer.pressed = static_cast<uint8_t>(pointer.pressed | mask);
    pointer.clickCount = static_cast<uint8_t>(std::min<NSInteger>(event.clickCount, 255));
    pointer.clickCounts[button] = pointer.clickCount;
}
//////////////////////////////////////////////////////////////////////////
- (void)releaseButton:(NSEvent *)event
{
    [self updatePointer:event];
    Mosaic::PointerState & pointer = _input.pointers.front();
    uint8_t mask = MosaicExample::Detail::pointerMask(event.buttonNumber);
    pointer.down = static_cast<uint8_t>(pointer.down & ~mask);
    pointer.released = static_cast<uint8_t>(pointer.released | mask);
}
//////////////////////////////////////////////////////////////////////////
- (void)mouseDown:(NSEvent *)event
{
    [self pressButton:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)rightMouseDown:(NSEvent *)event
{
    [self pressButton:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)otherMouseDown:(NSEvent *)event
{
    [self pressButton:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)mouseUp:(NSEvent *)event
{
    [self releaseButton:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)rightMouseUp:(NSEvent *)event
{
    [self releaseButton:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)otherMouseUp:(NSEvent *)event
{
    [self releaseButton:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)mouseMoved:(NSEvent *)event
{
    [self updatePointer:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)mouseEntered:(NSEvent *)event
{
    [self updatePointer:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)mouseExited:(NSEvent *)event
{
    (void)event;
    Mosaic::PointerState & pointer = _input.pointers.front();

    if(pointer.down == 0)
    {
        pointer.position = {-100000.f, -100000.f};
        pointer.delta = {};
    }
}
//////////////////////////////////////////////////////////////////////////
- (void)mouseDragged:(NSEvent *)event
{
    [self updatePointer:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)rightMouseDragged:(NSEvent *)event
{
    [self updatePointer:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)otherMouseDragged:(NSEvent *)event
{
    [self updatePointer:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)handleScrollWheelEvent:(NSEvent *)event
{
    [self updatePointer:event];
    Mosaic::Vec2 delta;
    if(Mosaic::MacOSPlatformAdapter::wheelDelta(event, &delta) == false)
    {
        return;
    }

    std::lock_guard lock(_wheelMutex);
    _pendingWheel = _pendingWheel + delta;
    const Mosaic::PointerState & pointer = _input.pointers.front();
    _pendingWheelPosition = pointer.position;
    _pendingWheelPositionValid = true;
}
//////////////////////////////////////////////////////////////////////////
- (void)scrollWheel:(NSEvent *)event
{
    [self handleScrollWheelEvent:event];
}
//////////////////////////////////////////////////////////////////////////
- (void)flagsChanged:(NSEvent *)event
{
    _input.modifiers = MosaicExample::Detail::modifiers(event.modifierFlags);
}
//////////////////////////////////////////////////////////////////////////
- (void)keyDown:(NSEvent *)event
{
    _input.modifiers = MosaicExample::Detail::modifiers(event.modifierFlags);
    Mosaic::KeyCode key = Mosaic::MacOSPlatformAdapter::keyCode(event);

    if(key != Mosaic::KeyCode::Unknown)
    {
        _input.keyboard.push_back({key, true, false, event.isARepeat, _input.modifiers});
    }

    // AppKit's text system produces UTF-8 and IME callbacks; command shortcuts remain key events.
    if(_input.modifiers.primary == false && _input.modifiers.control == false)
    {
        [self interpretKeyEvents:@[ event ]];
    }
}
//////////////////////////////////////////////////////////////////////////
- (void)keyUp:(NSEvent *)event
{
    _input.modifiers = MosaicExample::Detail::modifiers(event.modifierFlags);
    Mosaic::KeyCode key = Mosaic::MacOSPlatformAdapter::keyCode(event);

    if(key != Mosaic::KeyCode::Unknown)
    {
        _input.keyboard.push_back({key, false, true, false, _input.modifiers});
    }
}
//////////////////////////////////////////////////////////////////////////
- (void)drawInMTKView:(MTKView *)view
{
    if(_context == nullptr)
    {
        return;
    }

    NSTimeInterval now = _platform->monotonicTime();
    _input.deltaTime = static_cast<float>(std::clamp(now - _previousFrameTime, 0.0, 0.1));
    _input.timestamp = now;
    _input.windowFocused = self.window.isKeyWindow;
    _previousFrameTime = now;
    {
        std::lock_guard lock(_wheelMutex);
        _input.wheel = _input.wheel + _pendingWheel;

        if(_pendingWheelPositionValid == true)
        {
            Mosaic::PointerState & pointer = _input.pointers.front();
            pointer.delta = pointer.delta + (_pendingWheelPosition - pointer.position);
            pointer.position = _pendingWheelPosition;
        }

        _pendingWheel = {};
        _pendingWheelPositionValid = false;
    }

    Mosaic::Viewport viewport;
    viewport.id = 1;
    viewport.bounds = {0.f, 0.f, static_cast<float>(view.bounds.size.width), static_cast<float>(view.bounds.size.height)};
    viewport.workArea = viewport.bounds;
    viewport.dpiScale = static_cast<float>(self.window.backingScaleFactor);
    viewport.focused = _input.windowFocused;
    viewport.nativeHandle = (__bridge void *)self;

    _fontProvider->setScale(viewport.dpiScale);

    // A short AppKit click may contain both edges in one frame. Mosaic's item behavior
    // intentionally evaluates press before release, so the complete click stays atomic.
    Mosaic::beginFrame(_context, _input, viewport);
    _editor->draw(_context, {static_cast<float>(view.bounds.size.width), static_cast<float>(view.bounds.size.height)});
    const Mosaic::Frame & frame = Mosaic::endFrame(_context);

    if(_graphicsBridge.build(frame, &_mesh, *_platform) == true)
    {
        _renderer->render(frame.viewports.front(), _mesh);
        _editor->updateProfiler(frame, _mesh);
    }

    Mosaic::PointerState & pointer = _input.pointers.front();
    pointer.delta = {};
    pointer.pressed = 0;
    pointer.pressPositionValid = 0;
    pointer.clickCounts = {};
    pointer.clickCount = 0;
    pointer.released = 0;
    _input.keyboard.clear();
    _input.text.clear();
    _input.ime.clear();
    _input.wheel = {};
}
//////////////////////////////////////////////////////////////////////////
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
    (void)view;
    (void)size;
}
//////////////////////////////////////////////////////////////////////////
- (BOOL)hasMarkedText
{
    return _markedText.length != 0;
}
//////////////////////////////////////////////////////////////////////////
- (NSRange)markedRange
{
    auto returnedValue = Mosaic::TextInputAdapter::markedRange(Mosaic::getFrame(_context));

    return returnedValue;
}
//////////////////////////////////////////////////////////////////////////
- (NSRange)selectedRange
{
    auto returnedValue = Mosaic::TextInputAdapter::selectedRange(Mosaic::getFrame(_context));

    return returnedValue;
}
//////////////////////////////////////////////////////////////////////////
- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange
{
    (void)replacementRange;
    BOOL startingComposition = _markedText.length == 0;
    Mosaic::String text;
    if(MosaicExample::Detail::string(string, &text) == false)
    {
        return;
    }

    [_markedText setAttributedString:[[NSAttributedString alloc] initWithString:[NSString stringWithUTF8String:text.c_str()]]];

    if(startingComposition == true)
    {
        _input.ime.emplace_back().type = Mosaic::ImeEventType::Start;
    }

    _input.ime.push_back(Mosaic::TextInputAdapter::markedTextUpdate(_markedText.string, selectedRange));
}
//////////////////////////////////////////////////////////////////////////
- (void)unmarkText
{
    if(_markedText.length != 0)
    {
        Mosaic::String text;
        if(MosaicExample::Detail::string(_markedText.string, &text) == false)
        {
            return;
        }

        _input.ime.push_back({Mosaic::ImeEventType::Commit, std::move(text)});
        [_markedText setAttributedString:[[NSAttributedString alloc] initWithString:@""]];
    }
}
//////////////////////////////////////////////////////////////////////////
- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
    (void)replacementRange;
    Mosaic::String text;
    if(MosaicExample::Detail::string(string, &text) == false)
    {
        return;
    }

    if(text.empty() == false)
    {
        if(_markedText.length != 0)
        {
            _input.ime.push_back({Mosaic::ImeEventType::Commit, std::move(text)});
        }
        else
        {
            _input.text.emplace_back(std::move(text));
        }
    }

    [_markedText setAttributedString:[[NSAttributedString alloc] initWithString:@""]];
}
//////////////////////////////////////////////////////////////////////////
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText
{
    return @[];
}
//////////////////////////////////////////////////////////////////////////
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    auto returnedValue = Mosaic::TextInputAdapter::attributedSubstring(Mosaic::getFrame(_context), range, actualRange);

    return returnedValue;
}
//////////////////////////////////////////////////////////////////////////
- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
    (void)point;

    return NSNotFound;
}
//////////////////////////////////////////////////////////////////////////
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
    if(actualRange != nullptr)
    {
        *actualRange = range;
    }

    const Mosaic::Rect & candidate = _platform->imeCandidateRect();
    NSRect local = NSMakeRect(candidate.x, candidate.y, candidate.width, candidate.height);
    NSRect inWindow = [self convertRect:local toView:nil];

    return [self.window convertRectToScreen:inWindow];
}
//////////////////////////////////////////////////////////////////////////
- (void)doCommandBySelector:(SEL)selector
{
    (void)selector;
}

@end
