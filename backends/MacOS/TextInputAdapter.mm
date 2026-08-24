#include "TextInputAdapter.hpp"

#include <algorithm>

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSString * nativeText(StringView text) noexcept
        {
            NSString * result = [[NSString alloc] initWithBytes:text.data() length:text.size() encoding:NSUTF8StringEncoding];

            return result == nil ? @"" : result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t validByteOffset(StringView text, size_t offset) noexcept
        {
            size_t result = std::min(offset, text.size());
            while(result != 0 && result != text.size() && (static_cast<unsigned char>(text[result]) & 0xc0U) == 0x80U)
            {
                --result;
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSUInteger utf16Offset(StringView text, size_t byteOffset) noexcept
        {
            size_t validOffset = Detail::validByteOffset(text, byteOffset);
            auto returnedValue = Detail::nativeText(text.substr(0, validOffset)).length;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] size_t utf8Offset(NSString * text, NSUInteger utf16Offset) noexcept
        {
            if(text == nil)
            {
                return 0;
            }

            NSUInteger validOffset = std::min(utf16Offset, text.length);
            NSData * data = [[text substringToIndex:validOffset] dataUsingEncoding:NSUTF8StringEncoding];

            return data == nil ? 0 : data.length;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] String composedText(const TextInputState & input)
        {
            String result(input.value);

            if(input.composition.empty() == false)
            {
                size_t begin = std::min(input.compositionBegin, result.size());
                size_t end = std::min(std::max(input.cursor, input.anchor), result.size());
                result.replace(begin, std::max(begin, end) - begin, input.composition);
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSRange nativeRange(StringView text, size_t first, size_t second) noexcept
        {
            NSUInteger begin = Detail::utf16Offset(text, std::min(first, second));
            NSUInteger end = Detail::utf16Offset(text, std::max(first, second));
            auto returnedValue = NSMakeRange(begin, end - begin);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    NSRange TextInputAdapter::markedRange(const Frame & frame) noexcept
    {
        const TextInputState & input = frame.textInput;

        if(input.active == false)
        {
            auto returnedValue = NSMakeRange(NSNotFound, 0);

            return returnedValue;
        }

        if(input.password == true)
        {
            auto returnedValue = NSMakeRange(NSNotFound, 0);

            return returnedValue;
        }

        if(input.composition.empty() == true)
        {
            auto returnedValue = NSMakeRange(NSNotFound, 0);

            return returnedValue;
        }

        String text = Detail::composedText(input);
        size_t begin = std::min(input.compositionBegin, input.value.size());
        auto returnedValue = Detail::nativeRange(text, begin, begin + input.composition.size());

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    NSRange TextInputAdapter::selectedRange(const Frame & frame) noexcept
    {
        const TextInputState & input = frame.textInput;

        if(input.active == false)
        {
            auto returnedValue = NSMakeRange(NSNotFound, 0);

            return returnedValue;
        }

        if(input.password == true)
        {
            auto returnedValue = NSMakeRange(NSNotFound, 0);

            return returnedValue;
        }

        String text = Detail::composedText(input);

        if(input.composition.empty() == false)
        {
            size_t begin = std::min(input.compositionBegin, input.value.size());
            auto returnedValue = Detail::nativeRange(text, begin + std::min(input.compositionSelectionBegin, input.composition.size()), begin + std::min(input.compositionSelectionEnd, input.composition.size()));

            return returnedValue;
        }

        auto returnedValue = Detail::nativeRange(text, input.anchor, input.cursor);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    NSAttributedString * TextInputAdapter::attributedSubstring(const Frame & frame, NSRange proposedRange, NSRangePointer actualRange) noexcept
    {
        const TextInputState & input = frame.textInput;

        if(input.active == false)
        {
            if(actualRange != nullptr)
            {
                *actualRange = NSMakeRange(NSNotFound, 0);
            }

            return nil;
        }

        if(input.password == true)
        {
            if(actualRange != nullptr)
            {
                *actualRange = NSMakeRange(NSNotFound, 0);
            }

            return nil;
        }

        if(proposedRange.location == NSNotFound)
        {
            if(actualRange != nullptr)
            {
                *actualRange = NSMakeRange(NSNotFound, 0);
            }

            return nil;
        }

        String text = Detail::composedText(input);
        NSString * native = Detail::nativeText(text);
        NSRange validRange = NSIntersectionRange(proposedRange, NSMakeRange(0, native.length));

        if(actualRange != nullptr)
        {
            *actualRange = validRange;
        }

        return [[NSAttributedString alloc] initWithString:[native substringWithRange:validRange]];
    }
    //////////////////////////////////////////////////////////////////////////
    ImeEvent TextInputAdapter::markedTextUpdate(NSString * text, NSRange selectedRange)
    {
        ImeEvent event;
        event.type = ImeEventType::Update;
        event.text = text == nil ? String{} : String(text.UTF8String);

        if(selectedRange.location != NSNotFound)
        {
            event.selectionBegin = Detail::utf8Offset(text, selectedRange.location);
            event.selectionEnd = Detail::utf8Offset(text, NSMaxRange(selectedRange));
        }

        return event;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
