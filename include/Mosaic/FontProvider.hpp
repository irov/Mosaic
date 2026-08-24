#pragma once

#include "Mosaic/DrawTypes.hpp"

#include <utility>

namespace Mosaic
{
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
        bool mask = false;
    };

    struct FontCacheEntry
    {
        FontHandle font = DefaultFont;
        float size = 0.f;
        FontMetrics metrics;
    };

    using FontCacheEntryVector = Vector<FontCacheEntry>;

    struct GlyphCacheEntry
    {
        FontHandle font = DefaultFont;
        float size = 0.f;
        uint32_t face = 0;
        uint32_t glyph = 0;
        Glyph value;
        bool atlas = false;
    };

    using GlyphCacheEntryVector = Vector<GlyphCacheEntry>;

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

        virtual bool inspectGlyphCache(GlyphCacheEntryVector * const _out) const
        {
            if(_out == nullptr)
            {
                return false;
            }

            _out->clear();

            return true;
        }

        // Advance this value whenever cached font, shaping or glyph resources become invalid.
        [[nodiscard]] virtual uint64_t revision() const noexcept
        {
            return 0;
        }
    };
} // namespace Mosaic
