#pragma once

#include "Mosaic/DrawTypes.hpp"

#include <utility>

namespace Mosaic
{
    inline constexpr uint32_t FontLoaderScalable = 1U << 0U;
    inline constexpr uint32_t FontLoaderDynamicFallback = 1U << 1U;
    inline constexpr uint32_t FontLoaderMaskAtlas = 1U << 2U;

    struct FontMetrics
    {
        float ascent = 0.f;
        float descent = 0.f;
        float leading = 0.f;

        [[nodiscard]] constexpr float lineHeight() const noexcept
        {
            return ascent + descent + leading;
        }
    };

    struct TextShapeOptions
    {
        bool caretOffsets = false;
        bool wordWrap = false;
        float wrapWidth = 0.f;
    };

    struct ShapedTextLine
    {
        size_t begin = 0;
        size_t end = 0;
        float width = 0.f;
    };

    using ShapedTextLineVector = Vector<ShapedTextLine>;

    struct ShapedText
    {
        Vec2 size;
        ShapedGlyphVector glyphs;
        FloatVector offsets;
        SizeVector clusters;
        Vec2Vector positions;
        ShapedTextLineVector lines;
    };

    struct FontCacheMetrics
    {
        size_t fontCount = 0;
        size_t resolvedFaceCount = 0;
        size_t glyphCount = 0;
        size_t atlasPageCount = 0;
        size_t atlasMemory = 0;
    };

    struct FontAtlasPage
    {
        TextureHandle texture = 0;
        Vec2 size;
        Rect used;
        size_t packedRectCount = 0;
        size_t discardedRectCount = 0;
        size_t packedArea = 0;
        size_t discardedArea = 0;
        bool mask = false;
        bool active = false;
    };

    struct FontCacheEntry
    {
        FontHandle font = DefaultFont;
        float size = 0.f;
        float effectiveSize = 0.f;
        FontMetrics metrics;
        size_t glyphCount = 0;
        uint32_t firstGlyph = 0;
        uint32_t lastGlyph = 0;
    };

    using FontCacheEntryVector = Vector<FontCacheEntry>;

    struct FontGlyphRange
    {
        uint32_t first = 0;
        uint32_t last = 0;
    };

    using FontGlyphRangeVector = Vector<FontGlyphRange>;

    struct FontInfo
    {
        FontHandle font = DefaultFont;
        String name;
        String loader;
        String source;
        float effectiveScale = 1.f;
        uint32_t loaderFlags = 0;
        bool monospace = false;
        StringVector sources;
        FontGlyphRangeVector glyphRanges;
        uint32_t fallbackGlyph = 0xfffdU;
        uint32_t ellipsisGlyph = 0x2026U;
    };

    using FontInfoVector = Vector<FontInfo>;

    struct FontAtlasConfiguration
    {
        Vec2 pageSize;
        uint32_t glyphSpacing = 0;
        bool dynamicCoverage = false;
        bool mask = false;
    };

    struct GlyphCacheEntry
    {
        FontHandle font = DefaultFont;
        float size = 0.f;
        uint32_t face = 0;
        uint32_t glyph = 0;
        Glyph value;
        bool atlas = false;
        size_t atlasRect = std::numeric_limits<size_t>::max();
    };

    using GlyphCacheEntryVector = Vector<GlyphCacheEntry>;

    struct FontAtlasRectInfo
    {
        size_t page = 0;
        Rect bounds;
        FontHandle font = DefaultFont;
        float size = 0.f;
        uint32_t face = 0;
        uint32_t glyph = 0;
        bool packed = false;
        size_t index = 0;
    };

    using FontAtlasRectInfoVector = Vector<FontAtlasRectInfo>;

    enum class FontCacheAction : uint8_t
    {
        Clear,
        Compact,
        Grow,
        Rebuild
    };

    class FontProvider
    {
    public:
        virtual ~FontProvider() = default;
        [[nodiscard]] virtual bool measure(FontHandle font, float size, StringView text, Vec2 * const _out) const noexcept = 0;
        [[nodiscard]] virtual bool metrics(FontHandle font, float size, FontMetrics * const _out) const noexcept = 0;
        virtual bool shape(FontHandle font, float size, StringView text, ShapedGlyphVector * const _out) const = 0;
        [[nodiscard]] virtual bool getGlyph(FontHandle font, float size, uint32_t glyph, Glyph * const _out) const = 0;

        [[nodiscard]] virtual bool getGlyph(FontHandle font, float size, const ShapedGlyph & shaped, Glyph * const _out) const
        {
            bool returnedValue = getGlyph(font, size, shaped.glyph, _out);

            return returnedValue;
        }

        virtual bool shapeText(FontHandle font, float size, StringView text, const TextShapeOptions & options, ShapedText * const _out) const
        {
            if(_out == nullptr)
            {
                return false;
            }

            ShapedText output;
            if(measure(font, size, text, &output.size) == false)
            {
                return false;
            }

            if(shape(font, size, text, &output.glyphs) == false)
            {
                return false;
            }

            output.clusters.reserve(output.glyphs.size() + 2);
            output.clusters.push_back(0);
            for(const ShapedGlyph & glyph : output.glyphs)
            {
                output.clusters.push_back(std::min(glyph.cluster, text.size()));
            }
            output.clusters.push_back(text.size());
            std::sort(output.clusters.begin(), output.clusters.end());
            output.clusters.erase(std::unique(output.clusters.begin(), output.clusters.end()), output.clusters.end());

            if(options.caretOffsets == true)
            {
                output.offsets.resize(text.size() + 1, 0.f);
                output.positions.resize(text.size() + 1);
                for(const ShapedGlyph & glyph : output.glyphs)
                {
                    if(glyph.cluster < output.offsets.size())
                    {
                        output.offsets[glyph.cluster] = glyph.position.x;
                        output.positions[glyph.cluster] = glyph.position;
                    }
                }
            }

            output.lines.push_back({0, text.size(), output.size.x});

            *_out = std::move(output);

            return true;
        }

        [[nodiscard]] virtual FontCacheMetrics cacheMetrics() const noexcept
        {
            return {};
        }

        [[nodiscard]] virtual bool atlasPage(size_t, FontAtlasPage * const) const noexcept
        {
            return false;
        }

        virtual bool inspectFontCache(FontCacheEntryVector * const _out) const
        {
            if(_out == nullptr)
            {
                return false;
            }

            _out->clear();

            return true;
        }

        virtual bool inspectFonts(FontInfoVector * const _out) const
        {
            if(_out == nullptr)
            {
                return false;
            }

            _out->clear();

            return false;
        }

        [[nodiscard]] virtual bool atlasConfiguration(FontAtlasConfiguration * const _out) const noexcept
        {
            if(_out == nullptr)
            {
                return false;
            }

            *_out = {};

            return false;
        }

        virtual bool inspectGlyphCache(GlyphCacheEntryVector * const _out) const
        {
            if(_out == nullptr)
            {
                return false;
            }

            _out->clear();

            return true;
        }

        virtual bool inspectAtlasRects(FontAtlasRectInfoVector * const _out) const
        {
            if(_out == nullptr)
            {
                return false;
            }

            _out->clear();

            return true;
        }

        virtual bool cacheAction(FontCacheAction)
        {
            return false;
        }

        // Advance this value whenever cached font, shaping or glyph resources become invalid.
        [[nodiscard]] virtual uint64_t revision() const noexcept
        {
            return 0;
        }
    };
} // namespace Mosaic
