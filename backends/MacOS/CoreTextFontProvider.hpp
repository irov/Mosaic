#pragma once

#include <Mosaic/Mosaic.hpp>

namespace Mosaic
{
    class CoreTextFontProvider final : public FontProvider
    {
    public:
        explicit CoreTextFontProvider(RendererAdapter & renderer) noexcept;
        ~CoreTextFontProvider() override;

        void setScale(float scale) noexcept;

        [[nodiscard]] bool measure(FontHandle font, float size, StringView text, Vec2 * const _out) const noexcept override;
        [[nodiscard]] bool metrics(FontHandle font, float size, FontMetrics * const _out) const noexcept override;
        bool shape(FontHandle font, float size, StringView text, ShapedGlyphVector * const _out) const override;
        [[nodiscard]] bool getGlyph(FontHandle font, float size, uint32_t glyph, Glyph * const _out) const override;
        [[nodiscard]] bool getGlyph(FontHandle font, float size, const ShapedGlyph & shaped, Glyph * const _out) const override;
        bool shapeText(FontHandle font, float size, StringView text, const TextShapeOptions & options, ShapedText * const _out) const override;
        [[nodiscard]] FontCacheMetrics cacheMetrics() const noexcept override;
        [[nodiscard]] bool atlasPage(size_t index, FontAtlasPage * const _out) const noexcept override;
        [[nodiscard]] bool atlasConfiguration(FontAtlasConfiguration * const _out) const noexcept override;
        bool inspectFonts(FontInfoVector * const _out) const override;
        bool inspectFontCache(FontCacheEntryVector * const _out) const override;
        bool inspectGlyphCache(GlyphCacheEntryVector * const _out) const override;
        bool inspectAtlasRects(FontAtlasRectInfoVector * const _out) const override;
        bool cacheAction(FontCacheAction action) override;
        [[nodiscard]] uint64_t revision() const noexcept override;

    private:
        struct FontCacheKey
        {
            FontHandle font = DefaultFont;
            uint32_t size = 0;

            [[nodiscard]] bool operator==(const FontCacheKey &) const noexcept = default;
        };

        struct FontCacheKeyHash
        {
            [[nodiscard]] size_t operator()(const FontCacheKey & key) const noexcept;
        };

        struct CachedFont
        {
            const void * native = nullptr;
            FontMetrics metrics;
        };

        struct CachedGlyph
        {
            FontHandle font = DefaultFont;
            float size = 0.f;
            uint32_t face = 0;
            uint32_t glyph = 0;
            Glyph value;
            bool atlas = false;
        };

        struct ResolvedFont
        {
            const void * native = nullptr;
        };

        struct AtlasPage
        {
            TextureHandle texture = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t cursorX = 0;
            uint32_t cursorY = 0;
            uint32_t rowHeight = 0;
            bool mask = false;
        };

        struct AtlasRegion
        {
            size_t page = 0;
            Rect bounds;
            FontHandle font = DefaultFont;
            float size = 0.f;
            uint32_t face = 0;
            uint32_t glyph = 0;
            bool packed = false;
        };

        using FontCache = UnorderedMap<FontCacheKey, CachedFont, FontCacheKeyHash>;
        using GlyphCache = UnorderedMap<uint64_t, CachedGlyph>;
        using ResolvedFontVector = Vector<ResolvedFont>;
        using AtlasPageVector = Vector<AtlasPage>;
        using AtlasRegionVector = Vector<AtlasRegion>;

        [[nodiscard]] const CachedFont * findOrCreateFont(FontHandle font, float size) const noexcept;
        [[nodiscard]] uint32_t findOrCreateResolvedFont(const void * native) const noexcept;
        [[nodiscard]] const void * resolvedFont(uint32_t face) const noexcept;
        [[nodiscard]] bool allocateAtlasRegion(uint32_t width, uint32_t height, size_t * const _outPage, uint32_t * const _outX, uint32_t * const _outY) const;
        void clearFonts() noexcept;
        void clearGlyphs() noexcept;
        void clearResolvedFonts() noexcept;

        RendererAdapter & m_renderer;
        mutable FontCache m_fonts;
        mutable GlyphCache m_glyphs;
        mutable ResolvedFontVector m_resolvedFonts;
        mutable AtlasPageVector m_atlasPages;
        mutable AtlasRegionVector m_atlasRegions;
        float m_scale = 1.f;
        uint64_t m_revision = 1;
    };
} // namespace Mosaic
