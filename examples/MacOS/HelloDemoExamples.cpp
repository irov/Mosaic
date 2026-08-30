#include "HelloDemo.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <utility>

namespace MosaicExample::ExamplesDetail
{
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] Mosaic::Color demoHsv(float hue, float saturation, float value) noexcept
    {
        float huePosition = hue * 6.f;
        int sector = static_cast<int>(std::floor(huePosition));
        float fraction = huePosition - static_cast<float>(sector);
        float primary = value * (1.f - saturation);
        float secondary = value * (1.f - saturation * fraction);
        float tertiary = value * (1.f - saturation * (1.f - fraction));
        Mosaic::Color color;

        switch(sector % 6)
        {
        case 0:
            color = {value, tertiary, primary, 1.f};
            break;
        case 1:
            color = {secondary, value, primary, 1.f};
            break;
        case 2:
            color = {primary, value, tertiary, 1.f};
            break;
        case 3:
            color = {primary, secondary, value, 1.f};
            break;
        case 4:
            color = {tertiary, primary, value, 1.f};
            break;
        default:
            color = {value, primary, secondary, 1.f};
            break;
        }

        return color;
    }
    //////////////////////////////////////////////////////////////////////////
    template<class T> [[nodiscard]] Mosaic::String number(T value)
    {
        char buffer[64] = {};
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] Mosaic::String hexadecimal(Mosaic::Id value)
    {
        char digits[32] = {};
        auto result = std::to_chars(digits, digits + sizeof(digits), value, 16);

        if(result.ec != std::errc{})
        {
            Mosaic::String output = "0x?";

            return output;
        }

        size_t digitCount = static_cast<size_t>(result.ptr - digits);
        Mosaic::String output = "0x";
        output.append(16 - std::min<size_t>(16, digitCount), '0');
        output.append(digits, result.ptr);

        return output;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] size_t decodeUtf8(Mosaic::StringView value, size_t offset, char32_t * const _out) noexcept
    {
        if(_out == nullptr)
        {
            return 0;
        }

        if(offset >= value.size())
        {
            return 0;
        }

        unsigned char first = static_cast<unsigned char>(value[offset]);
        size_t length = 1;
        char32_t codepoint = first;

        if((first & 0xe0U) == 0xc0U)
        {
            length = 2;
            codepoint = first & 0x1fU;
        }
        else if((first & 0xf0U) == 0xe0U)
        {
            length = 3;
            codepoint = first & 0x0fU;
        }
        else if((first & 0xf8U) == 0xf0U)
        {
            length = 4;
            codepoint = first & 0x07U;
        }
        else if((first & 0x80U) != 0U)
        {
            *_out = U'\ufffd';

            return 1;
        }

        if(offset + length > value.size())
        {
            *_out = U'\ufffd';

            return 1;
        }

        for(size_t index = 1; index != length; ++index)
        {
            unsigned char continuation = static_cast<unsigned char>(value[offset + index]);

            if((continuation & 0xc0U) != 0x80U)
            {
                *_out = U'\ufffd';

                return 1;
            }

            codepoint = static_cast<char32_t>((codepoint << 6U) | (continuation & 0x3fU));
        }

        *_out = codepoint;

        return length;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] Mosaic::String codepoint(char32_t value)
    {
        char digits[16] = {};
        auto conversion = std::to_chars(digits, digits + sizeof(digits), static_cast<uint32_t>(value), 16);
        Mosaic::String output = "U+";

        if(conversion.ec != std::errc{})
        {
            output += '?';

            return output;
        }

        size_t count = static_cast<size_t>(conversion.ptr - digits);
        output.append(4 - std::min<size_t>(4, count), '0');
        output.append(digits, conversion.ptr);

        return output;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] Mosaic::String encodedPath(Mosaic::StringView value, bool encodeNonAscii)
    {
        if(encodeNonAscii == false)
        {
            Mosaic::String output(value);

            return output;
        }

        constexpr char hexadecimalDigits[] = "0123456789ABCDEF";
        Mosaic::String output;
        output.reserve(value.size());
        for(unsigned char character : value)
        {
            if(character < 0x80U)
            {
                output.push_back(static_cast<char>(character));
                continue;
            }

            output.push_back('%');
            output.push_back(hexadecimalDigits[character >> 4U]);
            output.push_back(hexadecimalDigits[character & 0x0fU]);
        }

        return output;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] Mosaic::String fixed(double value, int precision)
    {
        char buffer[64] = {};
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, precision);
        auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] bool containsIgnoreCase(Mosaic::StringView value, Mosaic::StringView pattern) noexcept
    {
        if(pattern.empty() == true)
        {
            return true;
        }

        if(pattern.size() > value.size())
        {
            return false;
        }

        for(size_t offset = 0; offset + pattern.size() <= value.size(); ++offset)
        {
            bool matches = true;
            for(size_t index = 0; index != pattern.size(); ++index)
            {
                unsigned char left = static_cast<unsigned char>(value[offset + index]);
                unsigned char right = static_cast<unsigned char>(pattern[index]);

                if(std::tolower(left) == std::tolower(right))
                {
                    continue;
                }

                matches = false;
                break;
            }

            if(matches == true)
            {
                return true;
            }
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] Mosaic::LayoutOptions fill(float height = 0.f) noexcept
    {
        Mosaic::LayoutOptions layout;
        layout.width = Mosaic::SizeRule::Fill;
        layout.height = height > 0.f ? Mosaic::Dimension::fixed(height) : Mosaic::Dimension(Mosaic::SizeRule::Fill);

        return layout;
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] Mosaic::StringView eventName(Mosaic::EventType type) noexcept
    {
        switch(type)
        {
        case Mosaic::EventType::PointerDown:
            return "PointerDown";
        case Mosaic::EventType::PointerUp:
            return "PointerUp";
        case Mosaic::EventType::FocusChanged:
            return "FocusChanged";
        case Mosaic::EventType::BeginEdit:
            return "BeginEdit";
        case Mosaic::EventType::Change:
            return "Change";
        case Mosaic::EventType::Commit:
            return "Commit";
        case Mosaic::EventType::Cancel:
            return "Cancel";
        case Mosaic::EventType::DragBegin:
            return "DragBegin";
        case Mosaic::EventType::Drop:
            return "Drop";
        case Mosaic::EventType::PopupOpen:
            return "PopupOpen";
        case Mosaic::EventType::PopupClose:
            return "PopupClose";
        }

        return "Unknown";
    }
    //////////////////////////////////////////////////////////////////////////
    [[nodiscard]] DebugDemoCategory eventCategory(Mosaic::EventType type) noexcept
    {
        switch(type)
        {
        case Mosaic::EventType::PointerDown:
        case Mosaic::EventType::PointerUp:
            return DebugDemoCategory::ActiveItem;
        case Mosaic::EventType::FocusChanged:
            return DebugDemoCategory::Focus;
        case Mosaic::EventType::BeginEdit:
        case Mosaic::EventType::Change:
        case Mosaic::EventType::Commit:
        case Mosaic::EventType::Cancel:
            return DebugDemoCategory::ActiveItem;
        case Mosaic::EventType::DragBegin:
        case Mosaic::EventType::Drop:
            return DebugDemoCategory::Selection;
        case Mosaic::EventType::PopupOpen:
        case Mosaic::EventType::PopupClose:
            return DebugDemoCategory::Popup;
        }

        return DebugDemoCategory::Errors;
    }
    //////////////////////////////////////////////////////////////////////////
    void checkerboard(Mosaic::Canvas & canvas, const Mosaic::Rect & bounds, float cell)
    {
        Mosaic::Rect content;
        if(canvas.contentRect(&content) == false)
        {
            return;
        }

        Mosaic::Rect visible = Mosaic::Rect::intersection(bounds, content);

        if(visible.empty() == true)
        {
            return;
        }

        if(cell <= 0.f)
        {
            return;
        }

        float firstX = std::floor((visible.x - bounds.x) / cell) * cell;
        float firstY = std::floor((visible.y - bounds.y) / cell) * cell;
        for(float y = firstY; y < visible.bottom() - bounds.y; y += cell)
        {
            for(float x = firstX; x < visible.right() - bounds.x; x += cell)
            {
                bool light = (static_cast<int>(x / cell) + static_cast<int>(y / cell)) % 2 == 0;
                Mosaic::Rect cellBounds = Mosaic::Rect::intersection({bounds.x + x, bounds.y + y, cell, cell}, visible);
                canvas.rect(cellBounds, light ? Mosaic::Color::fromBytes(82, 82, 82) : Mosaic::Color::fromBytes(42, 42, 42));
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void aspectRatioConstraint(Mosaic::Vec2 * desired, const Mosaic::Vec2 & current, void * userData)
    {
        if(desired == nullptr)
        {
            return;
        }

        if(userData == nullptr)
        {
            return;
        }

        float ratio = std::max(0.01f, *static_cast<const float *>(userData));
        float horizontalChange = std::abs(desired->x - current.x);
        float verticalChange = std::abs(desired->y - current.y) * ratio;

        if(horizontalChange >= verticalChange)
        {
            desired->y = desired->x / ratio;
        }
        else
        {
            desired->x = desired->y * ratio;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void squareConstraint(Mosaic::Vec2 * desired, const Mosaic::Vec2 & current, void *)
    {
        if(desired == nullptr)
        {
            return;
        }

        float horizontalChange = std::abs(desired->x - current.x);
        float verticalChange = std::abs(desired->y - current.y);
        float side = horizontalChange >= verticalChange ? desired->x : desired->y;
        *desired = {side, side};
    }
    //////////////////////////////////////////////////////////////////////////
    void stepConstraint(Mosaic::Vec2 * desired, const Mosaic::Vec2 &, void * userData)
    {
        if(desired == nullptr)
        {
            return;
        }

        if(userData == nullptr)
        {
            return;
        }

        float step = std::max(1.f, *static_cast<const float *>(userData));
        desired->x = std::round(desired->x / step) * step;
        desired->y = std::round(desired->y / step) * step;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace MosaicExample::ExamplesDetail

namespace MosaicExample
{
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawFontAtlas(Mosaic::Context * ui)
    {
        if(m_fontCacheActionPending == true)
        {
            (void)Mosaic::fontCacheAction(ui, m_pendingFontCacheAction);
            m_fontCacheActionPending = false;
        }

        if(Mosaic::availableFonts(ui, &m_availableFonts) == false)
        {
            m_availableFonts.clear();
        }

        m_availableFontNames.clear();
        m_availableFontNames.reserve(m_availableFonts.size());
        for(const Mosaic::FontInfo & font : m_availableFonts)
        {
            m_availableFontNames.push_back(font.name);
        }

        if(m_availableFonts.empty() == false)
        {
            m_fontPreviewSelection = std::clamp(m_fontPreviewSelection, 0, static_cast<int>(m_availableFonts.size() - 1));
            {
                auto selectorScope = Mosaic::line(ui, Mosaic::Key("font face selector"));
                Mosaic::comboBox(ui, "Font", &m_fontPreviewSelection, m_availableFontNames);
                Mosaic::checkbox(ui, "Show font preview", &m_showFontAtlasPreview);
            }
            const Mosaic::FontInfo & selectedFont = m_availableFonts[static_cast<size_t>(m_fontPreviewSelection)];

            if(m_showFontAtlasPreview == true)
            {
                Mosaic::pushFont(ui, selectedFont.font);
                Mosaic::text(ui, selectedFont.monospace ? "0123456789  WWWWWW  iiiiii" : "The quick brown fox jumps over the lazy dog");
                Mosaic::popFont(ui);
            }

            auto loaderDetails = Mosaic::treeNode(ui, Mosaic::Key("font loader details"), "Loader");

            if(loaderDetails.expanded() == true)
            {
                Mosaic::String loader = "Name: ";
                loader += selectedFont.loader;
                Mosaic::bulletText(ui, loader);
                for(size_t sourceIndex = 0; sourceIndex != selectedFont.sources.size(); ++sourceIndex)
                {
                    Mosaic::String source = sourceIndex == 0 ? "Source: " : "        ";
                    source += selectedFont.sources[sourceIndex];
                    Mosaic::bulletText(ui, source);
                }

                for(const Mosaic::FontGlyphRange & range : selectedFont.glyphRanges)
                {
                    Mosaic::String glyphRange = "Glyph range: ";
                    glyphRange += ExamplesDetail::hexadecimal(range.first);
                    glyphRange += "..";
                    glyphRange += ExamplesDetail::hexadecimal(range.last);
                    Mosaic::bulletText(ui, glyphRange);
                }
                Mosaic::String scale = "Effective scale: ";
                scale += ExamplesDetail::fixed(selectedFont.effectiveScale, 2);
                Mosaic::bulletText(ui, scale);
                Mosaic::String capabilities = "Flags:";

                if((selectedFont.loaderFlags & Mosaic::FontLoaderScalable) != 0)
                {
                    capabilities += " scalable";
                }

                if((selectedFont.loaderFlags & Mosaic::FontLoaderDynamicFallback) != 0)
                {
                    capabilities += " dynamic-fallback";
                }

                if((selectedFont.loaderFlags & Mosaic::FontLoaderMaskAtlas) != 0)
                {
                    capabilities += " mask-atlas";
                }

                Mosaic::bulletText(ui, capabilities);
                Mosaic::String fallback = "Fallback glyph: U+";
                fallback += ExamplesDetail::number(selectedFont.fallbackGlyph);
                fallback += ", ellipsis glyph: U+";
                fallback += ExamplesDetail::number(selectedFont.ellipsisGlyph);
                Mosaic::bulletText(ui, fallback);
            }
        }

        Mosaic::FontAtlasConfiguration atlasConfiguration;
        if(Mosaic::fontAtlasConfiguration(ui, &atlasConfiguration) == true)
        {
            auto configuration = Mosaic::treeNode(ui, Mosaic::Key("font atlas configuration"), "Atlas configuration");

            if(configuration.expanded() == true)
            {
                Mosaic::String pageSize = "Page size: ";
                pageSize += ExamplesDetail::number(static_cast<uint32_t>(atlasConfiguration.pageSize.x));
                pageSize += "x";
                pageSize += ExamplesDetail::number(static_cast<uint32_t>(atlasConfiguration.pageSize.y));
                Mosaic::bulletText(ui, pageSize);
                Mosaic::String spacing = "Glyph spacing: ";
                spacing += ExamplesDetail::number(atlasConfiguration.glyphSpacing);
                Mosaic::bulletText(ui, spacing);
                Mosaic::bulletText(ui, atlasConfiguration.dynamicCoverage ? "Coverage: dynamic Unicode fallback" : "Coverage: predefined ranges");
                Mosaic::bulletText(ui, atlasConfiguration.mask ? "Texture format: single-channel mask" : "Texture format: RGBA");
            }
        }

        Mosaic::FontCacheMetrics metrics;
        if(Mosaic::fontCacheMetrics(ui, &metrics) == false)
        {
            return;
        }

        {
            auto cacheActions = Mosaic::row(ui);

            if(Mosaic::button(ui, Mosaic::Key("font cache clear"), "Clear").clicked() == true)
            {
                m_pendingFontCacheAction = Mosaic::FontCacheAction::Clear;
                m_fontCacheActionPending = true;
            }

            if(Mosaic::button(ui, Mosaic::Key("font cache compact"), "Compact").clicked() == true)
            {
                m_pendingFontCacheAction = Mosaic::FontCacheAction::Compact;
                m_fontCacheActionPending = true;
            }

            if(Mosaic::button(ui, Mosaic::Key("font cache grow"), "Grow").clicked() == true)
            {
                m_pendingFontCacheAction = Mosaic::FontCacheAction::Grow;
                m_fontCacheActionPending = true;
            }

            if(Mosaic::button(ui, Mosaic::Key("font cache rebuild"), "Rebuild").clicked() == true)
            {
                m_pendingFontCacheAction = Mosaic::FontCacheAction::Rebuild;
                m_fontCacheActionPending = true;
            }
        }

        Mosaic::String summary = "Fonts ";
        summary += ExamplesDetail::number(metrics.fontCount);
        summary += ", resolved faces ";
        summary += ExamplesDetail::number(metrics.resolvedFaceCount);
        summary += ", glyphs ";
        summary += ExamplesDetail::number(metrics.glyphCount);
        summary += ", atlas memory ";
        summary += ExamplesDetail::number(metrics.atlasMemory);
        summary += " bytes";
        Mosaic::text(ui, summary);

        Mosaic::FontCacheEntryVector fontCache;
        if(Mosaic::fontCacheEntries(ui, &fontCache) == true)
        {
            auto entries = Mosaic::treeNode(ui, Mosaic::Key("effective font entries"), "Effective fonts");

            if(entries.expanded() == true)
            {
                Mosaic::TableOptions tableOptions;
                tableOptions.headers = true;
                tableOptions.rowBackground = true;
                tableOptions.bordersInnerHorizontal = true;
                auto table = Mosaic::table(ui, "Effective font cache", 7, tableOptions);
                Mosaic::tableSetupColumn(ui, 0, "Font");
                Mosaic::tableSetupColumn(ui, 1, "Requested");
                Mosaic::tableSetupColumn(ui, 2, "Effective");
                Mosaic::tableSetupColumn(ui, 3, "Ascent");
                Mosaic::tableSetupColumn(ui, 4, "Line height");
                Mosaic::tableSetupColumn(ui, 5, "Glyphs");
                Mosaic::tableSetupColumn(ui, 6, "Range");
                Mosaic::tableHeadersRow(ui);
                for(size_t index = 0; index != fontCache.size(); ++index)
                {
                    const Mosaic::FontCacheEntry & entry = fontCache[index];
                    Mosaic::tableNextRow(ui, Mosaic::Key(index));
                    (void)Mosaic::tableSetColumn(ui, 0);
                    Mosaic::text(ui, entry.font == Mosaic::MonospaceFont ? "Monospace" : "Default");
                    (void)Mosaic::tableSetColumn(ui, 1);
                    Mosaic::text(ui, ExamplesDetail::fixed(entry.size, 2));
                    (void)Mosaic::tableSetColumn(ui, 2);
                    Mosaic::text(ui, ExamplesDetail::fixed(entry.effectiveSize, 2));
                    (void)Mosaic::tableSetColumn(ui, 3);
                    Mosaic::text(ui, ExamplesDetail::fixed(entry.metrics.ascent, 2));
                    (void)Mosaic::tableSetColumn(ui, 4);
                    Mosaic::text(ui, ExamplesDetail::fixed(entry.metrics.lineHeight(), 2));
                    (void)Mosaic::tableSetColumn(ui, 5);
                    Mosaic::text(ui, ExamplesDetail::number(entry.glyphCount));
                    (void)Mosaic::tableSetColumn(ui, 6);
                    Mosaic::String range = ExamplesDetail::number(entry.firstGlyph);
                    range += "..";
                    range += ExamplesDetail::number(entry.lastGlyph);
                    Mosaic::text(ui, range);
                }
            }
        }

        Mosaic::FontAtlasRectInfoVector atlasRects;
        (void)Mosaic::fontAtlasRects(ui, &atlasRects);

        Mosaic::String atlasLabel = "Atlas pages (";
        atlasLabel += ExamplesDetail::number(metrics.atlasPageCount);
        atlasLabel += ")";
        auto atlas = Mosaic::treeNode(ui, Mosaic::Key("font atlas pages"), atlasLabel);

        if(atlas.expanded() == true)
        {
            for(size_t index = 0; index != metrics.atlasPageCount; ++index)
            {
                Mosaic::FontAtlasPage page;
                if(Mosaic::fontAtlasPage(ui, index, &page) == false)
                {
                    continue;
                }

                auto pageScope = Mosaic::scope(ui, Mosaic::Key(index));
                Mosaic::String pageLabel = "Page ";
                pageLabel += ExamplesDetail::number(index);
                pageLabel += ": ";
                pageLabel += ExamplesDetail::number(static_cast<uint32_t>(page.size.x));
                pageLabel += "x";
                pageLabel += ExamplesDetail::number(static_cast<uint32_t>(page.size.y));
                pageLabel += page.mask ? " alpha" : " RGBA";
                pageLabel += page.active ? " ready" : " unavailable";
                Mosaic::text(ui, pageLabel);
                float totalArea = std::max(1.f, page.size.x * page.size.y);
                float usedArea = page.used.width * page.used.height;
                Mosaic::String usage = "Used ";
                usage += ExamplesDetail::number(static_cast<uint32_t>(page.used.width));
                usage += "x";
                usage += ExamplesDetail::number(static_cast<uint32_t>(page.used.height));
                usage += " (";
                usage += ExamplesDetail::fixed(usedArea * 100.0 / totalArea, 1);
                usage += "%), packed ";
                usage += ExamplesDetail::number(page.packedRectCount);
                usage += ", discarded ";
                usage += ExamplesDetail::number(page.discardedRectCount);
                usage += ", packed surface ";
                usage += ExamplesDetail::number(page.packedArea);
                usage += ", discarded surface ";
                usage += ExamplesDetail::number(page.discardedArea);
                Mosaic::text(ui, usage);

                if(m_showFontAtlasPreview == false)
                {
                    continue;
                }

                float previewWidth = 256.f;
                float previewHeight = page.size.x > 0.f ? previewWidth * page.size.y / page.size.x : previewWidth;
                previewHeight = std::min(previewHeight, 256.f);
                Mosaic::LayoutOptions previewLayout;
                previewLayout.width = Mosaic::Dimension::fixed(previewWidth);
                previewLayout.height = Mosaic::Dimension::fixed(previewHeight);
                Mosaic::Canvas preview = Mosaic::canvas(ui, Mosaic::Key("font atlas preview"), "Font atlas preview", previewLayout);
                preview.image(page.texture, {0.f, 0.f, previewWidth, previewHeight}, {0.f, 0.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, Mosaic::SamplerFilter::Nearest);
                float scaleX = page.size.x > 0.f ? previewWidth / page.size.x : 1.f;
                float scaleY = page.size.y > 0.f ? previewHeight / page.size.y : 1.f;

                if(Mosaic::itemHovered(ui, preview.id()) == true)
                {
                    const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
                    Mosaic::Rect previewBounds;

                    if(pointer != nullptr && Mosaic::debugBounds(ui, preview.id(), &previewBounds) == true)
                    {
                        Mosaic::Vec2 atlasPosition = {(pointer->position.x - previewBounds.x) / std::max(scaleX, 0.0001f), (pointer->position.y - previewBounds.y) / std::max(scaleY, 0.0001f)};
                        for(const Mosaic::FontAtlasRectInfo & rect : atlasRects)
                        {
                            if(rect.page == index && rect.packed == true && rect.bounds.contains(atlasPosition) == true)
                            {
                                m_fontHoveredRect = rect.index;
                                break;
                            }
                        }
                    }
                }

                size_t visibleRectCount = 0;
                for(const Mosaic::FontAtlasRectInfo & rect : atlasRects)
                {
                    if(rect.page != index || rect.packed == false)
                    {
                        continue;
                    }

                    Mosaic::Rect outline = {rect.bounds.x * scaleX, rect.bounds.y * scaleY, rect.bounds.width * scaleX, rect.bounds.height * scaleY};
                    Mosaic::Array<Mosaic::Vec2, 5> points = {{{outline.x, outline.y}, {outline.right(), outline.y}, {outline.right(), outline.bottom()}, {outline.x, outline.bottom()}, {outline.x, outline.y}}};
                    Mosaic::Color outlineColor = rect.index == m_fontHoveredRect ? Mosaic::Color::fromBytes(255, 96, 64) : Mosaic::Color::fromBytes(255, 255, 0, 140);
                    preview.polyline(points, rect.index == m_fontHoveredRect ? 2.f : 1.f, outlineColor);
                    ++visibleRectCount;

                    if(visibleRectCount == 512)
                    {
                        break;
                    }
                }
            }
        }

        Mosaic::GlyphCacheEntryVector glyphs;
        if(Mosaic::glyphCacheEntries(ui, &glyphs) == true)
        {
            Mosaic::String glyphLabel = "Glyph/rect preview (";
            glyphLabel += ExamplesDetail::number(glyphs.size());
            glyphLabel += ")";
            auto glyphPreview = Mosaic::treeNode(ui, Mosaic::Key("glyph rect preview"), glyphLabel);

            if(glyphPreview.expanded() == true)
            {
                Mosaic::TableOptions tableOptions;
                tableOptions.headers = true;
                tableOptions.rowBackground = true;
                tableOptions.scrollVertical = true;
                Mosaic::LayoutOptions tableLayout;
                tableLayout.width = Mosaic::SizeRule::Fill;
                tableLayout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 12.f);
                auto table = Mosaic::table(ui, "Glyph cache preview", 5, tableOptions, tableLayout);
                Mosaic::tableSetupColumn(ui, 0, "Glyph");
                Mosaic::tableSetupColumn(ui, 1, "Face");
                Mosaic::tableSetupColumn(ui, 2, "Size");
                Mosaic::tableSetupColumn(ui, 3, "Advance");
                Mosaic::tableSetupColumn(ui, 4, "Preview");
                Mosaic::tableHeadersRow(ui);
                size_t previewCount = std::min(glyphs.size(), size_t{256});
                Mosaic::VisibleRange rows = {0, previewCount};
                (void)Mosaic::tableVisibleRows(ui, previewCount, Mosaic::getTheme(ui).metrics.controlHeight, &rows);
                for(size_t index = rows.begin; index != rows.end; ++index)
                {
                    const Mosaic::GlyphCacheEntry & glyph = glyphs[index];
                    Mosaic::tableNextRow(ui, Mosaic::Key(index));
                    (void)Mosaic::tableSetColumn(ui, 0);
                    Mosaic::Response glyphItem = Mosaic::selectable(ui, Mosaic::Key(glyph.glyph), ExamplesDetail::number(glyph.glyph), glyph.atlasRect == m_fontHoveredRect);

                    if(glyphItem.hovered() == true)
                    {
                        m_fontHoveredRect = glyph.atlasRect;
                    }
                    (void)Mosaic::tableSetColumn(ui, 1);
                    Mosaic::text(ui, ExamplesDetail::number(glyph.face));
                    (void)Mosaic::tableSetColumn(ui, 2);
                    Mosaic::text(ui, ExamplesDetail::fixed(glyph.size, 2));
                    (void)Mosaic::tableSetColumn(ui, 3);
                    Mosaic::text(ui, ExamplesDetail::fixed(glyph.value.advance, 2));
                    (void)Mosaic::tableSetColumn(ui, 4);

                    if(glyph.value.texture != 0)
                    {
                        Mosaic::ImageOptions imageOptions;
                        imageOptions.uv = glyph.value.uv;
                        imageOptions.sampler = Mosaic::SamplerFilter::Nearest;
                        imageOptions.backgroundEnabled = true;
                        imageOptions.background = Mosaic::Color::fromBytes(35, 35, 35);
                        Mosaic::image(ui, glyph.value.texture, {24.f, 24.f}, imageOptions);
                    }
                }
            }
        }

        auto rectIndex = Mosaic::treeNode(ui, Mosaic::Key("font rect index"), "Rect index");

        if(rectIndex.expanded() == true)
        {
            Mosaic::TableOptions tableOptions;
            tableOptions.headers = true;
            tableOptions.rowBackground = true;
            tableOptions.scrollVertical = true;
            Mosaic::LayoutOptions tableLayout;
            tableLayout.width = Mosaic::SizeRule::Fill;
            tableLayout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 12.f);
            auto table = Mosaic::table(ui, "Font rect index", 6, tableOptions, tableLayout);
            Mosaic::tableSetupColumn(ui, 0, "Index");
            Mosaic::tableSetupColumn(ui, 1, "Page");
            Mosaic::tableSetupColumn(ui, 2, "Glyph");
            Mosaic::tableSetupColumn(ui, 3, "Size");
            Mosaic::tableSetupColumn(ui, 4, "Bounds");
            Mosaic::tableSetupColumn(ui, 5, "Status");
            Mosaic::tableHeadersRow(ui);
            Mosaic::VisibleRange rows = {0, atlasRects.size()};
            (void)Mosaic::tableVisibleRows(ui, atlasRects.size(), Mosaic::getTheme(ui).metrics.controlHeight, &rows);
            for(size_t index = rows.begin; index != rows.end; ++index)
            {
                const Mosaic::FontAtlasRectInfo & rect = atlasRects[index];
                Mosaic::tableNextRow(ui, Mosaic::Key(rect.index));
                (void)Mosaic::tableSetColumn(ui, 0);
                Mosaic::Response item = Mosaic::selectable(ui, Mosaic::Key("rect"), ExamplesDetail::number(rect.index), rect.index == m_fontHoveredRect);

                if(item.hovered() == true)
                {
                    m_fontHoveredRect = rect.index;
                }

                (void)Mosaic::tableSetColumn(ui, 1);
                Mosaic::text(ui, rect.page == std::numeric_limits<size_t>::max() ? "-" : ExamplesDetail::number(rect.page));
                (void)Mosaic::tableSetColumn(ui, 2);
                Mosaic::text(ui, ExamplesDetail::number(rect.glyph));
                (void)Mosaic::tableSetColumn(ui, 3);
                Mosaic::text(ui, ExamplesDetail::fixed(rect.size, 2));
                (void)Mosaic::tableSetColumn(ui, 4);
                Mosaic::String bounds = ExamplesDetail::fixed(rect.bounds.x, 0);
                bounds += ",";
                bounds += ExamplesDetail::fixed(rect.bounds.y, 0);
                bounds += " ";
                bounds += ExamplesDetail::fixed(rect.bounds.width, 0);
                bounds += "x";
                bounds += ExamplesDetail::fixed(rect.bounds.height, 0);
                Mosaic::text(ui, bounds);
                (void)Mosaic::tableSetColumn(ui, 5);
                Mosaic::text(ui, rect.packed ? "Packed" : "Discarded");
            }
        }

    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::addConsoleLog(Mosaic::String value)
    {
        m_consoleItems.emplace_back(std::move(value));
        m_consoleVisibleDirty = true;
        // Layout learns the new scroll range one frame after the command adds a line.
        // Request the bottom twice so the console reaches the real range, not the stale one.
        m_consoleScrollToBottomFrames = 1;
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::executeConsoleCommand(Mosaic::StringView command)
    {
        while(command.empty() == false && std::isspace(static_cast<unsigned char>(command.front())) != 0)
        {
            command.remove_prefix(1);
        }
        while(command.empty() == false && std::isspace(static_cast<unsigned char>(command.back())) != 0)
        {
            command.remove_suffix(1);
        }

        if(command.empty() == true)
        {
            return;
        }

        Mosaic::String echoed = "# ";
        echoed += command;
        HelloDemo::addConsoleLog(std::move(echoed));

        for(size_t index = 0; index != m_consoleHistory.size(); ++index)
        {
            if(ExamplesDetail::containsIgnoreCase(m_consoleHistory[index], command) == true && m_consoleHistory[index].size() == command.size())
            {
                m_consoleHistory.erase(m_consoleHistory.begin() + static_cast<std::ptrdiff_t>(index));
                break;
            }
        }
        m_consoleHistory.emplace_back(command);
        m_consoleHistoryPosition = -1;

        auto equals = [command](Mosaic::StringView expected) noexcept
        {
            auto returnedValue = command.size() == expected.size() && ExamplesDetail::containsIgnoreCase(command, expected);

            return returnedValue;
        };

        if(equals("CLEAR"))
        {
            m_consoleItems.clear();
            m_consoleVisibleDirty = true;
        }
        else if(equals("HELP"))
        {
            HelloDemo::addConsoleLog("Commands:");
            HelloDemo::addConsoleLog("- HELP");
            HelloDemo::addConsoleLog("- HISTORY");
            HelloDemo::addConsoleLog("- CLEAR");
            HelloDemo::addConsoleLog("- CLASSIFY");
        }
        else if(equals("HISTORY"))
        {
            size_t begin = m_consoleHistory.size() > 10 ? m_consoleHistory.size() - 10 : 0;
            for(size_t index = begin; index != m_consoleHistory.size(); ++index)
            {
                Mosaic::String line = ExamplesDetail::number(index);
                line += ": ";
                line += m_consoleHistory[index];
                HelloDemo::addConsoleLog(std::move(line));
            }
        }
        else if(equals("CLASSIFY"))
        {
            HelloDemo::addConsoleLog("This is a command.");
        }
        else
        {
            Mosaic::String line = "Unknown command: '";
            line += command;
            line += "'";
            HelloDemo::addConsoleLog(std::move(line));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::consoleInputCallback(Mosaic::TextInputCallbackData & data)
    {
        auto * demo = static_cast<HelloDemo *>(data.userData);

        if(demo == nullptr)
        {
            return;
        }

        if(data.value == nullptr)
        {
            return;
        }

        bool historyNavigation = data.event == Mosaic::TextInputCallbackEvent::HistoryPrevious;

        if(data.event == Mosaic::TextInputCallbackEvent::HistoryNext)
        {
            historyNavigation = true;
        }

        if(historyNavigation == true)
        {
            int previous = demo->m_consoleHistoryPosition;

            if(data.event == Mosaic::TextInputCallbackEvent::HistoryPrevious)
            {
                if(demo->m_consoleHistoryPosition < 0)
                {
                    demo->m_consoleHistoryPosition = static_cast<int>(demo->m_consoleHistory.size()) - 1;
                }
                else if(demo->m_consoleHistoryPosition > 0)
                {
                    --demo->m_consoleHistoryPosition;
                }
            }
            else if(demo->m_consoleHistoryPosition >= 0)
            {
                ++demo->m_consoleHistoryPosition;

                if(demo->m_consoleHistoryPosition >= static_cast<int>(demo->m_consoleHistory.size()))
                {
                    demo->m_consoleHistoryPosition = -1;
                }
            }

            if(previous != demo->m_consoleHistoryPosition)
            {
                *data.value = demo->m_consoleHistoryPosition < 0 ? Mosaic::String{} : demo->m_consoleHistory[static_cast<size_t>(demo->m_consoleHistoryPosition)];
                data.cursor = data.value->size();
                data.anchor = data.cursor;
                data.changed = true;
            }

            return;
        }

        if(data.event != Mosaic::TextInputCallbackEvent::Completion)
        {
            return;
        }

        size_t cursor = std::min(data.cursor, data.value->size());
        size_t wordBegin = cursor;
        while(wordBegin > 0 && std::isspace(static_cast<unsigned char>((*data.value)[wordBegin - 1])) == 0)
        {
            --wordBegin;
        }
        Mosaic::StringView prefix(data.value->data() + wordBegin, cursor - wordBegin);
        constexpr Mosaic::Array<Mosaic::StringView, 4> commands = {"HELP", "HISTORY", "CLEAR", "CLASSIFY"};
        Mosaic::SizeVector matches;
        for(size_t index = 0; index != commands.size(); ++index)
        {
            if(prefix.size() <= commands[index].size() && ExamplesDetail::containsIgnoreCase(commands[index].substr(0, prefix.size()), prefix) == true)
            {
                matches.push_back(index);
            }
        }

        if(matches.empty() == true)
        {
            Mosaic::String line = "No match for \"";
            line += prefix;
            line += "\"!";
            demo->addConsoleLog(std::move(line));

            return;
        }

        if(matches.size() == 1)
        {
            Mosaic::StringView match = commands[matches.front()];
            data.value->replace(wordBegin, cursor - wordBegin, match);
            data.value->insert(wordBegin + match.size(), " ");
            data.cursor = wordBegin + match.size() + 1;
            data.anchor = data.cursor;
            data.changed = true;

            return;
        }

        size_t commonLength = prefix.size();
        bool common = true;
        while(common == true)
        {
            if(commonLength >= commands[matches.front()].size())
            {
                break;
            }

            char candidate = commands[matches.front()][commonLength];
            for(size_t match : matches)
            {
                if(commonLength >= commands[match].size())
                {
                    common = false;
                    break;
                }

                if(std::tolower(static_cast<unsigned char>(commands[match][commonLength])) != std::tolower(static_cast<unsigned char>(candidate)))
                {
                    common = false;
                    break;
                }
            }

            if(common == true)
            {
                ++commonLength;
            }
        }

        if(commonLength > prefix.size())
        {
            Mosaic::StringView commonPrefix = commands[matches.front()].substr(0, commonLength);
            data.value->replace(wordBegin, cursor - wordBegin, commonPrefix);
            data.cursor = wordBegin + commonLength;
            data.anchor = data.cursor;
            data.changed = true;
        }

        demo->addConsoleLog("Possible matches:");
        for(size_t match : matches)
        {
            Mosaic::String line = "- ";
            line += commands[match];
            demo->addConsoleLog(std::move(line));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawImageViewerContents(Mosaic::Context * ui)
    {
        {
            auto controls = Mosaic::row(ui);
            Mosaic::checkbox(ui, "Grid", &m_imageViewerGrid);
            float zoomPercent = m_imageViewerZoom * 100.f;
            Mosaic::SliderOptions zoomOptions;
            zoomOptions.minimum = 100.0;
            zoomOptions.maximum = 1000000.0;
            zoomOptions.dragSpeed = 5.0;
            zoomOptions.format = "%.0f%%";
            zoomOptions.width = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.fontSize * 10.f);
            auto zoomScope = Mosaic::scope(ui, Mosaic::Key("image viewer zoom"));
            Mosaic::Response zoom = Mosaic::dragValue(ui, {}, &zoomPercent, zoomOptions);

            if(zoom.changed() == true)
            {
                m_imageViewerZoom = zoomPercent / 100.f;
            }
        }

        Mosaic::FontAtlasPage atlas;
        bool hasAtlas = Mosaic::fontAtlasPage(ui, 0, &atlas) == true && atlas.texture != 0 && atlas.size.x > 0.f && atlas.size.y > 0.f;
        float sourceWidth = hasAtlas == true ? atlas.size.x : 320.f;
        float sourceHeight = hasAtlas == true ? atlas.size.y : 220.f;
        Mosaic::LayoutOptions canvasLayout;
        canvasLayout.width = Mosaic::SizeRule::Fill;
        canvasLayout.height = Mosaic::SizeRule::Fill;

        if(m_imageViewerViewReset == true)
        {
            canvasLayout.minimum = {sourceWidth * 3.f, sourceHeight * 4.f};
        }

        Mosaic::Canvas canvas = Mosaic::canvas(ui, "Image viewer canvas", canvasLayout);
        Mosaic::Rect bounds;

        if(canvas.contentRect(&bounds) == false)
        {
            return;
        }

        if(m_imageViewerViewReset == true)
        {
            m_imageViewerPan.x = bounds.width * 0.5f / m_imageViewerZoom - 0.5f;
            m_imageViewerPan.y = bounds.height * 0.5f / m_imageViewerZoom - 0.5f;
            m_imageViewerViewReset = false;
        }

        Mosaic::Vec2 wheel;

        if(Mosaic::consumeWheel(ui, canvas.id(), &wheel) == true && wheel.y != 0.f)
        {
            float factor = std::max(0.01f, 1.f + wheel.y * 0.1f);
            m_imageViewerZoom = std::clamp(m_imageViewerZoom * factor, 1.f, 10000.f);
        }

        const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
        Mosaic::Rect screenBounds;
        bool hasScreenBounds = Mosaic::debugBounds(ui, canvas.id(), &screenBounds);
        bool hovered = pointer != nullptr && hasScreenBounds == true && screenBounds.contains(pointer->position);

        if(pointer != nullptr && hovered == true && pointer->isPressed(Mosaic::PointerButton::Primary) == true)
        {
            m_imageViewerPanning = true;
            m_imageViewerPanStart = m_imageViewerPan;
        }

        if(pointer != nullptr && m_imageViewerPanning == true)
        {
            if(pointer->isDown(Mosaic::PointerButton::Primary) == true)
            {
                Mosaic::Vec2 drag = pointer->position - pointer->pressPosition(Mosaic::PointerButton::Primary);
                m_imageViewerPan = m_imageViewerPanStart - drag * (1.f / m_imageViewerZoom);
            }
            else
            {
                m_imageViewerPanning = false;
            }
        }

        canvas.rect(bounds, Mosaic::Color::fromBytes(24, 27, 31));
        float imageWidth = sourceWidth * m_imageViewerZoom;
        float imageHeight = sourceHeight * m_imageViewerZoom;
        float imageX = std::floor(bounds.x - m_imageViewerPan.x * m_imageViewerZoom + bounds.width * 0.5f);
        float imageY = std::floor(bounds.y - m_imageViewerPan.y * m_imageViewerZoom + bounds.height * 0.5f);
        Mosaic::Rect imageBounds = {imageX, imageY, imageWidth, imageHeight};
        canvas.rect(imageBounds, Mosaic::Color::fromBytes(100, 100, 100));

        if(hasAtlas == true)
        {
            canvas.image(atlas.texture, imageBounds, {0.f, 0.f, 1.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, Mosaic::SamplerFilter::Nearest);
        }

        if(m_imageViewerGrid == true && m_imageViewerZoom > 6.f)
        {
            int32_t firstColumn = static_cast<int32_t>((bounds.x - imageBounds.x) / m_imageViewerZoom);
            int32_t lastColumn = static_cast<int32_t>((bounds.right() - imageBounds.x) / m_imageViewerZoom);
            int32_t firstRow = static_cast<int32_t>((bounds.y - imageBounds.y) / m_imageViewerZoom);
            int32_t lastRow = static_cast<int32_t>((bounds.bottom() - imageBounds.y) / m_imageViewerZoom);
            Mosaic::Color gridColor = Mosaic::Color::fromBytes(255, 255, 255, 100);

            for(int32_t column = firstColumn; column <= lastColumn; ++column)
            {
                float x = imageBounds.x + static_cast<float>(column) * m_imageViewerZoom;
                canvas.line({x, bounds.y}, {x, bounds.bottom()}, 1.f, gridColor);
            }

            for(int32_t row = firstRow; row <= lastRow; ++row)
            {
                float y = imageBounds.y + static_cast<float>(row) * m_imageViewerZoom;
                canvas.line({bounds.x, y}, {bounds.right(), y}, 1.f, gridColor);
            }
        }

        Mosaic::Color border = Mosaic::Color::fromBytes(255, 255, 255);
        canvas.line({bounds.x, bounds.y}, {bounds.right(), bounds.y}, 1.f, border);
        canvas.line({bounds.right(), bounds.y}, {bounds.right(), bounds.bottom()}, 1.f, border);
        canvas.line({bounds.right(), bounds.bottom()}, {bounds.x, bounds.bottom()}, 1.f, border);
        canvas.line({bounds.x, bounds.bottom()}, {bounds.x, bounds.y}, 1.f, border);
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::initializePropertyEditor()
    {
        if(m_propertyNodes.empty() == false)
        {
            return;
        }

        constexpr size_t rootCount = 20;
        constexpr Mosaic::Array<Mosaic::StringView, 10> categories = {"Apple", "Banana", "Cherry", "Kiwi", "Mango", "Orange", "Pear", "Pineapple", "Strawberry", "Watermelon"};
        uint32_t nextUid = 1;
        PropertyDemoNode treeRoot;
        treeRoot.uid = nextUid++;
        treeRoot.expanded = true;
        treeRoot.name = "Root";
        m_propertyNodes.push_back(std::move(treeRoot));

        for(size_t root = 0; root != rootCount; ++root)
        {
            size_t rootIndex = m_propertyNodes.size();
            PropertyDemoNode rootNode;
            rootNode.parent = 0;
            rootNode.uid = nextUid++;
            rootNode.expanded = root < 2;
            rootNode.name = categories[root / 2];
            rootNode.name += " ";
            rootNode.name += ExamplesDetail::number(root % 2);
            m_propertyNodes.push_back(std::move(rootNode));

            size_t childCount = m_propertyNodes[rootIndex].name.size();
            for(size_t child = 0; child != childCount; ++child)
            {
                size_t childIndex = m_propertyNodes.size();
                PropertyDemoNode childNode;
                childNode.parent = rootIndex;
                childNode.uid = nextUid++;
                childNode.depth = 1;
                childNode.expanded = root == 0 && child == 0;
                childNode.hasData = true;
                childNode.name = "Child ";
                childNode.name += ExamplesDetail::number(child);
                childNode.integers = {128, static_cast<int32_t>(child), static_cast<int32_t>(root + child), static_cast<int32_t>(root * childCount + child)};
                childNode.scalars = {0.f, 3.141592f, 0.75f};
                m_propertyNodes.push_back(std::move(childNode));

                if(child == 0)
                {
                    PropertyDemoNode leafNode;
                    leafNode.parent = childIndex;
                    leafNode.uid = nextUid++;
                    leafNode.depth = 2;
                    leafNode.hasData = true;
                    leafNode.name = "Sub-child 0";
                    leafNode.integers = {128, 0, static_cast<int32_t>(root), static_cast<int32_t>(root * 100)};
                    leafNode.scalars = {0.f, 3.141592f, 0.75f};
                    m_propertyNodes.push_back(std::move(leafNode));
                }
            }
        }

        m_propertyOrder.reserve(m_propertyNodes.size());
        for(const PropertyDemoNode & node : m_propertyNodes)
        {
            m_propertyOrder.push_back(Mosaic::combineId(Mosaic::hashBytes("Property editor node"), node.uid));
        }

        if(m_propertyOrder.size() > 1)
        {
            m_propertySelection.select(m_propertyOrder[1]);
            m_propertySelectedNode = 1;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::appendVisiblePropertyNode(size_t index)
    {
        if(index >= m_propertyNodes.size())
        {
            return;
        }

        m_propertyVisibleNodes.push_back(index);

        if(m_propertyNodes[index].expanded == false)
        {
            return;
        }

        for(size_t child = index + 1; child != m_propertyNodes.size(); ++child)
        {
            if(m_propertyNodes[child].parent == index)
            {
                HelloDemo::appendVisiblePropertyNode(child);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawPropertyNode(Mosaic::Context * ui, size_t index)
    {
        if(index >= m_propertyNodes.size())
        {
            return;
        }

        PropertyDemoNode & node = m_propertyNodes[index];
        bool hasChildren = std::find_if(m_propertyNodes.begin(), m_propertyNodes.end(),
                                              [index](const PropertyDemoNode & candidate)
                                              {
                                                  return candidate.parent == index;
                                              }) != m_propertyNodes.end();
        Mosaic::TreeNodeOptions options;
        options.defaultExpanded = node.expanded;
        options.leaf = hasChildren == false;
        options.bullet = hasChildren == false;
        options.openOnArrow = true;
        options.openOnDoubleClick = true;
        options.spanAvailableWidth = true;
        options.navigationLeftJumpsToParent = true;
        options.lines = Mosaic::TreeLineMode::ToNodes;
        Mosaic::TreeScope tree = Mosaic::treeNode(ui, Mosaic::Key(node.uid), node.name, &m_propertySelection, m_propertyOrder[index], m_propertyOrder, options);
        Mosaic::Response response;
        if(Mosaic::itemResponse(ui, tree.id(), &response) == false)
        {
            return;
        }

        if(response.clicked() == true || response.focused() == true)
        {
            m_propertySelectedNode = index;
        }

        node.expanded = tree.expanded();

        if(tree.expanded() == false)
        {
            return;
        }

        for(size_t child = index + 1; child != m_propertyNodes.size(); ++child)
        {
            if(m_propertyNodes[child].parent == index)
            {
                HelloDemo::drawPropertyNode(ui, child);
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawExampleWindows(Mosaic::Context * ui)
    {
        Mosaic::Viewport viewport;
        if(Mosaic::currentViewport(ui, &viewport) == false)
        {
            return;
        }

        Mosaic::Rect documentDockBounds;
        const Mosaic::Array<Mosaic::String, 6> & documentNames = m_documentNames;
        auto requestDocumentClose = [this](size_t index)
        {
            if(index >= m_documentOpen.size())
            {
                return;
            }

            if(m_documentDirty[index] == false)
            {
                m_documentOpen[index] = false;
                m_documentTabClosed = static_cast<int>(index);

                return;
            }

            m_documentOpen[index] = true;
            auto queued = std::find(m_documentCloseQueue.begin(), m_documentCloseQueue.end(), index);

            if(queued == m_documentCloseQueue.end())
            {
                m_documentCloseQueue.push_back(index);
            }

            if(m_documentClosePending < 0 && m_documentCloseQueue.empty() == false)
            {
                m_documentClosePending = static_cast<int>(m_documentCloseQueue.front());
            }
        };
        auto requestDocumentRename = [this, ui](size_t index, Mosaic::Id owner)
        {
            if(index >= m_documentNames.size())
            {
                return;
            }

            m_documentRenamePending = static_cast<int>(index);
            m_documentRenameValue = m_documentNames[index];
            m_documentRenameOwner = owner;
            m_documentRenameAnchor = {};
            (void)Mosaic::debugBounds(ui, owner, &m_documentRenameAnchor);
            Mosaic::PopupOptions popupOptions;
            popupOptions.owner = owner;
            popupOptions.anchor = m_documentRenameAnchor;
            popupOptions.placement = Mosaic::PopupPlacement::Automatic;
            popupOptions.minimumSize = {390.f, 0.f};
            popupOptions.maximumSize = {390.f, 240.f};
            popupOptions.openOverExisting = true;
            Mosaic::openPopup(ui, Mosaic::Key("document rename"), popupOptions);
        };
        auto drawDocumentContents = [this, ui, &requestDocumentClose, &requestDocumentRename](size_t index)
        {
            Mosaic::String title = "Document \"";
            title += m_documentNames[index];
            title += "\"";
            Mosaic::text(ui, title);
            Mosaic::TextOptions paragraph;
            paragraph.wordWrap = true;
            paragraph.layout.width = Mosaic::SizeRule::Fill;
            Mosaic::Theme documentTheme = Mosaic::getTheme(ui);
            documentTheme.colors.text = m_documentColors[index];
            {
                auto colorText = Mosaic::styleScope(ui, documentTheme);
                Mosaic::text(ui, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.", paragraph);
            }
            auto actions = Mosaic::row(ui);
            Mosaic::Response renameButton = Mosaic::button(ui, "Rename..");
            bool renameRequested = renameButton.clicked();
            bool modifyRequested = Mosaic::button(ui, "Modify").clicked();
            bool saveRequested = Mosaic::button(ui, "Save").clicked();
            bool closeRequested = Mosaic::button(ui, "Close").clicked();
            Mosaic::colorEditorRgb(ui, "color", &m_documentColors[index]);

            Mosaic::ShortcutOptions shortcutOptions;
            shortcutOptions.route = Mosaic::ShortcutRoute::Focused;
            renameRequested = Mosaic::shortcut(ui, Mosaic::Shortcut::primary(Mosaic::KeyCode::R), shortcutOptions) || renameRequested;
            modifyRequested = Mosaic::shortcut(ui, Mosaic::Shortcut::primary(Mosaic::KeyCode::M), shortcutOptions) || modifyRequested;
            saveRequested = Mosaic::shortcut(ui, Mosaic::Shortcut::primary(Mosaic::KeyCode::S), shortcutOptions) || saveRequested;
            closeRequested = Mosaic::shortcut(ui, Mosaic::Shortcut::primary(Mosaic::KeyCode::W), shortcutOptions) || closeRequested;

            if(modifyRequested == true)
            {
                m_documentDirty[index] = true;
            }

            if(renameRequested == true)
            {
                requestDocumentRename(index, renameButton.id);
            }

            if(saveRequested == true)
            {
                m_documentDirty[index] = false;
            }

            if(closeRequested == true)
            {
                requestDocumentClose(index);
            }
        };

        if(m_showMainMenuBar == true)
        {
            auto mainMenu = Mosaic::mainMenuBar(ui);

            if(mainMenu.visible() == true)
            {
                {
                    auto file = Mosaic::menu(ui, "File");

                    if(file.expanded() == true)
                    {
                        HelloDemo::drawExampleMenuFile(ui);
                    }
                }
                {
                    auto edit = Mosaic::menu(ui, "Edit");

                    if(edit.expanded() == true)
                    {
                        Mosaic::MenuItemOptions undo;
                        undo.shortcut = "Cmd+Z";
                        Mosaic::menuItem(ui, "Undo", undo);
                        Mosaic::MenuItemOptions redo;
                        redo.shortcut = "Cmd+Y";
                        redo.enabled = false;
                        Mosaic::menuItem(ui, "Redo", redo);
                        Mosaic::separator(ui);
                        Mosaic::MenuItemOptions cut;
                        cut.shortcut = "Cmd+X";
                        Mosaic::menuItem(ui, "Cut", cut);
                        Mosaic::MenuItemOptions copy;
                        copy.shortcut = "Cmd+C";
                        Mosaic::menuItem(ui, "Copy", copy);
                        Mosaic::MenuItemOptions paste;
                        paste.shortcut = "Cmd+V";
                        Mosaic::menuItem(ui, "Paste", paste);
                    }
                }
            }
        }

        if(m_showAssetsBrowser == true)
        {
            constexpr Mosaic::Id assetIdSeed = 0xb2d50bf5183e2d87ULL;
            while(m_assetItems.size() < m_assetCount)
            {
                size_t serial = m_assetNextSerial++;
                m_assetItems.push_back(serial);
                m_assetOrderDirty = true;
            }

            if(m_assetItems.size() > m_assetCount)
            {
                m_assetItems.resize(m_assetCount);
                m_assetOrderDirty = true;
            }

            Mosaic::WindowOptions options;
            options.open = &m_showAssetsBrowser;
            Mosaic::setNextWindowSize(ui, {m_assetIconSize * 25.f, m_assetIconSize * 15.f}, Mosaic::Condition::FirstUse);
            auto window = Mosaic::window(ui, "Example: Assets Browser", options);

            if(window.visible() == true)
            {
                auto deleteAssetSelection = [this]()
                {
                    std::erase_if(m_assetItems,
                                  [this](size_t serial)
                                  {
                                      auto returnedValue = m_assetSelection.selected(Mosaic::combineId(assetIdSeed, serial));

                                      return returnedValue;
                                  });
                    m_assetSelection.clear();
                    m_assetCount = m_assetItems.size();
                    m_assetOrderDirty = true;
                };
                Mosaic::ShortcutOptions deleteShortcutOptions;
                deleteShortcutOptions.route = Mosaic::ShortcutRoute::Focused;
                deleteShortcutOptions.repeat = true;
                bool deleteRequested = m_assetSelection.empty() == false && Mosaic::shortcut(ui, {Mosaic::KeyCode::Delete, {}}, deleteShortcutOptions);
                {
                    auto bar = Mosaic::menuBar(ui);
                    {
                        auto file = Mosaic::menu(ui, "File");

                        if(file.expanded() == true)
                        {
                            if(Mosaic::menuItem(ui, "Add 10000 items").clicked() == true)
                            {
                                m_assetCount += 10000;
                            }

                            if(Mosaic::menuItem(ui, "Clear items").clicked() == true)
                            {
                                m_assetCount = 0;
                                m_assetItems.clear();
                                m_assetSelection.clear();
                                m_assetOrderDirty = true;
                            }

                            Mosaic::separator(ui);
                            Mosaic::MenuItemOptions close;
                            close.enabled = true;

                            if(Mosaic::menuItem(ui, "Close", close).clicked() == true)
                            {
                                m_showAssetsBrowser = false;
                            }
                        }
                    }
                    {
                        auto edit = Mosaic::menu(ui, "Edit");

                        if(edit.expanded() == true)
                        {
                            Mosaic::MenuItemOptions remove;
                            remove.shortcut = "Del";
                            remove.enabled = m_assetSelection.empty() == false;

                            if(Mosaic::menuItem(ui, "Delete", remove).clicked() == true)
                            {
                                deleteRequested = true;
                            }
                        }
                    }

                    {
                        auto menu = Mosaic::menu(ui, "Options");

                        if(menu.expanded() == true)
                        {
                            Mosaic::separatorText(ui, "Contents");
                            Mosaic::MenuItemOptions showTypes;
                            showTypes.checked = &m_assetShowTypeOverlay;
                            Mosaic::menuItem(ui, "Show Type Overlay", showTypes);
                            Mosaic::MenuItemOptions sorting;
                            sorting.checked = &m_assetAllowSorting;

                            if(Mosaic::menuItem(ui, "Allow Sorting", sorting).changed() == true)
                            {
                                m_assetOrderDirty = true;
                            }

                            Mosaic::separatorText(ui, "Selection Behavior");
                            Mosaic::MenuItemOptions boxSelect;
                            boxSelect.checked = &m_assetAllowBoxSelect;
                            Mosaic::menuItem(ui, "Allow box-selection", boxSelect);
                            Mosaic::MenuItemOptions boxInside;
                            boxInside.checked = &m_assetAllowBoxSelectInsideSelection;

                            if(Mosaic::menuItem(ui, "Allow box-selection from selected items", boxInside).changed() == true && m_assetAllowBoxSelectInsideSelection == true)
                            {
                                m_assetAllowDragUnselected = false;
                            }

                            Mosaic::MenuItemOptions dragUnselected;
                            dragUnselected.checked = &m_assetAllowDragUnselected;

                            if(Mosaic::menuItem(ui, "Allow dragging unselected item", dragUnselected).changed() == true && m_assetAllowDragUnselected == true)
                            {
                                m_assetAllowBoxSelectInsideSelection = false;
                            }

                            Mosaic::separatorText(ui, "Layout");
                            Mosaic::slider(ui, "Icon Size", &m_assetIconSize, 16.f, 128.f);
                            Mosaic::slider(ui, "Icon Spacing", &m_assetIconSpacing, int32_t{0}, int32_t{32});
                            Mosaic::slider(ui, "Icon Hit Spacing", &m_assetIconHitSpacing, int32_t{0}, int32_t{32});
                            Mosaic::MenuItemOptions stretch;
                            stretch.checked = &m_assetStretchSpacing;
                            Mosaic::menuItem(ui, "Stretch Spacing", stretch);
                            Mosaic::MenuItemOptions scrollX;
                            scrollX.checked = &m_assetUseScrollX;
                            Mosaic::menuItem(ui, "Use ScrollX", scrollX);
                        }
                    }
                }

                if(deleteRequested == true)
                {
                    deleteAssetSelection();
                }

                if(m_assetAllowSorting == true)
                {
                    Mosaic::TableOptions sortingOptions;
                    sortingOptions.sortable = true;
                    sortingOptions.multiSort = true;
                    sortingOptions.bordersInnerHorizontal = true;
                    sortingOptions.bordersInnerVertical = true;
                    sortingOptions.bordersOuterHorizontal = true;
                    sortingOptions.bordersOuterVertical = true;
                    Mosaic::LayoutOptions sortingLayout;
                    sortingLayout.width = Mosaic::SizeRule::Fill;
                    sortingLayout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.controlHeight);
                    auto sortingTable = Mosaic::table(ui, "for_sort_specs_only", 2, sortingOptions, sortingLayout);
                    Mosaic::TableColumnOptions indexColumn;
                    indexColumn.userId = Mosaic::Key("asset index").value();
                    indexColumn.defaultSort = true;
                    Mosaic::tableSetupColumn(ui, 0, "Index", indexColumn);
                    Mosaic::TableColumnOptions typeColumn;
                    typeColumn.userId = Mosaic::Key("asset type").value();
                    Mosaic::tableSetupColumn(ui, 1, "Type", typeColumn);
                    Mosaic::tableHeadersRow(ui);
                    Mosaic::TableSortState sortState;

                    if(Mosaic::tableSortState(ui, sortingTable.id(), &sortState) == true && (sortState.dirty == true || m_assetOrderDirty == true))
                    {
                        Mosaic::TableSortSpecVector specifications(sortState.specifications.begin(), sortState.specifications.end());
                        std::stable_sort(m_assetItems.begin(), m_assetItems.end(),
                                         [&specifications](size_t first, size_t second)
                                         {
                                             for(const Mosaic::TableSortSpec & specification : specifications)
                                             {
                                                 int comparison = 0;

                                                 if(specification.userId == Mosaic::Key("asset index").value())
                                                 {
                                                     comparison = first < second ? -1 : first > second ? 1 : 0;
                                                 }
                                                 else if(specification.userId == Mosaic::Key("asset type").value())
                                                 {
                                                     int firstType = first % 20 < 15 ? 0 : first % 20 < 18 ? 1 : 2;
                                                     int secondType = second % 20 < 15 ? 0 : second % 20 < 18 ? 1 : 2;
                                                     comparison = firstType < secondType ? -1 : firstType > secondType ? 1 : 0;
                                                 }

                                                 if(comparison == 0)
                                                 {
                                                     continue;
                                                 }

                                                 return specification.direction == Mosaic::SortDirection::Ascending ? comparison < 0 : comparison > 0;
                                             }

                                             return first < second;
                                         });
                        Mosaic::tableSortSpecsHandled(ui, sortingTable.id());
                        m_assetOrderDirty = true;
                    }
                }
                {
                        Mosaic::ScrollOptions scrollOptions;
                        scrollOptions.axes = m_assetUseScrollX ? Mosaic::ScrollAxes::Both : Mosaic::ScrollAxes::Vertical;
                        Mosaic::LayoutOptions browserLayout;
                        browserLayout.width = Mosaic::SizeRule::Fill;
                        browserLayout.height = Mosaic::Dimension::fixed(350.f);
                        browserLayout.gap = 0.f;
                        auto browser = Mosaic::scrollArea(ui, "Assets view", scrollOptions, browserLayout);

                        if(m_assetZoomScrollPending == true)
                        {
                            Mosaic::scrollTo(ui, browser.id(), m_assetZoomScrollTarget);
                            m_assetZoomScrollPending = false;
                        }

                        if(m_assetOrderDirty == true)
                        {
                            m_assetVisibleItems.clear();
                            m_assetVisibleItems.reserve(m_assetItems.size());
                            m_assetVisibleOrder.clear();
                            m_assetVisibleOrder.reserve(m_assetItems.size());
                            for(size_t serial : m_assetItems)
                            {
                                Mosaic::Id assetId = Mosaic::combineId(assetIdSeed, serial);
                                m_assetVisibleItems.push_back(serial);
                                m_assetVisibleOrder.push_back(assetId);
                            }
                            m_assetOrderDirty = false;
                        }

                        Mosaic::Rect browserBounds;
                        if(Mosaic::debugBounds(ui, browser.id(), &browserBounds) == false)
                        {
                            browserBounds = {viewport.bounds.x, viewport.bounds.y, 1.f, 1.f};
                        }

                        float browserWidth = std::max(1.f, browserBounds.width);
                        float previousItemExtent = std::max(24.f, m_assetIconSize + static_cast<float>(m_assetIconSpacing));
                        uint32_t previousColumns = std::max(uint32_t{1}, static_cast<uint32_t>(browserWidth / previousItemExtent));
                        float previousRowHeight = m_assetIconSize + static_cast<float>(m_assetIconSpacing);
                        const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();

                        if(pointer != nullptr && pointer->isPressed(Mosaic::PointerButton::Secondary) == true && browserBounds.contains(pointer->position) == true)
                        {
                            Mosaic::PopupOptions contextOptions;
                            contextOptions.owner = browser.id();
                            contextOptions.placement = Mosaic::PopupPlacement::Cursor;
                            Mosaic::openPopup(ui, Mosaic::Key("Assets browser context"), contextOptions);
                        }

                        {
                            Mosaic::PopupOptions contextOptions;
                            contextOptions.owner = browser.id();
                            contextOptions.placement = Mosaic::PopupPlacement::Cursor;
                            auto browserContext = Mosaic::popup(ui, Mosaic::Key("Assets browser context"), contextOptions);

                            if(browserContext.visible() == true)
                            {
                                Mosaic::String selectionText = "Selection: ";
                                selectionText += ExamplesDetail::number(m_assetSelection.size());
                                selectionText += " items";
                                Mosaic::text(ui, selectionText);
                                Mosaic::separator(ui);
                                Mosaic::MenuItemOptions remove;
                                remove.enabled = m_assetSelection.empty() == false;

                                if(Mosaic::menuItem(ui, "Delete", remove).clicked() == true)
                                {
                                    deleteAssetSelection();
                                }
                            }
                        }

                        if(Mosaic::input(ui).modifiers.primary == true && Mosaic::capturedPointerOwner(ui) == Mosaic::InvalidId)
                        {
                            Mosaic::Vec2 wheel;
                            if(Mosaic::consumeWheel(ui, browser.id(), &wheel) == true)
                            {
                                m_assetZoomWheelAccumulator += wheel.y;
                            }

                            if(std::abs(m_assetZoomWheelAccumulator) >= 1.f)
                            {
                                int32_t steps = static_cast<int32_t>(std::trunc(m_assetZoomWheelAccumulator));
                                m_assetZoomWheelAccumulator -= static_cast<float>(steps);
                                Mosaic::Vec2 previousScroll;
                                if(Mosaic::scrollOffset(ui, browser.id(), &previousScroll) == false)
                                {
                                    previousScroll = {};
                                }

                                float pointerLocalX = pointer == nullptr ? 0.f : pointer->position.x - browserBounds.x;
                                float pointerLocalY = pointer == nullptr ? 0.f : pointer->position.y - browserBounds.y;
                                float hoveredX = previousScroll.x + pointerLocalX;
                                float hoveredY = previousScroll.y + pointerLocalY;
                                size_t hoveredColumn = static_cast<size_t>(std::max(0.f, std::floor(hoveredX / previousItemExtent)));
                                size_t hoveredRow = static_cast<size_t>(std::max(0.f, std::floor(hoveredY / previousRowHeight)));
                                size_t hoveredItem = hoveredRow * previousColumns + std::min(hoveredColumn, static_cast<size_t>(previousColumns - 1));
                                float rowFraction = std::fmod(std::max(0.f, hoveredY), previousRowHeight) / previousRowHeight;
                                float columnFraction = std::fmod(std::max(0.f, hoveredX), previousItemExtent) / previousItemExtent;
                                m_assetIconSize = std::clamp(m_assetIconSize * std::pow(1.1f, static_cast<float>(steps)), 16.f, 128.f);
                                float nextItemExtent = std::max(24.f, m_assetIconSize + static_cast<float>(m_assetIconSpacing));
                                uint32_t nextColumns = std::max(uint32_t{1}, static_cast<uint32_t>(browserWidth / nextItemExtent));
                                float nextRowHeight = m_assetIconSize + static_cast<float>(m_assetIconSpacing);
                                size_t nextRow = hoveredItem / nextColumns;
                                size_t nextColumn = hoveredItem % nextColumns;
                                m_assetZoomScrollTarget = {static_cast<float>(nextColumn) * nextItemExtent + columnFraction * nextItemExtent - pointerLocalX, static_cast<float>(nextRow) * nextRowHeight + rowFraction * nextRowHeight - pointerLocalY};
                                m_assetZoomScrollPending = true;
                            }
                        }

                        float itemExtent = std::max(24.f, m_assetIconSize + static_cast<float>(m_assetIconSpacing));
                        uint32_t columns = std::max(uint32_t{1}, static_cast<uint32_t>(browserWidth / itemExtent));
                        size_t rowCount = (m_assetVisibleItems.size() + columns - 1) / columns;
                        float rowHeight = m_assetIconSize + static_cast<float>(m_assetIconSpacing);
                        Mosaic::ListClipperOptions clipperOptions;
                        clipperOptions.spacing = 0.f;
                        Mosaic::VisibleRange rows;
                        (void)Mosaic::beginListClipper(ui, rowCount, rowHeight, &rows, browser.id(), clipperOptions);
                        constexpr Mosaic::TypeId assetDragType = 0xa2c0f1d0b0912763ULL;
                        {
                            Mosaic::LayoutOptions gridLayout;
                            gridLayout.width = m_assetStretchSpacing ? Mosaic::Dimension(Mosaic::SizeRule::Fill) : Mosaic::Dimension::fixed(itemExtent * static_cast<float>(columns));
                            gridLayout.gap = static_cast<float>(m_assetIconSpacing);
                            auto grid = Mosaic::grid(ui, columns, gridLayout);
                            float gridGap = static_cast<float>(m_assetIconSpacing) * static_cast<float>(columns > 0 ? columns - 1 : 0);
                            float tileWidth = m_assetStretchSpacing == true ? std::max(24.f, (browserWidth - gridGap) / static_cast<float>(columns)) : itemExtent;
                            float tileHeight = m_assetIconSize;
                            size_t firstAsset = rows.begin * columns;
                            size_t lastAsset = std::min(m_assetVisibleItems.size(), rows.end * columns);
                            for(size_t index = firstAsset; index != lastAsset; ++index)
                            {
                                size_t serial = m_assetVisibleItems[index];
                                Mosaic::Id assetId = Mosaic::combineId(assetIdSeed, serial);
                                auto item = Mosaic::scope(ui, Mosaic::Key(serial));
                                Mosaic::LayoutOptions tileLayout;
                                tileLayout.width = Mosaic::Dimension::fixed(tileWidth);
                                tileLayout.height = Mosaic::Dimension::fixed(tileHeight);
                                auto tile = Mosaic::absolute(ui, tileLayout);
                                Mosaic::SelectionOptions selectionOptions;
                                float hitInset = std::clamp(static_cast<float>(m_assetIconHitSpacing) * 0.5f, 0.f, std::max(0.f, std::min(tileWidth, tileHeight) * 0.5f - 1.f));
                                selectionOptions.item.absoluteRect = {hitInset, hitInset, std::max(1.f, tileWidth - hitInset * 2.f), std::max(1.f, tileHeight - hitInset * 2.f)};
                                Mosaic::Response asset = Mosaic::selectable(ui, Mosaic::Key("asset"), {}, &m_assetSelection, assetId, m_assetVisibleOrder, selectionOptions);
                                Mosaic::LayoutOptions iconLayout;
                                iconLayout.absoluteRect = {0.f, 0.f, tileWidth, m_assetIconSize};
                                Mosaic::Canvas icon = Mosaic::canvas(ui, Mosaic::Key("asset icon"), {}, iconLayout);
                                Mosaic::Color iconBackground = Mosaic::Color::fromBytes(35, 35, 35, 220);
                                icon.rect({0.f, 0.f, tileWidth, m_assetIconSize}, iconBackground);
                                int type = serial % 20 < 15 ? 0 : serial % 20 < 18 ? 1 : 2;

                                if(m_assetShowTypeOverlay == true && type != 0)
                                {
                                    Mosaic::Color typeColor = type == 1 ? Mosaic::Color::fromBytes(200, 70, 70) : Mosaic::Color::fromBytes(70, 170, 70);
                                    icon.rect({std::max(0.f, tileWidth - 6.f), 2.f, 4.f, 4.f}, typeColor);
                                }

                                if(m_assetSelection.selected(assetId) == true || asset.hovered() == true)
                                {
                                    Mosaic::Color border = m_assetSelection.selected(assetId) == true ? Mosaic::getTheme(ui).colors.accent : Mosaic::getTheme(ui).colors.borderStrong;
                                    Mosaic::BoxStyle outline;
                                    outline.fill = Mosaic::solidFill({0.f, 0.f, 0.f, 0.f});
                                    outline.radii = {};
                                    outline.borderWidth = 2.f;
                                    outline.borderColor = border;
                                    icon.box({0.f, 0.f, tileWidth, m_assetIconSize}, outline);
                                }

                                if(m_assetIconSize >= Mosaic::getTheme(ui).metrics.fontSize * 3.f)
                                {
                                    Mosaic::Theme labelTheme = Mosaic::getTheme(ui);

                                    if(m_assetSelection.selected(assetId) == false)
                                    {
                                        labelTheme.colors.text = labelTheme.colors.textDisabled;
                                    }

                                    auto labelStyle = Mosaic::styleScope(ui, labelTheme);
                                    Mosaic::TextOptions nameOptions;
                                    nameOptions.layout.absoluteRect = {0.f, std::max(0.f, m_assetIconSize - Mosaic::getTheme(ui).metrics.lineHeight), tileWidth, Mosaic::getTheme(ui).metrics.lineHeight};
                                    Mosaic::text(ui, ExamplesDetail::number(serial), nameOptions);
                                }
                                bool dragAllowed = m_assetAllowDragUnselected || m_assetSelection.selected(assetId);

                                if(dragAllowed == true)
                                {
                                    Mosaic::IdSpan dragItems;

                                    if(m_assetSelection.selected(assetId) == true)
                                    {
                                        dragItems = m_assetSelection.values();
                                    }
                                    else
                                    {
                                        dragItems = Mosaic::IdSpan(&assetId, 1);
                                    }

                                    const auto * bytes = reinterpret_cast<const std::byte *>(dragItems.data());
                                    Mosaic::ByteSpan payload(bytes, dragItems.size_bytes());

                                    if(Mosaic::beginDragDropSource(ui, asset, assetDragType, payload) == true && Mosaic::dragDropSourcePreviewVisible(ui) == true)
                                    {
                                        Mosaic::String preview = "Moving ";
                                        preview += ExamplesDetail::number(dragItems.size());
                                        preview += dragItems.size() == 1 ? " asset" : " assets";
                                        Mosaic::text(ui, preview);

                                    }
                                }
                            }
                        }
                        Mosaic::BoxSelectionOptions boxOptions;
                        boxOptions.enabled = m_assetAllowBoxSelect;
                        boxOptions.clearOnClick = m_assetAllowBoxSelectInsideSelection == false;
                        boxOptions.allowFromSelectedItems = m_assetAllowBoxSelectInsideSelection;
                        Mosaic::boxSelect(ui, browser.id(), &m_assetSelection, boxOptions);
                        Mosaic::endListClipper(ui, rows, rowCount, rowHeight, clipperOptions);
                }

                Mosaic::String selected = "Selected: ";
                selected += ExamplesDetail::number(m_assetSelection.size());
                selected += "/";
                selected += ExamplesDetail::number(m_assetItems.size());
                selected += " items";
                Mosaic::text(ui, selected);
            }
        }

        if(m_showHorizontalContentsSizeWindow == true)
        {
            if(m_horizontalExplicitContentSize == true)
            {
                Mosaic::setNextWindowContentSize(ui, {m_horizontalContentsSize, 0.f});
            }

            Mosaic::WindowOptions options;
            options.open = &m_showHorizontalContentsSizeWindow;
            options.scrollable = true;
            options.scroll.axes = m_horizontalShowScrollbar ? Mosaic::ScrollAxes::Both : Mosaic::ScrollAxes::Vertical;
            auto window = Mosaic::window(ui, "Horizontal contents size demo window", options);

            if(window.visible() == true)
            {
                Mosaic::helpMarker(ui, "Test how different widgets react and grow the work rectangle when horizontal scrolling is enabled. Use Tools > Metrics/Debugger > Tools > Show windows rectangles to visualize the bounds.");
                Mosaic::checkbox(ui, "H-scrollbar", &m_horizontalShowScrollbar);
                Mosaic::checkbox(ui, "Button", &m_horizontalShowButton);
                Mosaic::checkbox(ui, "Tree nodes", &m_horizontalShowTreeNodes);
                Mosaic::checkbox(ui, "Text wrapped", &m_horizontalShowTextWrapped);
                Mosaic::checkbox(ui, "Columns", &m_horizontalShowColumns);
                Mosaic::checkbox(ui, "Tab bar", &m_horizontalShowTabBar);
                Mosaic::checkbox(ui, "Child", &m_horizontalShowChild);
                Mosaic::checkbox(ui, "Explicit content size", &m_horizontalExplicitContentSize);

                Mosaic::String position = "Scroll ";
                Mosaic::Vec2 offset;
                Mosaic::Vec2 range;
                if(Mosaic::scrollOffset(ui, window.id(), &offset) == false || Mosaic::scrollRange(ui, window.id(), &range) == false)
                {
                    offset = {};
                    range = {};
                }

                position += ExamplesDetail::fixed(offset.x, 1);
                position += "/";
                position += ExamplesDetail::fixed(range.x, 1);
                position += " ";
                position += ExamplesDetail::fixed(offset.y, 1);
                position += "/";
                position += ExamplesDetail::fixed(range.y, 1);
                Mosaic::text(ui, position);

                if(m_horizontalExplicitContentSize == true)
                {
                    Mosaic::SliderOptions sizeOptions;
                    sizeOptions.minimum = 100.0;
                    sizeOptions.maximum = 1200.0;
                    sizeOptions.step = 1.0;
                    auto valueScope = Mosaic::scope(ui, Mosaic::Key("horizontal contents size"));
                    Mosaic::dragValue(ui, {}, &m_horizontalContentsSize, sizeOptions);
                }

                Mosaic::separator(ui);

                if(m_horizontalShowButton == true)
                {
                    Mosaic::ButtonOptions buttonOptions;
                    buttonOptions.width = Mosaic::Dimension::fixed(300.f);
                    Mosaic::button(ui, Mosaic::Key("300-wide button"), "this is a 300-wide button", buttonOptions);
                }

                if(m_horizontalShowTreeNodes == true)
                {
                    auto tree = Mosaic::treeNode(ui, Mosaic::Key("wide tree"), "this is a tree node");

                    if(tree.expanded() == true)
                    {
                        auto nested = Mosaic::treeNode(ui, Mosaic::Key("wide nested tree"), "another one of those tree node...");

                        if(nested.expanded() == true)
                        {
                            Mosaic::text(ui, "Some tree contents");
                        }
                    }

                    static bool headerOpen = true;
                    auto header = Mosaic::collapsingHeader(ui, "CollapsingHeader", &headerOpen);

                    if(header.expanded() == true)
                    {
                        Mosaic::text(ui, "Some collapsing contents");
                    }
                }

                if(m_horizontalShowTextWrapped == true)
                {
                    Mosaic::TextOptions textOptions;
                    textOptions.wordWrap = true;
                    textOptions.layout.width = Mosaic::SizeRule::Fill;
                    Mosaic::text(ui, "This text should automatically wrap on the edge of the work rectangle.", textOptions);
                }

                if(m_horizontalShowColumns == true)
                {
                    Mosaic::text(ui, "Tables:");
                    Mosaic::TableOptions tableOptions;
                    tableOptions.resizable = false;
                    tableOptions.bordersInnerHorizontal = true;
                    tableOptions.bordersInnerVertical = true;
                    tableOptions.bordersOuterHorizontal = true;
                    tableOptions.bordersOuterVertical = true;
                    auto table = Mosaic::table(ui, "Horizontal table", 4, tableOptions);
                    for(uint32_t column = 0; column != 4; ++column)
                    {
                        Mosaic::tableSetupColumn(ui, column, {});
                    }
                    Mosaic::tableNextRow(ui, Mosaic::Key(0));
                    for(uint32_t column = 0; column != 4; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == true)
                        {
                            Mosaic::Vec2 available;
                            (void)Mosaic::contentRegionAvailable(ui, &available);
                            Mosaic::String width = "Width ";
                            width += ExamplesDetail::fixed(available.x, 2);
                            Mosaic::text(ui, width);
                        }
                    }

                    Mosaic::text(ui, "Columns:");
                    auto columns = Mosaic::columns(ui, "Horizontal legacy columns", 4);
                    for(uint32_t column = 0; column != 4; ++column)
                    {
                        Mosaic::String width = "Width ";
                        width += ExamplesDetail::fixed(Mosaic::columnWidth(ui), 2);
                        Mosaic::text(ui, width);
                        (void)Mosaic::nextColumn(ui);
                    }
                }

                if(m_horizontalShowTabBar == true)
                {
                    static int selectedTab = 0;
                    constexpr Mosaic::Array<Mosaic::StringView, 4> tabNames = {"OneOneOne", "TwoTwoTwo", "ThreeThreeThree", "FourFourFour"};
                    Mosaic::tabs(ui, "Horizontal content tabs", &selectedTab, tabNames);
                }

                if(m_horizontalShowChild == true)
                {
                    Mosaic::LayoutOptions childLayout;
                    childLayout.width = Mosaic::SizeRule::Fill;
                    childLayout.height = Mosaic::SizeRule::Fill;
                    Mosaic::ScrollOptions childOptions;
                    childOptions.framed = true;
                    auto child = Mosaic::scrollArea(ui, "child", childOptions, childLayout);
                }
            }
        }

        if(m_showConsole == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showConsole;
            options.collapsed = &m_consoleCollapsed;
            Mosaic::setNextWindowSize(ui, {520.f, 600.f}, Mosaic::Condition::FirstUse);
            auto console = Mosaic::window(ui, "Example: Console", options);

            if(console.visible() == true)
            {
                {
                    Mosaic::Response titleResponse;
                    (void)Mosaic::itemResponse(ui, console.id(), &titleResponse);
                    auto titleContext = Mosaic::contextPopup(ui, Mosaic::Key("console title context"), "Console title", titleResponse);

                    if(titleContext.visible() == true)
                    {
                        if(Mosaic::menuItem(ui, "Close Console").clicked() == true)
                        {
                            m_showConsole = false;
                            Mosaic::closeCurrentPopup(ui);
                        }
                    }
                }

                Mosaic::TextOptions introOptions;
                introOptions.wordWrap = true;
                introOptions.layout.width = Mosaic::SizeRule::Fill;
                Mosaic::text(ui, "This example implements a console with basic coloring, completion (Tab key) and history (Up/Down keys). A more elaborate implementation may want to store entries along with extra data such as timestamp, emitter, etc.", introOptions);
                Mosaic::text(ui, "Enter 'HELP' for help.");
                {
                    auto row = Mosaic::row(ui);

                    if(Mosaic::smallButton(ui, "Add Debug Text").clicked() == true)
                    {
                        HelloDemo::addConsoleLog("some text");
                        HelloDemo::addConsoleLog("some more text");
                        HelloDemo::addConsoleLog("display very important message here!");
                    }

                    if(Mosaic::smallButton(ui, "Add Debug Error").clicked() == true)
                    {
                        HelloDemo::addConsoleLog("[error] something went wrong");
                    }

                    if(Mosaic::smallButton(ui, "Clear").clicked() == true)
                    {
                        m_consoleItems.clear();
                        m_consoleVisibleDirty = true;
                    }

                    if(Mosaic::smallButton(ui, "Copy").clicked() == true)
                    {
                        Mosaic::String clipboard;
                        for(const Mosaic::String & line : m_consoleItems)
                        {
                            clipboard += line;
                            clipboard += "\n";
                        }
                        Mosaic::platform(ui).setClipboardText(clipboard);
                    }
                }
                Mosaic::separator(ui);
                Mosaic::Response optionsResponse;
                Mosaic::Rect optionsBounds;
                {
                    auto row = Mosaic::row(ui);
                    optionsResponse = Mosaic::button(ui, "Options");
                    bool hasOptionsBounds = Mosaic::debugBounds(ui, optionsResponse.id, &optionsBounds);

                    if(optionsResponse.clicked() == true && hasOptionsBounds == true)
                    {
                        Mosaic::PopupOptions popupOptions;
                        popupOptions.owner = optionsResponse.id;
                        popupOptions.anchor = optionsBounds;
                        popupOptions.placement = Mosaic::PopupPlacement::Below;
                        Mosaic::openPopup(ui, Mosaic::Key("console options"), popupOptions);
                    }

                    if(Mosaic::searchField(ui, "Filter (\"incl,-excl\") (\"error\")", &m_consoleFilter).changed() == true)
                    {
                        Mosaic::setTextFilter(&m_consoleTextFilter, m_consoleFilter);
                        m_consoleVisibleDirty = true;
                    }
                }
                {
                    Mosaic::PopupOptions popupOptions;
                    popupOptions.owner = optionsResponse.id;
                    popupOptions.anchor = optionsBounds;
                    popupOptions.placement = Mosaic::PopupPlacement::Below;
                    auto optionsPopup = Mosaic::popup(ui, Mosaic::Key("console options"), popupOptions);

                    if(optionsPopup.visible() == true)
                    {
                        Mosaic::checkbox(ui, "Auto-scroll", &m_consoleAutoScroll);
                    }
                }
                Mosaic::separator(ui);
                Mosaic::LayoutOptions logLayout;
                logLayout.width = Mosaic::SizeRule::Fill;
                logLayout.height = Mosaic::SizeRule::Fill;
                Mosaic::Id consoleLogId = Mosaic::InvalidId;
                {
                    auto log = Mosaic::scrollArea(ui, "Console log", Mosaic::ScrollOptions{}, logLayout);
                    consoleLogId = log.id();
                    if(m_consoleVisibleDirty == true)
                    {
                        m_consoleVisibleItems.clear();
                        for(size_t index = 0; index != m_consoleItems.size(); ++index)
                        {
                            const Mosaic::String & line = m_consoleItems[index];

                            if(Mosaic::textFilterPasses(m_consoleTextFilter, line) == true)
                            {
                                m_consoleVisibleItems.push_back(index);
                            }
                        }
                        m_consoleVisibleDirty = false;
                    }
                    float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                    Mosaic::Theme errorTheme = Mosaic::getTheme(ui);
                    errorTheme.colors.text = Mosaic::Color::fromBytes(255, 92, 92);
                    Mosaic::Theme commandTheme = Mosaic::getTheme(ui);
                    commandTheme.colors.text = Mosaic::Color::fromBytes(255, 196, 96);
                    Mosaic::VisibleRange range;
                    (void)Mosaic::beginListClipper(ui, m_consoleVisibleItems.size(), itemHeight, &range, log.id());
                    for(size_t visible = range.begin; visible != range.end; ++visible)
                    {
                        size_t index = m_consoleVisibleItems[visible];
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        const Mosaic::String & line = m_consoleItems[index];

                        if(line.find("[error]") != Mosaic::String::npos || line.find("Unknown command") != Mosaic::String::npos)
                        {
                            auto style = Mosaic::styleScope(ui, errorTheme);
                            Mosaic::text(ui, line);
                        }
                        else if(line.starts_with("# "))
                        {
                            auto style = Mosaic::styleScope(ui, commandTheme);
                            Mosaic::text(ui, line);
                        }
                        else
                        {
                            Mosaic::text(ui, line);
                        }
                    }
                    Mosaic::endListClipper(ui, range, m_consoleVisibleItems.size(), itemHeight);
                    Mosaic::Vec2 scrollOffset;
                    Mosaic::Vec2 scrollRange;
                    bool hasScroll = Mosaic::scrollOffset(ui, log.id(), &scrollOffset) == true && Mosaic::scrollRange(ui, log.id(), &scrollRange) == true;

                    if(m_consoleScrollToBottomFrames != 0 || (m_consoleAutoScroll == true && hasScroll == true && scrollOffset.y >= scrollRange.y - itemHeight))
                    {
                        Mosaic::scrollToEnd(ui, log.id(), Mosaic::ScrollAxes::Vertical);

                        if(m_consoleScrollToBottomFrames != 0)
                        {
                            --m_consoleScrollToBottomFrames;
                        }
                    }
                }
                Mosaic::Response consoleLogResponse;
                if(Mosaic::itemResponse(ui, consoleLogId, &consoleLogResponse) == true)
                {
                    auto context = Mosaic::contextPopup(ui, Mosaic::Key("console context"), "Console", consoleLogResponse);

                    if(context.visible() == true)
                    {
                        if(Mosaic::menuItem(ui, "Copy all").clicked() == true)
                        {
                            Mosaic::String clipboard;
                            for(const Mosaic::String & line : m_consoleItems)
                            {
                                clipboard += line;
                                clipboard += "\n";
                            }
                            Mosaic::platform(ui).setClipboardText(clipboard);
                            Mosaic::closeCurrentPopup(ui);
                        }

                        if(Mosaic::menuItem(ui, "Clear").clicked() == true)
                        {
                            m_consoleItems.clear();
                            m_consoleVisibleDirty = true;
                            Mosaic::closeCurrentPopup(ui);
                        }
                    }
                }
                Mosaic::TextInputOptions commandOptions;
                commandOptions.escapeClearsAll = true;
                commandOptions.callbackCompletion = true;
                commandOptions.callbackHistory = true;
                commandOptions.callback = &HelloDemo::consoleInputCallback;
                commandOptions.callbackUserData = this;
                Mosaic::Response command = Mosaic::inputText(ui, "Input", &m_commandInput, commandOptions);

                if(command.committed() == true && m_commandInput.empty() == false)
                {
                    HelloDemo::executeConsoleCommand(m_commandInput);
                    ++m_consoleCommandCount;
                    m_commandInput.clear();
                    Mosaic::focus(ui, command.id);
                }

            }
        }

        if(m_showDocuments == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showDocuments;
            auto window = Mosaic::window(ui, "Example: Documents", options);

            if(window.visible() == true)
            {
                {
                    auto bar = Mosaic::menuBar(ui);
                    {
                        auto file = Mosaic::menu(ui, "File");

                        if(file.expanded() == true)
                        {
                            auto openMenu = Mosaic::menu(ui, "Open");

                            if(openMenu.expanded() == true)
                            {
                                bool hasClosedDocument = false;
                                for(size_t index = 0; index != m_documentOpen.size(); ++index)
                                {
                                    if(m_documentOpen[index] == true)
                                    {
                                        continue;
                                    }

                                    hasClosedDocument = true;

                                    if(Mosaic::menuItem(ui, Mosaic::Key(index), documentNames[index]).clicked() == true)
                                    {
                                        m_documentOpen[index] = true;
                                        m_documentTab = static_cast<int>(index);
                                        m_documentsRedockRequested = true;
                                    }
                                }

                                if(hasClosedDocument == false)
                                {
                                    auto disabled = Mosaic::disabledScope(ui, true);
                                    Mosaic::menuItem(ui, "All documents are open");
                                }
                            }

                            if(Mosaic::menuItem(ui, "Close All Documents").clicked() == true)
                            {
                                m_documentCloseQueue.clear();
                                m_documentClosePending = -1;
                                m_documentCloseAllPending = true;
                                for(size_t index = 0; index != m_documentOpen.size(); ++index)
                                {
                                    if(m_documentOpen[index] == false)
                                    {
                                        continue;
                                    }

                                    if(m_documentDirty[index] == true)
                                    {
                                        requestDocumentClose(index);

                                        continue;
                                    }

                                    m_documentOpen[index] = false;
                                }

                                if(m_documentCloseQueue.empty() == true)
                                {
                                    m_documentCloseAllPending = false;
                                }
                            }

                            if(Mosaic::menuItem(ui, "Exit").clicked() == true)
                            {
                                m_showDocuments = false;
                            }
                        }
                    }
                }
                {
                    Mosaic::LineOptions documentLineOptions;
                    documentLineOptions.spacing = 0.f;
                    auto openDocuments = Mosaic::line(ui, documentLineOptions);
                    for(size_t index = 0; index != documentNames.size(); ++index)
                    {
                        auto documentScope = Mosaic::scope(ui, Mosaic::Key(index));
                        bool wasOpen = m_documentOpen[index];

                        if(Mosaic::checkbox(ui, Mosaic::Key(index), documentNames[index], &m_documentOpen[index]).changed() == true)
                        {
                            if(wasOpen == true && m_documentOpen[index] == false && m_documentDirty[index] == true)
                            {
                                requestDocumentClose(index);
                            }
                            else if(m_documentOpen[index] == true)
                            {
                                m_documentTab = static_cast<int>(index);
                            }

                            m_documentsRedockRequested = true;
                        }
                    }
                }
                {
                    auto outputLine = Mosaic::line(ui);
                    constexpr Mosaic::Array<Mosaic::StringView, 3> outputTargets = {"None", "TabBar+Tabs", "DockSpace+Window"};
                    int previousOutput = m_documentOutput;
                    Mosaic::setNextItemWidth(ui, Mosaic::getTheme(ui).metrics.fontSize * 12.f);
                    Mosaic::comboBox(ui, "Output", &m_documentOutput, outputTargets);

                    if(previousOutput != m_documentOutput && m_documentOutput == 2)
                    {
                        m_documentsRedockRequested = true;
                    }

                    if(m_documentOutput == 1)
                    {
                        Mosaic::checkbox(ui, "Reorderable Tabs", &m_documentsReorderable);
                    }
                    else if(m_documentOutput == 2)
                    {
                        if(Mosaic::button(ui, "Redock all").clicked() == true)
                        {
                            m_documentsRedockRequested = true;
                        }
                    }
                }
                bool hasOpenDocument = false;
                for(size_t index = 0; index != documentNames.size(); ++index)
                {
                    if(m_documentOpen[index] == true)
                    {
                        hasOpenDocument = true;
                    }
                }

                if(hasOpenDocument == false)
                {
                    Mosaic::text(ui, "No open documents.");
                }
                else if(m_documentOutput == 0)
                {
                    Mosaic::text(ui, "Documents are open, but this output mode does not submit their contents.");
                }
                else if(m_documentOutput == 1)
                {
                    Mosaic::TabsOptions tabOptions;
                    tabOptions.reorderable = m_documentsReorderable;
                    tabOptions.autoSelectNewTabs = true;
                    tabOptions.tabListPopupButton = true;
                    auto tabBar = Mosaic::beginTabBar(ui, "documents", tabOptions);

                    if(m_documentTabClosed >= 0)
                    {
                        Mosaic::setTabItemClosed(ui, Mosaic::Key(static_cast<size_t>(m_documentTabClosed)));
                        m_documentTabClosed = -1;
                    }

                    for(size_t index = 0; index != documentNames.size(); ++index)
                    {
                        if(m_documentOpen[index] == false)
                        {
                            continue;
                        }

                        Mosaic::TabItemOptions itemOptions;
                        itemOptions.unsavedDocument = m_documentDirty[index];
                        itemOptions.noAssumedClosure = m_documentDirty[index];
                        bool wasOpen = m_documentOpen[index];
                        auto tabItem = Mosaic::beginTabItem(ui, Mosaic::Key(index), documentNames[index], &m_documentOpen[index], itemOptions);
                        Mosaic::Response tabResponse;
                        (void)Mosaic::itemResponse(ui, tabItem.id(), &tabResponse);

                        if(tabItem.expanded() == true)
                        {
                            m_documentTab = static_cast<int>(index);
                            drawDocumentContents(index);
                        }

                        Mosaic::PopupOptions contextOptions;
                        auto context = Mosaic::contextPopup(ui, Mosaic::Key(0xd0c800U + index), "Document", tabResponse, contextOptions);

                        if(context.visible() == true)
                        {
                            Mosaic::MenuItemOptions saveOptions;
                            saveOptions.enabled = m_documentDirty[index];
                            saveOptions.shortcut = "Cmd+S";

                            Mosaic::String saveLabel = "Save ";
                            saveLabel += documentNames[index];

                            if(Mosaic::menuItem(ui, saveLabel, saveOptions).clicked() == true)
                            {
                                m_documentDirty[index] = false;
                            }

                            Mosaic::MenuItemOptions renameOptions;
                            renameOptions.shortcut = "Cmd+R";

                            Mosaic::Response renameItem = Mosaic::menuItem(ui, "Rename...", renameOptions);

                            if(renameItem.clicked() == true)
                            {
                                requestDocumentRename(index, renameItem.id);
                            }

                            Mosaic::MenuItemOptions closeOptions;
                            closeOptions.shortcut = "Cmd+W";

                            if(Mosaic::menuItem(ui, "Close", closeOptions).clicked() == true)
                            {
                                requestDocumentClose(index);
                            }
                        }

                        if(wasOpen == true && m_documentOpen[index] == false)
                        {
                            requestDocumentClose(index);
                        }
                    }
                }
                else
                {
                    Mosaic::LayoutOptions dockLayout;
                    dockLayout.width = Mosaic::SizeRule::Fill;
                    dockLayout.height = Mosaic::Dimension::fixed(245.f);
                    Mosaic::DockSpaceOptions dockOptions;
                    dockOptions.group = 4;
                    dockOptions.backgroundColor = Mosaic::Color::fromBytes(15, 18, 22);
                    auto dockCanvas = Mosaic::dockSpace(ui, "Document dock space", dockOptions, dockLayout);
                    (void)Mosaic::debugBounds(ui, dockCanvas.id(), &documentDockBounds);
                }
            }
        }

        if(m_showDocuments == true && m_documentOutput == 2 && documentDockBounds.empty() == false)
        {
            constexpr uint32_t documentDockGroup = 4;
            Mosaic::Array<Mosaic::Id, 6> documentWindows = {};
            for(size_t index = 0; index != documentNames.size(); ++index)
            {
                if(m_documentOpen[index] == false)
                {
                    continue;
                }

                Mosaic::WindowOptions documentOptions;
                documentOptions.open = &m_documentOpen[index];
                documentOptions.initialBounds = {documentDockBounds.x + 12.f + static_cast<float>(index) * 18.f, documentDockBounds.y + 12.f + static_cast<float>(index) * 18.f, std::max(220.f, documentDockBounds.width * 0.7f), std::max(140.f, documentDockBounds.height * 0.72f)};
                documentOptions.minimumSize = {160.f, 100.f};
                documentOptions.dockGroup = documentDockGroup;
                documentOptions.unsavedDocument = m_documentDirty[index];
                Mosaic::String label(documentNames[index]);

                bool wasOpen = m_documentOpen[index];
                auto documentWindow = Mosaic::window(ui, Mosaic::Key(0xd0c400U + index), label, documentOptions);
                documentWindows[index] = documentWindow.id();

                if(documentWindow.visible() == true)
                {
                    drawDocumentContents(index);
                }

                if(wasOpen == true && m_documentOpen[index] == false)
                {
                    requestDocumentClose(index);
                }
            }

            if(m_documentsDockInitialized == false || m_documentsRedockRequested == true)
            {
                Mosaic::clearDockSpace(ui, documentDockGroup);
                Mosaic::DockNodeId root = Mosaic::dockSpaceRoot(ui, documentDockGroup);
                for(Mosaic::Id id : documentWindows)
                {
                    if(id != Mosaic::InvalidId)
                    {
                        (void)Mosaic::dockWindow(ui, documentDockGroup, id, root, Mosaic::DockPlacement::Center);
                    }
                }
                m_documentsDockInitialized = true;
                m_documentsRedockRequested = false;
            }
        }

        if(m_documentClosePending >= 0)
        {
            bool open = true;
            auto modal = Mosaic::modal(ui, "Save?", &open, {});

            if(modal.visible() == true)
            {
                Mosaic::text(ui, "Save change to the following items?");
                Mosaic::ScrollOptions scrollOptions;
                Mosaic::LayoutOptions listLayout;
                listLayout.width = Mosaic::SizeRule::Fill;
                listLayout.height = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.lineHeight * 6.25f);
                auto list = Mosaic::scrollArea(ui, Mosaic::Key("documents to save"), "Documents to save", scrollOptions, listLayout);
                for(size_t index : m_documentCloseQueue)
                {
                    auto documentScope = Mosaic::scope(ui, Mosaic::Key(index));

                    if(m_documentDirty[index] == true)
                    {
                        Mosaic::text(ui, documentNames[index]);
                    }
                }
                auto actions = Mosaic::row(ui);
                Mosaic::ButtonOptions actionOptions;
                actionOptions.width = Mosaic::Dimension::fixed(Mosaic::getTheme(ui).metrics.fontSize * 7.f);

                if(Mosaic::button(ui, Mosaic::Key("save documents"), "Yes", actionOptions).clicked() == true)
                {
                    for(size_t index : m_documentCloseQueue)
                    {
                        m_documentDirty[index] = false;
                        m_documentOpen[index] = false;
                        m_documentTabClosed = static_cast<int>(index);
                    }

                    m_documentCloseQueue.clear();
                    m_documentClosePending = -1;
                    m_documentCloseAllPending = false;
                }

                if(Mosaic::button(ui, Mosaic::Key("discard documents"), "No", actionOptions).clicked() == true)
                {
                    for(size_t index : m_documentCloseQueue)
                    {
                        m_documentDirty[index] = false;
                        m_documentOpen[index] = false;
                        m_documentTabClosed = static_cast<int>(index);
                    }

                    m_documentCloseQueue.clear();
                    m_documentClosePending = -1;
                    m_documentCloseAllPending = false;
                }

                if(Mosaic::button(ui, Mosaic::Key("cancel document close"), "Cancel", actionOptions).clicked() == true)
                {
                    m_documentCloseQueue.clear();
                    m_documentClosePending = -1;
                    m_documentCloseAllPending = false;
                }
            }

            if(open == false)
            {
                m_documentCloseQueue.clear();
                m_documentClosePending = -1;
                m_documentCloseAllPending = false;
            }
        }

        if(m_documentRenamePending >= 0)
        {
            Mosaic::PopupOptions popupOptions;
            popupOptions.owner = m_documentRenameOwner;
            popupOptions.anchor = m_documentRenameAnchor;
            popupOptions.placement = Mosaic::PopupPlacement::Automatic;
            popupOptions.minimumSize = {390.f, 0.f};
            popupOptions.maximumSize = {390.f, 240.f};
            auto renamePopup = Mosaic::popup(ui, Mosaic::Key("document rename"), popupOptions);

            if(renamePopup.visible() == true)
            {
                Mosaic::TextInputOptions renameOptions;
                renameOptions.selectAllOnFocus = true;
                Mosaic::Response renameInput;
                {
                    auto renameScope = Mosaic::scope(ui, Mosaic::Key("document rename input"));
                    renameInput = Mosaic::inputText(ui, "", &m_documentRenameValue, renameOptions);
                }

                if(renameInput.committed() == true && m_documentRenameValue.empty() == false)
                {
                    m_documentNames[static_cast<size_t>(m_documentRenamePending)] = m_documentRenameValue;
                    m_documentRenamePending = -1;
                    m_documentRenameOwner = Mosaic::InvalidId;
                    m_documentRenameAnchor = {};
                    Mosaic::closeCurrentPopup(ui);
                }
            }

            if(Mosaic::isPopupOpen(ui, Mosaic::Key("document rename")) == false)
            {
                m_documentRenamePending = -1;
                m_documentRenameOwner = Mosaic::InvalidId;
                m_documentRenameAnchor = {};
            }
        }

        if(m_showDockspace == true)
        {
            Mosaic::Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
            Mosaic::Rect fullscreenBounds = workArea;
            bool advancedMode = m_dockspaceMode == 1;
            uint32_t dockGroup = advancedMode == true ? 3U : 2U;

            if(advancedMode == false && m_configurationFlags[11] == true)
            {
                Mosaic::LayoutOptions dockLayout;
                dockLayout.absoluteRect = workArea;
                Mosaic::DockSpaceOptions dockSpaceOptions;
                dockSpaceOptions.group = dockGroup;
                dockSpaceOptions.passthroughCentral = m_dockspacePassthruCentral;
                dockSpaceOptions.background = m_dockspacePassthruCentral == false;
                auto dockCanvas = Mosaic::dockSpace(ui, Mosaic::Key("Basic viewport dockspace"), "Basic viewport dockspace", dockSpaceOptions, dockLayout);
            }

            Mosaic::WindowOptions controlOptions;
            controlOptions.open = &m_showDockspace;
            controlOptions.menuBar = true;
            controlOptions.dockable = false;

            if(m_dockspaceModeChanged == true)
            {
                Mosaic::setNextWindowFocus(ui);
                m_dockspaceModeChanged = false;
            }

            auto controls = Mosaic::window(ui, "Examples: Dockspace", controlOptions);

            if(controls.visible() == true)
            {
                {
                    auto bar = Mosaic::menuBar(ui);
                    auto help = Mosaic::menu(ui, "Help");

                    if(help.expanded() == true)
                    {
                        Mosaic::TextOptions helpText;
                        helpText.wordWrap = true;
                        helpText.layout.width = Mosaic::Dimension::fixed(430.f);
                        Mosaic::text(ui, "This demonstrates a DockSpace which allows you to manually create a docking node inside another window. The Basic version places it over the viewport; most applications can use this mode.", helpText);
                        Mosaic::separator(ui);
                        Mosaic::text(ui, "When docking is enabled, most windows can be docked into another:", helpText);
                        Mosaic::bulletText(ui, "Drag from a window title bar or tab to dock or undock.");
                        Mosaic::bulletText(ui, "Drag from the window menu button to undock an entire node.");
                        Mosaic::bulletText(ui, "Drag a tab to reorder it or move it to another compatible dock node.");
                        Mosaic::bulletText(ui, "Drag a splitter to resize both adjacent dock nodes.");
                        Mosaic::bulletText(ui, "Closing a docked tab releases its space to the remaining tabs and nodes.");
                        Mosaic::bulletText(ui, "Basic and Advanced modes keep independent docking layouts.");
                        Mosaic::separator(ui);
                        Mosaic::text(ui, "More details:");
                        Mosaic::bulletText(ui, "Read the docking API documentation and this example source.");
                    }
                }
                {
                    auto modes = Mosaic::row(ui);

                    if(Mosaic::radioButton(ui, "Basic demo mode", m_dockspaceMode == 0).clicked() == true)
                    {
                        m_dockspaceMode = 0;
                        m_dockspaceFullscreen = false;
                        m_dockspaceModeChanged = true;
                    }

                    if(Mosaic::radioButton(ui, "Advanced demo mode", m_dockspaceMode == 1).clicked() == true)
                    {
                        m_dockspaceMode = 1;
                        m_dockspaceModeChanged = true;
                    }
                }
                Mosaic::separatorText(ui, "Options");

                if(m_configurationFlags[11] == false)
                {
                    Mosaic::text(ui, "ERROR: Docking is not enabled! See Demo > Configuration.");
                    auto enableRow = Mosaic::row(ui);
                    Mosaic::text(ui, "Enable docking in the window options, or ");

                    if(Mosaic::button(ui, "click here").clicked() == true)
                    {
                        m_configurationFlags[11] = true;
                    }
                }
                else if(m_dockspaceMode == 0)
                {
                    Mosaic::checkbox(ui, "Flag: PassthruCentralNode", &m_dockspacePassthruCentral);
                }
                else
                {
                    if(Mosaic::checkbox(ui, "Fullscreen", &m_dockspaceFullscreen).changed() == true && m_dockspaceFullscreen == false)
                    {
                        m_dockspacePassthruCentral = false;
                    }

                    Mosaic::checkbox(ui, "Keep Window Padding", &m_dockspaceKeepPadding);
                    Mosaic::helpMarker(ui, "This option highlights that the DockSpace is placed inside a regular window.");
                    {
                        auto disabled = Mosaic::disabledScope(ui, m_dockspaceFullscreen == false);
                        Mosaic::checkbox(ui, "Flag: PassthruCentralNode", &m_dockspacePassthruCentral);
                    }
                    Mosaic::checkbox(ui, "Flag: NoDockingOverCentralNode", &m_dockspaceNoDockingOverCentral);
                    Mosaic::checkbox(ui, "Flag: NoDockingSplit", &m_configurationFlags[12]);
                    Mosaic::checkbox(ui, "Flag: NoUndocking", &m_dockspaceNoUndocking);
                    Mosaic::checkbox(ui, "Flag: NoResize", &m_dockspaceNoResize);
                    Mosaic::checkbox(ui, "Flag: AutoHideTabBar", &m_dockspaceAutoHideTabBar);

                    if(m_dockspaceFullscreen == false)
                    {
                        m_dockspacePassthruCentral = false;
                    }
                }
            }

            if(advancedMode == true && m_configurationFlags[11] == true)
            {
                Mosaic::WindowOptions hostOptions;
                hostOptions.open = &m_showDockspace;
                hostOptions.movable = m_dockspaceFullscreen == false;
                hostOptions.resizable = m_dockspaceFullscreen == false;
                hostOptions.navigationFocus = m_dockspaceFullscreen == false;
                hostOptions.bringToFront = m_dockspaceFullscreen == false;
                hostOptions.dockable = false;
                hostOptions.titleBar = m_dockspaceFullscreen == false;
                hostOptions.background = m_dockspaceFullscreen == false || m_dockspacePassthruCentral == false;
                hostOptions.padding = m_dockspaceKeepPadding;

                if(m_dockspaceFullscreen == true)
                {
                    Mosaic::setNextWindowPosition(ui, {fullscreenBounds.x, fullscreenBounds.y});
                    Mosaic::setNextWindowSize(ui, {fullscreenBounds.width, fullscreenBounds.height});
                }

                auto host = Mosaic::window(ui, "Window with a DockSpace", hostOptions);

                Mosaic::LayoutOptions dockLayout;
                dockLayout.width = Mosaic::SizeRule::Fill;
                dockLayout.height = Mosaic::SizeRule::Fill;
                Mosaic::DockSpaceOptions dockSpaceOptions;
                dockSpaceOptions.group = dockGroup;
                dockSpaceOptions.noSplit = m_configurationFlags[12];
                dockSpaceOptions.noResize = m_dockspaceNoResize;
                dockSpaceOptions.noUndocking = m_dockspaceNoUndocking;
                dockSpaceOptions.noDockingOverCentralNode = m_dockspaceNoDockingOverCentral;
                dockSpaceOptions.passthroughCentral = m_dockspaceFullscreen == true && m_dockspacePassthruCentral == true;
                dockSpaceOptions.autoHideTabBar = m_dockspaceAutoHideTabBar;
                dockSpaceOptions.background = dockSpaceOptions.passthroughCentral == false;
                auto dockCanvas = Mosaic::dockSpace(ui, Mosaic::Key("Advanced host dockspace"), "Advanced host dockspace", dockSpaceOptions, dockLayout);
            }
        }

        if(m_showImageViewer == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showImageViewer;
            auto window = Mosaic::window(ui, "Example: Image Viewer", options);

            if(window.visible() == true)
            {
                drawImageViewerContents(ui);
            }
        }

        if(m_showLog == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showLog;
            Mosaic::setNextWindowSize(ui, {500.f, 400.f}, Mosaic::Condition::FirstUse);
            {
                auto debugWindow = Mosaic::window(ui, "Example: Log", options);

                if(debugWindow.visible() == true && Mosaic::smallButton(ui, "[Debug] Add 5 entries").clicked() == true)
                {
                    constexpr Mosaic::Array<Mosaic::StringView, 3> categories = {"info", "warn", "error"};
                    constexpr Mosaic::Array<Mosaic::StringView, 7> words = {"Bumfuzzled", "Cattywampus", "Snickersnee", "Abibliophobia", "Absquatulate", "Nincompoop", "Pauciloquent"};

                    for(size_t index = 0; index != 5; ++index)
                    {
                        Mosaic::String line = "[";
                        Mosaic::String frameNumber = ExamplesDetail::number(Mosaic::getFrame(ui).number);
                        while(frameNumber.size() < 5)
                        {
                            frameNumber.insert(frameNumber.begin(), '0');
                        }
                        line += frameNumber;
                        line += "] [";
                        line += categories[m_logCounter % categories.size()];
                        line += "] Hello, current time is ";
                        line += ExamplesDetail::fixed(Mosaic::input(ui).timestamp, 1);
                        line += ", here's a word: '";
                        line += words[m_logCounter % words.size()];
                        line += "'";
                        m_logItems.push_back(std::move(line));
                        ++m_logCounter;
                    }

                    m_logVisibleDirty = true;
                }
            }

            auto window = Mosaic::window(ui, "Example: Log", options);

            if(window.visible() == true)
            {

                Mosaic::Response optionsResponse;
                Mosaic::Rect optionsBounds;
                {
                    auto actions = Mosaic::row(ui);
                    optionsResponse = Mosaic::button(ui, "Options");
                    bool hasOptionsBounds = Mosaic::debugBounds(ui, optionsResponse.id, &optionsBounds);

                    if(optionsResponse.clicked() == true && hasOptionsBounds == true)
                    {
                        Mosaic::PopupOptions popupOptions;
                        popupOptions.owner = optionsResponse.id;
                        popupOptions.anchor = optionsBounds;
                        popupOptions.placement = Mosaic::PopupPlacement::Below;
                        Mosaic::openPopup(ui, Mosaic::Key("log options"), popupOptions);
                    }

                    if(Mosaic::button(ui, "Clear").clicked() == true)
                    {
                        m_logItems.clear();
                        m_logVisibleDirty = true;
                    }

                    if(Mosaic::button(ui, "Copy").clicked() == true)
                    {
                        Mosaic::String clipboard;

                        for(const Mosaic::String & line : m_logItems)
                        {
                            if(Mosaic::textFilterPasses(m_logTextFilter, line) == false)
                            {
                                continue;
                            }

                            clipboard += line;
                            clipboard += "\n";
                        }

                        Mosaic::platform(ui).setClipboardText(clipboard);
                    }

                    if(Mosaic::searchField(ui, "Filter", &m_logFilter).changed() == true)
                    {
                        Mosaic::setTextFilter(&m_logTextFilter, m_logFilter);
                        m_logVisibleDirty = true;
                    }
                }
                {
                    Mosaic::PopupOptions popupOptions;
                    popupOptions.owner = optionsResponse.id;
                    popupOptions.anchor = optionsBounds;
                    popupOptions.placement = Mosaic::PopupPlacement::Below;
                    auto optionsPopup = Mosaic::popup(ui, Mosaic::Key("log options"), popupOptions);

                    if(optionsPopup.visible() == true)
                    {
                        Mosaic::checkbox(ui, "Auto-scroll", &m_logAutoScroll);
                    }
                }
                Mosaic::LayoutOptions logLayout;
                logLayout.width = Mosaic::SizeRule::Fill;
                logLayout.height = Mosaic::Dimension::fixed(240.f);
                auto log = Mosaic::scrollArea(ui, "Log entries", Mosaic::ScrollOptions{}, logLayout);
                if(m_logVisibleDirty == true)
                {
                    m_logVisibleItems.clear();
                    for(size_t index = 0; index != m_logItems.size(); ++index)
                    {
                        const Mosaic::String & line = m_logItems[index];

                        if(Mosaic::textFilterPasses(m_logTextFilter, line) == true)
                        {
                            m_logVisibleItems.push_back(index);
                        }
                    }
                    m_logVisibleDirty = false;
                }
                float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;

                if(m_logFilter.empty() == false)
                {
                    for(size_t visible = 0; visible != m_logVisibleItems.size(); ++visible)
                    {
                        size_t index = m_logVisibleItems[visible];
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::text(ui, m_logItems[index]);
                    }
                }
                else
                {
                    Mosaic::VisibleRange range;
                    (void)Mosaic::beginListClipper(ui, m_logItems.size(), itemHeight, &range, log.id());
                    for(size_t index = range.begin; index != range.end; ++index)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::text(ui, m_logItems[index]);
                    }
                    Mosaic::endListClipper(ui, range, m_logItems.size(), itemHeight);
                }

                Mosaic::Vec2 logOffset;
                Mosaic::Vec2 logRange;
                bool hasLogScroll = Mosaic::scrollOffset(ui, log.id(), &logOffset) == true && Mosaic::scrollRange(ui, log.id(), &logRange) == true;
                bool logWasAtEnd = hasLogScroll == false || logOffset.y >= logRange.y - itemHeight;

                if(m_logAutoScroll == true && logWasAtEnd == true)
                {
                    Mosaic::scrollToEnd(ui, log.id(), Mosaic::ScrollAxes::Vertical);
                }
            }
        }

        if(m_showPropertyEditor == true)
        {
            HelloDemo::initializePropertyEditor();
            Mosaic::WindowOptions options;
            options.open = &m_showPropertyEditor;
            Mosaic::setNextWindowSize(ui, {430.f, 450.f}, Mosaic::Condition::FirstUse);
            auto window = Mosaic::window(ui, "Example: Property editor", options);

            if(window.visible() == true)
            {
                Mosaic::SplitOptions splitOptions;
                splitOptions.minimumFirst = 160.f;
                splitOptions.minimumSecond = 100.f;
                Mosaic::LayoutOptions splitLayout;
                splitLayout.width = Mosaic::SizeRule::Fill;
                splitLayout.height = Mosaic::SizeRule::Fill;
                auto split = Mosaic::split(ui, "Property editor panes", Mosaic::Orientation::Horizontal, &m_propertySplitRatio, splitOptions, splitLayout);
                {
                    auto objectsPane = Mosaic::column(ui, ExamplesDetail::fill());
                    {
                        auto controls = Mosaic::row(ui);
                        Mosaic::checkbox(ui, "Use Clipper", &m_propertyUseClipper);
                        Mosaic::text(ui, "(20 root nodes)");
                    }
                    Mosaic::TextInputOptions filterOptions;
                    filterOptions.hint = "incl,-excl";
                    filterOptions.escapeClearsAll = true;
                    Mosaic::inputText(ui, "Filter", &m_propertyFilter, filterOptions);
                    Mosaic::LayoutOptions treeLayout = ExamplesDetail::fill();
                    auto objects = Mosaic::scrollArea(ui, "Property tree", Mosaic::ScrollOptions{}, treeLayout);

                    m_propertyVisibleNodes.clear();
                    for(size_t index = 1; index != m_propertyNodes.size(); ++index)
                    {
                        if(m_propertyNodes[index].parent != 0)
                        {
                            continue;
                        }

                        bool passesFilter = m_propertyFilter.empty() == true;
                        for(size_t candidate = index; candidate != m_propertyNodes.size(); ++candidate)
                        {
                            if(candidate != index && m_propertyNodes[candidate].depth == 0)
                            {
                                break;
                            }

                            if(ExamplesDetail::containsIgnoreCase(m_propertyNodes[candidate].name, m_propertyFilter) == true)
                            {
                                passesFilter = true;
                                break;
                            }
                        }

                        if(passesFilter == true)
                        {
                            HelloDemo::appendVisiblePropertyNode(index);
                        }
                    }

                    float itemExtent = Mosaic::getTheme(ui).metrics.controlHeight;
                    Mosaic::VisibleRange range = {0, m_propertyVisibleNodes.size()};

                    if(m_propertyUseClipper == true)
                    {
                        (void)Mosaic::beginListClipper(ui, m_propertyVisibleNodes.size(), itemExtent, &range, objects.id());
                    }

                    for(size_t visible = range.begin; visible != range.end; ++visible)
                    {
                        size_t index = m_propertyVisibleNodes[visible];
                        PropertyDemoNode & node = m_propertyNodes[index];
                        auto itemScope = Mosaic::scope(ui, Mosaic::Key(node.uid));

                        if(m_propertyUseClipper == true)
                        {
                            Mosaic::LayoutOptions indentLayout;
                            indentLayout.width = Mosaic::SizeRule::Fill;
                            indentLayout.padding.left = static_cast<float>(node.depth) * Mosaic::getTheme(ui).metrics.indent;
                            auto indent = Mosaic::column(ui, indentLayout);
                            bool hasChildren = std::find_if(m_propertyNodes.begin(), m_propertyNodes.end(),
                                                                  [index](const PropertyDemoNode & candidate)
                                                                  {
                                                                      return candidate.parent == index;
                                                                  }) != m_propertyNodes.end();
                            Mosaic::TreeNodeOptions treeOptions;
                            treeOptions.defaultExpanded = node.expanded;
                            treeOptions.leaf = hasChildren == false;
                            treeOptions.bullet = hasChildren == false;
                            treeOptions.openOnArrow = true;
                            treeOptions.openOnDoubleClick = true;
                            treeOptions.spanAvailableWidth = true;
                            treeOptions.navigationLeftJumpsToParent = true;
                            Mosaic::TreeScope tree = Mosaic::treeNode(ui, Mosaic::Key("property row"), node.name, &m_propertySelection, m_propertyOrder[index], m_propertyOrder, treeOptions);
                            Mosaic::Response response;
                            if(Mosaic::itemResponse(ui, tree.id(), &response) == true && (response.clicked() == true || response.focused() == true))
                            {
                                m_propertySelectedNode = index;
                            }

                            node.expanded = tree.expanded();
                        }
                        else if(node.depth == 0)
                        {
                            HelloDemo::drawPropertyNode(ui, index);
                            visible += std::count_if(m_propertyVisibleNodes.begin() + static_cast<std::ptrdiff_t>(visible + 1), m_propertyVisibleNodes.end(),
                                                     [index, this](size_t candidate)
                                                     {
                                                         size_t parent = m_propertyNodes[candidate].parent;
                                                         while(parent != std::numeric_limits<size_t>::max())
                                                         {
                                                             if(parent == index)
                                                             {
                                                                 return true;
                                                             }

                                                             parent = m_propertyNodes[parent].parent;
                                                         }

                                                         return false;
                                                     });
                        }
                    }

                    if(m_propertyUseClipper == true)
                    {
                        Mosaic::endListClipper(ui, range, m_propertyVisibleNodes.size(), itemExtent);
                    }
                }

                {
                    auto properties = Mosaic::scrollArea(ui, "Properties", Mosaic::ScrollOptions{}, ExamplesDetail::fill());

                    if(m_propertySelectedNode >= m_propertyNodes.size())
                    {
                        m_propertySelectedNode = m_propertyNodes.size() > 1 ? 1 : 0;
                    }

                    PropertyDemoNode & selected = m_propertyNodes[m_propertySelectedNode];
                    Mosaic::text(ui, selected.name);
                    Mosaic::String uid = "UID: 0x";
                    uid += ExamplesDetail::number(selected.uid);
                    {
                        Mosaic::Theme disabledTheme = Mosaic::getTheme(ui);
                        disabledTheme.colors.text = disabledTheme.colors.textDisabled;
                        auto disabledStyle = Mosaic::styleScope(ui, disabledTheme);
                        Mosaic::text(ui, uid);
                    }
                    Mosaic::separator(ui);

                    Mosaic::TableOptions tableOptions;
                    tableOptions.headers = false;
                    tableOptions.rowBackground = true;
                    tableOptions.scrollVertical = true;
                    Mosaic::LayoutOptions tableLayout = ExamplesDetail::fill();
                    auto table = Mosaic::table(ui, "Selected object properties", 2, tableOptions, tableLayout);
                    Mosaic::TableColumnOptions nameColumn;
                    nameColumn.sizing = Mosaic::TableSizing::FixedFit;
                    nameColumn.widthOrWeight = 110.f;
                    nameColumn.sortable = false;
                    Mosaic::tableSetupColumn(ui, 0, "Property", nameColumn);
                    Mosaic::TableColumnOptions valueColumn;
                    valueColumn.sizing = Mosaic::TableSizing::Stretch;
                    valueColumn.widthOrWeight = 2.f;
                    valueColumn.sortable = false;
                    Mosaic::tableSetupColumn(ui, 1, "Value", valueColumn);

                    auto nextProperty = [ui](size_t identity, Mosaic::StringView label)
                    {
                        Mosaic::tableNextRow(ui, Mosaic::Key(identity));
                        (void)Mosaic::tableSetColumn(ui, 0);
                        Mosaic::text(ui, label);
                        (void)Mosaic::tableSetColumn(ui, 1);
                    };
                    if(selected.hasData == true)
                    {
                        nextProperty(0, "MyName");
                        Mosaic::inputText(ui, "MyName", &selected.name);
                        nextProperty(1, "MyBool");
                        Mosaic::checkbox(ui, "MyBool", &selected.enabled);
                        nextProperty(2, "MyInt");
                        Mosaic::dragValue(ui, "MyInt", &selected.integers[0], std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max());
                        nextProperty(3, "MyVec2");
                        Mosaic::SliderOptions scalarOptions;
                        scalarOptions.minimum = 0.0;
                        scalarOptions.maximum = 1.0;
                        scalarOptions.width = Mosaic::SizeRule::Fill;
                        Mosaic::sliderFloatVector(ui, "MyVec2", Mosaic::FloatSpan(selected.scalars.data(), 2), scalarOptions);
                    }
                }
            }
        }

        if(m_showSimpleOverlay == true && m_overlayOpen == true)
        {
            Mosaic::Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
            constexpr float margin = 10.f;
            constexpr float overlayWidth = 310.f;
            constexpr float overlayHeight = 102.f;
            Mosaic::WindowOptions options;
            options.open = &m_overlayOpen;
            Mosaic::Array<Mosaic::Rect, 5> positions = {Mosaic::Rect{workArea.x + margin, workArea.y + margin, overlayWidth, overlayHeight}, Mosaic::Rect{workArea.right() - overlayWidth - margin, workArea.y + margin, overlayWidth, overlayHeight}, Mosaic::Rect{workArea.x + margin, workArea.bottom() - overlayHeight - margin, overlayWidth, overlayHeight}, Mosaic::Rect{workArea.right() - overlayWidth - margin, workArea.bottom() - overlayHeight - margin, overlayWidth, overlayHeight}, Mosaic::Rect{workArea.x + (workArea.width - overlayWidth) * 0.5f, workArea.y + (workArea.height - overlayHeight) * 0.5f, overlayWidth, overlayHeight}};
            options.initialBounds = m_overlayCorner >= 0 && m_overlayCorner < 5 ? positions[static_cast<size_t>(m_overlayCorner)] : positions[4];
            options.minimumSize = {240.f, 90.f};
            options.titleBar = false;
            options.movable = m_overlayCorner == 5;
            options.resizable = false;
            options.navigation = false;
            options.bringToFront = false;
            options.dockable = false;
            options.backgroundAlpha = 0.35f;
            auto window = Mosaic::window(ui, "Example: Simple overlay", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "Simple overlay\n(right-click to change position)");
                Mosaic::separator(ui);
                Mosaic::Vec2 pointerPosition;
                if(Mosaic::pointerPosition(ui, &pointerPosition) == false)
                {
                    Mosaic::text(ui, "Mouse Position: <invalid>");
                }
                else
                {
                    Mosaic::String position = "Mouse Position: (";
                    position += ExamplesDetail::fixed(pointerPosition.x, 1);
                    position += ", ";
                    position += ExamplesDetail::fixed(pointerPosition.y, 1);
                    position += ")";
                    Mosaic::text(ui, position);
                }
                const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
                Mosaic::Rect windowBounds;
                bool hasWindowBounds = Mosaic::debugBounds(ui, window.id(), &windowBounds);

                if(pointer != nullptr && hasWindowBounds == true && pointer->isReleased(Mosaic::PointerButton::Secondary) == true && windowBounds.contains(pointer->position) == true)
                {
                    Mosaic::openPopup(ui, Mosaic::Key("Overlay context"));
                }

                Mosaic::PopupOptions popupOptions;
                popupOptions.owner = window.id();
                popupOptions.placement = Mosaic::PopupPlacement::Cursor;
                auto popup = Mosaic::popup(ui, Mosaic::Key("Overlay context"), popupOptions);

                if(popup.visible() == true)
                {
                    constexpr Mosaic::Array<Mosaic::StringView, 6> labels = {"Custom", "Center", "Top-left", "Top-right", "Bottom-left", "Bottom-right"};
                    constexpr Mosaic::Array<int, 6> locations = {5, 4, 0, 1, 2, 3};
                    for(size_t index = 0; index != labels.size(); ++index)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        int location = locations[index];
                        Mosaic::MenuItemOptions menuOptions;
                        menuOptions.selected = m_overlayCorner == location;

                        if(Mosaic::menuItem(ui, labels[index], menuOptions).clicked() == true)
                        {
                            m_overlayCorner = location;

                            if(location >= 0 && static_cast<size_t>(location) < positions.size())
                            {
                                Mosaic::setWindowBounds(ui, window.id(), positions[static_cast<size_t>(location)]);
                            }

                            Mosaic::closeCurrentPopup(ui);
                        }
                    }
                    Mosaic::separator(ui);

                    if(Mosaic::menuItem(ui, "Close").clicked() == true)
                    {
                        m_overlayOpen = false;
                        Mosaic::closeCurrentPopup(ui);
                    }
                }
            }
        }

        if(m_showAutoResize == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showAutoResize;
            options.fitContentWidth = true;
            options.fitContentHeight = true;
            auto window = Mosaic::window(ui, "Example: Auto-resizing window", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "Window will resize every-frame to the size of its content.\nNote that you probably don't want to query the window size to\noutput your content because that would create a feedback loop.");
                Mosaic::slider(ui, "Number of lines", &m_autoResizeLines, int32_t{1}, int32_t{20});
                for(int32_t index = 0; index < m_autoResizeLines; ++index)
                {
                    auto item = Mosaic::scope(ui, Mosaic::Key(index));
                    Mosaic::String line(static_cast<size_t>(index * 4), ' ');
                    line += "This is line ";
                    line += ExamplesDetail::number(index);
                    Mosaic::text(ui, line);
                }
            }
        }

        if(m_showConstrainedResize == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showConstrainedResize;
            options.padding = m_constrainedWindowPadding;
            switch(m_resizeType)
            {
            case 0:
                options.minimumSize = {100.f, 100.f};
                options.maximumSize = {500.f, 500.f};
                break;
            case 1:
                options.minimumSize = {100.f, 100.f};
                break;
            case 2:
                options.resizeHorizontal = false;
                break;
            case 3:
                options.resizeVertical = false;
                break;
            case 4:
                options.minimumSize.x = 400.f;
                options.maximumSize.x = 500.f;
                break;
            case 5:
                options.minimumSize.y = 400.f;
                break;
            case 6:
                options.sizeConstraint = ExamplesDetail::aspectRatioConstraint;
                options.sizeConstraintUserData = &m_constrainedAspect;
                break;
            case 7:
                options.sizeConstraint = ExamplesDetail::squareConstraint;
                break;
            case 8:
                options.sizeConstraint = ExamplesDetail::stepConstraint;
                options.sizeConstraintUserData = &m_constrainedStep;
                break;
            default:
                break;
            }
            options.fitContentHeight = m_constrainedAutoResize;
            auto window = Mosaic::window(ui, "Example: Constrained Resize", options);

            if(window.visible() == true)
            {
                if(Mosaic::input(ui).modifiers.shift == true)
                {
                    Mosaic::LayoutOptions viewportLayout;
                    viewportLayout.width = Mosaic::SizeRule::Fill;
                    viewportLayout.height = Mosaic::SizeRule::Fill;
                    Mosaic::Canvas viewport = Mosaic::canvas(ui, "viewport", viewportLayout);
                    Mosaic::Rect bounds;
                    if(viewport.contentRect(&bounds) == true)
                    {
                        viewport.rect(bounds, Mosaic::Color{0.5f, 0.2f, 0.5f, 1.f});
                        Mosaic::String size = ExamplesDetail::fixed(bounds.width, 2);
                        size += " x ";
                        size += ExamplesDetail::fixed(bounds.height, 2);
                        Mosaic::text(ui, size);
                    }
                }
                else
                {
                    Mosaic::text(ui, "(Hold Shift to display a dummy viewport)");

                    if(Mosaic::dockNodeForWindow(ui, 1, window.id()) != Mosaic::InvalidId)
                    {
                        Mosaic::text(ui, "Warning: sizing constraints won't work while this window is docked!");
                    }

                    {
                        auto sizes = Mosaic::row(ui);
                        Mosaic::Rect current;
                        if(Mosaic::windowBounds(ui, window.id(), &current) == false)
                        {
                            current = options.initialBounds;
                        }

                        if(Mosaic::button(ui, "Set 200x200").clicked() == true)
                        {
                            Mosaic::setWindowBounds(ui, window.id(), {current.x, current.y, 200.f, 200.f});
                        }

                        if(Mosaic::button(ui, "Set 500x500").clicked() == true)
                        {
                            Mosaic::setWindowBounds(ui, window.id(), {current.x, current.y, 500.f, 500.f});
                        }

                        if(Mosaic::button(ui, "Set 800x200").clicked() == true)
                        {
                            Mosaic::setWindowBounds(ui, window.id(), {current.x, current.y, 800.f, 200.f});
                        }
                    }

                    constexpr Mosaic::Array<Mosaic::StringView, 9> constraints = {"Between 100x100 and 500x500", "At least 100x100", "Resize vertical + lock current width", "Resize horizontal + lock current height", "Width Between 400 and 500", "Height at least 400", "Custom: Aspect Ratio 16:9", "Custom: Always Square", "Custom: Fixed Steps (100)"};
                    Mosaic::comboBox(ui, "Constraint", &m_resizeType, constraints);
                    Mosaic::SliderOptions lineOptions;
                    lineOptions.minimum = 1.0;
                    lineOptions.maximum = 100.0;
                    lineOptions.dragSpeed = 0.2;
                    Mosaic::dragValue(ui, "Lines", &m_constrainedLines, int32_t{1}, int32_t{100}, lineOptions);
                    Mosaic::checkbox(ui, "Auto-resize", &m_constrainedAutoResize);
                    Mosaic::checkbox(ui, "Window padding", &m_constrainedWindowPadding);
                    for(int32_t line = 0; line < m_constrainedLines; ++line)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(line));
                        Mosaic::String text(static_cast<size_t>(line * 4), ' ');
                        text += "Hello, sailor! Making this line long enough for the example.";
                        Mosaic::text(ui, text);
                    }
                }
            }
        }

        if(m_showFullscreen == true)
        {
            Mosaic::Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
            Mosaic::Rect fullscreenArea = m_fullscreenUseWorkArea ? workArea : viewport.bounds;
            Mosaic::WindowOptions options;
            options.open = &m_showFullscreen;
            options.collapsed = m_fullscreenNoCollapse || m_fullscreenNoDecoration ? nullptr : &m_fullscreenCollapsed;
            options.initialBounds = fullscreenArea;
            options.minimumSize = {640.f, 360.f};
            options.movable = false;
            options.resizable = false;
            options.dockable = false;
            options.saveSettings = false;
            options.titleBar = m_fullscreenNoDecoration == false && m_fullscreenNoTitleBar == false;
            options.background = m_fullscreenNoBackground == false;
            options.scrollable = m_fullscreenNoDecoration == false && m_fullscreenNoScrollbar == false;
            auto window = Mosaic::window(ui, "Example: Fullscreen window", options);
            Mosaic::setWindowBounds(ui, window.id(), fullscreenArea);

            if(window.visible() == true)
            {
                {
                    auto workAreaOption = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "Use work area instead of main area", &m_fullscreenUseWorkArea);
                    Mosaic::helpMarker(ui, "Main area covers the complete viewport. Work area excludes reserved menu and system regions.");
                }
                Mosaic::checkbox(ui, "MosaicWindowFlags_NoBackground", &m_fullscreenNoBackground);
                Mosaic::checkbox(ui, "MosaicWindowFlags_NoDecoration", &m_fullscreenNoDecoration);
                {
                    Mosaic::LayoutOptions indented;
                    indented.padding.left = Mosaic::getTheme(ui).metrics.indent;
                    auto flags = Mosaic::column(ui, indented);
                    Mosaic::checkbox(ui, "MosaicWindowFlags_NoTitleBar", &m_fullscreenNoTitleBar);
                    Mosaic::checkbox(ui, "MosaicWindowFlags_NoCollapse", &m_fullscreenNoCollapse);
                    Mosaic::checkbox(ui, "MosaicWindowFlags_NoScrollbar", &m_fullscreenNoScrollbar);
                }

                if(Mosaic::button(ui, "Close this window").clicked() == true)
                {
                    m_showFullscreen = false;
                }
            }
        }

        if(m_showLongText == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showLongText;
            Mosaic::setNextWindowSize(ui, {520.f, 600.f}, Mosaic::Condition::FirstUse);
            auto window = Mosaic::window(ui, "Example: Long text display", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "Printing unusually long amount of text.");
                constexpr Mosaic::Array<Mosaic::StringView, 3> modes = {"Single call to TextUnformatted()", "Multiple calls to Text(), clipped", "Multiple calls to Text(), not clipped (slow)"};
                Mosaic::comboBox(ui, "Test type", &m_longTextMode, modes);

                if(m_longTextBufferLines != m_longTextLines)
                {
                    m_longTextBuffer.clear();
                    m_longTextBuffer.reserve(static_cast<size_t>(m_longTextLines) * 56U);
                    for(int32_t index = 0; index < m_longTextLines; ++index)
                    {
                        m_longTextBuffer += ExamplesDetail::number(index);
                        m_longTextBuffer += " The quick brown fox jumps over the lazy dog\n";
                    }
                    m_longTextBufferLines = m_longTextLines;
                }

                Mosaic::String statistics = "Buffer contents: ";
                statistics += ExamplesDetail::number(m_longTextLines);
                statistics += " lines, ";
                statistics += ExamplesDetail::number(m_longTextBuffer.size());
                statistics += " bytes";
                Mosaic::text(ui, statistics);

                {
                    auto actions = Mosaic::row(ui);

                    if(Mosaic::button(ui, "Clear").clicked() == true)
                    {
                        m_longTextLines = 0;
                    }

                    if(Mosaic::button(ui, "Add 1000 lines").clicked() == true)
                    {
                        m_longTextLines += 1000;
                    }
                }

                Mosaic::LayoutOptions contentLayout;
                contentLayout.width = Mosaic::SizeRule::Fill;
                contentLayout.height = Mosaic::SizeRule::Fill;
                auto content = Mosaic::scrollArea(ui, "Long text", Mosaic::ScrollOptions{}, contentLayout);
                float lineHeight = Mosaic::getTheme(ui).metrics.lineHeight;

                if(m_longTextMode == 0)
                {
                    Mosaic::text(ui, m_longTextBuffer);
                }
                else
                {
                    Mosaic::VisibleRange range = {0, static_cast<size_t>(m_longTextLines)};

                    if(m_longTextMode == 1)
                    {
                        (void)Mosaic::beginListClipper(ui, static_cast<size_t>(m_longTextLines), lineHeight, &range, content.id());
                    }

                    for(size_t index = range.begin; index < range.end; ++index)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::String line = ExamplesDetail::number(index);
                        line += " The quick brown fox jumps over the lazy dog";
                        Mosaic::text(ui, line);
                    }

                    if(m_longTextMode == 1)
                    {
                        Mosaic::endListClipper(ui, range, static_cast<size_t>(m_longTextLines), lineHeight);
                    }
                }
            }
        }

        if(m_showWindowTitles == true)
        {
            Mosaic::Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
            Mosaic::WindowOptions first;
            first.dockable = false;
            Mosaic::setNextWindowPosition(ui, {workArea.x + 100.f, workArea.y + 100.f}, Mosaic::Condition::FirstUse);
            auto firstWindow = Mosaic::window(ui, Mosaic::Key("SameTitle1"), "Same title as another window", first);

            if(firstWindow.visible() == true)
            {
                Mosaic::text(ui, "This is window 1.\nMy title is the same as window 2, but my identifier is unique.");
            }

            Mosaic::WindowOptions second;
            second.dockable = false;
            Mosaic::setNextWindowPosition(ui, {workArea.x + 100.f, workArea.y + 200.f}, Mosaic::Condition::FirstUse);
            auto secondWindow = Mosaic::window(ui, Mosaic::Key("SameTitle2"), "Same title as another window", second);

            if(secondWindow.visible() == true)
            {
                Mosaic::text(ui, "This is window 2.\nMy title is the same as window 1, but my identifier is unique.");
            }

            constexpr Mosaic::Array<char, 4> spinner = {'|', '/', '-', '\\'};
            size_t spinnerIndex = static_cast<size_t>(Mosaic::input(ui).timestamp / 0.25) % spinner.size();
            Mosaic::String animated = "Animated title ";
            animated.push_back(spinner[spinnerIndex]);
            animated += " ";
            animated += ExamplesDetail::fixed(Mosaic::input(ui).timestamp, 0);
            Mosaic::WindowOptions third;
            third.dockable = false;
            Mosaic::setNextWindowPosition(ui, {workArea.x + 100.f, workArea.y + 300.f}, Mosaic::Condition::FirstUse);
            auto animatedWindow = Mosaic::window(ui, Mosaic::Key("AnimatedTitle"), animated, third);

            if(animatedWindow.visible() == true)
            {
                Mosaic::text(ui, "This window has a changing title.");
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::drawToolWindows(Mosaic::Context * ui)
    {
        if(m_showMetrics == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showMetrics;
            auto window = Mosaic::window(ui, "Mosaic Metrics/Debugger", options);

            if(window.visible() == true)
            {
                const Mosaic::Frame & frame = Mosaic::getFrame(ui);

                if(m_metricsMemoryFrame != frame.number)
                {
                    size_t memoryBytes = frame.metrics.frameMemory + frame.metrics.persistentMemory;
                    m_metricsMemoryHistory[m_metricsMemoryOffset] = static_cast<float>(memoryBytes) / 1024.f;
                    m_metricsMemoryOffset = (m_metricsMemoryOffset + 1) % m_metricsMemoryHistory.size();
                    m_metricsMemoryFrame = frame.number;
                }

                Mosaic::ContextDebugSnapshot contextSummary;
                Mosaic::WindowDebugSnapshotVector windowSummary;
                (void)Mosaic::contextDebugSnapshot(ui, &contextSummary);
                (void)Mosaic::windowDebugSnapshots(ui, &windowSummary);
                float milliseconds = Mosaic::input(ui).deltaTime * 1000.f;
                float framesPerSecond = Mosaic::input(ui).deltaTime > 0.f ? 1.f / Mosaic::input(ui).deltaTime : 0.f;
                Mosaic::String overview = "Mosaic ";
                overview += Mosaic::Version;
                overview += ", ";
                overview += ExamplesDetail::fixed(milliseconds, 3);
                overview += " ms/frame (";
                overview += ExamplesDetail::fixed(framesPerSecond, 1);
                overview += " FPS)";
                Mosaic::text(ui, overview);
                Mosaic::String meshOverview = "Vertices ";
                meshOverview += ExamplesDetail::number(m_renderVertexCount);
                meshOverview += ", indices ";
                meshOverview += ExamplesDetail::number(m_renderIndexCount);
                meshOverview += ", triangles ";
                meshOverview += ExamplesDetail::number(m_renderIndexCount / 3);
                meshOverview += ", windows ";
                meshOverview += ExamplesDetail::number(windowSummary.size());
                meshOverview += ", state records ";
                meshOverview += ExamplesDetail::number(contextSummary.persistentEntryCount);
                Mosaic::text(ui, meshOverview);
                Mosaic::separatorText(ui, "Metrics");
                Mosaic::String widgets = "Widgets: ";
                widgets += ExamplesDetail::number(frame.metrics.widgetCount);
                widgets += " (visible ";
                widgets += ExamplesDetail::number(frame.metrics.visibleWidgetCount);
                widgets += ", culled ";
                widgets += ExamplesDetail::number(frame.metrics.culledWidgetCount);
                widgets += ")";
                Mosaic::text(ui, widgets);
                Mosaic::String textRuns = "Text runs: ";
                textRuns += ExamplesDetail::number(frame.metrics.textRunCount);
                textRuns += " (shaped ";
                textRuns += ExamplesDetail::number(frame.metrics.shapedTextRunCount);
                textRuns += ", deferred ";
                textRuns += ExamplesDetail::number(frame.metrics.deferredTextRunCount);
                textRuns += ")";
                Mosaic::text(ui, textRuns);
                Mosaic::String geometry = "Draw commands: ";
                geometry += ExamplesDetail::number(frame.metrics.drawCommandCount);
                geometry += ", vertices: ";
                geometry += ExamplesDetail::number(m_renderVertexCount);
                geometry += ", indices: ";
                geometry += ExamplesDetail::number(m_renderIndexCount);
                geometry += ", batches: ";
                geometry += ExamplesDetail::number(m_renderBatchCount);
                Mosaic::text(ui, geometry);
                Mosaic::String stateChanges = "State changes: clip ";
                stateChanges += ExamplesDetail::number(frame.metrics.clipChanges);
                stateChanges += ", texture ";
                stateChanges += ExamplesDetail::number(frame.metrics.textureChanges);
                Mosaic::text(ui, stateChanges);
                Mosaic::String retainedState = "State: ";
                retainedState += ExamplesDetail::number(frame.metrics.persistentStateCount);
                retainedState += " persistent entries, ";
                retainedState += ExamplesDetail::number(frame.renderStates.size());
                retainedState += " render states, ";
                retainedState += ExamplesDetail::number(frame.semantics.size());
                retainedState += " semantic nodes";
                Mosaic::text(ui, retainedState);
                Mosaic::String memory = "Memory: frame ";
                memory += ExamplesDetail::number(frame.metrics.frameMemory);
                memory += " bytes, persistent ";
                memory += ExamplesDetail::number(frame.metrics.persistentMemory);
                memory += " bytes";
                Mosaic::text(ui, memory);
                Mosaic::String diagnostics = "Diagnostics: ";
                diagnostics += ExamplesDetail::number(frame.diagnostics.size());
                diagnostics += ", events this frame: ";
                diagnostics += ExamplesDetail::number(frame.events.size());
                Mosaic::text(ui, diagnostics);
                {
                    auto tools = Mosaic::treeNode(ui, Mosaic::Key("metrics tools"), "Tools", true);

                    if(tools.expanded() == true)
                    {
                        Mosaic::checkbox(ui, "Show Debug Log", &m_showDebugLog);
                        Mosaic::checkbox(ui, "Show ID Stack Tool", &m_showIdStack);
                        {
                            auto pickerActions = Mosaic::line(ui);

                            if(Mosaic::button(ui, m_itemPickerArmed ? "Cancel item picker" : "Pick item").clicked() == true)
                            {
                                m_itemPickerArmed = m_itemPickerArmed == false;
                                Mosaic::setItemPickerEnabled(ui, m_itemPickerArmed);
                            }

                            if(Mosaic::button(ui, "Clear picker target").clicked() == true)
                            {
                                Mosaic::clearItemPicker(ui);
                                m_itemPickerTarget = Mosaic::InvalidId;
                                m_metricsSelectedNode = Mosaic::InvalidId;
                            }
                        }

                        Mosaic::ItemPickerState pickerState;
                        if(Mosaic::itemPickerState(ui, &pickerState) == true)
                        {
                            m_itemPickerArmed = pickerState.enabled;
                            Mosaic::Id target = pickerState.selected != Mosaic::InvalidId ? pickerState.selected : pickerState.hovered;

                            if(target != Mosaic::InvalidId)
                            {
                                m_itemPickerTarget = target;

                                if(pickerState.selected != Mosaic::InvalidId)
                                {
                                    m_metricsSelectedNode = pickerState.selected;
                                }

                                Mosaic::ItemDebugSnapshot item;

                                if(Mosaic::itemDebugSnapshot(ui, target, &item) == true)
                                {
                                    Mosaic::String value = "Target: ";
                                    value += item.type;
                                    value += " \"";
                                    value += item.label;
                                    value += "\" ";
                                    value += ExamplesDetail::hexadecimal(item.id);
                                    Mosaic::text(ui, value);
                                    Mosaic::text(ui, item.path);
                                }
                            }
                        }

                        Mosaic::separatorText(ui, "Visualize");
                        Mosaic::checkbox(ui, "Show windows rectangles", &m_metricsOverlayFlags[0]);
                        Mosaic::checkbox(ui, "Show draw command mesh", &m_metricsOverlayFlags[1]);
                        Mosaic::helpMarker(ui, "Shows the bounds of renderer batches composing the previous frame mesh.");
                        Mosaic::checkbox(ui, "Show clipping rectangles", &m_metricsOverlayFlags[2]);
                        Mosaic::checkbox(ui, "Show table rectangles", &m_metricsOverlayFlags[3]);
                        Mosaic::checkbox(ui, "Show group rectangles", &m_metricsOverlayFlags[4]);
                        Mosaic::separatorText(ui, "Debug validation");
                        Mosaic::Configuration configuration;

                        if(Mosaic::getConfiguration(ui, &configuration) == true)
                        {
                            Mosaic::Response onceResponse = Mosaic::checkbox(ui, "Debug Begin/End return value once", &configuration.debugBeginReturnValueOnce);
                            Mosaic::Response loopResponse = Mosaic::checkbox(ui, "Debug Begin/End return value loop", &configuration.debugBeginReturnValueLoop);
                            bool configurationChanged = onceResponse.changed();

                            if(loopResponse.changed() == true)
                            {
                                configurationChanged = true;
                            }

                            if(configurationChanged == true)
                            {
                                Mosaic::setConfiguration(ui, configuration);
                            }
                        }

                        bool debugBreakEnabled = Mosaic::debugBreakAvailable(ui);
                        {
                            auto unavailable = Mosaic::disabledScope(ui, debugBreakEnabled == false);

                            if(Mosaic::button(ui, "Debug break").clicked() == true)
                            {
                                (void)Mosaic::requestDebugBreak(ui);
                            }
                        }
                        Mosaic::helpMarker(ui, debugBreakEnabled == true ? "Invoke the application-provided debug-break callback." : "Disabled because the application did not provide a debug-break callback.");

                        Mosaic::separatorText(ui, "UTF-8 encoding viewer");
                        Mosaic::inputText(ui, "Text", &m_metricsEncodingInput);
                        constexpr char hexadecimalDigits[] = "0123456789ABCDEF";
                        for(size_t offset = 0; offset < m_metricsEncodingInput.size();)
                        {
                            char32_t decoded = U'\0';
                            size_t length = ExamplesDetail::decodeUtf8(m_metricsEncodingInput, offset, &decoded);

                            if(length == 0)
                            {
                                break;
                            }

                            auto characterScope = Mosaic::scope(ui, Mosaic::Key(offset));
                            Mosaic::String encoded = "offset ";
                            encoded += ExamplesDetail::number(offset);
                            encoded += ", bytes";
                            for(size_t byte = 0; byte != length; ++byte)
                            {
                                unsigned char value = static_cast<unsigned char>(m_metricsEncodingInput[offset + byte]);
                                encoded.push_back(' ');
                                encoded.push_back(hexadecimalDigits[value >> 4U]);
                                encoded.push_back(hexadecimalDigits[value & 0x0fU]);
                            }
                            encoded += ", codepoint ";
                            encoded += ExamplesDetail::codepoint(decoded);
                            encoded += ", glyph \"";
                            encoded.append(m_metricsEncodingInput.data() + offset, length);
                            encoded += '"';
                            Mosaic::text(ui, encoded);
                            offset += length;
                        }
                    }
                }

                auto drawNodeCategory = [this, ui, &frame](Mosaic::StringView identity, Mosaic::StringView title, Mosaic::StringView type)
                {
                    m_metricsNodeIndices.clear();
                    for(size_t index = 0; index != frame.debug.size(); ++index)
                    {
                        if(type.empty() == true || frame.debug[index].type == type)
                        {
                            m_metricsNodeIndices.push_back(index);
                        }
                    }
                    Mosaic::String label(title);
                    label += " (";
                    label += ExamplesDetail::number(m_metricsNodeIndices.size());
                    label += ")";
                    auto category = Mosaic::treeNode(ui, Mosaic::Key(identity), label);

                    if(category.expanded() == false)
                    {
                        return;
                    }

                    Mosaic::LayoutOptions listLayout;
                    listLayout.width = Mosaic::SizeRule::Fill;
                    listLayout.height = Mosaic::Dimension::fixed(190.f);
                    auto list = Mosaic::scrollArea(ui, identity, Mosaic::ScrollOptions{}, listLayout);
                    float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                    Mosaic::VisibleRange range;
                    (void)Mosaic::beginListClipper(ui, m_metricsNodeIndices.size(), itemHeight, &range, list.id());
                    for(size_t visible = range.begin; visible != range.end; ++visible)
                    {
                        size_t index = m_metricsNodeIndices[visible];
                        const Mosaic::DebugInfo & node = frame.debug[index];
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::String line(node.type);
                        line += ": ";
                        line += node.label;
                        line += " [";
                        line += ExamplesDetail::number(node.id);
                        line += "]";

                        if(node.zOrder != 0)
                        {
                            line += " z=";
                            line += ExamplesDetail::number(node.zOrder);
                        }

                        if(Mosaic::selectable(ui, Mosaic::Key(node.id), line, m_metricsSelectedNode == node.id).clicked() == true)
                        {
                            m_metricsSelectedNode = node.id;
                        }
                    }
                    Mosaic::endListClipper(ui, range, m_metricsNodeIndices.size(), itemHeight);
                };
                drawNodeCategory("metrics windows", "Windows", "Window");
                drawNodeCategory("metrics tables", "Tables", "Table");
                drawNodeCategory("metrics scroll areas", "Scrolling", "Scroll");
                drawNodeCategory("metrics all nodes", "All submitted nodes", {});

                {
                    Mosaic::String label = "Viewports (";
                    label += ExamplesDetail::number(frame.viewports.size());
                    label += ")";
                    auto viewports = Mosaic::treeNode(ui, Mosaic::Key("metrics viewports"), label);

                    if(viewports.expanded() == true)
                    {
                        for(size_t index = 0; index != frame.viewports.size(); ++index)
                        {
                            const Mosaic::FrameViewport & viewport = frame.viewports[index];
                            Mosaic::String line = "Viewport ";
                            line += ExamplesDetail::number(viewport.id);
                            line += ": ";
                            line += ExamplesDetail::fixed(viewport.bounds.width, 0);
                            line += "x";
                            line += ExamplesDetail::fixed(viewport.bounds.height, 0);
                            line += ", DPI ";
                            line += ExamplesDetail::fixed(viewport.dpiScale, 2);
                            line += ", commands ";
                            line += ExamplesDetail::number(viewport.drawCommandCount());
                            Mosaic::bulletText(ui, line);
                        }
                    }
                }
                {
                    auto drawLists = Mosaic::treeNode(ui, Mosaic::Key("metrics draw lists"), "Draw lists");

                    if(drawLists.expanded() == true)
                    {
                        Mosaic::DrawListDebugSnapshotVector drawListSnapshots;
                        (void)Mosaic::drawListDebugSnapshots(ui, &drawListSnapshots);

                        for(const Mosaic::DrawListDebugSnapshot & drawList : drawListSnapshots)
                        {
                            auto drawListScope = Mosaic::scope(ui, Mosaic::Key(drawList.viewport));
                            Mosaic::String summary = drawList.owner;
                            summary += " viewport=";
                            summary += ExamplesDetail::number(drawList.viewport);
                            summary += " commands=";
                            summary += ExamplesDetail::number(drawList.commandCount);
                            summary += " vertices=";
                            summary += ExamplesDetail::number(drawList.vertexCount);
                            summary += " indices=";
                            summary += ExamplesDetail::number(drawList.indexCount);
                            summary += " triangles=";
                            summary += ExamplesDetail::number(drawList.triangleCount);
                            Mosaic::bulletText(ui, summary);
                        }

                        if(m_renderMesh != nullptr)
                        {
                            auto meshRanges = Mosaic::treeNode(ui, Mosaic::Key("render mesh ranges"), "Render mesh ranges");

                            if(meshRanges.expanded() == true)
                            {
                                for(size_t batchIndex = 0; batchIndex != m_renderMesh->batches.size(); ++batchIndex)
                                {
                                    auto batchScope = Mosaic::scope(ui, Mosaic::Key(batchIndex));
                                    const Mosaic::RenderBatch & batch = m_renderMesh->batches[batchIndex];
                                    Mosaic::String range = "Batch ";
                                    range += ExamplesDetail::number(batchIndex);
                                    range += ": vertices [";
                                    range += ExamplesDetail::number(batch.vertexOffset);
                                    range += ", ";
                                    range += ExamplesDetail::number(batch.vertexOffset + batch.vertexCount);
                                    range += "), indices [";
                                    range += ExamplesDetail::number(batch.indexOffset);
                                    range += ", ";
                                    range += ExamplesDetail::number(batch.indexOffset + batch.indexCount);
                                    range += "), render key ";
                                    range += ExamplesDetail::number(batch.renderKey);
                                    Mosaic::bulletText(ui, range);
                                }
                            }
                        }

                        Mosaic::DrawCommandDebugSnapshotVector commandSnapshots;
                        (void)Mosaic::drawCommandDebugSnapshots(ui, &commandSnapshots);
                        for(size_t viewportIndex = 0; viewportIndex != frame.viewports.size(); ++viewportIndex)
                        {
                            const Mosaic::FrameViewport & viewport = frame.viewports[viewportIndex];
                            auto viewportScope = Mosaic::scope(ui, Mosaic::Key(viewport.id));
                            Mosaic::String viewportLabel = "Viewport ";
                            viewportLabel += ExamplesDetail::number(viewport.id);
                            viewportLabel += " (";
                            viewportLabel += ExamplesDetail::number(viewport.drawCommandCount());
                            viewportLabel += " commands)";
                            auto viewportNode = Mosaic::treeNode(ui, Mosaic::Key("draw commands"), viewportLabel);

                            if(viewportNode.expanded() == false)
                            {
                                continue;
                            }

                            m_metricsNodeIndices.clear();
                            for(size_t commandIndex = 0; commandIndex != commandSnapshots.size(); ++commandIndex)
                            {
                                if(commandSnapshots[commandIndex].viewport == viewport.id)
                                {
                                    m_metricsNodeIndices.push_back(commandIndex);
                                }
                            }

                            Mosaic::LayoutOptions listLayout;
                            listLayout.width = Mosaic::SizeRule::Fill;
                            listLayout.height = Mosaic::Dimension::fixed(180.f);
                            auto commandList = Mosaic::scrollArea(ui, "Draw command list", Mosaic::ScrollOptions{}, listLayout);
                            float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                            Mosaic::VisibleRange range;
                            (void)Mosaic::beginListClipper(ui, m_metricsNodeIndices.size(), itemHeight, &range, commandList.id());
                            for(size_t commandIndex = range.begin; commandIndex != range.end; ++commandIndex)
                            {
                                auto commandScope = Mosaic::scope(ui, Mosaic::Key(commandIndex));
                                const Mosaic::DrawCommandDebugSnapshot & command = commandSnapshots[m_metricsNodeIndices[commandIndex]];
                                Mosaic::String line = ExamplesDetail::number(command.index);
                                line += ": ";
                                line += command.type;
                                line += ", render key ";
                                line += ExamplesDetail::number(command.renderKey);
                                line += ", texture ";
                                line += ExamplesDetail::number(command.texture);
                                line += ", elements ";
                                line += ExamplesDetail::number(command.elementCount);
                                line += ", vertex offset ";
                                line += ExamplesDetail::number(command.vertexOffset);
                                line += ", index offset ";
                                line += ExamplesDetail::number(command.indexOffset);
                                line += ", vertices ";
                                line += ExamplesDetail::number(command.vertexCount);
                                line += ", indices ";
                                line += ExamplesDetail::number(command.indexCount);
                                line += ", triangles ";
                                line += ExamplesDetail::number(command.triangleCount);
                                Mosaic::Response commandResponse = Mosaic::text(ui, line);
                                Mosaic::String clip = "Clip ";
                                clip += ExamplesDetail::fixed(command.clip.x, 0);
                                clip += ",";
                                clip += ExamplesDetail::fixed(command.clip.y, 0);
                                clip += " ";
                                clip += ExamplesDetail::fixed(command.clip.width, 0);
                                clip += "x";
                                clip += ExamplesDetail::fixed(command.clip.height, 0);
                                clip += ", bounds ";
                                clip += ExamplesDetail::fixed(command.bounds.x, 0);
                                clip += ",";
                                clip += ExamplesDetail::fixed(command.bounds.y, 0);
                                clip += " ";
                                clip += ExamplesDetail::fixed(command.bounds.width, 0);
                                clip += "x";
                                clip += ExamplesDetail::fixed(command.bounds.height, 0);
                                Mosaic::itemTooltip(ui, commandResponse, clip);
                            }
                            Mosaic::endListClipper(ui, range, m_metricsNodeIndices.size(), itemHeight);
                        }
                    }
                }
                {
                    Mosaic::String renderStateLabel = "Render states (";
                    renderStateLabel += ExamplesDetail::number(frame.renderStates.size());
                    renderStateLabel += ")";
                    auto renderStates = Mosaic::treeNode(ui, Mosaic::Key("metrics render states"), renderStateLabel);

                    if(renderStates.expanded() == true)
                    {
                        for(size_t index = 0; index != frame.renderStates.size(); ++index)
                        {
                            auto stateScope = Mosaic::scope(ui, Mosaic::Key(index));
                            const Mosaic::RenderState & state = frame.renderStates[index];
                            Mosaic::String line = ExamplesDetail::number(index);
                            line += ": texture ";
                            line += ExamplesDetail::number(state.texture);
                            line += ", target ";
                            line += ExamplesDetail::number(state.renderTarget);
                            line += ", variant ";
                            line += ExamplesDetail::number(state.variant);
                            line += ", clip ";
                            line += ExamplesDetail::fixed(state.clip.x, 0);
                            line += ",";
                            line += ExamplesDetail::fixed(state.clip.y, 0);
                            line += " ";
                            line += ExamplesDetail::fixed(state.clip.width, 0);
                            line += "x";
                            line += ExamplesDetail::fixed(state.clip.height, 0);
                            Mosaic::bulletText(ui, line);
                        }
                    }
                }
                {
                    Mosaic::WindowDebugSnapshotVector windows;
                    (void)Mosaic::windowDebugSnapshots(ui, &windows);
                    Mosaic::String label = "Windows by submission/Z order (";
                    label += ExamplesDetail::number(windows.size());
                    label += ")";
                    auto windowOrder = Mosaic::treeNode(ui, Mosaic::Key("metrics window order"), label);

                    if(windowOrder.expanded() == true)
                    {
                        auto hierarchy = Mosaic::treeNode(ui, Mosaic::Key("window hierarchy"), "Hierarchy", true);

                        if(hierarchy.expanded() == true)
                        {
                            auto drawHierarchy = [&windows, ui](auto && self, Mosaic::Id parent) -> void
                            {
                                for(const Mosaic::WindowDebugSnapshot & snapshot : windows)
                                {
                                    if(snapshot.parent != parent)
                                    {
                                        continue;
                                    }

                                    auto windowNode = Mosaic::treeNode(ui, Mosaic::Key(snapshot.id), snapshot.label);

                                    if(windowNode.expanded() == true)
                                    {
                                        self(self, snapshot.id);
                                    }
                                }
                            };
                            drawHierarchy(drawHierarchy, Mosaic::InvalidId);
                        }

                        bool hasWindows = windows.empty() == false;
                        bool hasViewports = frame.viewports.empty() == false;

                        if(hasWindows == true && hasViewports == true)
                        {
                            Mosaic::LayoutOptions minimapLayout;
                            minimapLayout.width = Mosaic::Dimension::fixed(240.f);
                            minimapLayout.height = Mosaic::Dimension::fixed(140.f);
                            Mosaic::Canvas minimap = Mosaic::canvas(ui, Mosaic::Key("window minimap"), "Window minimap", minimapLayout);
                            const Mosaic::Rect & viewportBounds = frame.viewports.front().bounds;
                            float scaleX = viewportBounds.width > 0.f ? 240.f / viewportBounds.width : 1.f;
                            float scaleY = viewportBounds.height > 0.f ? 140.f / viewportBounds.height : 1.f;

                            for(const Mosaic::WindowDebugSnapshot & snapshot : windows)
                            {
                                Mosaic::Rect minimapBounds;
                                minimapBounds.x = (snapshot.bounds.x - viewportBounds.x) * scaleX;
                                minimapBounds.y = (snapshot.bounds.y - viewportBounds.y) * scaleY;
                                minimapBounds.width = snapshot.bounds.width * scaleX;
                                minimapBounds.height = snapshot.bounds.height * scaleY;
                                Mosaic::BoxStyle windowStyle;
                                windowStyle.fill = Mosaic::solidFill(snapshot.focused == true ? Mosaic::Color::fromBytes(64, 120, 190, 90) : Mosaic::Color::fromBytes(70, 78, 88, 80));
                                windowStyle.borderColor = snapshot.focused == true ? Mosaic::Color::fromBytes(90, 175, 255, 240) : Mosaic::Color::fromBytes(150, 160, 170, 170);
                                windowStyle.borderWidth = 1.f;
                                minimap.box(minimapBounds, windowStyle);
                            }
                        }

                        for(size_t index = 0; index != windows.size(); ++index)
                        {
                            const Mosaic::WindowDebugSnapshot & snapshot = windows[index];
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(snapshot.id));
                            Mosaic::String line = ExamplesDetail::number(index);
                            line += ": ";
                            line += snapshot.label;
                            line += " submit=";
                            line += ExamplesDetail::number(snapshot.submission);
                            line += " begin=";
                            line += ExamplesDetail::number(snapshot.beginOrder);
                            line += " focus=";
                            line += ExamplesDetail::number(snapshot.focusOrder);
                            line += " z=";
                            line += ExamplesDetail::number(snapshot.zOrder);
                            line += " parent=";
                            line += ExamplesDetail::number(snapshot.parent);
                            line += " children=";
                            line += ExamplesDetail::number(snapshot.childCount);
                            line += snapshot.visible == true ? " visible" : " hidden";
                            line += snapshot.focused == true ? " focused" : "";
                            line += snapshot.docked == true ? " docked" : " floating";
                            line += snapshot.active == true ? " active" : " inactive";
                            line += snapshot.skipped == true ? " skipped" : "";
                            Mosaic::bulletText(ui, line);
                            Mosaic::String geometry = "Bounds ";
                            geometry += ExamplesDetail::fixed(snapshot.bounds.x, 0);
                            geometry += ",";
                            geometry += ExamplesDetail::fixed(snapshot.bounds.y, 0);
                            geometry += " ";
                            geometry += ExamplesDetail::fixed(snapshot.bounds.width, 0);
                            geometry += "x";
                            geometry += ExamplesDetail::fixed(snapshot.bounds.height, 0);
                            geometry += ", scroll ";
                            geometry += ExamplesDetail::fixed(snapshot.scrollOffset.x, 0);
                            geometry += ",";
                            geometry += ExamplesDetail::fixed(snapshot.scrollOffset.y, 0);
                            Mosaic::text(ui, geometry);
                            Mosaic::String rectangles = "Inner ";
                            rectangles += ExamplesDetail::fixed(snapshot.innerBounds.width, 0);
                            rectangles += "x";
                            rectangles += ExamplesDetail::fixed(snapshot.innerBounds.height, 0);
                            rectangles += ", work ";
                            rectangles += ExamplesDetail::fixed(snapshot.workBounds.width, 0);
                            rectangles += "x";
                            rectangles += ExamplesDetail::fixed(snapshot.workBounds.height, 0);
                            rectangles += ", content ";
                            rectangles += ExamplesDetail::fixed(snapshot.content.width, 0);
                            rectangles += "x";
                            rectangles += ExamplesDetail::fixed(snapshot.content.height, 0);
                            rectangles += ", ideal ";
                            rectangles += ExamplesDetail::fixed(snapshot.contentIdeal.width, 0);
                            rectangles += "x";
                            rectangles += ExamplesDetail::fixed(snapshot.contentIdeal.height, 0);
                            Mosaic::text(ui, rectangles);
                            Mosaic::String clips = "Outer clipped ";
                            clips += ExamplesDetail::fixed(snapshot.outerRectClipped.width, 0);
                            clips += "x";
                            clips += ExamplesDetail::fixed(snapshot.outerRectClipped.height, 0);
                            clips += ", inner clip ";
                            clips += ExamplesDetail::fixed(snapshot.innerClipRect.width, 0);
                            clips += "x";
                            clips += ExamplesDetail::fixed(snapshot.innerClipRect.height, 0);
                            clips += ", content region ";
                            clips += ExamplesDetail::fixed(snapshot.contentRegionRect.width, 0);
                            clips += "x";
                            clips += ExamplesDetail::fixed(snapshot.contentRegionRect.height, 0);
                            clips += ", title bar ";
                            clips += ExamplesDetail::fixed(snapshot.titleBarRect.height, 0);
                            Mosaic::text(ui, clips);
                            Mosaic::String state = "Clip ";
                            state += ExamplesDetail::fixed(snapshot.clip.x, 0);
                            state += ",";
                            state += ExamplesDetail::fixed(snapshot.clip.y, 0);
                            state += " ";
                            state += ExamplesDetail::fixed(snapshot.clip.width, 0);
                            state += "x";
                            state += ExamplesDetail::fixed(snapshot.clip.height, 0);
                            state += snapshot.scrollbarHorizontal == true ? ", horizontal scrollbar" : "";
                            state += snapshot.scrollbarVertical == true ? ", vertical scrollbar" : "";
                            state += ", dock node ";
                            state += ExamplesDetail::number(snapshot.dockNode);
                            Mosaic::text(ui, state);
                        }
                    }
                }
                {
                    Mosaic::FontInfoVector fonts;
                    (void)Mosaic::availableFonts(ui, &fonts);
                    Mosaic::String label = "Fonts (";
                    label += ExamplesDetail::number(fonts.size());
                    label += ")";
                    auto fontList = Mosaic::treeNode(ui, Mosaic::Key("metrics fonts"), label);

                    if(fontList.expanded() == true)
                    {
                        for(size_t index = 0; index != fonts.size(); ++index)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));
                            Mosaic::String line = fonts[index].name;
                            line += ", size ";
                            float effectiveSize = Mosaic::getTheme(ui).metrics.fontSize * fonts[index].effectiveScale;
                            line += ExamplesDetail::fixed(effectiveSize, 1);
                            line += ", scale ";
                            line += ExamplesDetail::fixed(fonts[index].effectiveScale, 2);
                            Mosaic::bulletText(ui, line);
                        }
                    }
                }
                {
                    Mosaic::PopupDebugSnapshotVector popups;
                    (void)Mosaic::popupDebugSnapshots(ui, &popups);
                    Mosaic::String label = "Popup stack (";
                    label += ExamplesDetail::number(popups.size());
                    label += ")";
                    auto popupStack = Mosaic::treeNode(ui, Mosaic::Key("metrics popup stack"), label);

                    if(popupStack.expanded() == true)
                    {
                        for(size_t index = 0; index != popups.size(); ++index)
                        {
                            const Mosaic::PopupDebugSnapshot & popup = popups[index];
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(popup.id));
                            Mosaic::String line = "Level ";
                            line += ExamplesDetail::number(popup.level);
                            line += ": ID ";
                            line += ExamplesDetail::number(popup.id);
                            line += ", owner ";
                            line += ExamplesDetail::number(popup.owner);
                            line += ", parent ";
                            line += ExamplesDetail::number(popup.parent);
                            line += ", restore focus ";
                            line += ExamplesDetail::number(popup.restoreFocus);
                            line += ", window ";
                            line += ExamplesDetail::number(popup.window);
                            line += popup.modal == true ? " modal" : " popup";
                            line += popup.focused == true ? " focused" : "";
                            Mosaic::bulletText(ui, line);
                            Mosaic::String bounds = "Bounds ";
                            bounds += ExamplesDetail::fixed(popup.bounds.x, 0);
                            bounds += ",";
                            bounds += ExamplesDetail::fixed(popup.bounds.y, 0);
                            bounds += " ";
                            bounds += ExamplesDetail::fixed(popup.bounds.width, 0);
                            bounds += "x";
                            bounds += ExamplesDetail::fixed(popup.bounds.height, 0);
                            Mosaic::text(ui, bounds);
                        }
                    }
                }
                {
                    Mosaic::TabBarDebugSnapshotVector tabBars;
                    (void)Mosaic::tabBarDebugSnapshots(ui, &tabBars);
                    Mosaic::String label = "Tab bars (";
                    label += ExamplesDetail::number(tabBars.size());
                    label += ")";
                    auto tabs = Mosaic::treeNode(ui, Mosaic::Key("metrics tab bars"), label);

                    if(tabs.expanded() == true)
                    {
                        for(const Mosaic::TabBarDebugSnapshot & tabBar : tabBars)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(tabBar.id));
                            Mosaic::String line = "ID ";
                            line += ExamplesDetail::number(tabBar.id);
                            line += ", items ";
                            line += ExamplesDetail::number(tabBar.itemCount);
                            line += ", visible ";
                            line += ExamplesDetail::number(tabBar.visibleCount);
                            line += ", selected ";
                            line += ExamplesDetail::number(tabBar.selected);
                            line += ", scroll ";
                            line += ExamplesDetail::fixed(tabBar.scrollOffset.x, 0);
                            line += "/";
                            line += ExamplesDetail::fixed(tabBar.scrollRange.x, 0);
                            Mosaic::bulletText(ui, line);
                            for(const Mosaic::TabItemDebugSnapshot & item : tabBar.items)
                            {
                                auto tabScope = Mosaic::scope(ui, Mosaic::Key(item.id));
                                Mosaic::String itemLine = "#";
                                itemLine += ExamplesDetail::number(item.order);
                                itemLine += " id=";
                                itemLine += ExamplesDetail::number(item.id);
                                itemLine += " offset=";
                                itemLine += ExamplesDetail::fixed(item.offset, 0);
                                itemLine += " width=";
                                itemLine += ExamplesDetail::fixed(item.width, 0);
                                itemLine += item.selected == true ? " selected" : "";
                                itemLine += item.visible == true ? " visible" : " clipped";
                                Mosaic::bulletText(ui, itemLine);
                            }
                        }
                    }
                }
                {
                    Mosaic::TableDebugSnapshotVector tables;
                    (void)Mosaic::tableDebugSnapshots(ui, &tables);
                    Mosaic::String label = "Tables (";
                    label += ExamplesDetail::number(tables.size());
                    label += ")";
                    auto tableList = Mosaic::treeNode(ui, Mosaic::Key("metrics table snapshots"), label);

                    if(tableList.expanded() == true)
                    {
                        for(const Mosaic::TableDebugSnapshot & table : tables)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(table.table));
                            Mosaic::String line = "ID ";
                            line += ExamplesDetail::number(table.table);
                            line += ", columns ";
                            line += ExamplesDetail::number(table.columnCount);
                            line += ", rows ";
                            line += ExamplesDetail::number(table.rowCount);
                            line += table.rowsVirtualized == true ? " clipped" : " submitted";
                            line += table.active == true ? " active" : " inactive";
                            line += table.noClip == true ? " no-clip" : " clipped-cells";
                            line += table.legacyColumns == true ? " legacy-columns" : " table";
                            Mosaic::bulletText(ui, line);
                            Mosaic::String details = "Settings ";
                            details += ExamplesDetail::number(table.settings);
                            details += ", instances ";
                            details += ExamplesDetail::number(table.instanceCount);
                            details += ", current row ";
                            details += ExamplesDetail::number(table.currentRow);
                            details += ", column ";
                            details += ExamplesDetail::number(table.currentColumn);
                            details += ", headers ";
                            details += ExamplesDetail::number(table.headerRowCount);
                            details += table.angledHeadersSubmitted == true ? " angled" : " horizontal";
                            details += table.sortSpecsDirty == true ? ", sort dirty" : ", sort stable";
                            details += table.displayOrderDirty == true ? ", display order dirty" : ", display order stable";
                            Mosaic::text(ui, details);
                            Mosaic::String scrolling = "Scroll ";
                            scrolling += ExamplesDetail::fixed(table.scrollOffset.x, 0);
                            scrolling += "/";
                            scrolling += ExamplesDetail::fixed(table.scrollRange.x, 0);
                            scrolling += ", ";
                            scrolling += ExamplesDetail::fixed(table.scrollOffset.y, 0);
                            scrolling += "/";
                            scrolling += ExamplesDetail::fixed(table.scrollRange.y, 0);
                            scrolling += ", frozen ";
                            scrolling += ExamplesDetail::number(table.frozenColumns);
                            scrolling += "x";
                            scrolling += ExamplesDetail::number(table.frozenRows);
                            scrolling += ", virtual rows ";
                            scrolling += ExamplesDetail::number(table.virtualRowCount);
                            Mosaic::text(ui, scrolling);
                            Mosaic::String rectangles = "Bounds ";
                            rectangles += ExamplesDetail::fixed(table.bounds.x, 0);
                            rectangles += ",";
                            rectangles += ExamplesDetail::fixed(table.bounds.y, 0);
                            rectangles += " ";
                            rectangles += ExamplesDetail::fixed(table.bounds.width, 0);
                            rectangles += "x";
                            rectangles += ExamplesDetail::fixed(table.bounds.height, 0);
                            rectangles += ", content ";
                            rectangles += ExamplesDetail::fixed(table.contentBounds.width, 0);
                            rectangles += "x";
                            rectangles += ExamplesDetail::fixed(table.contentBounds.height, 0);
                            rectangles += ", clip ";
                            rectangles += ExamplesDetail::fixed(table.clipBounds.width, 0);
                            rectangles += "x";
                            rectangles += ExamplesDetail::fixed(table.clipBounds.height, 0);
                            Mosaic::text(ui, rectangles);
                            Mosaic::String rows = "Rows measured ";
                            rows += ExamplesDetail::number(table.rowHeights.size());
                            rows += ", positions ";
                            rows += ExamplesDetail::number(table.rowPositions.size());
                            rows += ", virtual first ";
                            rows += ExamplesDetail::number(table.virtualFirstRow);
                            rows += ", virtual height ";
                            rows += ExamplesDetail::fixed(table.virtualRowHeight, 1);
                            Mosaic::text(ui, rows);
                            Mosaic::String interaction = "Dragging column ";
                            interaction += table.draggingColumn == std::numeric_limits<uint32_t>::max() ? "none" : ExamplesDetail::number(table.draggingColumn);
                            interaction += ", context column ";
                            interaction += table.contextColumn == std::numeric_limits<uint32_t>::max() ? "none" : ExamplesDetail::number(table.contextColumn);
                            Mosaic::text(ui, interaction);

                            for(const Mosaic::TableSortSpec & specification : table.sortSpecifications)
                            {
                                auto sortScope = Mosaic::scope(ui, Mosaic::Key(specification.order));
                                Mosaic::String sortLine = "Sort #";
                                sortLine += ExamplesDetail::number(specification.order);
                                sortLine += ": column ";
                                sortLine += ExamplesDetail::number(specification.column);
                                sortLine += ", user ";
                                sortLine += ExamplesDetail::number(specification.userId);
                                sortLine += specification.direction == Mosaic::SortDirection::Ascending ? " ascending" : specification.direction == Mosaic::SortDirection::Descending ? " descending" : " none";
                                Mosaic::bulletText(ui, sortLine);
                            }

                            for(const Mosaic::TableColumnDebugSnapshot & column : table.columns)
                            {
                                auto columnScope = Mosaic::scope(ui, Mosaic::Key(column.displayOrder));
                                Mosaic::String columnLine = column.label;
                                columnLine += " user=";
                                columnLine += ExamplesDetail::number(column.userId);
                                columnLine += " width=";
                                columnLine += ExamplesDetail::fixed(column.width, 1);
                                columnLine += " order=";
                                columnLine += ExamplesDetail::number(column.displayOrder);
                                columnLine += column.visible == true ? " visible" : " hidden";
                                columnLine += column.sorted == true ? " sorted" : "";
                                Mosaic::bulletText(ui, columnLine);
                            }
                        }
                    }
                }
                {
                    Mosaic::SelectionDebugSnapshotVector selections;
                    (void)Mosaic::selectionDebugSnapshots(ui, &selections);
                    Mosaic::String label = "Typing-select and multi-select state (";
                    label += ExamplesDetail::number(selections.size());
                    label += ")";
                    auto selectionState = Mosaic::treeNode(ui, Mosaic::Key("metrics selection state"), label);

                    if(selectionState.expanded() == true)
                    {
                        for(const Mosaic::SelectionDebugSnapshot & selection : selections)
                        {
                            auto itemScope = Mosaic::scope(ui, Mosaic::Key(selection.node));
                            Mosaic::String line = "Item ";
                            line += ExamplesDetail::number(selection.item);
                            line += ", scope ";
                            line += ExamplesDetail::number(selection.scope);
                            line += selection.selected == true ? " selected" : " not selected";
                            line += selection.boxSelecting == true ? " box-selecting" : "";
                            Mosaic::bulletText(ui, line);
                            Mosaic::String bounds = "Bounds ";
                            bounds += ExamplesDetail::fixed(selection.bounds.x, 0);
                            bounds += ",";
                            bounds += ExamplesDetail::fixed(selection.bounds.y, 0);
                            bounds += " ";
                            bounds += ExamplesDetail::fixed(selection.bounds.width, 0);
                            bounds += "x";
                            bounds += ExamplesDetail::fixed(selection.bounds.height, 0);

                            if(selection.boxSelecting == true)
                            {
                                bounds += ", box ";
                                bounds += ExamplesDetail::fixed(selection.boxBounds.x, 0);
                                bounds += ",";
                                bounds += ExamplesDetail::fixed(selection.boxBounds.y, 0);
                                bounds += " ";
                                bounds += ExamplesDetail::fixed(selection.boxBounds.width, 0);
                                bounds += "x";
                                bounds += ExamplesDetail::fixed(selection.boxBounds.height, 0);
                            }

                            Mosaic::text(ui, bounds);
                        }
                    }
                }
                {
                    Mosaic::DockDebugSnapshotVector docking;
                    (void)Mosaic::dockDebugSnapshots(ui, &docking);
                    Mosaic::String label = "Docking (";
                    label += ExamplesDetail::number(docking.size());
                    label += ")";
                    auto dockingState = Mosaic::treeNode(ui, Mosaic::Key("metrics docking state"), label);

                    if(dockingState.expanded() == true)
                    {
                        for(const Mosaic::DockDebugSnapshot & dock : docking)
                        {
                            auto dockScope = Mosaic::scope(ui, Mosaic::Key(dock.group));
                            Mosaic::String line = "Group ";
                            line += ExamplesDetail::number(dock.group);
                            line += ": root ";
                            line += ExamplesDetail::number(dock.root);
                            line += ", central ";
                            line += ExamplesDetail::number(dock.central);
                            line += ", nodes ";
                            line += ExamplesDetail::number(dock.nodes.size());
                            line += ", windows ";
                            line += ExamplesDetail::number(dock.windowCount);
                            Mosaic::bulletText(ui, line);
                            Mosaic::String area = "Area ";
                            area += ExamplesDetail::fixed(dock.area.x, 0);
                            area += ",";
                            area += ExamplesDetail::fixed(dock.area.y, 0);
                            area += " ";
                            area += ExamplesDetail::fixed(dock.area.width, 0);
                            area += "x";
                            area += ExamplesDetail::fixed(dock.area.height, 0);
                            Mosaic::text(ui, area);
                            for(const Mosaic::DockNode & node : dock.nodes)
                            {
                                auto nodeScope = Mosaic::scope(ui, Mosaic::Key(node.id));
                                Mosaic::String nodeLine = "Node ";
                                nodeLine += ExamplesDetail::number(node.id);
                                nodeLine += node.type == Mosaic::DockNodeType::Tabs ? " tabs" : " split";
                                nodeLine += ", parent ";
                                nodeLine += ExamplesDetail::number(node.parent);
                                nodeLine += ", items ";
                                nodeLine += ExamplesDetail::number(node.tabs.size());
                                Mosaic::bulletText(ui, nodeLine);
                            }
                        }
                    }
                }
                {
                    Mosaic::GroupDebugSnapshotVector groups;
                    (void)Mosaic::groupDebugSnapshots(ui, &groups);
                    Mosaic::String label = "Groups and legacy columns (";
                    label += ExamplesDetail::number(groups.size());
                    label += ")";
                    auto groupState = Mosaic::treeNode(ui, Mosaic::Key("metrics group state"), label);

                    if(groupState.expanded() == true)
                    {
                        for(const Mosaic::GroupDebugSnapshot & group : groups)
                        {
                            auto groupScope = Mosaic::scope(ui, Mosaic::Key(group.id));
                            Mosaic::String line(group.type);
                            line += " id=";
                            line += ExamplesDetail::number(group.id);
                            line += " parent=";
                            line += ExamplesDetail::number(group.parent);
                            line += " children=";
                            line += ExamplesDetail::number(group.childCount);
                            line += group.visible == true ? " visible" : " clipped";
                            Mosaic::bulletText(ui, line);
                            Mosaic::String bounds = "Bounds ";
                            bounds += ExamplesDetail::fixed(group.bounds.x, 0);
                            bounds += ",";
                            bounds += ExamplesDetail::fixed(group.bounds.y, 0);
                            bounds += " ";
                            bounds += ExamplesDetail::fixed(group.bounds.width, 0);
                            bounds += "x";
                            bounds += ExamplesDetail::fixed(group.bounds.height, 0);
                            bounds += ", clip ";
                            bounds += ExamplesDetail::fixed(group.clip.width, 0);
                            bounds += "x";
                            bounds += ExamplesDetail::fixed(group.clip.height, 0);
                            Mosaic::text(ui, bounds);
                        }
                    }
                }
                {
                    auto settings = Mosaic::treeNode(ui, Mosaic::Key("metrics settings"), "Settings");

                    if(settings.expanded() == true)
                    {
                        Mosaic::ContextDebugSnapshot contextSnapshot;

                        if(Mosaic::contextDebugSnapshot(ui, &contextSnapshot) == true)
                        {
                            Mosaic::String contextState = "Persistent entries ";
                            contextState += ExamplesDetail::number(contextSnapshot.persistentEntryCount);
                            contextState += ", window settings ";
                            contextState += ExamplesDetail::number(contextSnapshot.windowSettingCount);
                            contextState += ", table settings ";
                            contextState += ExamplesDetail::number(contextSnapshot.tableSettingCount);
                            contextState += ", dock models ";
                            contextState += ExamplesDetail::number(contextSnapshot.dockModelCount);
                            Mosaic::bulletText(ui, contextState);
                            Mosaic::String editorState = "State: tabs ";
                            editorState += ExamplesDetail::number(contextSnapshot.tabBarStateCount);
                            editorState += ", text editors ";
                            editorState += ExamplesDetail::number(contextSnapshot.textEditorStateCount);
                            editorState += ", color editors ";
                            editorState += ExamplesDetail::number(contextSnapshot.colorEditorStateCount);
                            editorState += ", selection items ";
                            editorState += ExamplesDetail::number(contextSnapshot.selectionItemCount);
                            Mosaic::bulletText(ui, editorState);
                            Mosaic::String cacheState = "Text cache entries ";
                            cacheState += ExamplesDetail::number(contextSnapshot.textCacheEntryCount);
                            cacheState += ", memory ";
                            cacheState += ExamplesDetail::number(contextSnapshot.textCacheMemory);
                            cacheState += " bytes, frame styles ";
                            cacheState += ExamplesDetail::number(contextSnapshot.frameStyleCount);
                            Mosaic::bulletText(ui, cacheState);
                            Mosaic::String settingsState = "Settings records ";
                            settingsState += ExamplesDetail::number(contextSnapshot.settings.size());
                            settingsState += ", memory ";
                            settingsState += ExamplesDetail::number(contextSnapshot.settingsMemory);
                            settingsState += " bytes";
                            Mosaic::bulletText(ui, settingsState);
                            for(size_t settingIndex = 0; settingIndex != contextSnapshot.settings.size(); ++settingIndex)
                            {
                                auto settingScope = Mosaic::scope(ui, Mosaic::Key(settingIndex));
                                const Mosaic::SettingDebugSnapshot & setting = contextSnapshot.settings[settingIndex];
                                Mosaic::String settingLine(setting.type);
                                settingLine += " ";
                                settingLine += ExamplesDetail::hexadecimal(setting.id);
                                settingLine += ", frame ";
                                settingLine += ExamplesDetail::number(setting.lastFrame);
                                settingLine += ", memory ";
                                settingLine += ExamplesDetail::number(setting.memory);
                                settingLine += setting.active == true ? ", active" : ", inactive";
                                Mosaic::text(ui, settingLine);
                            }
                            Mosaic::String interactionState = "Active ";
                            interactionState += ExamplesDetail::hexadecimal(contextSnapshot.activeItem);
                            interactionState += ", pointer focus ";
                            interactionState += ExamplesDetail::hexadecimal(contextSnapshot.pointerFocusedItem);
                            interactionState += ", keyboard focus ";
                            interactionState += ExamplesDetail::hexadecimal(contextSnapshot.keyboardFocusedItem);
                            interactionState += ", navigation focus ";
                            interactionState += ExamplesDetail::hexadecimal(contextSnapshot.navigationFocusedItem);
                            interactionState += ", captured ";
                            interactionState += ExamplesDetail::hexadecimal(contextSnapshot.capturedItem);
                            Mosaic::bulletText(ui, interactionState);
                            Mosaic::String windowingState = "Hovered window ";
                            windowingState += ExamplesDetail::hexadecimal(contextSnapshot.hoveredWindow);
                            windowingState += ", moving ";
                            windowingState += ExamplesDetail::hexadecimal(contextSnapshot.movingWindow);
                            windowingState += ", resizing ";
                            windowingState += ExamplesDetail::hexadecimal(contextSnapshot.resizingWindow);
                            windowingState += ", docking drag ";
                            windowingState += ExamplesDetail::hexadecimal(contextSnapshot.dockingDragWindow);
                            windowingState += ", wheel owner ";
                            windowingState += ExamplesDetail::hexadecimal(contextSnapshot.wheelOwner);
                            Mosaic::bulletText(ui, windowingState);
                            Mosaic::String dragDropState = "Drag/drop phase ";
                            dragDropState += ExamplesDetail::number(static_cast<uint32_t>(contextSnapshot.dragDropPhase));
                            dragDropState += ", source ";
                            dragDropState += ExamplesDetail::hexadecimal(contextSnapshot.dragDropSource);
                            dragDropState += ", target ";
                            dragDropState += ExamplesDetail::hexadecimal(contextSnapshot.dragDropTarget);
                            Mosaic::bulletText(ui, dragDropState);
                        }

                        Mosaic::Configuration configuration;

                        if(Mosaic::getConfiguration(ui, &configuration) == true)
                        {
                            Mosaic::bulletText(ui, configuration.settingsSaveLastUsedDate == true ? "Settings metadata: last-used date enabled" : "Settings metadata: last-used date disabled");
                            Mosaic::bulletText(ui, configuration.debugIniSettings == true ? "Settings diagnostics: enabled" : "Settings diagnostics: disabled");
                            Mosaic::bulletText(ui, configuration.errorRecovery == true ? "Error recovery: enabled" : "Error recovery: disabled");
                            Mosaic::bulletText(ui, configuration.debugHighlightIdConflicts == true ? "Identifier conflict highlighting: enabled" : "Identifier conflict highlighting: disabled");
                        }

                        Mosaic::ColorEditOptions colorDefaults;

                        if(Mosaic::colorEditDefaults(ui, &colorDefaults) == true)
                        {
                            Mosaic::String colorSettings = "Color editor defaults: ";
                            colorSettings += colorDefaults.alphaOpaque == true ? "opaque alpha" : "editable alpha";
                            colorSettings += colorDefaults.showInputs == true ? ", numeric inputs" : ", preview only";
                            Mosaic::bulletText(ui, colorSettings);
                        }

                        const Mosaic::Theme & theme = Mosaic::getTheme(ui);
                        Mosaic::String styleSettings = "Style: font ";
                        styleSettings += ExamplesDetail::fixed(theme.metrics.fontSize, 1);
                        styleSettings += " px, scale ";
                        styleSettings += ExamplesDetail::fixed(Mosaic::fontScale(ui), 2);
                        Mosaic::bulletText(ui, styleSettings);
                        float memoryMaximum = 1.f;

                        for(float value : m_metricsMemoryHistory)
                        {
                            memoryMaximum = std::max(memoryMaximum, value);
                        }

                        Mosaic::PlotOptions memoryPlot;
                        memoryPlot.height = 72.f;
                        memoryPlot.minimum = 0.f;
                        memoryPlot.maximum = memoryMaximum;
                        memoryPlot.offset = m_metricsMemoryOffset;
                        memoryPlot.overlay = "Context memory history (KiB)";
                        Mosaic::plotLines(ui, "Memory history", m_metricsMemoryHistory, memoryPlot);
                    }
                }
                for(const Mosaic::DebugInfo & node : frame.debug)
                {
                    if(node.id != m_metricsSelectedNode)
                    {
                        continue;
                    }

                    auto selected = Mosaic::treeNode(ui, Mosaic::Key("metrics selected node"), "Selected item", true);

                    if(selected.expanded() == true)
                    {
                        Mosaic::String state = "Type: ";
                        state += node.type;
                        state += ", hovered/active/focused/disabled: ";
                        state += node.hovered ? "1/" : "0/";
                        state += node.active ? "1/" : "0/";
                        state += node.focused ? "1/" : "0/";
                        state += node.disabled ? "1" : "0";
                        Mosaic::text(ui, state);
                        Mosaic::String identity = "Parent seed: ";
                        identity += ExamplesDetail::number(node.parentId);
                        identity += ", result ID: ";
                        identity += ExamplesDetail::number(node.id);
                        Mosaic::text(ui, identity);
                        Mosaic::String zOrder = "Window z-order: ";
                        zOrder += ExamplesDetail::number(node.zOrder);
                        Mosaic::text(ui, zOrder);
                        Mosaic::String bounds = "Bounds: ";
                        bounds += ExamplesDetail::fixed(node.bounds.x, 1);
                        bounds += ", ";
                        bounds += ExamplesDetail::fixed(node.bounds.y, 1);
                        bounds += ", ";
                        bounds += ExamplesDetail::fixed(node.bounds.width, 1);
                        bounds += "x";
                        bounds += ExamplesDetail::fixed(node.bounds.height, 1);
                        Mosaic::text(ui, bounds);
                        Mosaic::String source(node.file);
                        source += ":";
                        source += ExamplesDetail::number(node.line);
                        Mosaic::text(ui, source);
                        Mosaic::text(ui, node.path);
                    }

                    break;
                }
            }
        }

        if(m_showMetrics == true && (m_metricsOverlayFlags[0] == true || m_metricsOverlayFlags[1] == true || m_metricsOverlayFlags[2] == true || m_metricsOverlayFlags[3] == true || m_metricsOverlayFlags[4] == true))
        {
            Mosaic::LayoutOptions overlayLayout;
            overlayLayout.width = Mosaic::Dimension::fixed(1.f);
            overlayLayout.height = Mosaic::Dimension::fixed(1.f);
            Mosaic::Canvas overlay = Mosaic::canvas(ui, "Metrics overlays", overlayLayout);
            overlay.setLayer(Mosaic::CanvasLayer::Foreground);
            Mosaic::Rect overlayBounds;
            if(Mosaic::debugBounds(ui, overlay.id(), &overlayBounds) == true)
            {
                Mosaic::Vec2 origin = {overlayBounds.x, overlayBounds.y};
                auto drawBounds = [&overlay, &origin](const Mosaic::Rect & source, const Mosaic::Color & color, float width)
                {
                    Mosaic::Rect bounds = {source.x - origin.x, source.y - origin.y, source.width, source.height};
                    Mosaic::BoxStyle outline;
                    outline.fill = Mosaic::solidFill({0.f, 0.f, 0.f, 0.f});
                    outline.borderColor = color;
                    outline.borderWidth = width;
                    overlay.box(bounds, outline);
                };

                if(m_metricsOverlayFlags[1] == true && m_renderMesh != nullptr)
                {
                    for(const Mosaic::RenderBatch & batch : m_renderMesh->batches)
                    {
                        drawBounds(batch.bounds, Mosaic::Color::fromBytes(70, 215, 255, 150), 1.f);
                    }
                }

                for(const Mosaic::DebugInfo & node : Mosaic::getFrame(ui).debug)
                {
                    bool selected = node.id == m_metricsSelectedNode;
                    bool drawWindow = m_metricsOverlayFlags[0] && node.type == "Window";
                    bool drawClip = m_metricsOverlayFlags[2] && selected;

                    if(drawWindow == true)
                    {
                        drawBounds(node.bounds, Mosaic::Color::fromBytes(255, 210, 40, 220), 1.f);
                    }

                    if(drawClip == true)
                    {
                        drawBounds(node.clip, Mosaic::Color::fromBytes(255, 80, 170, 230), 2.f);
                    }
                }

                if(m_metricsOverlayFlags[3] == true)
                {
                    Mosaic::TableDebugSnapshotVector tables;
                    (void)Mosaic::tableDebugSnapshots(ui, &tables);

                    for(const Mosaic::TableDebugSnapshot & table : tables)
                    {
                        drawBounds(table.bounds, Mosaic::Color::fromBytes(255, 126, 68, 220), 1.f);
                    }
                }

                if(m_metricsOverlayFlags[4] == true)
                {
                    Mosaic::GroupDebugSnapshotVector groups;
                    (void)Mosaic::groupDebugSnapshots(ui, &groups);

                    for(const Mosaic::GroupDebugSnapshot & group : groups)
                    {
                        drawBounds(group.bounds, Mosaic::Color::fromBytes(125, 225, 118, 180), 1.f);
                    }
                }
            }
        }

        if(m_showDebugLog == true)
        {
            const Mosaic::Frame & debugFrame = Mosaic::getFrame(ui);
            static bool outputToConsole = false;
            bool appendedEvents = false;
            auto appendDebugEvent = [this, ui, &appendedEvents](DebugDemoCategory category, Mosaic::Id id, Mosaic::String line)
            {
                if(outputToConsole == true)
                {
                    Mosaic::String consoleLine = line;
                    consoleLine += '\n';
                    (void)Mosaic::platform(ui).writeConsole(consoleLine);
                }

                constexpr size_t eventCapacity = 4096;

                if(m_debugEventItems.size() < eventCapacity)
                {
                    m_debugEventItems.push_back({category, id, std::move(line)});
                }
                else
                {
                    m_debugEventItems[m_debugEventHead] = {category, id, std::move(line)};
                    m_debugEventHead = (m_debugEventHead + 1) % eventCapacity;
                }

                appendedEvents = true;
            };
            for(const Mosaic::EventTraceEntry & event : debugFrame.events)
            {
                Mosaic::StringView eventName = ExamplesDetail::eventName(event.type);
                Mosaic::String line(eventName.data(), eventName.size());
                line += " [";
                line += ExamplesDetail::hexadecimal(event.id);
                line += "] ";
                line += event.path;
                line += " @ ";
                line += ExamplesDetail::fixed(event.timestamp, 3);
                appendDebugEvent(ExamplesDetail::eventCategory(event.type), event.id, std::move(line));
            }
            for(const Mosaic::String & diagnostic : debugFrame.diagnostics)
            {
                Mosaic::String line = "Error: ";
                line += diagnostic;
                appendDebugEvent(DebugDemoCategory::Errors, Mosaic::InvalidId, std::move(line));
            }

            Mosaic::DockDebugSnapshotVector debugDocking;
            Mosaic::PopupDebugSnapshotVector debugPopups;
            Mosaic::SelectionDebugSnapshotVector debugSelections;
            Mosaic::TableDebugSnapshotVector debugTables;
            (void)Mosaic::dockDebugSnapshots(ui, &debugDocking);
            (void)Mosaic::popupDebugSnapshots(ui, &debugPopups);
            (void)Mosaic::selectionDebugSnapshots(ui, &debugSelections);
            (void)Mosaic::tableDebugSnapshots(ui, &debugTables);
            Mosaic::Array<size_t, 7> snapshotCounts = {debugFrame.metrics.culledWidgetCount, debugDocking.size(), debugFrame.metrics.glyphCount, debugPopups.size(), debugSelections.size(), debugTables.size(), debugFrame.viewports.size()};
            constexpr Mosaic::Array<DebugDemoCategory, 7> snapshotCategories = {DebugDemoCategory::Clipper, DebugDemoCategory::Docking, DebugDemoCategory::Font, DebugDemoCategory::Popup, DebugDemoCategory::Selection, DebugDemoCategory::Table, DebugDemoCategory::Viewport};
            constexpr Mosaic::Array<Mosaic::StringView, 7> snapshotNames = {"Culled items", "Dock spaces", "Cached glyphs", "Open popups", "Selection items", "Tables", "Viewports"};
            for(size_t index = 0; index != snapshotCounts.size(); ++index)
            {
                if(snapshotCounts[index] == m_debugSnapshotCounts[index])
                {
                    continue;
                }

                Mosaic::String line(snapshotNames[index]);
                line += ": ";
                line += ExamplesDetail::number(snapshotCounts[index]);
                appendDebugEvent(snapshotCategories[index], Mosaic::InvalidId, std::move(line));
                m_debugSnapshotCounts[index] = snapshotCounts[index];
            }
            if(appendedEvents == true)
            {
                m_debugVisibleEventsDirty = true;
            }

            Mosaic::WindowOptions options;
            options.open = &m_showDebugLog;
            Mosaic::setNextWindowSize(ui, {0.f, Mosaic::getTheme(ui).metrics.lineHeight * 12.f}, Mosaic::Condition::FirstUse);
            auto window = Mosaic::window(ui, "Mosaic Debug Log", options);

            if(window.visible() == true)
            {
                {
                    auto actions = Mosaic::row(ui);

                    if(Mosaic::button(ui, "Clear").clicked() == true)
                    {
                        m_debugEventItems.clear();
                        m_debugEventHead = 0;
                        m_debugVisibleEventsDirty = true;
                    }

                    if(Mosaic::button(ui, "Copy").clicked() == true)
                    {
                        Mosaic::String contents;
                        for(size_t logical = 0; logical != m_debugEventItems.size(); ++logical)
                        {
                            size_t physical = (m_debugEventHead + logical) % m_debugEventItems.size();
                            size_t category = static_cast<size_t>(m_debugEventItems[physical].category);

                            if(m_debugLogCategories[category] == false)
                            {
                                continue;
                            }

                            contents += m_debugEventItems[physical].text;
                            contents += "\n";
                        }
                        Mosaic::platform(ui).setClipboardText(contents);
                    }

                    if(Mosaic::button(ui, "Configure Outputs...").clicked() == true)
                    {
                        Mosaic::openPopup(ui, Mosaic::Key("debug output configuration"));
                    }

                    Mosaic::checkbox(ui, "Auto-scroll", &m_debugLogAutoScroll);
                }
                {
                    auto outputConfiguration = Mosaic::popup(ui, Mosaic::Key("debug output configuration"));

                    if(outputConfiguration.visible() == true)
                    {
                        Mosaic::checkbox(ui, "OutputToConsole", &outputToConsole);
                        Mosaic::text(ui, "Output is routed through the platform callback.");
                    }
                }
                {
                    bool allCategories = std::all_of(m_debugLogCategories.begin(), m_debugLogCategories.end(), [](bool enabled)
                                                     {
                                                         return enabled;
                                                     });

                    if(Mosaic::checkbox(ui, "All", &allCategories).changed() == true)
                    {
                        m_debugLogCategories.fill(allCategories);
                        m_debugVisibleEventsDirty = true;
                    }

                    auto categories = Mosaic::treeNode(ui, Mosaic::Key("debug log categories"), "Event categories", true);

                    if(categories.expanded() == true)
                    {
                        bool categoryChanged = false;
                        constexpr Mosaic::Array<Mosaic::StringView, static_cast<size_t>(DebugDemoCategory::Count)> categoryNames = {"Errors", "Active Item", "Clipper", "Docking", "Focus", "Font", "Popup", "Selection", "Table", "Viewport"};

                        for(size_t row = 0; row != 2; ++row)
                        {
                            auto categoryLine = Mosaic::line(ui);
                            size_t begin = row * 5;
                            size_t end = begin + 5;
                            for(size_t index = begin; index != end; ++index)
                            {
                                auto categoryScope = Mosaic::scope(ui, Mosaic::Key(index));
                                bool changed = Mosaic::checkbox(ui, categoryNames[index], &m_debugLogCategories[index]).changed();
                                categoryChanged = changed == true || categoryChanged == true;
                            }
                        }

                        if(categoryChanged == true)
                        {
                            m_debugVisibleEventsDirty = true;
                        }
                    }
                }
                Mosaic::separator(ui);

                if(m_debugVisibleEventsDirty == true)
                {
                    m_debugVisibleEvents.clear();

                    for(size_t logical = 0; logical != m_debugEventItems.size(); ++logical)
                    {
                        size_t physical = (m_debugEventHead + logical) % m_debugEventItems.size();
                        size_t category = static_cast<size_t>(m_debugEventItems[physical].category);

                        if(m_debugLogCategories[category] == true)
                        {
                            m_debugVisibleEvents.push_back(logical);
                        }
                    }

                    m_debugVisibleEventsDirty = false;
                }

                Mosaic::LayoutOptions logLayout;
                logLayout.width = Mosaic::SizeRule::Fill;
                logLayout.height = Mosaic::SizeRule::Fill;
                auto log = Mosaic::scrollArea(ui, "Debug event entries", Mosaic::ScrollOptions{}, logLayout);
                Mosaic::Vec2 previousOffset;
                Mosaic::Vec2 previousRange;
                bool hasPreviousScroll = Mosaic::scrollOffset(ui, log.id(), &previousOffset) == true && Mosaic::scrollRange(ui, log.id(), &previousRange) == true;
                bool wasAtEnd = hasPreviousScroll == false || previousOffset.y >= previousRange.y - 1.f;
                float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                Mosaic::VisibleRange range;
                (void)Mosaic::beginListClipper(ui, m_debugVisibleEvents.size(), itemHeight, &range, log.id());
                for(size_t index = range.begin; index != range.end; ++index)
                {
                    size_t logical = m_debugVisibleEvents[index];
                    size_t physical = (m_debugEventHead + logical) % m_debugEventItems.size();
                    const DebugDemoEvent & event = m_debugEventItems[physical];
                    auto item = Mosaic::scope(ui, Mosaic::Key(logical));
                    Mosaic::SelectableOptions eventOptions;
                    Mosaic::String eventLabel;

                    if(event.id != Mosaic::InvalidId)
                    {
                        eventLabel += '[';
                        eventLabel += ExamplesDetail::hexadecimal(event.id);
                        eventLabel += "] ";
                    }

                    eventLabel += event.text;
                    Mosaic::Response eventResponse = Mosaic::selectable(ui, Mosaic::Key("debug event"), eventLabel, event.id != Mosaic::InvalidId && event.id == m_metricsSelectedNode, eventOptions);

                    if(event.id != Mosaic::InvalidId && eventResponse.hovered() == true)
                    {
                        m_itemPickerTarget = event.id;
                        Mosaic::setItemPickerTarget(ui, event.id);
                    }

                    if(event.id != Mosaic::InvalidId && eventResponse.clicked() == true)
                    {
                        m_itemPickerTarget = event.id;
                        m_metricsSelectedNode = event.id;
                        Mosaic::setItemPickerTarget(ui, event.id);
                    }
                }
                Mosaic::endListClipper(ui, range, m_debugVisibleEvents.size(), itemHeight);

                if(appendedEvents == true && m_debugLogAutoScroll == true && wasAtEnd == true)
                {
                    Mosaic::scrollToEnd(ui, log.id(), Mosaic::ScrollAxes::Vertical);
                }
            }
        }

        if(m_showIdStack == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showIdStack;
            Mosaic::setNextWindowSize(ui, {0.f, Mosaic::getTheme(ui).metrics.lineHeight * 8.f}, Mosaic::Condition::FirstUse);
            auto window = Mosaic::window(ui, "Mosaic ID Stack Tool", options);

            if(window.visible() == true)
            {
                static bool encodeNonAscii = false;
                static bool copyPathOnShortcut = true;
                const Mosaic::DebugInfoVector & debug = Mosaic::getFrame(ui).debug;
                Mosaic::ItemPickerState pickerState;
                (void)Mosaic::itemPickerState(ui, &pickerState);
                Mosaic::Id inspected = pickerState.selected;
                for(auto iterator = debug.rbegin(); iterator != debug.rend(); ++iterator)
                {
                    if(inspected != Mosaic::InvalidId)
                    {
                        break;
                    }

                    if(iterator->hovered == false && iterator->active == false && iterator->focused == false)
                    {
                        continue;
                    }

                    inspected = iterator->id;
                    break;
                }

                Mosaic::IdentityDebugEntryVector identities;
                (void)Mosaic::identityDebugEntries(ui, &identities);
                Mosaic::Vector<const Mosaic::IdentityDebugEntry *> chain;
                Mosaic::Id cursor = inspected;
                while(cursor != Mosaic::InvalidId)
                {
                    auto found = std::find_if(identities.rbegin(), identities.rend(),
                                              [cursor](const Mosaic::IdentityDebugEntry & entry)
                                              {
                                                  return entry.id == cursor;
                                              });

                    if(found == identities.rend())
                    {
                        break;
                    }

                    chain.push_back(&*found);

                    if(found->parent == cursor)
                    {
                        break;
                    }

                    cursor = found->parent;
                }
                std::reverse(chain.begin(), chain.end());

                Mosaic::String mainIdentifier = "ID ";
                mainIdentifier += ExamplesDetail::hexadecimal(inspected);
                Mosaic::text(ui, mainIdentifier);
                Mosaic::helpMarker(ui, "Hover an item to display the identity scopes leading to its final ID. Each row is a stable keyed scope or a static callsite inside its parent scope.");
                {
                    auto optionsRow = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "Hex-encode non-ASCII", &encodeNonAscii);
                    Mosaic::checkbox(ui, "Cmd+C: copy path", &copyPathOnShortcut);
                }

                Mosaic::String path;
                if(chain.empty() == false)
                {
                    path = ExamplesDetail::encodedPath(chain.back()->path, encodeNonAscii);
                }
                Mosaic::String pathLine = chain.empty() == true ? "Label/fallback: identity path is incomplete" : "Label/fallback path: \"";
                pathLine += path;

                if(chain.empty() == false)
                {
                    pathLine += '"';
                }

                Mosaic::text(ui, pathLine);

                bool copyRequested = false;
                {
                    auto copyLine = Mosaic::line(ui);
                    copyRequested = Mosaic::button(ui, "Copy path").clicked();

                    if(Mosaic::input(ui).timestamp < m_idStackCopyFeedbackUntil)
                    {
                        Mosaic::text(ui, "Copied!");
                    }
                }
                Mosaic::ShortcutOptions copyOptions;
                copyOptions.route = Mosaic::ShortcutRoute::Global;

                if(copyPathOnShortcut == true && Mosaic::shortcut(ui, Mosaic::Shortcut::primary(Mosaic::KeyCode::C), copyOptions) == true)
                {
                    copyRequested = true;
                }

                if(copyRequested == true && path.empty() == false)
                {
                    Mosaic::platform(ui).setClipboardText(path);
                    m_idStackCopyFeedbackUntil = Mosaic::input(ui).timestamp + 1.5;
                }

                Mosaic::separator(ui);
                Mosaic::TableOptions tableOptions;
                tableOptions.headers = true;
                tableOptions.resizable = true;
                tableOptions.bordersInnerHorizontal = true;
                tableOptions.bordersInnerVertical = true;
                tableOptions.bordersOuterHorizontal = true;
                tableOptions.bordersOuterVertical = true;
                auto table = Mosaic::table(ui, "Identity stack", 3, tableOptions);
                Mosaic::TableColumnOptions identifierColumn;
                identifierColumn.sizing = Mosaic::TableSizing::FixedFit;
                identifierColumn.widthOrWeight = Mosaic::getTheme(ui).metrics.fontSize * 11.f;
                Mosaic::tableSetupColumn(ui, 0, "Seed", identifierColumn);
                Mosaic::TableColumnOptions scopeColumn;
                scopeColumn.sizing = Mosaic::TableSizing::Stretch;
                Mosaic::tableSetupColumn(ui, 1, "PushID", scopeColumn);
                Mosaic::tableSetupColumn(ui, 2, "Result", identifierColumn);
                Mosaic::tableHeadersRow(ui);
                for(size_t depth = 0; depth != chain.size(); ++depth)
                {
                    const Mosaic::IdentityDebugEntry & entry = *chain[depth];
                    Mosaic::tableNextRow(ui, Mosaic::Key(entry.id));
                    (void)Mosaic::tableSetColumn(ui, 0);
                    Mosaic::Id seed = depth == 0 ? Mosaic::InvalidId : chain[depth - 1]->id;
                    Mosaic::text(ui, ExamplesDetail::hexadecimal(seed));
                    (void)Mosaic::tableSetColumn(ui, 1);
                    Mosaic::String scope;

                    if(entry.anonymous == true)
                    {
                        scope = "<anonymous structural>";
                    }
                    else if(entry.valueKind == Mosaic::IdentityValueKind::String)
                    {
                        scope = "String \"";
                        scope += entry.value;
                        scope += '"';
                    }
                    else if(entry.valueKind == Mosaic::IdentityValueKind::Integral)
                    {
                        scope = "Integral ";
                        scope += entry.value;
                    }
                    else
                    {
                        scope = "Callsite ";
                        scope += entry.value;
                    }

                    scope += " local=";
                    scope += ExamplesDetail::hexadecimal(entry.local);

                    if(entry.file.empty() == false)
                    {
                        scope += " @ ";
                        scope += entry.file;
                        scope += ':';
                        scope += ExamplesDetail::number(entry.line);
                    }

                    Mosaic::text(ui, scope);
                    (void)Mosaic::tableSetColumn(ui, 2);

                    if(entry.id == inspected)
                    {
                        Mosaic::Theme highlight = Mosaic::getTheme(ui);
                        highlight.colors.text = highlight.colors.accent;
                        auto highlightScope = Mosaic::styleScope(ui, highlight);
                        Mosaic::text(ui, ExamplesDetail::hexadecimal(entry.id));
                    }
                    else
                    {
                        Mosaic::text(ui, ExamplesDetail::hexadecimal(entry.id));
                    }
                }
            }
        }

        if(m_showStyleEditor == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showStyleEditor;
            options.scrollable = true;
            auto window = Mosaic::window(ui, "Mosaic Style Editor", options);

            if(window.visible() == true)
            {
                if(m_styleReferenceInitialized == false)
                {
                    m_styleReference = m_demoTheme;
                    m_styleReferenceInitialized = true;
                }

                Mosaic::Theme theme = m_demoTheme;
                theme.behavior.alpha = m_styleAlpha;
                theme.metrics.fontSize = m_styleFontSize;
                theme.metrics.lineHeight = std::ceil(m_styleFontSize * 1.3f);
                static int selectedTab = 0;
                {
                    static int stylePreset = 0;
                    constexpr Mosaic::Array<Mosaic::StringView, 3> stylePresets = {"Dark", "Light", "Classic"};

                    if(Mosaic::comboBox(ui, "Colors", &stylePreset, stylePresets).changed() == true)
                    {
                        theme = stylePreset == 1 ? Mosaic::Theme::light() : Mosaic::Theme::dark();

                        if(stylePreset == 2)
                        {
                            theme.metrics.cornerRadius = 0.f;
                            theme.metrics.frameCornerRadius = 0.f;
                            theme.metrics.popupCornerRadius = 0.f;
                            theme.metrics.tabCornerRadius = 0.f;
                            theme.colors.header = Mosaic::Color::fromBytes(64, 64, 86);
                            theme.colors.button = Mosaic::Color::fromBytes(54, 54, 69);
                            theme.colors.frame = Mosaic::Color::fromBytes(42, 42, 54);
                        }

                        m_styleAlpha = theme.behavior.alpha;
                        m_styleFontSize = theme.metrics.fontSize;
                        m_styleReference = theme;
                    }

                    if(Mosaic::availableFonts(ui, &m_availableFonts) == true)
                    {
                        m_availableFontNames.clear();
                        m_availableFontNames.reserve(m_availableFonts.size());
                        for(size_t index = 0; index != m_availableFonts.size(); ++index)
                        {
                            const Mosaic::FontInfo & font = m_availableFonts[index];
                            m_availableFontNames.push_back(font.name);

                            if(font.font == theme.metrics.font)
                            {
                                m_fontPreviewSelection = static_cast<int>(index);
                            }
                        }

                        if(m_availableFonts.empty() == false && Mosaic::comboBox(ui, "Fonts", &m_fontPreviewSelection, m_availableFontNames).changed() == true)
                        {
                            size_t selected = static_cast<size_t>(std::clamp(m_fontPreviewSelection, 0, static_cast<int>(m_availableFonts.size() - 1)));
                            theme.metrics.font = m_availableFonts[selected].font;
                        }
                    }

                    Mosaic::slider(ui, "FontSizeBase", &m_styleFontSize, 5.f, 100.f);
                    float mainScale = Mosaic::fontScale(ui);

                    if(Mosaic::slider(ui, "FontScaleMain", &mainScale, 0.5f, 4.f).changed() == true)
                    {
                        Mosaic::setFontScale(ui, mainScale);
                    }

                    Mosaic::Viewport styleViewport;
                    if(Mosaic::currentViewport(ui, &styleViewport) == true)
                    {
                        float dpiScale = styleViewport.dpiScale;
                        auto readOnly = Mosaic::disabledScope(ui, true);
                        Mosaic::slider(ui, "FontScaleDpi", &dpiScale, 0.5f, 4.f);
                    }

                    if(Mosaic::slider(ui, "FrameRounding", &theme.metrics.frameCornerRadius, 0.f, 12.f).changed() == true)
                    {
                        theme.metrics.grabCornerRadius = theme.metrics.frameCornerRadius;
                    }

                    bool windowBorder = theme.metrics.windowBorderSize > 0.f;
                    bool frameBorder = theme.metrics.frameBorderSize > 0.f;
                    bool popupBorder = theme.metrics.popupBorderSize > 0.f;
                    {
                        auto borders = Mosaic::row(ui);

                        if(Mosaic::checkbox(ui, "WindowBorder", &windowBorder).changed() == true)
                        {
                            theme.metrics.windowBorderSize = windowBorder ? 1.f : 0.f;
                        }

                        if(Mosaic::checkbox(ui, "FrameBorder", &frameBorder).changed() == true)
                        {
                            theme.metrics.frameBorderSize = frameBorder ? 1.f : 0.f;
                        }

                        if(Mosaic::checkbox(ui, "PopupBorder", &popupBorder).changed() == true)
                        {
                            theme.metrics.popupBorderSize = popupBorder ? 1.f : 0.f;
                        }
                    }
                }

                constexpr Mosaic::Array<Mosaic::StringView, 4> editorTabs = {"Sizes", "Colors", "Fonts", "Rendering"};

                struct StyleColorEntry
                {
                    Mosaic::StringView name;
                    Mosaic::Color * color = nullptr;
                };

                Mosaic::Array<StyleColorEntry, 81> styleColors = {{{"Text", &theme.colors.text},
                                                                   {"TextDisabled", &theme.colors.textDisabled},
                                                                   {"WindowBg", &theme.colors.background},
                                                                   {"ChildBg", &theme.colors.panel},
                                                                   {"PanelHeader", &theme.colors.panelHeader},
                                                                   {"TitleBg", &theme.colors.titleBackground},
                                                                   {"TitleBgActive", &theme.colors.titleBackgroundActive},
                                                                   {"TitleBgCollapsed", &theme.colors.titleBackgroundCollapsed},
                                                                   {"Input", &theme.colors.input},
                                                                   {"PanelHovered", &theme.colors.panelHovered},
                                                                   {"PanelActive", &theme.colors.panelActive},
                                                                   {"PopupBg", &theme.colors.popup},
                                                                   {"PopupBorder", &theme.colors.popupBorder},
                                                                   {"Border", &theme.colors.border},
                                                                   {"BorderStrong", &theme.colors.borderStrong},
                                                                   {"BorderShadow", &theme.colors.borderShadow},
                                                                   {"FrameBg", &theme.colors.frame},
                                                                   {"FrameBgHovered", &theme.colors.frameHovered},
                                                                   {"FrameBgActive", &theme.colors.frameActive},
                                                                   {"Button", &theme.colors.button},
                                                                   {"ButtonHovered", &theme.colors.buttonHovered},
                                                                   {"ButtonActive", &theme.colors.buttonActive},
                                                                   {"Header", &theme.colors.header},
                                                                   {"HeaderHovered", &theme.colors.headerHovered},
                                                                   {"HeaderActive", &theme.colors.headerActive},
                                                                   {"Menu", &theme.colors.menu},
                                                                   {"MenuBarBg", &theme.colors.menuBarBackground},
                                                                   {"MenuHovered", &theme.colors.menuHovered},
                                                                   {"MenuActive", &theme.colors.menuActive},
                                                                   {"Tab", &theme.colors.tab},
                                                                   {"TabHovered", &theme.colors.tabHovered},
                                                                   {"TabActive", &theme.colors.tabActive},
                                                                   {"TabSelected", &theme.colors.tabSelected},
                                                                   {"TabSelectedOverline", &theme.colors.tabSelectedOverline},
                                                                   {"TabDimmed", &theme.colors.tabDimmed},
                                                                   {"TabDimmedSelected", &theme.colors.tabDimmedSelected},
                                                                   {"TabDimmedSelectedOverline", &theme.colors.tabDimmedSelectedOverline},
                                                                   {"ScrollbarBg", &theme.colors.scrollbar},
                                                                   {"ScrollbarHovered", &theme.colors.scrollbarHovered},
                                                                   {"ScrollbarActive", &theme.colors.scrollbarActive},
                                                                   {"ScrollbarGrab", &theme.colors.scrollbarGrab},
                                                                   {"ScrollbarGrabHovered", &theme.colors.scrollbarGrabHovered},
                                                                   {"ScrollbarGrabActive", &theme.colors.scrollbarGrabActive},
                                                                   {"SliderGrab", &theme.colors.sliderGrab},
                                                                   {"SliderGrabHovered", &theme.colors.sliderGrabHovered},
                                                                   {"SliderGrabActive", &theme.colors.sliderGrabActive},
                                                                   {"Separator", &theme.colors.separator},
                                                                   {"SeparatorHovered", &theme.colors.separatorHovered},
                                                                   {"SeparatorActive", &theme.colors.separatorActive},
                                                                   {"ResizeGrip", &theme.colors.resizeGrip},
                                                                   {"ResizeGripHovered", &theme.colors.resizeGripHovered},
                                                                   {"ResizeGripActive", &theme.colors.resizeGripActive},
                                                                   {"Selection", &theme.colors.selection},
                                                                   {"TextSelectedBg", &theme.colors.textSelectionBackground},
                                                                   {"InputTextCursor", &theme.colors.textCursor},
                                                                   {"Accent", &theme.colors.accent},
                                                                   {"Warning", &theme.colors.warning},
                                                                   {"Error", &theme.colors.error},
                                                                   {"Success", &theme.colors.success},
                                                                   {"TextLink", &theme.colors.textLink},
                                                                   {"CheckMark", &theme.colors.checkMark},
                                                                   {"CheckboxSelectedBg", &theme.colors.checkboxSelectedBackground},
                                                                   {"TreeLines", &theme.colors.treeLines},
                                                                   {"TableHeaderBg", &theme.colors.tableHeader},
                                                                   {"TableBorderStrong", &theme.colors.tableBorderStrong},
                                                                   {"TableBorderLight", &theme.colors.tableBorderLight},
                                                                   {"TableRowBg", &theme.colors.tableRow},
                                                                   {"TableRowBgAlt", &theme.colors.tableRowAlternate},
                                                                   {"DragDropTarget", &theme.colors.dragDropTarget},
                                                                   {"DragDropTargetBg", &theme.colors.dragDropTargetBackground},
                                                                   {"DockingPreview", &theme.colors.dockingPreview},
                                                                   {"DockingEmptyBg", &theme.colors.dockingEmptyBackground},
                                                                   {"NavCursor", &theme.colors.navigationCursor},
                                                                   {"NavWindowingHighlight", &theme.colors.navigationWindowingHighlight},
                                                                   {"NavWindowingDimBg", &theme.colors.navigationWindowingDimBackground},
                                                                   {"UnsavedMarker", &theme.colors.unsavedMarker},
                                                                   {"ModalWindowDimBg", &theme.colors.modalDimBackground},
                                                                   {"PlotLines", &theme.colors.plotLines},
                                                                   {"PlotLinesHovered", &theme.colors.plotLinesHovered},
                                                                   {"PlotHistogram", &theme.colors.plotHistogram},
                                                                   {"PlotHistogramHovered", &theme.colors.plotHistogramHovered}}};
                Mosaic::separatorText(ui, "General");
                {
                    auto referenceActions = Mosaic::line(ui);

                    if(Mosaic::button(ui, "Save Ref").clicked() == true)
                    {
                        m_styleReference = theme;
                    }

                    if(Mosaic::button(ui, "Revert Ref").clicked() == true)
                    {
                        theme = m_styleReference;
                        m_styleAlpha = theme.behavior.alpha;
                        m_styleFontSize = theme.metrics.fontSize;
                    }

                    Mosaic::helpMarker(ui, "Save and Revert use local non-persistent storage. Use Export in the Colors tab to copy a reusable definition.");
                }
                Mosaic::tabs(ui, "Style editor tabs", &selectedTab, editorTabs);
                static bool exportOnlyModified = true;
                static int exportDestination = 0;

                if(selectedTab == 1)
                {
                    auto referenceActions = Mosaic::row(ui);

                    if(Mosaic::button(ui, "Export").clicked() == true)
                    {
                        Mosaic::String output = "Mosaic::Theme colors\n";
                        auto appendColor = [&output](Mosaic::StringView name, const Mosaic::Color & color)
                        {
                            output += name;
                            output += " = {";
                            output += ExamplesDetail::fixed(color.r, 3);
                            output += ", ";
                            output += ExamplesDetail::fixed(color.g, 3);
                            output += ", ";
                            output += ExamplesDetail::fixed(color.b, 3);
                            output += ", ";
                            output += ExamplesDetail::fixed(color.a, 3);
                            output += "}\n";
                        };
                        for(const StyleColorEntry & entry : styleColors)
                        {
                            std::ptrdiff_t offset = reinterpret_cast<const char *>(entry.color) - reinterpret_cast<const char *>(&theme.colors);
                            const auto * reference = reinterpret_cast<const Mosaic::Color *>(reinterpret_cast<const char *>(&m_styleReference.colors) + offset);

                            if(exportOnlyModified == false || *entry.color != *reference)
                            {
                                appendColor(entry.name, *entry.color);
                            }
                        }
                        if(exportDestination == 0)
                        {
                            Mosaic::platform(ui).setClipboardText(output);
                        }
                        else
                        {
                            (void)Mosaic::platform(ui).writeConsole(output);
                        }
                    }
                    constexpr Mosaic::Array<Mosaic::StringView, 2> exportDestinations = {"To Clipboard", "To TTY"};
                    Mosaic::comboBox(ui, "Output", &exportDestination, exportDestinations);
                    Mosaic::checkbox(ui, "Only Modified Colors", &exportOnlyModified);
                }

                if(selectedTab == 0)
                {
                    Mosaic::separatorText(ui, "Main");
                    Mosaic::slider(ui, "Alpha", &m_styleAlpha, 0.2f, 1.f);
                    Mosaic::slider(ui, "Font size", &m_styleFontSize, 9.f, 32.f);
                    Mosaic::separatorText(ui, "Sizes");
                    Mosaic::slider(ui, "WindowPadding", &theme.metrics.padding, 0.f, 20.f);
                    float framePaddingX = theme.metrics.framePadding.left;
                    float framePaddingY = theme.metrics.framePadding.top;
                    Mosaic::slider(ui, "FramePadding.x", &framePaddingX, 0.f, 20.f);
                    Mosaic::slider(ui, "FramePadding.y", &framePaddingY, 0.f, 20.f);
                    theme.metrics.framePadding.left = framePaddingX;
                    theme.metrics.framePadding.right = framePaddingX;
                    theme.metrics.framePadding.top = framePaddingY;
                    theme.metrics.framePadding.bottom = framePaddingY;
                    Mosaic::slider(ui, "ItemSpacing.x", &theme.metrics.itemSpacing.x, 0.f, 20.f);
                    Mosaic::slider(ui, "ItemSpacing.y", &theme.metrics.itemSpacing.y, 0.f, 20.f);
                    Mosaic::slider(ui, "ItemInnerSpacing.x", &theme.metrics.innerSpacing.x, 0.f, 20.f);
                    Mosaic::slider(ui, "ItemInnerSpacing.y", &theme.metrics.innerSpacing.y, 0.f, 20.f);
                    Mosaic::slider(ui, "IndentSpacing", &theme.metrics.indent, 0.f, 40.f);
                    Mosaic::slider(ui, "GrabMinSize", &theme.metrics.grabMinimumSize, 1.f, 24.f);
                    Mosaic::slider(ui, "TouchExtraPadding.x", &theme.metrics.touchExtraPadding.x, 0.f, 20.f);
                    Mosaic::slider(ui, "TouchExtraPadding.y", &theme.metrics.touchExtraPadding.y, 0.f, 20.f);
                    Mosaic::separatorText(ui, "Borders");
                    Mosaic::slider(ui, "WindowBorderSize", &theme.metrics.windowBorderSize, 0.f, 4.f);
                    Mosaic::slider(ui, "ChildBorderSize", &theme.metrics.childBorderSize, 0.f, 4.f);
                    Mosaic::slider(ui, "PopupBorderSize", &theme.metrics.popupBorderSize, 0.f, 4.f);
                    Mosaic::slider(ui, "FrameBorderSize", &theme.metrics.frameBorderSize, 0.f, 4.f);
                    Mosaic::slider(ui, "TabBorderSize", &theme.metrics.tabBorderSize, 0.f, 4.f);
                    Mosaic::slider(ui, "TabBarBorderSize", &theme.metrics.tabBarBorderSize, 0.f, 4.f);
                    Mosaic::separatorText(ui, "Rounding");
                    Mosaic::slider(ui, "WindowRounding", &theme.metrics.cornerRadius, 0.f, 12.f);
                    Mosaic::slider(ui, "ChildRounding", &theme.metrics.childCornerRadius, 0.f, 12.f);
                    Mosaic::slider(ui, "FrameRounding", &theme.metrics.frameCornerRadius, 0.f, 12.f);
                    Mosaic::slider(ui, "PopupRounding", &theme.metrics.popupCornerRadius, 0.f, 12.f);
                    Mosaic::slider(ui, "MenuItemRounding", &theme.metrics.menuItemCornerRadius, 0.f, 12.f);
                    Mosaic::slider(ui, "TabRounding", &theme.metrics.tabCornerRadius, 0.f, 12.f);
                    Mosaic::slider(ui, "TabCloseButtonMinWidthSelected", &theme.metrics.tabCloseButtonMinimumWidthSelected, -1.f, 100.f);
                    Mosaic::slider(ui, "TabCloseButtonMinWidthUnselected", &theme.metrics.tabCloseButtonMinimumWidthUnselected, -1.f, 100.f);
                    Mosaic::slider(ui, "TabMinWidthBase", &theme.metrics.tabMinimumWidthBase, 1.f, 500.f);
                    Mosaic::slider(ui, "TabMinWidthShrink", &theme.metrics.tabMinimumWidthForShrink, 1.f, 500.f);
                    Mosaic::slider(ui, "TabBarOverlineSize", &theme.metrics.tabOverlineSize, 0.f, 6.f);
                    Mosaic::slider(ui, "ScrollbarRounding", &theme.metrics.scrollbarCornerRadius, 0.f, 12.f);
                    Mosaic::slider(ui, "GrabRounding", &theme.metrics.grabCornerRadius, 0.f, 12.f);
                    Mosaic::separatorText(ui, "Scrollbar");
                    Mosaic::slider(ui, "ScrollbarSize", &theme.metrics.scrollbarWidth, 1.f, 24.f);
                    Mosaic::slider(ui, "ScrollbarPadding", &theme.metrics.scrollbarPadding, 0.f, 8.f);
                    Mosaic::separatorText(ui, "Tables");
                    Mosaic::slider(ui, "CellPadding.x", &theme.metrics.cellPadding.left, 0.f, 20.f);
                    Mosaic::slider(ui, "CellPadding.y", &theme.metrics.cellPadding.top, 0.f, 20.f);
                    theme.metrics.cellPadding.right = theme.metrics.cellPadding.left;
                    theme.metrics.cellPadding.bottom = theme.metrics.cellPadding.top;
                    Mosaic::slider(ui, "TableAngledHeadersAngle", &theme.metrics.tableAngledHeadersAngleDegrees, -50.f, 50.f);
                    Mosaic::slider(ui, "TableAngledHeadersTextAlign", &theme.metrics.tableAngledHeadersTextAlignment, 0.f, 1.f);
                    Mosaic::separatorText(ui, "Trees");
                    constexpr Mosaic::Array<Mosaic::StringView, 3> treeLineNames = {"DrawLinesNone", "DrawLinesFull", "DrawLinesToNodes"};
                    int treeLineMode = static_cast<int>(theme.metrics.treeLineMode) - static_cast<int>(Mosaic::TreeLineMode::None);

                    if(Mosaic::comboBox(ui, "TreeLinesFlags", &treeLineMode, treeLineNames).changed() == true)
                    {
                        theme.metrics.treeLineMode = static_cast<Mosaic::TreeLineMode>(treeLineMode + static_cast<int>(Mosaic::TreeLineMode::None));
                    }

                    Mosaic::slider(ui, "TreeLinesSize", &theme.metrics.treeLinesSize, 0.f, 4.f);
                    Mosaic::slider(ui, "TreeLinesRounding", &theme.metrics.treeLinesRounding, 0.f, 12.f);
                    Mosaic::separatorText(ui, "Windows");
                    Mosaic::slider(ui, "WindowTitleAlign.x", &theme.metrics.windowTitleAlignment.x, 0.f, 1.f);
                    Mosaic::slider(ui, "WindowTitleAlign.y", &theme.metrics.windowTitleAlignment.y, 0.f, 1.f);
                    Mosaic::slider(ui, "WindowBorderHoverPadding", &theme.metrics.windowBorderHoverPadding, 1.f, 20.f);
                    constexpr Mosaic::Array<Mosaic::StringView, 3> windowMenuPositions = {"None", "Left", "Right"};
                    int windowMenuPosition = static_cast<int>(theme.metrics.windowMenuButtonPosition);

                    if(Mosaic::comboBox(ui, "WindowMenuButtonPosition", &windowMenuPosition, windowMenuPositions).changed() == true)
                    {
                        theme.metrics.windowMenuButtonPosition = static_cast<Mosaic::WindowMenuButtonPosition>(windowMenuPosition);
                    }

                    Mosaic::separatorText(ui, "Widgets");
                    Mosaic::slider(ui, "ColorMarkerSize", &theme.metrics.colorMarkerSize, 0.f, 8.f);
                    Mosaic::slider(ui, "LogSliderDeadzone", &theme.metrics.logarithmicSliderDeadzone, 0.f, 20.f);
                    Mosaic::slider(ui, "ButtonTextAlign.x", &theme.metrics.buttonTextAlignment.x, 0.f, 1.f);
                    Mosaic::slider(ui, "ButtonTextAlign.y", &theme.metrics.buttonTextAlignment.y, 0.f, 1.f);
                    Mosaic::slider(ui, "SelectableTextAlign.x", &theme.metrics.selectableTextAlignment.x, 0.f, 1.f);
                    Mosaic::slider(ui, "SelectableTextAlign.y", &theme.metrics.selectableTextAlignment.y, 0.f, 1.f);
                    Mosaic::slider(ui, "SeparatorSize", &theme.metrics.separatorSize, 0.f, 10.f);
                    Mosaic::slider(ui, "SeparatorTextBorderSize", &theme.metrics.separatorTextBorderSize, 0.f, 10.f);
                    Mosaic::slider(ui, "SeparatorTextAlign.x", &theme.metrics.separatorTextAlignment.x, 0.f, 1.f);
                    Mosaic::slider(ui, "SeparatorTextAlign.y", &theme.metrics.separatorTextAlignment.y, 0.f, 1.f);
                    Mosaic::slider(ui, "SeparatorTextPadding.x", &theme.metrics.separatorTextPadding.x, 0.f, 40.f);
                    Mosaic::slider(ui, "SeparatorTextPadding.y", &theme.metrics.separatorTextPadding.y, 0.f, 40.f);
                    Mosaic::slider(ui, "ImageRounding", &theme.metrics.imageRounding, 0.f, 12.f);
                    Mosaic::slider(ui, "ImageBorderSize", &theme.metrics.imageBorderSize, 0.f, 4.f);
                    Mosaic::text(ui, "ColorButtonPosition");
                    {
                        auto colorButtonPosition = Mosaic::row(ui);

                        if(Mosaic::radioButton(ui, "Left", theme.metrics.colorButtonPosition == Mosaic::ColorButtonPosition::Left).clicked() == true)
                        {
                            theme.metrics.colorButtonPosition = Mosaic::ColorButtonPosition::Left;
                        }

                        if(Mosaic::radioButton(ui, "Right", theme.metrics.colorButtonPosition == Mosaic::ColorButtonPosition::Right).clicked() == true)
                        {
                            theme.metrics.colorButtonPosition = Mosaic::ColorButtonPosition::Right;
                        }
                    }
                    Mosaic::separatorText(ui, "Docking");
                    Mosaic::checkbox(ui, "DockingNodeHasCloseButton", &theme.metrics.dockingNodeHasCloseButton);
                    Mosaic::slider(ui, "DockingSeparatorSize", &theme.metrics.splitterWidth, 2.f, 16.f);
                    Mosaic::separatorText(ui, "Tooltips");
                    Mosaic::slider(ui, "HoverDelayNormal", &theme.behavior.tooltipNormalDelay, 0.f, 2.f);
                    Mosaic::slider(ui, "HoverDelayShort", &theme.behavior.tooltipShortDelay, 0.f, 2.f);
                    {
                        auto mouseFlags = Mosaic::treeNode(ui, Mosaic::Key("tooltip mouse flags"), "HoverFlagsForTooltipMouse");

                        if(mouseFlags.expanded() == true)
                        {
                            constexpr Mosaic::Array<Mosaic::StringView, 4> tooltipPolicies = {"Default", "DelayNone", "DelayShort", "DelayNormal"};
                            int tooltipPolicy = static_cast<int>(theme.behavior.tooltipMouseDelay);

                            if(Mosaic::comboBox(ui, "Delay policy", &tooltipPolicy, tooltipPolicies).changed() == true)
                            {
                                theme.behavior.tooltipMouseDelay = static_cast<Mosaic::TooltipDelay>(tooltipPolicy);
                            }

                            Mosaic::checkbox(ui, "Stationary", &theme.behavior.tooltipMouseStationary);
                            Mosaic::checkbox(ui, "Shared delay", &theme.behavior.tooltipMouseSharedDelay);
                        }
                    }
                    {
                        auto navigationFlags = Mosaic::treeNode(ui, Mosaic::Key("tooltip navigation flags"), "HoverFlagsForTooltipNav");

                        if(navigationFlags.expanded() == true)
                        {
                            Mosaic::checkbox(ui, "Navigation focus", &theme.behavior.tooltipNavigationFocus);
                        }
                    }
                    Mosaic::separatorText(ui, "Misc");
                    Mosaic::slider(ui, "DisplayWindowPadding.x", &theme.metrics.displayWindowPadding.x, 0.f, 30.f);
                    Mosaic::slider(ui, "DisplayWindowPadding.y", &theme.metrics.displayWindowPadding.y, 0.f, 30.f);
                    Mosaic::slider(ui, "DisplaySafeAreaPadding.x", &theme.metrics.displaySafeAreaPadding.x, 0.f, 30.f);
                    Mosaic::slider(ui, "DisplaySafeAreaPadding.y", &theme.metrics.displaySafeAreaPadding.y, 0.f, 30.f);
                }
                else if(selectedTab == 1)
                {
                    static bool identifyColors = false;
                    static Mosaic::String identifiedColor;
                    {
                        auto identifyRow = Mosaic::row(ui);

                        if(Mosaic::button(ui, identifyColors == true ? "Stop identifying colors" : "Identify colors").clicked() == true)
                        {
                            identifyColors = identifyColors == false;
                            identifiedColor.clear();
                        }

                        Mosaic::helpMarker(ui, "While enabled, hover a color editor to identify the corresponding style token.");

                        if(identifiedColor.empty() == false)
                        {
                            Mosaic::text(ui, identifiedColor);
                        }
                    }
                    Mosaic::searchField(ui, "Filter colors", &m_styleColorFilter);
                    {
                        auto modes = Mosaic::row(ui);

                        if(Mosaic::radioButton(ui, "Opaque", m_styleAlphaMode == 0).clicked() == true)
                        {
                            m_styleAlphaMode = 0;
                        }

                        if(Mosaic::radioButton(ui, "Alpha", m_styleAlphaMode == 1).clicked() == true)
                        {
                            m_styleAlphaMode = 1;
                        }

                        if(Mosaic::radioButton(ui, "Both", m_styleAlphaMode == 2).clicked() == true)
                        {
                            m_styleAlphaMode = 2;
                        }
                    }
                    Mosaic::ColorEditOptions colorOptions;
                    colorOptions.showInputs = true;
                    colorOptions.alphaOpaque = m_styleAlphaMode == 0;
                    colorOptions.alphaPreviewHalf = m_styleAlphaMode == 2;
                    for(size_t entryIndex = 0; entryIndex != styleColors.size(); ++entryIndex)
                    {
                        const StyleColorEntry & entry = styleColors[entryIndex];

                        if(ExamplesDetail::containsIgnoreCase(entry.name, m_styleColorFilter) == false)
                        {
                            continue;
                        }

                        auto entryScope = Mosaic::scope(ui, Mosaic::Key(entryIndex));
                        std::ptrdiff_t offset = reinterpret_cast<const char *>(entry.color) - reinterpret_cast<const char *>(&theme.colors);
                        auto * reference = reinterpret_cast<Mosaic::Color *>(reinterpret_cast<char *>(&m_styleReference.colors) + offset);
                        auto colorRow = Mosaic::row(ui);
                        Mosaic::Response colorResponse = Mosaic::colorEditorRgba(ui, entry.name, entry.color, colorOptions);

                        if(identifyColors == true && colorResponse.hovered() == true)
                        {
                            identifiedColor = entry.name;
                        }

                        if(*entry.color != *reference)
                        {
                            if(Mosaic::button(ui, Mosaic::Key("save color"), "Save").clicked() == true)
                            {
                                *reference = *entry.color;
                            }

                            if(Mosaic::button(ui, Mosaic::Key("revert color"), "Revert").clicked() == true)
                            {
                                *entry.color = *reference;
                            }
                        }
                    }
                }
                else if(selectedTab == 2)
                {
                    HelloDemo::drawFontAtlas(ui);
                }
                else if(selectedTab == 3)
                {
                    Mosaic::slider(ui, "Alpha", &m_styleAlpha, 0.2f, 1.f);
                    Mosaic::slider(ui, "Disabled Alpha", &theme.behavior.disabledAlpha, 0.f, 1.f);
                    Mosaic::checkbox(ui, "Anti-aliased lines", &theme.behavior.antiAliasedLines);
                    Mosaic::checkbox(ui, "Anti-aliased lines use texture", &theme.behavior.antiAliasedLinesUseTexture);
                    Mosaic::helpMarker(ui, "Retained for renderer capability parity. Graphics geometry remains the fallback when no line texture is available.");
                    Mosaic::checkbox(ui, "Anti-aliased fill", &theme.behavior.antiAliasedFill);
                    Mosaic::separatorText(ui, "Tessellation");
                    Mosaic::slider(ui, "Curve tessellation max error", &theme.behavior.curveTessellationMaximumError, 0.1f, 10.f);
                    Mosaic::slider(ui, "Circle tessellation max error", &theme.behavior.circleTessellationMaximumError, 0.1f, 10.f);
                    Mosaic::helpMarker(ui, "Graphics derives the curve and circle segment counts from the requested maximum geometric error.");
                    Mosaic::LayoutOptions previewLayout;
                    previewLayout.width = Mosaic::SizeRule::Fill;
                    previewLayout.height = Mosaic::Dimension::fixed(112.f);
                    Mosaic::Canvas preview = Mosaic::canvas(ui, "Tessellation preview", previewLayout);
                    Mosaic::Rect previewBounds;

                    if(preview.contentRect(&previewBounds) == true)
                    {
                        Mosaic::Color guide = theme.colors.border;
                        Mosaic::Color stroke = theme.colors.accent;
                        preview.rect(previewBounds, theme.colors.panel);
                        float centerY = previewBounds.height * 0.5f;
                        float third = previewBounds.width / 3.f;
                        preview.line({third, 8.f}, {third, previewBounds.height - 8.f}, 1.f, guide);
                        preview.line({third * 2.f, 8.f}, {third * 2.f, previewBounds.height - 8.f}, 1.f, guide);
                        preview.bezierCubic({12.f, centerY + 28.f}, {third * 0.3f, 2.f}, {third * 0.7f, previewBounds.height - 2.f}, {third - 12.f, centerY - 28.f}, 2.f, stroke, 0);
                        preview.ellipse({third * 1.5f, centerY}, {third * 0.34f, previewBounds.height * 0.34f}, -0.28f, 2.f, stroke, 0);
                        preview.roundedRect({third * 2.f + 12.f, 14.f, third - 24.f, previewBounds.height - 28.f}, 18.f, stroke);
                    }
                }

                m_demoTheme = theme;
            }
        }

        if(m_showAbout == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showAbout;
            options.fitContentHeight = true;
            auto window = Mosaic::window(ui, "About Mosaic", options);

            if(window.visible() == true)
            {
                static bool showConfigInfo = false;
                Mosaic::String version = "Mosaic ";
                version += Mosaic::Version;
                Mosaic::text(ui, version);
                Mosaic::separator(ui);
                Mosaic::text(ui, "This executable is the interactive Mosaic component catalog.");
                Mosaic::hyperlink(ui, "Homepage", "https://github.com/irov/Mosaic");
                Mosaic::sameLine(ui);
                Mosaic::hyperlink(ui, "README", "https://github.com/irov/Mosaic/blob/master/README.md");
                Mosaic::sameLine(ui);
                Mosaic::hyperlink(ui, "Examples", "https://github.com/irov/Mosaic/tree/master/examples");
                Mosaic::sameLine(ui);
                Mosaic::hyperlink(ui, "Issues", "https://github.com/irov/Mosaic/issues");
                Mosaic::sameLine(ui);
                Mosaic::hyperlink(ui, "Releases", "https://github.com/irov/Mosaic/releases");
                Mosaic::sameLine(ui);
                Mosaic::hyperlink(ui, "Funding", "https://github.com/sponsors/irov");
                Mosaic::sameLine(ui);
                Mosaic::hyperlink(ui, "License", "https://github.com/irov/Mosaic/blob/master/LICENSE");
                Mosaic::text(ui, "Mosaic is an immediate-hybrid C++20 UI library with a C-like public API.");
                Mosaic::text(ui, "Developed by the Mosaic contributors.");
                Mosaic::text(ui, "Mosaic is licensed under the MIT License, see LICENSE for more information.");
                Mosaic::text(ui, "If your company uses Mosaic, please consider contributing fixes, examples or funding ongoing development.");
                Mosaic::checkbox(ui, "Config/Build Information", &showConfigInfo);

                if(showConfigInfo == true)
                {
                    Mosaic::Configuration configuration;
                    Mosaic::Viewport viewport;
                    bool hasConfiguration = Mosaic::getConfiguration(ui, &configuration);
                    bool hasViewport = Mosaic::currentViewport(ui, &viewport);

                    if(hasConfiguration == false || hasViewport == false)
                    {
                        Mosaic::text(ui, "Configuration information is not available for the current frame.");
                    }
                    else
                    {
                        Mosaic::String information;
                        auto appendValue = [&information](Mosaic::StringView name, const auto & value)
                        {
                            information += name;
                            information += ExamplesDetail::number(value);
                            information += '\n';
                        };
                        auto appendFlag = [&information](Mosaic::StringView name, bool value)
                        {
                            information += name;
                            information += value ? "true\n" : "false\n";
                        };
                        information += "Mosaic ";
                        information += Mosaic::Version;
                        information += "\n";
                        appendValue("C++ standard: ", __cplusplus);
                        appendValue("sizeof(size_t): ", sizeof(size_t));
                        appendValue("sizeof(uint32_t): ", sizeof(uint32_t));
                        appendValue("sizeof(Mosaic index): ", sizeof(Mosaic::IndexVector::value_type));
                        appendValue("sizeof(Mosaic::Vertex): ", sizeof(Mosaic::Vertex));
#if defined(__APPLE__)
                        information += "define: __APPLE__\n";
#endif
#if defined(__GNUC__)
                        appendValue("define: __GNUC__=", __GNUC__);
#endif
#if defined(__clang_version__)
                        information += "define: __clang_version__=";
                        information += __clang_version__;
                        information += '\n';
#endif
#if defined(NDEBUG)
                        information += "define: NDEBUG\n";
#else
                        information += "build: assertions enabled\n";
#endif
                        information += "Allocator: Mosaic::defaultAllocator() (replaceable)\n";
                        appendValue("Viewport width: ", viewport.bounds.width);
                        appendValue("Viewport height: ", viewport.bounds.height);
                        appendValue("DPI scale: ", viewport.dpiScale);
                        const Mosaic::Theme & theme = Mosaic::getTheme(ui);
                        appendValue("Style alpha: ", theme.behavior.alpha);
                        appendValue("Style font size: ", theme.metrics.fontSize);
                        appendValue("Main font scale: ", Mosaic::fontScale(ui));
                        Mosaic::FontInfoVector aboutFonts;

                        if(Mosaic::availableFonts(ui, &aboutFonts) == true)
                        {
                            appendValue("Available fonts: ", aboutFonts.size());
                        }

                        Mosaic::FontAtlasConfiguration atlasConfiguration;

                        if(Mosaic::fontAtlasConfiguration(ui, &atlasConfiguration) == true)
                        {
                            appendValue("Atlas width: ", atlasConfiguration.pageSize.x);
                            appendValue("Atlas height: ", atlasConfiguration.pageSize.y);
                            appendValue("Atlas glyph spacing: ", atlasConfiguration.glyphSpacing);
                        }

                        information += "\nStyle\n";
                        appendValue("Window padding: ", theme.metrics.padding);
                        appendValue("Window border size: ", theme.metrics.windowBorderSize);
                        appendValue("Frame padding left: ", theme.metrics.framePadding.left);
                        appendValue("Frame padding top: ", theme.metrics.framePadding.top);
                        appendValue("Frame rounding: ", theme.metrics.frameCornerRadius);
                        appendValue("Frame border size: ", theme.metrics.frameBorderSize);
                        appendValue("Item spacing X: ", theme.metrics.itemSpacing.x);
                        appendValue("Item spacing Y: ", theme.metrics.itemSpacing.y);
                        appendValue("Item inner spacing X: ", theme.metrics.innerSpacing.x);
                        appendValue("Item inner spacing Y: ", theme.metrics.innerSpacing.y);

                        information += "\nConfiguration\n";
                        appendFlag("Pointer input: ", configuration.pointerInput);
                        appendFlag("Keyboard input: ", configuration.keyboardInput);
                        appendFlag("Keyboard navigation: ", configuration.keyboardNavigation);
                        appendFlag("Docking: ", configuration.dockingEnabled);
                        appendFlag("Resize from edges: ", configuration.windowResizeFromEdges);
                        appendFlag("Cursor changes: ", configuration.cursorChanges);

                        if(Mosaic::button(ui, "Copy to clipboard").clicked() == true)
                        {
                            Mosaic::platform(ui).setClipboardText(information);
                        }

                        Mosaic::ScrollOptions scrollOptions;
                        scrollOptions.framed = true;
                        scrollOptions.frameStyle = true;
                        Mosaic::LayoutOptions layout;
                        layout.width = Mosaic::SizeRule::Fill;
                        layout.height = Mosaic::Dimension::fixed(m_demoTheme.metrics.lineHeight * 18.f);
                        auto configInfo = Mosaic::scrollArea(ui, "Build information", scrollOptions, layout);

                        if(configInfo.visible() == true)
                        {
                            Mosaic::text(ui, information);
                        }
                    }
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace MosaicExample
