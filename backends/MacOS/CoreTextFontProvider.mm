#include "CoreTextFontProvider.hpp"

#include <algorithm>
#include <cmath>

#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

namespace Mosaic
{
    namespace Detail
    {
        using NativeGlyphVector = Vector<CGGlyph>;
        using NativePointVector = Vector<CGPoint>;
        using NativeSizeVector = Vector<CGSize>;
        using NativeIndexVector = Vector<CFIndex>;
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] SizeVector utf16ByteOffsets(StringView text)
        {
            SizeVector output;
            output.push_back(0);
            for(size_t byte = 0; byte < text.size();)
            {
                uint8_t lead = static_cast<uint8_t>(text[byte]);
                size_t length = 1;
                uint32_t codepoint = lead;

                if((lead & 0xe0U) == 0xc0U && byte + 1 < text.size())
                {
                    length = 2;
                    codepoint = static_cast<uint32_t>(lead & 0x1fU);
                }
                else if((lead & 0xf0U) == 0xe0U && byte + 2 < text.size())
                {
                    length = 3;
                    codepoint = static_cast<uint32_t>(lead & 0x0fU);
                }
                else if((lead & 0xf8U) == 0xf0U && byte + 3 < text.size())
                {
                    length = 4;
                    codepoint = static_cast<uint32_t>(lead & 0x07U);
                }

                for(size_t index = 1; index < length; ++index)
                {
                    codepoint = (codepoint << 6U) | static_cast<uint32_t>(static_cast<uint8_t>(text[byte + index]) & 0x3fU);
                }

                size_t next = byte + length;

                if(codepoint > 0xffffU)
                {
                    output.push_back(byte);
                }

                output.push_back(next);
                byte = next;
            }

            return output;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] CTFontRef createFont(FontHandle font, float size) noexcept
        {
            CTFontUIFontType type = font == MonospaceFont ? kCTFontUIFontUserFixedPitch : kCTFontUIFontSystem;
            auto returnedValue = CTFontCreateUIFontForLanguage(type, std::max(1.f, size), nullptr);

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] NSString * makeFontString(StringView value) noexcept
        {
            auto returnedValue = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] uint64_t glyphKey(FontHandle font, float size, float scale, uint32_t face, uint32_t glyph) noexcept
        {
            uint32_t sizeKey = static_cast<uint32_t>(std::max(1.f, std::round(size * 64.f)));
            uint32_t scaleKey = static_cast<uint32_t>(std::max(1.f, std::round(scale * 64.f)));
            uint64_t result = RootId;
            auto mix = [&result](uint64_t value)
            {
                for(size_t index = 0; index != sizeof(value); ++index)
                {
                    result ^= static_cast<unsigned char>(value >> (index * 8U));
                    result *= 0x100000001b3ULL;
                }
            };
            mix(font);
            mix(sizeKey);
            mix(scaleKey);
            mix(face);
            mix(glyph);

            return result;
        }

        inline constexpr uint32_t AtlasPageSize = 1024;
        inline constexpr uint32_t AtlasSpacing = 1;
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    CoreTextFontProvider::CoreTextFontProvider(RendererAdapter & renderer) noexcept : m_renderer(renderer)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CoreTextFontProvider::~CoreTextFontProvider()
    {
        clearGlyphs();
        clearResolvedFonts();
        clearFonts();
    }
    //////////////////////////////////////////////////////////////////////////
    size_t CoreTextFontProvider::FontCacheKeyHash::operator()(const FontCacheKey & key) const noexcept
    {
        auto returnedValue = static_cast<size_t>(combineId(key.font, key.size));

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    const CoreTextFontProvider::CachedFont * CoreTextFontProvider::findOrCreateFont(FontHandle font, float size) const noexcept
    {
        FontCacheKey key = {font, static_cast<uint32_t>(std::max(1.f, std::round(size * 64.f)))};
        auto cached = m_fonts.find(key);

        if(cached != m_fonts.end())
        {
            return &cached->second;
        }

        CTFontRef nativeFont = Detail::createFont(font, static_cast<float>(key.size) / 64.f);

        if(nativeFont == nullptr)
        {
            return nullptr;
        }

        FontMetrics fontMetrics = {static_cast<float>(CTFontGetAscent(nativeFont)), static_cast<float>(CTFontGetDescent(nativeFont)), static_cast<float>(CTFontGetLeading(nativeFont))};
        auto [iterator, inserted] = m_fonts.emplace(key, CachedFont{nativeFont, fontMetrics});

        if(inserted == false)
        {
            CFRelease(nativeFont);
        }

        return &iterator->second;
    }
    //////////////////////////////////////////////////////////////////////////
    uint32_t CoreTextFontProvider::findOrCreateResolvedFont(const void * native) const noexcept
    {
        if(native == nullptr)
        {
            return 0;
        }

        for(size_t index = 0; index != m_resolvedFonts.size(); ++index)
        {
            if(CFEqual(m_resolvedFonts[index].native, native) == true)
            {
                auto returnedValue = static_cast<uint32_t>(index + 1);

                return returnedValue;
            }
        }

        if(m_resolvedFonts.size() >= std::numeric_limits<uint32_t>::max())
        {
            return 0;
        }

        CFRetain(native);
        m_resolvedFonts.push_back({native});
        auto returnedValue = static_cast<uint32_t>(m_resolvedFonts.size());

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    const void * CoreTextFontProvider::resolvedFont(uint32_t face) const noexcept
    {
        if(face == 0)
        {
            return nullptr;
        }

        if(face > m_resolvedFonts.size())
        {
            return nullptr;
        }

        return m_resolvedFonts[face - 1].native;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::allocateAtlasRegion(uint32_t width, uint32_t height, size_t * const _outPage, uint32_t * const _outX, uint32_t * const _outY) const
    {
        if(_outPage == nullptr)
        {
            return false;
        }

        if(_outX == nullptr)
        {
            return false;
        }

        if(_outY == nullptr)
        {
            return false;
        }

        if(width + Detail::AtlasSpacing > Detail::AtlasPageSize)
        {
            return false;
        }

        if(height + Detail::AtlasSpacing > Detail::AtlasPageSize)
        {
            return false;
        }

        auto tryPage = [width, height](AtlasPage & page, uint32_t * const _outCandidateX, uint32_t * const _outCandidateY)
        {
            uint32_t candidateX = page.cursorX;
            uint32_t candidateY = page.cursorY;
            uint32_t candidateRowHeight = page.rowHeight;

            if(candidateX + width > page.width)
            {
                candidateX = 0;
                candidateY += candidateRowHeight;
                candidateRowHeight = 0;
            }

            if(candidateY + height > page.height)
            {
                return false;
            }

            *_outCandidateX = candidateX;
            *_outCandidateY = candidateY;
            page.cursorX = candidateX + width + Detail::AtlasSpacing;
            page.cursorY = candidateY;
            page.rowHeight = std::max(candidateRowHeight, height + Detail::AtlasSpacing);

            return true;
        };

        for(size_t index = 0; index != m_atlasPages.size(); ++index)
        {
            uint32_t x = 0;
            uint32_t y = 0;
            if(tryPage(m_atlasPages[index], &x, &y) == true)
            {

                *_outPage = index;
                *_outX = x;
                *_outY = y;

                return true;
            }
        }

        bool maskTexture = true;
        ByteVector emptyPixels(static_cast<size_t>(Detail::AtlasPageSize) * Detail::AtlasPageSize, std::byte{0});
        TextureHandle texture = m_renderer.createMaskTexture(Detail::AtlasPageSize, Detail::AtlasPageSize, emptyPixels);

        if(texture == 0)
        {
            maskTexture = false;
            emptyPixels.assign(static_cast<size_t>(Detail::AtlasPageSize) * Detail::AtlasPageSize * 4, std::byte{0});
            texture = m_renderer.createTexture(Detail::AtlasPageSize, Detail::AtlasPageSize, emptyPixels);
        }

        if(texture == 0)
        {
            return false;
        }

        m_atlasPages.push_back({texture, Detail::AtlasPageSize, Detail::AtlasPageSize, 0, 0, 0, maskTexture});
        uint32_t x = 0;
        uint32_t y = 0;
        if(tryPage(m_atlasPages.back(), &x, &y) == false)
        {
            return false;
        }

        *_outPage = m_atlasPages.size() - 1;
        *_outX = x;
        *_outY = y;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void CoreTextFontProvider::clearFonts() noexcept
    {
        for(const auto & [key, font] : m_fonts)
        {
            (void)key;
            CFRelease(font.native);
        }
        m_fonts.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    void CoreTextFontProvider::clearGlyphs() noexcept
    {
        for(const auto & [key, glyph] : m_glyphs)
        {
            (void)key;

            if(glyph.atlas == false)
            {
                m_renderer.destroyTexture(glyph.value.texture);
            }
        }
        m_glyphs.clear();
        for(const AtlasPage & page : m_atlasPages)
        {
            m_renderer.destroyTexture(page.texture);
        }
        m_atlasPages.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    void CoreTextFontProvider::clearResolvedFonts() noexcept
    {
        for(const ResolvedFont & font : m_resolvedFonts)
        {
            CFRelease(font.native);
        }
        m_resolvedFonts.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    void CoreTextFontProvider::setScale(float scale) noexcept
    {
        // Rasterize at the native backing scale. Forcing a 2x atlas on a 1x display made Metal
        // downsample every glyph and produced the soft, uneven baselines visible in the editor.
        float updated = std::max(1.f, scale);

        if(updated == m_scale)
        {
            return;
        }

        clearGlyphs();
        clearResolvedFonts();
        clearFonts();
        m_scale = updated;
        ++m_revision;

        if(m_revision == 0)
        {
            ++m_revision;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::measure(FontHandle font, float size, StringView text, Vec2 * const _out) const noexcept
    {
        if(_out == nullptr)
        {
            return false;
        }

        if(text.empty() == true)
        {

            *_out = {0.f, size};

            return true;
        }

        const CachedFont * cachedFont = findOrCreateFont(font, size);

        if(cachedFont == nullptr)
        {
            return false;
        }

        CTFontRef nativeFont = static_cast<CTFontRef>(cachedFont->native);

        NSDictionary * attributes = @{(__bridge id)kCTFontAttributeName : (__bridge id)nativeFont};
        float lineHeight = cachedFont->metrics.lineHeight();
        float maximumWidth = 0.f;
        size_t lineCount = 0;
        size_t lineStart = 0;
        while(lineStart <= text.size())
        {
            size_t newline = text.find('\n', lineStart);
            size_t lineEnd = newline == StringView::npos ? text.size() : newline;
            NSString * string = Detail::makeFontString(text.substr(lineStart, lineEnd - lineStart));

            if(string == nil)
            {
                return false;
            }

            NSAttributedString * attributed = [[NSAttributedString alloc] initWithString:string attributes:attributes];

            if(attributed == nil)
            {
                return false;
            }

            CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)attributed);

            if(line == nullptr)
            {
                return false;
            }

            maximumWidth = std::max(maximumWidth, static_cast<float>(CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr)));
            CFRelease(line);
            ++lineCount;

            if(newline == StringView::npos)
            {
                break;
            }

            lineStart = newline + 1;
        }

        *_out = {maximumWidth, lineHeight * static_cast<float>(lineCount)};

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::metrics(FontHandle font, float size, FontMetrics * const _out) const noexcept
    {
        if(_out == nullptr)
        {
            return false;
        }

        const CachedFont * cachedFont = findOrCreateFont(font, size);

        if(cachedFont == nullptr)
        {
            return false;
        }

        *_out = cachedFont->metrics;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::shape(FontHandle font, float size, StringView text, ShapedGlyphVector * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        ShapedText shaped;
        if(shapeText(font, size, text, {}, &shaped) == false)
        {
            return false;
        }

        *_out = std::move(shaped.glyphs);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::shapeText(FontHandle font, float size, StringView text, const TextShapeOptions & options, ShapedText * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        ShapedText output;
        const CachedFont * cachedFont = findOrCreateFont(font, size);

        if(cachedFont == nullptr)
        {
            return false;
        }

        if(text.empty() == true)
        {
            output.size = {0.f, cachedFont->metrics.lineHeight()};
            output.clusters.push_back(0);

            if(options.caretOffsets == true)
            {
                output.offsets.push_back(0.f);
                output.positions.push_back({});
            }

            output.lines.push_back({0, 0, 0.f});

            *_out = std::move(output);

            return true;
        }

        CTFontRef nativeFont = static_cast<CTFontRef>(cachedFont->native);
        NSDictionary * attributes = @{(__bridge id)kCTFontAttributeName : (__bridge id)nativeFont};
        NSString * string = Detail::makeFontString(text);

        if(string == nil)
        {
            return false;
        }

        NSAttributedString * attributed = [[NSAttributedString alloc] initWithString:string attributes:attributes];
        CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString((__bridge CFAttributedStringRef)attributed);

        if(typesetter == nullptr)
        {
            return false;
        }

        float baseline = cachedFont->metrics.ascent;
        float lineHeight = cachedFont->metrics.lineHeight();
        SizeVector byteOffsets = Detail::utf16ByteOffsets(text);
        CFIndex utf16Length = static_cast<CFIndex>([string length]);

        if(options.caretOffsets == true)
        {
            output.offsets.resize(text.size() + 1, 0.f);
            output.positions.resize(text.size() + 1);
        }

        output.clusters.reserve(text.size() + 1);
        output.clusters.push_back(0);

        float maximumWidth = 0.f;
        size_t lineIndex = 0;
        CFIndex lineStart = 0;
        Detail::NativeGlyphVector glyphStorage;
        Detail::NativePointVector positionStorage;
        Detail::NativeSizeVector advanceStorage;
        Detail::NativeIndexVector indexStorage;
        while(lineStart <= utf16Length)
        {
            CFIndex paragraphEnd = lineStart;
            while(paragraphEnd < utf16Length && [string characterAtIndex:static_cast<NSUInteger>(paragraphEnd)] != '\n')
            {
                ++paragraphEnd;
            }

            CFIndex lineLength = paragraphEnd - lineStart;

            if(options.wordWrap == true && options.wrapWidth > 0.f && lineLength > 0)
            {
                CFIndex suggested = CTTypesetterSuggestLineBreak(typesetter, lineStart, options.wrapWidth);
                lineLength = std::clamp(suggested, CFIndex{1}, lineLength);
            }

            CTLineRef line = CTTypesetterCreateLine(typesetter, CFRangeMake(lineStart, lineLength));

            if(line == nullptr)
            {
                CFRelease(typesetter);

                return false;
            }

            float lineWidth = static_cast<float>(CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr));
            maximumWidth = std::max(maximumWidth, lineWidth);
            size_t byteBegin = byteOffsets[static_cast<size_t>(lineStart)];
            size_t byteEnd = byteOffsets[static_cast<size_t>(lineStart + lineLength)];
            output.lines.push_back({byteBegin, byteEnd, lineWidth});

            if(options.caretOffsets == true)
            {
                for(CFIndex utf16 = lineStart; utf16 <= lineStart + lineLength; ++utf16)
                {
                    CGFloat secondary = 0.0;
                    CGFloat offset = CTLineGetOffsetForStringIndex(line, utf16, &secondary);
                    size_t byte = byteOffsets[static_cast<size_t>(utf16)];

                    if(byte < output.offsets.size())
                    {
                        output.offsets[byte] = static_cast<float>(offset);
                        output.positions[byte] = {static_cast<float>(offset), static_cast<float>(lineIndex) * lineHeight};
                        output.clusters.push_back(byte);
                    }
                }
            }

            CFArrayRef runs = CTLineGetGlyphRuns(line);
            for(CFIndex runIndex = 0; runIndex != CFArrayGetCount(runs); ++runIndex)
            {
                CTRunRef run = static_cast<CTRunRef>(const_cast<void *>(CFArrayGetValueAtIndex(runs, runIndex)));
                CFIndex count = CTRunGetGlyphCount(run);
                size_t glyphCount = static_cast<size_t>(count);
                const CGGlyph * glyphs = CTRunGetGlyphsPtr(run);
                const CGPoint * positions = CTRunGetPositionsPtr(run);
                const CGSize * advances = CTRunGetAdvancesPtr(run);
                const CFIndex * indices = CTRunGetStringIndicesPtr(run);

                if(glyphs == nullptr)
                {
                    glyphStorage.resize(glyphCount);
                    CTRunGetGlyphs(run, CFRangeMake(0, 0), glyphStorage.data());
                    glyphs = glyphStorage.data();
                }

                if(positions == nullptr)
                {
                    positionStorage.resize(glyphCount);
                    CTRunGetPositions(run, CFRangeMake(0, 0), positionStorage.data());
                    positions = positionStorage.data();
                }

                if(advances == nullptr)
                {
                    advanceStorage.resize(glyphCount);
                    CTRunGetAdvances(run, CFRangeMake(0, 0), advanceStorage.data());
                    advances = advanceStorage.data();
                }

                if(indices == nullptr)
                {
                    indexStorage.resize(glyphCount);
                    CTRunGetStringIndices(run, CFRangeMake(0, 0), indexStorage.data());
                    indices = indexStorage.data();
                }

                CFDictionaryRef runAttributes = CTRunGetAttributes(run);
                CTFontRef runFont = static_cast<CTFontRef>(const_cast<void *>(CFDictionaryGetValue(runAttributes, kCTFontAttributeName)));
                uint32_t face = findOrCreateResolvedFont(runFont == nullptr ? nativeFont : runFont);
                for(size_t index = 0; index != glyphCount; ++index)
                {
                    size_t stringIndex = indices[index] < 0 ? 0 : std::min(static_cast<size_t>(indices[index]), byteOffsets.size() - 1);
                    size_t cluster = byteOffsets[stringIndex];
                    output.glyphs.push_back({static_cast<uint32_t>(glyphs[index]), face, {static_cast<float>(positions[index].x), baseline + static_cast<float>(lineIndex) * lineHeight - static_cast<float>(positions[index].y)}, static_cast<float>(advances[index].width), cluster});
                    output.clusters.push_back(cluster);
                }
            }
            CFRelease(line);
            output.clusters.push_back(byteEnd);
            ++lineIndex;

            if(lineLength < paragraphEnd - lineStart)
            {
                lineStart += lineLength;
                continue;
            }

            if(paragraphEnd < utf16Length)
            {
                size_t newlineByte = byteOffsets[static_cast<size_t>(paragraphEnd)];
                size_t afterNewline = byteOffsets[static_cast<size_t>(paragraphEnd + 1)];
                output.clusters.push_back(newlineByte);
                output.clusters.push_back(afterNewline);
                lineStart = paragraphEnd + 1;
                continue;
            }

            break;
        }

        CFRelease(typesetter);
        output.size = {maximumWidth, lineHeight * static_cast<float>(lineIndex)};
        output.clusters.push_back(text.size());
        std::sort(output.clusters.begin(), output.clusters.end());
        output.clusters.erase(std::unique(output.clusters.begin(), output.clusters.end()), output.clusters.end());

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::getGlyph(FontHandle font, float size, uint32_t glyphId, Glyph * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        ShapedGlyph shaped;
        shaped.glyph = glyphId;
        bool returnedValue = getGlyph(font, size, shaped, _out);

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::getGlyph(FontHandle font, float size, const ShapedGlyph & shaped, Glyph * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        float scale = m_scale;
        uint64_t key = Detail::glyphKey(font, size, scale, shaped.face, shaped.glyph);
        auto cached = m_glyphs.find(key);

        if(cached != m_glyphs.end())
        {

            *_out = cached->second.value;

            return true;
        }

        // Layout stays in logical points, but glyph bitmaps are generated in backing pixels.
        // On a Retina display this gives the fake editor a real 2x glyph instead of asking
        // Metal to stretch a low-resolution texture across twice as many physical pixels.
        CTFontRef nativeFont = nullptr;
        bool releaseNativeFont = false;
        const void * resolved = resolvedFont(shaped.face);

        if(resolved != nullptr)
        {
            CTFontRef logicalFont = static_cast<CTFontRef>(resolved);
            nativeFont = CTFontCreateCopyWithAttributes(logicalFont, std::max(1.f, static_cast<float>(CTFontGetSize(logicalFont)) * scale), nullptr, nullptr);
            releaseNativeFont = true;
        }
        else
        {
            const CachedFont * cachedFont = findOrCreateFont(font, size * scale);

            if(cachedFont != nullptr)
            {
                nativeFont = static_cast<CTFontRef>(cachedFont->native);
            }
        }

        if(nativeFont == nullptr)
        {
            return false;
        }

        CGGlyph nativeGlyph = static_cast<CGGlyph>(shaped.glyph);
        CGRect bounds = CTFontGetBoundingRectsForGlyphs(nativeFont, kCTFontOrientationHorizontal, &nativeGlyph, nullptr, 1);
        CGSize advance = {};
        CTFontGetAdvancesForGlyphs(nativeFont, kCTFontOrientationHorizontal, &nativeGlyph, &advance, 1);

        size_t padding = static_cast<size_t>(std::ceil(scale));
        float minimumX = std::floor(static_cast<float>(CGRectGetMinX(bounds)));
        float minimumY = std::floor(static_cast<float>(CGRectGetMinY(bounds)));
        float maximumX = std::ceil(static_cast<float>(CGRectGetMaxX(bounds)));
        float maximumY = std::ceil(static_cast<float>(CGRectGetMaxY(bounds)));
        uint32_t width = static_cast<uint32_t>(std::max(1.f, maximumX - minimumX + padding * 2));
        uint32_t height = static_cast<uint32_t>(std::max(1.f, maximumY - minimumY + padding * 2));

        ByteVector mask(static_cast<size_t>(width) * height, std::byte{0});
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceGray();
        CGContextRef context = CGBitmapContextCreate(mask.data(), width, height, 8, width, colorSpace, kCGImageAlphaNone);
        CGColorSpaceRelease(colorSpace);

        if(context == nullptr)
        {
            if(releaseNativeFont == true)
            {
                CFRelease(nativeFont);
            }

            return false;
        }

        CGContextSetGrayFillColor(context, 1.0, 1.0);
        CGContextSetAllowsAntialiasing(context, true);
        CGContextSetShouldAntialias(context, true);
        CGContextSetAllowsFontSmoothing(context, true);
        CGContextSetShouldSmoothFonts(context, true);
        // The atlas stores a single grayscale coverage channel, so subpixel RGB positioning
        // cannot survive the upload. Stable pixel-aligned masks look sharper on both 1x and 2x.
        CGContextSetShouldSubpixelPositionFonts(context, false);
        CGContextSetShouldSubpixelQuantizeFonts(context, false);
        CGPoint position = {padding - minimumX, padding - minimumY};
        CTFontDrawGlyphs(nativeFont, &nativeGlyph, &position, 1, context);
        CGContextRelease(context);

        if(releaseNativeFont == true)
        {
            CFRelease(nativeFont);
        }

        ByteVector rgba;
        auto prepareRgba = [&rgba, &mask, width, height]()
        {
            if(rgba.empty() == false)
            {
                return;
            }

            rgba.assign(static_cast<size_t>(width) * height * 4, std::byte{0xff});
            for(uint32_t y = 0; y != height; ++y)
            {
                for(uint32_t x = 0; x != width; ++x)
                {
                    size_t target = (static_cast<size_t>(y) * width + x) * 4;
                    rgba[target + 3] = mask[static_cast<size_t>(y) * width + x];
                }
            }
        };

        Glyph result;
        bool atlas = false;
        size_t pageIndex = 0;
        uint32_t atlasX = 0;
        uint32_t atlasY = 0;
        if(allocateAtlasRegion(width, height, &pageIndex, &atlasX, &atlasY) == true)
        {
            const AtlasPage & page = m_atlasPages[pageIndex];
            bool updated = false;

            if(page.mask == true)
            {
                updated = m_renderer.updateMaskTextureRegion(page.texture, atlasX, atlasY, width, height, width, mask);
            }
            else
            {
                prepareRgba();
                updated = m_renderer.updateTextureRegion(page.texture, atlasX, atlasY, width, height, width * 4, rgba);
            }

            if(updated == true)
            {
                result.texture = page.texture;
                result.uv = {static_cast<float>(atlasX) / static_cast<float>(page.width), static_cast<float>(atlasY) / static_cast<float>(page.height), static_cast<float>(width) / static_cast<float>(page.width), static_cast<float>(height) / static_cast<float>(page.height)};
                atlas = true;
            }
        }

        if(atlas == false)
        {
            result.texture = m_renderer.createMaskTexture(width, height, mask);

            if(result.texture == 0)
            {
                prepareRgba();
                result.texture = m_renderer.createTexture(width, height, rgba);
            }

            result.uv = {0.f, 0.f, 1.f, 1.f};
        }

        result.size = {static_cast<float>(width) / scale, static_cast<float>(height) / scale};
        result.bearing = {(minimumX - static_cast<float>(padding)) / scale, (-maximumY - static_cast<float>(padding)) / scale};
        result.advance = static_cast<float>(advance.width) / scale;
        m_glyphs.emplace(key, CachedGlyph{font, size, shaped.face, shaped.glyph, result, atlas});

        if(result.texture == 0)
        {
            return false;
        }

        *_out = result;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    uint64_t CoreTextFontProvider::revision() const noexcept
    {
        return m_revision;
    }
    //////////////////////////////////////////////////////////////////////////
    FontCacheMetrics CoreTextFontProvider::cacheMetrics() const noexcept
    {
        size_t atlasMemory = 0;
        for(const AtlasPage & page : m_atlasPages)
        {
            atlasMemory += static_cast<size_t>(page.width) * page.height * (page.mask ? 1 : 4);
        }
        FontCacheMetrics result = {m_fonts.size(), m_resolvedFonts.size(), m_glyphs.size(), m_atlasPages.size(), atlasMemory};

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::atlasPage(size_t index, FontAtlasPage * const _out) const noexcept
    {
        if(_out == nullptr)
        {
            return false;
        }

        if(index >= m_atlasPages.size())
        {
            return false;
        }

        const AtlasPage & page = m_atlasPages[index];

        if(page.texture == 0)
        {
            return false;
        }

        FontAtlasPage output = {page.texture, {static_cast<float>(page.width), static_cast<float>(page.height)}, page.mask};

        *_out = output;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::inspectFontCache(FontCacheEntryVector * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        FontCacheEntryVector output;
        output.reserve(m_fonts.size());
        for(const auto & [key, cached] : m_fonts)
        {
            output.push_back({key.font, static_cast<float>(key.size) / 64.f, cached.metrics});
        }
        std::sort(output.begin(), output.end(),
                  [](const FontCacheEntry & left, const FontCacheEntry & right)
                  {
                      return left.font == right.font ? left.size < right.size : left.font < right.font;
                  });

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool CoreTextFontProvider::inspectGlyphCache(GlyphCacheEntryVector * const _out) const
    {
        if(_out == nullptr)
        {
            return false;
        }

        GlyphCacheEntryVector output;
        output.reserve(m_glyphs.size());
        for(const auto & [key, cached] : m_glyphs)
        {
            static_cast<void>(key);
            output.push_back({cached.font, cached.size, cached.face, cached.glyph, cached.value, cached.atlas});
        }
        std::sort(output.begin(), output.end(),
                  [](const GlyphCacheEntry & left, const GlyphCacheEntry & right)
                  {
                      if(left.font != right.font)
                      {
                          return left.font < right.font;
                      }

                      if(left.size != right.size)
                      {
                          return left.size < right.size;
                      }

                      if(left.face != right.face)
                      {
                          return left.face < right.face;
                      }

                      return left.glyph < right.glyph;
                  });

        *_out = std::move(output);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
