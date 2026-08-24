#pragma once

#include <Mosaic/Mosaic.hpp>

#import <Cocoa/Cocoa.h>

namespace Mosaic
{
    class TextInputAdapter final
    {
    public:
        [[nodiscard]] static NSRange markedRange(const Frame & frame) noexcept;
        [[nodiscard]] static NSRange selectedRange(const Frame & frame) noexcept;
        [[nodiscard]] static NSAttributedString * attributedSubstring(const Frame & frame, NSRange proposedRange, NSRangePointer actualRange) noexcept;
        [[nodiscard]] static ImeEvent markedTextUpdate(NSString * text, NSRange selectedRange);
    };
} // namespace Mosaic
