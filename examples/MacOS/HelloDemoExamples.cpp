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
    template<class T> [[nodiscard]] Mosaic::String number(T value)
    {
        char buffer[64] = {};
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        auto returnedValue = result.ec == std::errc{} ? Mosaic::String(buffer, result.ptr) : Mosaic::String("?");

        return returnedValue;
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
    [[nodiscard]] size_t eventCategory(Mosaic::EventType type) noexcept
    {
        switch(type)
        {
        case Mosaic::EventType::PointerDown:
        case Mosaic::EventType::PointerUp:
        case Mosaic::EventType::FocusChanged:
            return 0;
        case Mosaic::EventType::BeginEdit:
        case Mosaic::EventType::Change:
        case Mosaic::EventType::Commit:
        case Mosaic::EventType::Cancel:
            return 1;
        case Mosaic::EventType::DragBegin:
        case Mosaic::EventType::Drop:
            return 2;
        case Mosaic::EventType::PopupOpen:
        case Mosaic::EventType::PopupClose:
            return 3;
        }

        return 0;
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
    void HelloDemo::drawFontCacheInspector(Mosaic::Context * ui)
    {
        if(Mosaic::fontCacheEntries(ui, &m_fontCacheEntries) == false)
        {
            m_fontCacheEntries.clear();
        }

        if(Mosaic::glyphCacheEntries(ui, &m_glyphCacheEntries) == false)
        {
            m_glyphCacheEntries.clear();
        }

        Mosaic::String fontsLabel = "Cached fonts (";
        fontsLabel += ExamplesDetail::number(m_fontCacheEntries.size());
        fontsLabel += ")";
        auto fonts = Mosaic::treeNode(ui, Mosaic::Key("cached fonts"), fontsLabel, true);

        if(fonts.expanded() == true)
        {
            Mosaic::TableOptions options;
            options.headers = true;
            options.rowBackground = true;
            options.bordersOuterVertical = true;
            options.bordersInnerHorizontal = true;
            auto table = Mosaic::table(ui, "Font cache entries", 5, options);
            Mosaic::tableSetupColumn(ui, 0, "Font");
            Mosaic::tableSetupColumn(ui, 1, "Size");
            Mosaic::tableSetupColumn(ui, 2, "Ascent");
            Mosaic::tableSetupColumn(ui, 3, "Descent");
            Mosaic::tableSetupColumn(ui, 4, "Leading");
            Mosaic::tableHeadersRow(ui);
            for(size_t index = 0; index != m_fontCacheEntries.size(); ++index)
            {
                const Mosaic::FontCacheEntry & entry = m_fontCacheEntries[index];
                auto row = Mosaic::scope(ui, Mosaic::Key(index));
                Mosaic::tableNextRow(ui);
                (void)Mosaic::tableSetColumn(ui, 0);
                Mosaic::text(ui, entry.font == Mosaic::MonospaceFont ? "MonospaceFont" : "DefaultFont");
                (void)Mosaic::tableSetColumn(ui, 1);
                Mosaic::text(ui, ExamplesDetail::fixed(entry.size, 2));
                (void)Mosaic::tableSetColumn(ui, 2);
                Mosaic::text(ui, ExamplesDetail::fixed(entry.metrics.ascent, 2));
                (void)Mosaic::tableSetColumn(ui, 3);
                Mosaic::text(ui, ExamplesDetail::fixed(entry.metrics.descent, 2));
                (void)Mosaic::tableSetColumn(ui, 4);
                Mosaic::text(ui, ExamplesDetail::fixed(entry.metrics.leading, 2));
            }
        }

        Mosaic::FontCacheMetrics metrics;
        if(Mosaic::fontCacheMetrics(ui, &metrics) == false)
        {
            return;
        }

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
                Mosaic::text(ui, pageLabel);
                float previewWidth = 256.f;
                float previewHeight = page.size.x > 0.f ? previewWidth * page.size.y / page.size.x : previewWidth;
                Mosaic::image(ui, page.texture, {previewWidth, std::min(previewHeight, 256.f)});
            }
        }

        Mosaic::String glyphsLabel = "Cached glyphs (";
        glyphsLabel += ExamplesDetail::number(m_glyphCacheEntries.size());
        glyphsLabel += ")";
        auto glyphs = Mosaic::treeNode(ui, Mosaic::Key("cached glyphs"), glyphsLabel);

        if(glyphs.expanded() == true)
        {
            Mosaic::searchField(ui, "Filter glyph or face", &m_fontGlyphFilter);
            m_fontGlyphIndices.clear();
            for(size_t index = 0; index != m_glyphCacheEntries.size(); ++index)
            {
                const Mosaic::GlyphCacheEntry & entry = m_glyphCacheEntries[index];
                Mosaic::String glyphValue = ExamplesDetail::number(entry.glyph);
                Mosaic::String faceValue = ExamplesDetail::number(entry.face);
                bool includeGlyph = m_fontGlyphFilter.empty();

                if(ExamplesDetail::containsIgnoreCase(glyphValue, m_fontGlyphFilter) == true)
                {
                    includeGlyph = true;
                }

                if(ExamplesDetail::containsIgnoreCase(faceValue, m_fontGlyphFilter) == true)
                {
                    includeGlyph = true;
                }

                if(includeGlyph == true)
                {
                    m_fontGlyphIndices.push_back(index);
                }
            }
            Mosaic::TableOptions options;
            options.headers = true;
            options.rowBackground = true;
            options.bordersInnerHorizontal = true;
            options.scrollVertical = true;
            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.height = Mosaic::Dimension::fixed(240.f);
            auto table = Mosaic::table(ui, "Glyph cache entries", 6, options, layout);
            Mosaic::tableSetupColumn(ui, 0, "Glyph");
            Mosaic::tableSetupColumn(ui, 1, "Face");
            Mosaic::tableSetupColumn(ui, 2, "Font");
            Mosaic::tableSetupColumn(ui, 3, "Size");
            Mosaic::tableSetupColumn(ui, 4, "Advance");
            Mosaic::tableSetupColumn(ui, 5, "Atlas");
            Mosaic::tableHeadersRow(ui);
            float rowHeight = Mosaic::getTheme(ui).metrics.controlHeight;
            Mosaic::VisibleRange range;
            (void)Mosaic::tableVisibleRows(ui, m_fontGlyphIndices.size(), rowHeight, &range);
            for(size_t visible = range.begin; visible != range.end; ++visible)
            {
                size_t index = m_fontGlyphIndices[visible];
                const Mosaic::GlyphCacheEntry & entry = m_glyphCacheEntries[index];
                Mosaic::String glyphValue = ExamplesDetail::number(entry.glyph);
                Mosaic::String faceValue = ExamplesDetail::number(entry.face);
                auto row = Mosaic::scope(ui, Mosaic::Key(index));
                Mosaic::tableNextRow(ui);
                (void)Mosaic::tableSetColumn(ui, 0);
                Mosaic::text(ui, glyphValue);
                (void)Mosaic::tableSetColumn(ui, 1);
                Mosaic::text(ui, faceValue);
                (void)Mosaic::tableSetColumn(ui, 2);
                Mosaic::text(ui, entry.font == Mosaic::MonospaceFont ? "Mono" : "Default");
                (void)Mosaic::tableSetColumn(ui, 3);
                Mosaic::text(ui, ExamplesDetail::fixed(entry.size, 2));
                (void)Mosaic::tableSetColumn(ui, 4);
                Mosaic::text(ui, ExamplesDetail::fixed(entry.value.advance, 2));
                (void)Mosaic::tableSetColumn(ui, 5);
                Mosaic::text(ui, entry.atlas ? "yes" : "standalone");
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::addConsoleLog(Mosaic::String value)
    {
        m_consoleItems.emplace_back(std::move(value));
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
    void HelloDemo::drawImageViewerContents(Mosaic::Context * ui, float canvasHeight)
    {
        {
            auto controls = Mosaic::row(ui);
            Mosaic::slider(ui, "Zoom", &m_imageViewerZoom, 0.25f, 64.f);
            Mosaic::checkbox(ui, "Grid", &m_imageViewerGrid);

            if(Mosaic::button(ui, "Reset view").clicked() == true)
            {
                m_imageViewerZoom = 1.f;
                m_imageViewerPan = {};
            }
        }
        Mosaic::text(ui, "Mouse wheel: zoom. Left drag: pan.");
        Mosaic::LayoutOptions canvasLayout;
        canvasLayout.width = Mosaic::SizeRule::Fill;
        canvasLayout.height = Mosaic::Dimension::fixed(canvasHeight);
        Mosaic::Canvas canvas = Mosaic::canvas(ui, "Image viewer canvas", canvasLayout);
        Mosaic::Rect bounds;
        if(canvas.contentRect(&bounds) == false)
        {
            return;
        }

        Mosaic::FontAtlasPage atlas;
        bool hasAtlas = Mosaic::fontAtlasPage(ui, 0, &atlas) == true && atlas.texture != 0 && atlas.size.x > 0.f && atlas.size.y > 0.f;

        Mosaic::Vec2 wheel;
        if(Mosaic::consumeWheel(ui, canvas.id(), &wheel) == true && wheel.y != 0.f)
        {
            float previousZoom = m_imageViewerZoom;
            Mosaic::Vec2 pointerPosition;
            if(canvas.localPointerPosition(&pointerPosition) == true)
            {
                Mosaic::Vec2 center = {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
                Mosaic::Vec2 imagePoint = (pointerPosition - center - m_imageViewerPan) * (1.f / previousZoom);
                m_imageViewerZoom = std::clamp(previousZoom * std::exp(wheel.y * 0.015f), 0.25f, 64.f);
                m_imageViewerPan = pointerPosition - center - imagePoint * m_imageViewerZoom;
            }
        }

        const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();
        Mosaic::Rect screenBounds;
        bool hasScreenBounds = Mosaic::debugBounds(ui, canvas.id(), &screenBounds);
        bool hovered = pointer != nullptr && hasScreenBounds == true && screenBounds.contains(pointer->position);

        if(pointer != nullptr && hovered == true && Mosaic::capturedPointerOwner(ui) == Mosaic::InvalidId && pointer->isPressed(Mosaic::PointerButton::Primary) == true)
        {
            m_imageViewerPanning = true;
            m_imageViewerPanStart = m_imageViewerPan;
        }

        if(pointer != nullptr && m_imageViewerPanning == true)
        {
            if(pointer->isDown(Mosaic::PointerButton::Primary) == true)
            {
                m_imageViewerPan = m_imageViewerPanStart + (pointer->position - pointer->pressPosition(Mosaic::PointerButton::Primary));
            }
            else
            {
                m_imageViewerPanning = false;
            }
        }

        canvas.rect(bounds, Mosaic::Color::fromBytes(24, 27, 31));
        float sourceWidth = hasAtlas ? atlas.size.x : 320.f;
        float sourceHeight = hasAtlas ? atlas.size.y : 220.f;
        float imageWidth = sourceWidth * m_imageViewerZoom;
        float imageHeight = sourceHeight * m_imageViewerZoom;
        Mosaic::Rect imageBounds = {(bounds.width - imageWidth) * 0.5f + m_imageViewerPan.x, (bounds.height - imageHeight) * 0.5f + m_imageViewerPan.y, imageWidth, imageHeight};
        ExamplesDetail::checkerboard(canvas, imageBounds, std::max(4.f, 18.f * m_imageViewerZoom));

        if(hasAtlas == true)
        {
            canvas.image(atlas.texture, imageBounds);
        }
        else
        {
            Mosaic::Vec2 center = {imageBounds.x + imageBounds.width * 0.5f, imageBounds.y + imageBounds.height * 0.5f};
            canvas.line({imageBounds.x, center.y}, {imageBounds.right(), center.y}, 1.f, Mosaic::Color::fromBytes(255, 90, 90));
            canvas.line({center.x, imageBounds.y}, {center.x, imageBounds.bottom()}, 1.f, Mosaic::Color::fromBytes(90, 190, 255));
        }

        if(m_imageViewerGrid == true && m_imageViewerZoom >= 6.f)
        {
            Mosaic::Rect visible = Mosaic::Rect::intersection(imageBounds, bounds);
            int32_t firstColumn = static_cast<int32_t>(std::ceil((visible.x - imageBounds.x) / m_imageViewerZoom));
            int32_t lastColumn = static_cast<int32_t>(std::floor((visible.right() - imageBounds.x) / m_imageViewerZoom));
            int32_t firstRow = static_cast<int32_t>(std::ceil((visible.y - imageBounds.y) / m_imageViewerZoom));
            int32_t lastRow = static_cast<int32_t>(std::floor((visible.bottom() - imageBounds.y) / m_imageViewerZoom));
            Mosaic::Color gridColor = Mosaic::Color::fromBytes(255, 255, 255, 72);
            for(int32_t column = std::max(0, firstColumn); column <= std::min(static_cast<int32_t>(sourceWidth), lastColumn); ++column)
            {
                float x = imageBounds.x + static_cast<float>(column) * m_imageViewerZoom;
                canvas.line({x, visible.y}, {x, visible.bottom()}, 1.f, gridColor);
            }
            for(int32_t row = std::max(0, firstRow); row <= std::min(static_cast<int32_t>(sourceHeight), lastRow); ++row)
            {
                float y = imageBounds.y + static_cast<float>(row) * m_imageViewerZoom;
                canvas.line({visible.x, y}, {visible.right(), y}, 1.f, gridColor);
            }
        }

        Mosaic::String status = hasAtlas ? "Font atlas " : "Fallback ";
        status += ExamplesDetail::number(static_cast<uint32_t>(sourceWidth));
        status += "x";
        status += ExamplesDetail::number(static_cast<uint32_t>(sourceHeight));
        status += ", zoom ";
        status += ExamplesDetail::fixed(m_imageViewerZoom, 2);
        status += "x";
        Mosaic::Vec2 textSize;
        if(canvas.text({bounds.x + 8.f, bounds.y + 8.f}, status, Mosaic::Color::fromBytes(235, 235, 235), &textSize) == false)
        {
            return;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void HelloDemo::initializePropertyEditor()
    {
        if(m_propertyNodes.empty() == false)
        {
            return;
        }

        constexpr size_t rootCount = 10;
        constexpr size_t childrenPerRoot = 3;
        constexpr size_t leavesPerChild = 2;
        uint32_t nextUid = 1;
        for(size_t root = 0; root != rootCount; ++root)
        {
            size_t rootIndex = m_propertyNodes.size();
            PropertyDemoNode rootNode;
            rootNode.uid = nextUid++;
            rootNode.expanded = root < 2;
            rootNode.name = "Root ";
            rootNode.name += ExamplesDetail::number(root);
            m_propertyNodes.push_back(std::move(rootNode));

            for(size_t child = 0; child != childrenPerRoot; ++child)
            {
                size_t childIndex = m_propertyNodes.size();
                PropertyDemoNode childNode;
                childNode.parent = rootIndex;
                childNode.uid = nextUid++;
                childNode.depth = 1;
                childNode.expanded = root == 0 && child == 0;
                childNode.name = "Child ";
                childNode.name += ExamplesDetail::number(root);
                childNode.name += ".";
                childNode.name += ExamplesDetail::number(child);
                childNode.integers = {static_cast<int32_t>(root), static_cast<int32_t>(child), static_cast<int32_t>(root + child), static_cast<int32_t>(root * childrenPerRoot + child)};
                m_propertyNodes.push_back(std::move(childNode));

                for(size_t leaf = 0; leaf != leavesPerChild; ++leaf)
                {
                    PropertyDemoNode leafNode;
                    leafNode.parent = childIndex;
                    leafNode.uid = nextUid++;
                    leafNode.depth = 2;
                    leafNode.name = "Field ";
                    leafNode.name += ExamplesDetail::number(root);
                    leafNode.name += ".";
                    leafNode.name += ExamplesDetail::number(child);
                    leafNode.name += ".";
                    leafNode.name += ExamplesDetail::number(leaf);
                    leafNode.integers = {static_cast<int32_t>(root), static_cast<int32_t>(child), static_cast<int32_t>(leaf), static_cast<int32_t>(root * 100 + child * 10 + leaf)};
                    leafNode.scalars = {static_cast<float>(root) / static_cast<float>(rootCount), static_cast<float>(child) / static_cast<float>(childrenPerRoot), static_cast<float>(leaf) / static_cast<float>(leavesPerChild)};
                    m_propertyNodes.push_back(std::move(leafNode));
                }
            }
        }

        m_propertyOrder.reserve(m_propertyNodes.size());
        for(const PropertyDemoNode & node : m_propertyNodes)
        {
            m_propertyOrder.push_back(Mosaic::combineId(Mosaic::hashBytes("Property editor node"), node.uid));
        }

        if(m_propertyOrder.empty() == false)
        {
            m_propertySelection.select(m_propertyOrder.front());
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

        Mosaic::Rect dockspaceBounds;
        Mosaic::Rect documentDockBounds;
        const Mosaic::Array<Mosaic::String, 4> & documentNames = m_documentNames;
        auto drawDocumentContents = [this, ui](size_t index)
        {
            Mosaic::String title = "Document \"";
            title += m_documentNames[index];
            title += "\"";
            Mosaic::text(ui, title);
            Mosaic::TextOptions paragraph;
            paragraph.wordWrap = true;
            paragraph.layout.width = Mosaic::SizeRule::Fill;
            Mosaic::text(ui,
                         "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                         "This document demonstrates persistent per-document state.",
                         paragraph);
            Mosaic::colorEditorRgb(ui, "color", &m_documentColors[index]);
            auto actions = Mosaic::row(ui);

            if(Mosaic::button(ui, "Modify").clicked() == true)
            {
                m_documentDirty[index] = true;
            }

            if(Mosaic::button(ui, "Save").clicked() == true)
            {
                m_documentDirty[index] = false;
            }
        };

        if(m_showMainMenuBar == true)
        {
            Mosaic::Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
            float menuHeight = Mosaic::getTheme(ui).metrics.controlHeight + Mosaic::getTheme(ui).metrics.framePadding.top + Mosaic::getTheme(ui).metrics.framePadding.bottom;
            Mosaic::WindowOptions options;
            options.initialBounds = {workArea.x, workArea.y, workArea.width, menuHeight};
            options.minimumSize = {1.f, menuHeight};
            options.maximumSize = {workArea.width, menuHeight};
            options.titleBar = false;
            options.movable = false;
            options.resizable = false;
            options.dockable = false;
            auto window = Mosaic::window(ui, Mosaic::Key("Example Main Menu Bar"), {}, options);
            Mosaic::setWindowBounds(ui, window.id(), options.initialBounds);

            if(window.visible() == true)
            {
                auto bar = Mosaic::menuBar(ui);
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
                        Mosaic::menuItem(ui, "Undo");
                        Mosaic::menuItem(ui, "Redo");
                        Mosaic::separator(ui);
                        Mosaic::menuItem(ui, "Cut");
                        Mosaic::menuItem(ui, "Copy");
                        Mosaic::menuItem(ui, "Paste");
                    }
                }
            }
        }

        if(m_showAssetsBrowser == true)
        {
            constexpr Mosaic::Id assetIdSeed = 0xb2d50bf5183e2d87ULL;
            while(m_assetItems.size() < m_assetCount)
            {
                m_assetItems.push_back(m_assetNextSerial++);
                m_assetOrderDirty = true;
            }

            if(m_assetItems.size() > m_assetCount)
            {
                m_assetItems.resize(m_assetCount);
                m_assetOrderDirty = true;
            }

            Mosaic::WindowOptions options;
            options.open = &m_showAssetsBrowser;
            options.initialBounds = {70.f, 110.f, 620.f, 520.f};
            auto window = Mosaic::window(ui, "Example: Assets Browser", options);

            if(window.visible() == true)
            {
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
                                m_assetOrder.clear();
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
                                std::erase_if(m_assetItems,
                                              [this](size_t serial)
                                              {
                                                  auto returnedValue = m_assetSelection.selected(Mosaic::combineId(assetIdSeed, serial));

                                                  return returnedValue;
                                              });
                                m_assetSelection.clear();
                                m_assetCount = m_assetItems.size();
                                m_assetOrderDirty = true;
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

                            Mosaic::MenuItemOptions ascending;
                            ascending.checked = &m_assetSortAscending;
                            ascending.enabled = m_assetAllowSorting;

                            if(Mosaic::menuItem(ui, "Sort Ascending", ascending).changed() == true)
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
                Mosaic::searchField(ui, "Filter", &m_assetFilter);
                Mosaic::separator(ui);
                auto folders = Mosaic::treeNode(ui, Mosaic::Key("assets"), "Assets", true);

                if(folders.expanded() == true)
                {
                    auto textures = Mosaic::treeNode(ui, Mosaic::Key("textures"), "Textures", true);

                    if(textures.expanded() == true)
                    {
                        constexpr Mosaic::Array<Mosaic::StringView, 12> names = {"Brick", "Cloud", "Metal", "Grass", "Water", "Wood", "Rock", "Sand", "Smoke", "Spark", "Noise", "Checker"};
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
                            if(m_assetAllowSorting == true)
                            {
                                std::sort(m_assetItems.begin(), m_assetItems.end());

                                if(m_assetSortAscending == false)
                                {
                                    std::reverse(m_assetItems.begin(), m_assetItems.end());
                                }
                            }

                            m_assetOrder.clear();
                            m_assetOrder.reserve(m_assetItems.size());
                            for(size_t serial : m_assetItems)
                            {
                                m_assetOrder.push_back(Mosaic::combineId(assetIdSeed, serial));
                            }
                            m_assetOrderDirty = false;
                        }

                        Mosaic::Rect browserBounds;
                        if(Mosaic::debugBounds(ui, browser.id(), &browserBounds) == false)
                        {
                            return;
                        }

                        float browserWidth = std::max(1.f, browserBounds.width);
                        float previousItemExtent = std::max(24.f, m_assetIconSize + static_cast<float>(m_assetIconSpacing));
                        uint32_t previousColumns = std::clamp(static_cast<uint32_t>(browserWidth / previousItemExtent), uint32_t{1}, uint32_t{8});
                        float previousRowHeight = m_assetIconSize + 30.f + static_cast<float>(m_assetIconSpacing);
                        const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();

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
                                    return;
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
                                uint32_t nextColumns = std::clamp(static_cast<uint32_t>(browserWidth / nextItemExtent), uint32_t{1}, uint32_t{8});
                                float nextRowHeight = m_assetIconSize + 30.f + static_cast<float>(m_assetIconSpacing);
                                size_t nextRow = hoveredItem / nextColumns;
                                size_t nextColumn = hoveredItem % nextColumns;
                                m_assetZoomScrollTarget = {static_cast<float>(nextColumn) * nextItemExtent + columnFraction * nextItemExtent - pointerLocalX, static_cast<float>(nextRow) * nextRowHeight + rowFraction * nextRowHeight - pointerLocalY};
                                m_assetZoomScrollPending = true;
                            }
                        }

                        float itemExtent = std::max(24.f, m_assetIconSize + static_cast<float>(m_assetIconSpacing));
                        uint32_t columns = std::clamp(static_cast<uint32_t>(browserWidth / itemExtent), uint32_t{1}, uint32_t{8});
                        size_t rowCount = (m_assetItems.size() + columns - 1) / columns;
                        float rowHeight = m_assetIconSize + 30.f + static_cast<float>(m_assetIconSpacing);
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
                            size_t firstAsset = rows.begin * columns;
                            size_t lastAsset = std::min(m_assetItems.size(), rows.end * columns);
                            for(size_t index = firstAsset; index != lastAsset; ++index)
                            {
                                size_t serial = m_assetItems[index];
                                Mosaic::Id assetId = Mosaic::combineId(assetIdSeed, serial);
                                auto item = Mosaic::scope(ui, Mosaic::Key(serial));
                                Mosaic::StringView baseName = names[serial % names.size()];
                                Mosaic::String name(baseName.data(), baseName.size());
                                name += " ";
                                name += ExamplesDetail::number(serial);

                                if(m_assetFilter.empty() == false && name.find(m_assetFilter) == Mosaic::String::npos)
                                {
                                    continue;
                                }

                                Mosaic::Theme itemTheme = Mosaic::getTheme(ui);
                                itemTheme.metrics.controlHeight = m_assetIconSize;
                                itemTheme.metrics.itemSpacing.x = static_cast<float>(m_assetIconHitSpacing);
                                auto itemStyle = Mosaic::styleScope(ui, itemTheme);
                                Mosaic::Response asset = Mosaic::selectable(ui, Mosaic::Key("asset"), name, &m_assetSelection, assetId, m_assetOrder);
                                bool dragAllowed = m_assetAllowDragUnselected || m_assetSelection.selected(assetId);

                                if(dragAllowed == true)
                                {
                                    const auto * bytes = reinterpret_cast<const std::byte *>(&assetId);
                                    (void)Mosaic::beginDragDropSource(ui, asset, assetDragType, Mosaic::ByteSpan(bytes, sizeof(assetId)));
                                }

                                if(m_assetShowTypeOverlay == true)
                                {
                                    Mosaic::text(ui, serial % 3 == 0 ? "Folder" : "File");
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
            options.initialBounds = {80.f, 80.f, 640.f, 520.f};
            options.scrollable = true;
            options.scroll.axes = m_horizontalShowScrollbar ? Mosaic::ScrollAxes::Both : Mosaic::ScrollAxes::Vertical;
            auto window = Mosaic::window(ui, "Horizontal contents size demo window", options);

            if(window.visible() == true)
            {
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
                    return;
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
                    Mosaic::text(ui, "This is a long wrapped text line used to demonstrate how explicit horizontal "
                                     "content size participates in measurement and scrolling.");
                }

                if(m_horizontalShowColumns == true)
                {
                    Mosaic::text(ui, "Columns:");
                    Mosaic::TableOptions tableOptions;
                    tableOptions.resizable = true;
                    auto table = Mosaic::table(ui, "Horizontal columns", 4, tableOptions);
                    for(uint32_t column = 0; column != 4; ++column)
                    {
                        Mosaic::String label = "Column ";
                        label += ExamplesDetail::number(column);
                        Mosaic::tableSetupColumn(ui, column, label);
                    }
                    Mosaic::tableHeadersRow(ui);
                    Mosaic::tableNextRow(ui);
                    for(uint32_t column = 0; column != 4; ++column)
                    {
                        if(Mosaic::tableSetColumn(ui, column) == true)
                        {
                            Mosaic::text(ui, "Width 150.00");
                        }
                    }
                }

                if(m_horizontalShowTabBar == true)
                {
                    static int selectedTab = 0;
                    constexpr Mosaic::Array<Mosaic::StringView, 4> tabNames = {"AAAA", "BBBB", "CCCC", "DDDD"};
                    Mosaic::tabs(ui, "Horizontal content tabs", &selectedTab, tabNames);
                }

                if(m_horizontalShowChild == true)
                {
                    Mosaic::LayoutOptions childLayout;
                    childLayout.width = Mosaic::Dimension::fixed(480.f);
                    childLayout.height = Mosaic::Dimension::fixed(80.f);
                    auto child = Mosaic::scrollArea(ui, "Nested child", Mosaic::ScrollOptions{}, childLayout);
                    Mosaic::text(ui, "Child contents grow the horizontal extent.");
                }
            }
        }

        if(m_showConsole == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showConsole;
            options.collapsed = &m_consoleCollapsed;
            options.initialBounds = {220.f, 160.f, 520.f, 360.f};
            options.dockable = false;
            auto console = Mosaic::window(ui, "Example: Console", options);

            if(console.visible() == true)
            {
                Mosaic::text(ui, "This example implements a console with basic coloring, completion, command history "
                                 "and filtering.");
                {
                    auto row = Mosaic::row(ui);

                    if(Mosaic::button(ui, "Clear").clicked() == true)
                    {
                        m_consoleItems.clear();
                    }

                    if(Mosaic::button(ui, "Add Debug Text").clicked() == true)
                    {
                        HelloDemo::addConsoleLog("some text");
                        HelloDemo::addConsoleLog("some more text");
                        HelloDemo::addConsoleLog("display very important message here!");
                    }

                    if(Mosaic::button(ui, "Add Debug Error").clicked() == true)
                    {
                        HelloDemo::addConsoleLog("[error] something went wrong");
                    }

                    if(Mosaic::button(ui, "Copy").clicked() == true)
                    {
                        Mosaic::String clipboard;
                        for(const Mosaic::String & line : m_consoleItems)
                        {
                            clipboard += line;
                            clipboard += "\n";
                        }
                        Mosaic::platform(ui).setClipboardText(clipboard);
                    }

                    if(Mosaic::button(ui, "Close Console").clicked() == true)
                    {
                        m_showConsole = false;
                    }
                }
                {
                    auto row = Mosaic::row(ui);
                    Mosaic::checkbox(ui, "Auto-scroll", &m_consoleAutoScroll);
                    Mosaic::text(ui, "Filter supports comma-separated include and -exclude terms.");
                }
                Mosaic::searchField(ui, "Filter", &m_consoleFilter);
                Mosaic::separator(ui);
                Mosaic::LayoutOptions logLayout;
                logLayout.width = Mosaic::SizeRule::Fill;
                logLayout.height = Mosaic::Dimension::fixed(190.f);
                {
                    auto log = Mosaic::scrollArea(ui, "Console log", Mosaic::ScrollOptions{}, logLayout);
                    m_consoleVisibleItems.clear();
                    for(size_t index = 0; index != m_consoleItems.size(); ++index)
                    {
                        const Mosaic::String & line = m_consoleItems[index];
                        bool visible = true;
                        bool hasInclude = false;
                        bool includeMatched = false;
                        size_t begin = 0;
                        while(begin <= m_consoleFilter.size())
                        {
                            size_t comma = m_consoleFilter.find(',', begin);
                            size_t end = comma == Mosaic::String::npos ? m_consoleFilter.size() : comma;
                            while(begin < end && std::isspace(static_cast<unsigned char>(m_consoleFilter[begin])) != 0)
                            {
                                ++begin;
                            }
                            size_t trimmedEnd = end;
                            while(trimmedEnd > begin && std::isspace(static_cast<unsigned char>(m_consoleFilter[trimmedEnd - 1])) != 0)
                            {
                                --trimmedEnd;
                            }
                            Mosaic::StringView token(m_consoleFilter.data() + begin, trimmedEnd - begin);
                            bool exclude = token.empty() == false && token.front() == '-';

                            if(exclude == true)
                            {
                                token.remove_prefix(1);
                            }

                            if(token.empty() == false)
                            {
                                bool matched = ExamplesDetail::containsIgnoreCase(line, token);

                                if(exclude == true && matched == true)
                                {
                                    visible = false;
                                }

                                if(exclude == false)
                                {
                                    hasInclude = true;
                                    includeMatched = includeMatched || matched;
                                }
                            }

                            if(comma == Mosaic::String::npos)
                            {
                                break;
                            }

                            begin = comma + 1;
                        }

                        if(visible == true && (hasInclude == false || includeMatched == true))
                        {
                            m_consoleVisibleItems.push_back(index);
                        }
                    }
                    float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                    Mosaic::VisibleRange range;
                    (void)Mosaic::beginListClipper(ui, m_consoleVisibleItems.size(), itemHeight, &range, log.id());
                    for(size_t visible = range.begin; visible != range.end; ++visible)
                    {
                        size_t index = m_consoleVisibleItems[visible];
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::text(ui, m_consoleItems[index]);
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

                Mosaic::text(ui, "Commands: HELP, HISTORY, CLEAR, CLASSIFY");
            }
        }

        if(m_showDocuments == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showDocuments;
            options.initialBounds = {180.f, 100.f, 580.f, 390.f};
            auto window = Mosaic::window(ui, "Example: Documents", options);

            if(window.visible() == true)
            {
                {
                    auto bar = Mosaic::menuBar(ui);
                    {
                        auto file = Mosaic::menu(ui, "File");

                        if(file.expanded() == true)
                        {
                            if(Mosaic::menuItem(ui, "Open").clicked() == true)
                            {
                                for(size_t index = 0; index != m_documentOpen.size(); ++index)
                                {
                                    if(m_documentOpen[index])
                                    {
                                        continue;
                                    }

                                    m_documentOpen[index] = true;
                                    m_documentTab = static_cast<int>(index);
                                    m_documentsRedockRequested = true;
                                    break;
                                }
                            }

                            if(Mosaic::menuItem(ui, "Close").clicked() == true)
                            {
                                size_t index = static_cast<size_t>(std::clamp(m_documentTab, 0, 3));

                                if(m_documentDirty[index])
                                {
                                    m_documentClosePending = static_cast<int>(index);
                                }
                                else
                                {
                                    m_documentOpen[index] = false;
                                }
                            }

                            if(Mosaic::menuItem(ui, "Save").clicked() == true)
                            {
                                m_documentDirty[static_cast<size_t>(std::clamp(m_documentTab, 0, 3))] = false;
                            }

                            Mosaic::separator(ui);

                            if(Mosaic::menuItem(ui, "Close All Documents").clicked() == true)
                            {
                                m_documentOpen.fill(false);
                                for(size_t index = 0; index != m_documentDirty.size(); ++index)
                                {
                                    if(m_documentDirty[index] == false)
                                    {
                                        continue;
                                    }

                                    m_documentOpen[index] = true;

                                    if(m_documentClosePending < 0)
                                    {
                                        m_documentClosePending = static_cast<int>(index);
                                    }
                                }
                            }

                            if(Mosaic::menuItem(ui, "Exit").clicked() == true)
                            {
                                m_showDocuments = false;
                            }
                        }
                    }
                    {
                        auto edit = Mosaic::menu(ui, "Edit");

                        if(edit.expanded() == true)
                        {
                            if(Mosaic::menuItem(ui, "Rename...").clicked() == true)
                            {
                                m_documentRenamePending = std::clamp(m_documentTab, 0, 3);
                                m_documentRenameValue = m_documentNames[static_cast<size_t>(m_documentRenamePending)];
                            }

                            Mosaic::MenuItemOptions reorderable;
                            reorderable.checked = &m_documentsReorderable;
                            Mosaic::menuItem(ui, "Reorderable Tabs", reorderable);
                        }
                    }
                }
                constexpr Mosaic::Array<Mosaic::StringView, 3> outputTargets = {"None", "TabBar+Tabs", "DockSpace+Window"};
                int previousOutput = m_documentOutput;
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

                {
                    auto openDocuments = Mosaic::row(ui);
                    for(size_t index = 0; index != documentNames.size(); ++index)
                    {
                        auto documentScope = Mosaic::scope(ui, Mosaic::Key(index));
                        bool wasOpen = m_documentOpen[index];

                        if(Mosaic::checkbox(ui, documentNames[index], &m_documentOpen[index]).changed() == true)
                        {
                            if(wasOpen == true && m_documentOpen[index] == false && m_documentDirty[index])
                            {
                                m_documentOpen[index] = true;
                                m_documentClosePending = static_cast<int>(index);
                            }
                            else if(m_documentOpen[index])
                            {
                                m_documentTab = static_cast<int>(index);
                            }

                            m_documentsRedockRequested = true;
                        }
                    }
                }
                bool hasOpenDocument = false;
                for(size_t index = 0; index != documentNames.size(); ++index)
                {
                    if(m_documentOpen[index])
                    {
                        hasOpenDocument = true;
                    }
                }

                if(hasOpenDocument == false)
                {
                    Mosaic::text(ui, "No open documents.");

                    if(Mosaic::button(ui, "Reopen all").clicked() == true)
                    {
                        m_documentOpen.fill(true);
                        m_documentsRedockRequested = true;
                    }
                }
                else if(m_documentOutput == 0)
                {
                    size_t selected = static_cast<size_t>(std::clamp(m_documentTab, 0, 3));

                    if(m_documentOpen[selected] == false)
                    {
                        for(size_t index = 0; index != m_documentOpen.size(); ++index)
                        {
                            if(m_documentOpen[index] == false)
                            {
                                continue;
                            }

                            m_documentTab = static_cast<int>(index);
                            break;
                        }
                    }

                    drawDocumentContents(static_cast<size_t>(m_documentTab));
                }
                else if(m_documentOutput == 1)
                {
                    Mosaic::StringVector labels;
                    Mosaic::StringViewVector labelViews;
                    labels.reserve(documentNames.size());
                    labelViews.reserve(documentNames.size());
                    for(size_t index = 0; index != documentNames.size(); ++index)
                    {
                        labels.emplace_back(documentNames[index]);

                        if(m_documentDirty[index])
                        {
                            labels.back() += " *";
                        }
                    }
                    for(const Mosaic::String & label : labels)
                    {
                        labelViews.emplace_back(label);
                    }
                    Mosaic::TabsOptions tabOptions;
                    tabOptions.reorderable = m_documentsReorderable;
                    tabOptions.autoSelectNewTabs = true;
                    tabOptions.tabListPopupButton = true;
                    Mosaic::Array<bool, 4> previouslyOpen = m_documentOpen;
                    Mosaic::tabs(ui, "documents", &m_documentTab, labelViews, Mosaic::BoolSpan(m_documentOpen.data(), m_documentOpen.size()), tabOptions);
                    for(size_t index = 0; index != m_documentOpen.size(); ++index)
                    {
                        if(previouslyOpen[index] == false)
                        {
                            continue;
                        }

                        if(m_documentOpen[index])
                        {
                            continue;
                        }

                        if(m_documentDirty[index] == false)
                        {
                            continue;
                        }

                        m_documentOpen[index] = true;
                        m_documentClosePending = static_cast<int>(index);
                    }

                    if(m_documentTab >= 0 && m_documentOpen[static_cast<size_t>(m_documentTab)] == true)
                    {
                        drawDocumentContents(static_cast<size_t>(m_documentTab));
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
            Mosaic::Array<Mosaic::Id, 4> documentWindows = {};
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
                Mosaic::String label(documentNames[index]);

                if(m_documentDirty[index])
                {
                    label += " *";
                }

                bool wasOpen = m_documentOpen[index];
                auto documentWindow = Mosaic::window(ui, Mosaic::Key(0xd0c400U + index), label, documentOptions);
                documentWindows[index] = documentWindow.id();

                if(documentWindow.visible() == true)
                {
                    drawDocumentContents(index);
                }

                if(wasOpen == true && m_documentOpen[index] == false && m_documentDirty[index])
                {
                    m_documentOpen[index] = true;
                    m_documentClosePending = static_cast<int>(index);
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
            size_t pending = static_cast<size_t>(m_documentClosePending);
            bool open = true;
            auto modal = Mosaic::modal(ui, "Save document?", &open, {390.f, 150.f});

            if(modal.visible() == true)
            {
                Mosaic::String message = "Save changes to \"";
                message += documentNames[pending];
                message += "\"?";
                Mosaic::text(ui, message);
                auto actions = Mosaic::row(ui);

                if(Mosaic::button(ui, "Save").clicked() == true)
                {
                    m_documentDirty[pending] = false;
                    m_documentOpen[pending] = false;
                    m_documentClosePending = -1;
                }

                if(Mosaic::button(ui, "Discard").clicked() == true)
                {
                    m_documentOpen[pending] = false;
                    m_documentClosePending = -1;
                }

                if(Mosaic::button(ui, "Cancel").clicked() == true)
                {
                    m_documentClosePending = -1;
                }
            }

            if(open == false)
            {
                m_documentClosePending = -1;
            }
        }

        if(m_documentRenamePending >= 0)
        {
            bool open = true;
            auto modal = Mosaic::modal(ui, "Rename document", &open, {390.f, 145.f});

            if(modal.visible() == true)
            {
                Mosaic::TextInputOptions renameOptions;
                renameOptions.selectAllOnFocus = true;
                Mosaic::inputText(ui, "Name", &m_documentRenameValue, renameOptions);
                auto actions = Mosaic::row(ui);

                if(Mosaic::button(ui, "Rename").clicked() == true && m_documentRenameValue.empty() == false)
                {
                    m_documentNames[static_cast<size_t>(m_documentRenamePending)] = m_documentRenameValue;
                    m_documentRenamePending = -1;
                }

                if(Mosaic::button(ui, "Cancel").clicked() == true)
                {
                    m_documentRenamePending = -1;
                }
            }

            if(open == false)
            {
                m_documentRenamePending = -1;
            }
        }

        if(m_showDockspace == true)
        {
            Mosaic::Rect fullscreenBounds = m_dockspaceUseWorkArea ? viewport.workArea : viewport.bounds;
            Mosaic::WindowOptions options;
            options.open = &m_showDockspace;
            options.initialBounds = m_dockspaceFullscreen ? fullscreenBounds : Mosaic::Rect{120.f, 80.f, 760.f, 520.f};
            options.movable = m_dockspaceFullscreen == false;
            options.resizable = m_dockspaceFullscreen == false;
            options.dockable = false;
            auto window = Mosaic::window(ui, "Example: DockSpace", options);

            if(window.visible() == true)
            {
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
                else
                {
                    {
                        auto modes = Mosaic::row(ui);

                        if(Mosaic::radioButton(ui, "Basic demo mode", m_dockspaceMode == 0).clicked() == true)
                        {
                            m_dockspaceMode = 0;
                        }

                        if(Mosaic::radioButton(ui, "Advanced demo mode", m_dockspaceMode == 1).clicked() == true)
                        {
                            m_dockspaceMode = 1;
                        }
                    }
                    Mosaic::text(ui, "This host demonstrates a central dock area. Drag dockable Mosaic windows by "
                                     "their title bars to split, tab or float them.");

                    if(m_dockspaceMode == 1)
                    {
                        Mosaic::separatorText(ui, "Options");
                        Mosaic::checkbox(ui, "NoSplit", &m_configurationFlags[12]);
                        Mosaic::checkbox(ui, "NoResize", &m_dockspaceNoResize);
                        Mosaic::checkbox(ui, "NoUndocking", &m_dockspaceNoUndocking);
                        Mosaic::checkbox(ui, "NoDockingOverCentralNode", &m_dockspaceNoDockingOverCentral);
                        Mosaic::checkbox(ui, "PassthruCentralNode", &m_dockspacePassthruCentral);
                        Mosaic::checkbox(ui, "AutoHideTabBar", &m_dockspaceAutoHideTabBar);
                        Mosaic::checkbox(ui, "Use work area instead of main area", &m_dockspaceUseWorkArea);
                        Mosaic::checkbox(ui, "Keep Window Padding", &m_dockspaceKeepPadding);

                        if(Mosaic::checkbox(ui, "Fullscreen", &m_dockspaceFullscreen).changed() == true)
                        {
                            Mosaic::setWindowBounds(ui, window.id(), m_dockspaceFullscreen ? fullscreenBounds : Mosaic::Rect{120.f, 80.f, 760.f, 520.f});
                        }
                    }

                    if(m_dockspaceFullscreen == true)
                    {
                        Mosaic::setWindowBounds(ui, window.id(), fullscreenBounds);
                    }

                    {
                        auto actions = Mosaic::row(ui);

                        if(Mosaic::button(ui, "Redock all").clicked() == true)
                        {
                            m_dockspaceRedockRequested = true;
                        }

                        if(Mosaic::button(ui, "Close this window").clicked() == true)
                        {
                            m_showDockspace = false;
                        }
                    }
                    {
                        auto panels = Mosaic::row(ui);
                        Mosaic::checkbox(ui, "Scene", &m_dockspacePanels[0]);
                        Mosaic::checkbox(ui, "Inspector", &m_dockspacePanels[1]);
                        Mosaic::checkbox(ui, "Console", &m_dockspacePanels[2]);
                    }
                    Mosaic::LayoutOptions dockLayout;
                    dockLayout.width = Mosaic::SizeRule::Fill;
                    dockLayout.height = Mosaic::Dimension::fixed(390.f);
                    Mosaic::DockSpaceOptions dockSpaceOptions;
                    dockSpaceOptions.group = 2;
                    dockSpaceOptions.noSplit = m_configurationFlags[12];
                    dockSpaceOptions.noMerge = m_configurationFlags[13];
                    dockSpaceOptions.noResize = m_dockspaceNoResize;
                    dockSpaceOptions.noUndocking = m_dockspaceNoUndocking;
                    dockSpaceOptions.noDockingOverCentralNode = m_dockspaceNoDockingOverCentral;
                    dockSpaceOptions.passthroughCentral = m_dockspacePassthruCentral;
                    dockSpaceOptions.autoHideTabBar = m_dockspaceAutoHideTabBar;
                    auto dockCanvas = Mosaic::dockSpace(ui, "DockSpace", dockSpaceOptions, dockLayout);
                    (void)Mosaic::debugBounds(ui, dockCanvas.id(), &dockspaceBounds);
                }
            }
        }

        if(m_showDockspace == true && dockspaceBounds.empty() == false)
        {
            constexpr uint32_t dockGroup = 2;
            Mosaic::Array<Mosaic::Id, 3> dockWindows = {};
            constexpr Mosaic::Array<Mosaic::StringView, 3> labels = {"Scene", "Inspector", "Console"};
            for(size_t index = 0; index != labels.size(); ++index)
            {
                if(m_dockspacePanels[index] == false)
                {
                    continue;
                }

                Mosaic::WindowOptions dockOptions;
                dockOptions.open = &m_dockspacePanels[index];
                dockOptions.initialBounds = {dockspaceBounds.x + 20.f + static_cast<float>(index) * 28.f, dockspaceBounds.y + 20.f + static_cast<float>(index) * 24.f, std::max(180.f, dockspaceBounds.width * 0.42f), std::max(140.f, dockspaceBounds.height * 0.56f)};
                dockOptions.minimumSize = {140.f, 90.f};
                dockOptions.resizable = m_dockspaceNoResize == false;
                dockOptions.dockAutoHideTabBar = m_dockspaceAutoHideTabBar;
                dockOptions.padding = m_dockspaceKeepPadding;
                dockOptions.dockGroup = dockGroup;
                auto dockWindow = Mosaic::window(ui, Mosaic::Key(0xd0c000U + index), labels[index], dockOptions);
                dockWindows[index] = dockWindow.id();

                if(dockWindow.visible() == false)
                {
                    continue;
                }

                if(index == 0)
                {
                    Mosaic::text(ui, "Dockable scene contents");
                    Mosaic::button(ui, "Play");
                }
                else if(index == 1)
                {
                    Mosaic::text(ui, "Dockable inspector contents");
                    Mosaic::property(ui, "Visible", &m_dockspacePropertyVisible);
                }
                else
                {
                    Mosaic::text(ui, "Dockable console contents");
                    Mosaic::bulletText(ui, "Drag this title tab to rearrange the dock tree.");
                }
            }

            if(m_dockspaceInitialized == false || m_dockspaceRedockRequested == true)
            {
                Mosaic::clearDockSpace(ui, dockGroup);
                Mosaic::DockNodeId root = Mosaic::dockSpaceRoot(ui, dockGroup);
                bool firstDocked = false;
                for(size_t index = 0; index != dockWindows.size(); ++index)
                {
                    if(dockWindows[index] == Mosaic::InvalidId)
                    {
                        continue;
                    }

                    if(firstDocked == false)
                    {
                        (void)Mosaic::dockWindow(ui, dockGroup, dockWindows[index], root, Mosaic::DockPlacement::Center);
                        root = Mosaic::dockNodeForWindow(ui, dockGroup, dockWindows[index]);
                        firstDocked = true;
                    }
                    else
                    {
                        Mosaic::DockPlacement placement = index == 2 ? Mosaic::DockPlacement::Bottom : Mosaic::DockPlacement::Right;
                        float ratio = index == 2 ? 0.32f : 0.35f;
                        (void)Mosaic::dockWindow(ui, dockGroup, dockWindows[index], root, placement, ratio);
                    }
                }
                m_dockspaceInitialized = true;
                m_dockspaceRedockRequested = false;
            }
        }

        if(m_showImageViewer == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showImageViewer;
            options.initialBounds = {240.f, 90.f, 580.f, 470.f};
            auto window = Mosaic::window(ui, "Example: Image Viewer", options);

            if(window.visible() == true)
            {
                drawImageViewerContents(ui, 350.f);
            }
        }

        if(m_showLog == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showLog;
            options.initialBounds = {250.f, 130.f, 520.f, 360.f};
            auto window = Mosaic::window(ui, "Example: Log", options);

            if(window.visible() == true)
            {
                auto actions = Mosaic::row(ui);

                if(Mosaic::button(ui, "Clear").clicked() == true)
                {
                    m_logItems.clear();
                }

                if(Mosaic::button(ui, "[Debug] Add 5 entries").clicked() == true)
                {
                    for(size_t index = 0; index != 5; ++index)
                    {
                        Mosaic::String line = "[info] Log entry ";
                        line += ExamplesDetail::number(m_logItems.size());
                        m_logItems.push_back(std::move(line));
                    }
                }

                Mosaic::checkbox(ui, "Auto-scroll", &m_logAutoScroll);
                Mosaic::searchField(ui, "Filter", &m_logFilter);
                Mosaic::LayoutOptions logLayout;
                logLayout.width = Mosaic::SizeRule::Fill;
                logLayout.height = Mosaic::Dimension::fixed(240.f);
                auto log = Mosaic::scrollArea(ui, "Log entries", Mosaic::ScrollOptions{}, logLayout);
                m_logVisibleItems.clear();
                for(size_t index = 0; index != m_logItems.size(); ++index)
                {
                    const Mosaic::String & line = m_logItems[index];

                    if(m_logFilter.empty() == true || line.find(m_logFilter) != Mosaic::String::npos)
                    {
                        m_logVisibleItems.push_back(index);
                    }
                }
                float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                Mosaic::VisibleRange range;
                (void)Mosaic::beginListClipper(ui, m_logVisibleItems.size(), itemHeight, &range, log.id());
                for(size_t visible = range.begin; visible != range.end; ++visible)
                {
                    size_t index = m_logVisibleItems[visible];
                    auto item = Mosaic::scope(ui, Mosaic::Key(index));
                    Mosaic::text(ui, m_logItems[index]);
                }
                Mosaic::endListClipper(ui, range, m_logVisibleItems.size(), itemHeight);

                if(m_logAutoScroll == true)
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
            options.initialBounds = {300.f, 80.f, 760.f, 560.f};
            auto window = Mosaic::window(ui, "Example: Property editor", options);

            if(window.visible() == true)
            {
                Mosaic::SplitOptions splitOptions;
                splitOptions.minimumFirst = 240.f;
                splitOptions.minimumSecond = 300.f;
                Mosaic::LayoutOptions splitLayout;
                splitLayout.width = Mosaic::SizeRule::Fill;
                splitLayout.height = Mosaic::SizeRule::Fill;
                auto split = Mosaic::split(ui, "Property editor panes", Mosaic::Orientation::Horizontal, &m_propertySplitRatio, splitOptions, splitLayout);
                {
                    auto objectsPane = Mosaic::column(ui, ExamplesDetail::fill());
                    {
                        auto controls = Mosaic::row(ui);
                        Mosaic::checkbox(ui, "Use Clipper", &m_propertyUseClipper);
                        Mosaic::text(ui, "(10 root nodes)");
                    }
                    Mosaic::TextInputOptions filterOptions;
                    filterOptions.hint = "incl,-excl";
                    filterOptions.escapeClearsAll = true;
                    Mosaic::inputText(ui, "Filter", &m_propertyFilter, filterOptions);
                    Mosaic::LayoutOptions treeLayout = ExamplesDetail::fill();
                    auto objects = Mosaic::scrollArea(ui, "Property tree", Mosaic::ScrollOptions{}, treeLayout);

                    m_propertyVisibleNodes.clear();
                    for(size_t index = 0; index != m_propertyNodes.size(); ++index)
                    {
                        if(m_propertyNodes[index].parent != std::numeric_limits<size_t>::max())
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
                        m_propertySelectedNode = 0;
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

                    auto nextProperty = [ui](Mosaic::StringView label)
                    {
                        Mosaic::tableNextRow(ui);
                        (void)Mosaic::tableSetColumn(ui, 0);
                        Mosaic::text(ui, label);
                        (void)Mosaic::tableSetColumn(ui, 1);
                    };
                    nextProperty("Enabled");
                    Mosaic::checkbox(ui, "Enabled", &selected.enabled);
                    nextProperty("Integer values");
                    Mosaic::inputIntVector(ui, "Integer values", selected.integers);
                    nextProperty("Scalar values");
                    Mosaic::SliderOptions scalarOptions;
                    scalarOptions.minimum = 0.0;
                    scalarOptions.maximum = 1.0;
                    scalarOptions.width = Mosaic::SizeRule::Fill;
                    Mosaic::sliderFloatVector(ui, "Scalar values", selected.scalars, scalarOptions);
                    nextProperty("Name");
                    Mosaic::inputText(ui, "Name", &selected.name);
                    nextProperty("Tint");
                    Mosaic::colorEditorRgba(ui, "Tint", &m_propertyTint);
                }
            }
        }

        if(m_showSimpleOverlay == true && m_overlayOpen == true)
        {
            Mosaic::Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
            constexpr float margin = 10.f;
            constexpr float overlayWidth = 310.f;
            constexpr float overlayHeight = 118.f;
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
                Mosaic::text(ui, "Simple overlay\nin the corner of the screen.\n(right-click to change position)");
                Mosaic::Vec2 pointerPosition;
                if(Mosaic::pointerPosition(ui, &pointerPosition) == false)
                {
                    return;
                }

                Mosaic::String position = "Mouse Position: (";
                position += ExamplesDetail::fixed(pointerPosition.x, 1);
                position += ", ";
                position += ExamplesDetail::fixed(pointerPosition.y, 1);
                position += ")";
                Mosaic::text(ui, position);
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
                    constexpr Mosaic::Array<Mosaic::StringView, 6> labels = {"Top-left", "Top-right", "Bottom-left", "Bottom-right", "Center", "Custom"};
                    for(size_t index = 0; index != labels.size(); ++index)
                    {
                        auto item = Mosaic::scope(ui, Mosaic::Key(index));
                        Mosaic::MenuItemOptions menuOptions;
                        menuOptions.selected = m_overlayCorner == static_cast<int>(index);

                        if(Mosaic::menuItem(ui, labels[index], menuOptions).clicked() == true)
                        {
                            m_overlayCorner = static_cast<int>(index);

                            if(index < positions.size())
                            {
                                Mosaic::setWindowBounds(ui, window.id(), positions[index]);
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
            options.initialBounds = {90.f, 180.f, 330.f, 180.f};
            options.fitContentWidth = true;
            options.fitContentHeight = true;
            auto window = Mosaic::window(ui, "Example: Auto-resizing window", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "Window will resize every-frame to the size of its contents.");
                Mosaic::slider(ui, "Number of lines", &m_autoResizeLines, int32_t{1}, int32_t{20});
                for(int32_t index = 0; index < m_autoResizeLines; ++index)
                {
                    Mosaic::String line = "Line ";
                    line += ExamplesDetail::number(index);
                    Mosaic::text(ui, line);
                }
            }
        }

        if(m_showConstrainedResize == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showConstrainedResize;
            options.initialBounds = {100.f, 100.f, 520.f, 360.f};
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
                    if(viewport.contentRect(&bounds) == false)
                    {
                        return;
                    }

                    viewport.rect(bounds, Mosaic::Color{0.5f, 0.2f, 0.5f, 1.f});
                    Mosaic::String size = ExamplesDetail::fixed(bounds.width, 2);
                    size += " x ";
                    size += ExamplesDetail::fixed(bounds.height, 2);
                    Mosaic::text(ui, size);
                }
                else
                {
                    constexpr Mosaic::Array<Mosaic::StringView, 9> constraints = {"Between 100x100 and 500x500", "At least 100x100", "Resize vertical + lock current width", "Resize horizontal + lock current height", "Width between 400 and 500", "Height at least 400", "Custom: 16:9 aspect ratio", "Custom: Square", "Custom: Fixed steps (100)"};
                    Mosaic::comboBox(ui, "Constraint", &m_resizeType, constraints);

                    if(m_resizeType == 6)
                    {
                        Mosaic::slider(ui, "Aspect ratio", &m_constrainedAspect, 0.5f, 3.f);
                    }

                    if(m_resizeType == 8)
                    {
                        Mosaic::slider(ui, "Step", &m_constrainedStep, 10.f, 200.f);
                    }

                    Mosaic::checkbox(ui, "Auto-resize", &m_constrainedAutoResize);
                    Mosaic::checkbox(ui, "Window padding", &m_constrainedWindowPadding);

                    if(m_constrainedAutoResize == true)
                    {
                        Mosaic::slider(ui, "Lines", &m_constrainedLines, int32_t{1}, int32_t{30});
                    }

                    {
                        auto sizes = Mosaic::row(ui);
                        Mosaic::Rect current;
                        if(Mosaic::windowBounds(ui, window.id(), &current) == false)
                        {
                            return;
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
                    Mosaic::text(ui, "Drag a window edge. Axis locks, minimum size and custom size callbacks are "
                                     "applied by WindowOptions.");
                    for(int32_t line = 0; line < m_constrainedLines; ++line)
                    {
                        Mosaic::String text = "Line ";
                        text += ExamplesDetail::number(line + 1);
                        Mosaic::text(ui, text);
                    }
                }
            }
        }

        if(m_showFullscreen == true)
        {
            Mosaic::Rect workArea = viewport.workArea.empty() == true ? viewport.bounds : viewport.workArea;
            Mosaic::WindowOptions options;
            options.open = &m_showFullscreen;
            options.initialBounds = workArea;
            options.minimumSize = {640.f, 360.f};
            options.movable = false;
            options.resizable = false;
            options.dockable = false;
            options.titleBar = m_fullscreenNoDecoration == false;
            options.background = m_fullscreenNoBackground == false;
            auto window = Mosaic::window(ui, "Example: Fullscreen window", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "This window covers the main viewport work area. Use the close button to return to the demo.");
                Mosaic::checkbox(ui, "No background", &m_fullscreenNoBackground);
                Mosaic::checkbox(ui, "No decoration", &m_fullscreenNoDecoration);
            }
        }

        if(m_showLongText == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showLongText;
            options.initialBounds = {100.f, 100.f, 620.f, 520.f};
            auto window = Mosaic::window(ui, "Example: Long text display", options);

            if(window.visible() == true)
            {
                if(Mosaic::radioButton(ui, "Single call to Text()", m_longTextMode == 0).clicked() == true)
                {
                    m_longTextMode = 0;
                }

                Mosaic::helpMarker(ui, "One large text submission. Unchanged content reuses the text cache.");

                if(Mosaic::radioButton(ui, "Multiple calls, clipped", m_longTextMode == 1).clicked() == true)
                {
                    m_longTextMode = 1;
                }

                Mosaic::helpMarker(ui, "The list clipper skips layout nodes and text preparation outside the visible range.");

                if(Mosaic::radioButton(ui, "Multiple calls, not clipped (slow)", m_longTextMode == 2).clicked() == true)
                {
                    m_longTextMode = 2;
                }

                {
                    auto actions = Mosaic::row(ui);

                    if(Mosaic::button(ui, "Add 1000 lines").clicked() == true)
                    {
                        m_longTextLines = std::min(m_longTextLines + 1000, int32_t{20000});
                    }

                    if(Mosaic::button(ui, "Clear").clicked() == true)
                    {
                        m_longTextLines = 0;
                    }
                }
                Mosaic::slider(ui, "Lines", &m_longTextLines, int32_t{0}, int32_t{20000});

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

                Mosaic::String statistics = ExamplesDetail::number(m_longTextLines);
                statistics += " lines, ";
                statistics += ExamplesDetail::number(m_longTextBuffer.size());
                statistics += " bytes";
                Mosaic::text(ui, statistics);
                Mosaic::LayoutOptions contentLayout;
                contentLayout.width = Mosaic::SizeRule::Fill;
                contentLayout.height = Mosaic::Dimension::fixed(350.f);
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
            double timestamp = Mosaic::input(ui).timestamp;
            Mosaic::String animated = "Animated title ";
            animated += ExamplesDetail::fixed(timestamp, 1);
            Mosaic::WindowOptions first;
            first.initialBounds = {90.f, 90.f, 330.f, 110.f};
            first.dockable = false;
            auto animatedWindow = Mosaic::window(ui, Mosaic::Key("AnimatedTitle"), animated, first);

            if(animatedWindow.visible() == true)
            {
                Mosaic::text(ui, "The visible title changes while the stable key does not.");
            }

            Mosaic::WindowOptions second;
            second.initialBounds = {90.f, 220.f, 330.f, 110.f};
            second.dockable = false;
            auto secondWindow = Mosaic::window(ui, Mosaic::Key("SameTitle1"), "Same title as another window", second);

            if(secondWindow.visible() == true)
            {
                Mosaic::text(ui, "Window 1");
            }

            Mosaic::WindowOptions third;
            third.initialBounds = {90.f, 350.f, 330.f, 110.f};
            third.dockable = false;
            auto thirdWindow = Mosaic::window(ui, Mosaic::Key("SameTitle2"), "Same title as another window", third);

            if(thirdWindow.visible() == true)
            {
                Mosaic::text(ui, "Window 2");
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
            options.initialBounds = {40.f, 80.f, 520.f, 560.f};
            auto window = Mosaic::window(ui, "Mosaic Metrics/Debugger", options);

            if(window.visible() == true)
            {
                const Mosaic::Frame & frame = Mosaic::getFrame(ui);
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
                Mosaic::String textCache = "Text cache: ";
                textCache += ExamplesDetail::number(frame.metrics.textCacheEntryCount);
                textCache += " entries, ";
                textCache += ExamplesDetail::number(frame.metrics.textCacheHitCount);
                textCache += " hits, ";
                textCache += ExamplesDetail::number(frame.metrics.textCacheMissCount);
                textCache += " misses, ";
                textCache += ExamplesDetail::number(frame.metrics.textCacheEvictionCount);
                textCache += " evictions, ";
                textCache += ExamplesDetail::number(frame.metrics.textCacheMemory);
                textCache += " bytes";
                Mosaic::text(ui, textCache);
                Mosaic::String fontCache = "Font cache: ";
                fontCache += ExamplesDetail::number(frame.metrics.fontCount);
                fontCache += " fonts, ";
                fontCache += ExamplesDetail::number(frame.metrics.glyphCount);
                fontCache += " glyphs, ";
                fontCache += ExamplesDetail::number(frame.metrics.fontAtlasPageCount);
                fontCache += " atlas pages, ";
                fontCache += ExamplesDetail::number(frame.metrics.fontAtlasMemory);
                fontCache += " bytes";
                Mosaic::text(ui, fontCache);
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
                Mosaic::String timings = "CPU ms: build ";
                timings += ExamplesDetail::fixed(frame.metrics.buildMilliseconds, 3);
                timings += ", layout ";
                timings += ExamplesDetail::fixed(frame.metrics.layoutMilliseconds, 3);
                timings += ", emit ";
                timings += ExamplesDetail::fixed(frame.metrics.emitMilliseconds, 3);
                timings += ", mesh ";
                timings += ExamplesDetail::fixed(m_renderMetrics.buildMilliseconds, 3);
                Mosaic::text(ui, timings);
                {
                    auto tools = Mosaic::treeNode(ui, Mosaic::Key("metrics tools"), "Tools", true);

                    if(tools.expanded() == true)
                    {
                        Mosaic::checkbox(ui, "Show Debug Log", &m_showDebugLog);
                        Mosaic::checkbox(ui, "Show Item Picker", &m_showItemPicker);
                        Mosaic::separatorText(ui, "Visualize");
                        Mosaic::checkbox(ui, "Show windows rectangles", &m_metricsOverlayFlags[0]);
                        Mosaic::checkbox(ui, "Show draw command mesh", &m_metricsOverlayFlags[1]);
                        Mosaic::helpMarker(ui, "Shows the bounds of renderer batches composing the previous frame mesh.");
                        Mosaic::checkbox(ui, "Show clipping rectangles", &m_metricsOverlayFlags[2]);
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
                            line += ExamplesDetail::number(viewport.drawCommands.size());
                            Mosaic::bulletText(ui, line);
                        }
                    }
                }
                {
                    auto docking = Mosaic::treeNode(ui, Mosaic::Key("metrics docking"), "Docking");

                    if(docking.expanded() == true)
                    {
                        for(uint32_t group = 1; group != 5; ++group)
                        {
                            const Mosaic::DockModel & model = Mosaic::docking(ui, group);
                            Mosaic::String groupLabel = "Group ";
                            groupLabel += ExamplesDetail::number(group);
                            groupLabel += ": root ";
                            groupLabel += ExamplesDetail::number(model.root());
                            groupLabel += ", nodes ";
                            groupLabel += ExamplesDetail::number(model.nodes().size());
                            Mosaic::bulletText(ui, groupLabel);
                            for(const Mosaic::DockNode & node : model.nodes())
                            {
                                auto nodeScope = Mosaic::scope(ui, Mosaic::Key(node.id));
                                Mosaic::String nodeLabel = "    node ";
                                nodeLabel += ExamplesDetail::number(node.id);
                                nodeLabel += node.type == Mosaic::DockNodeType::Split ? " split" : " tabs";
                                nodeLabel += ", parent ";
                                nodeLabel += ExamplesDetail::number(node.parent);
                                nodeLabel += ", windows ";
                                nodeLabel += ExamplesDetail::number(node.tabs.size());
                                Mosaic::text(ui, nodeLabel);
                            }
                        }
                    }
                }
                {
                    auto inputState = Mosaic::treeNode(ui, Mosaic::Key("metrics input state"), "Inputs");

                    if(inputState.expanded() == true)
                    {
                        Mosaic::String focus = "Focused: ";
                        focus += ExamplesDetail::number(Mosaic::focused(ui));
                        Mosaic::text(ui, focus);
                        Mosaic::String navigation = "Navigation focus: ";
                        navigation += ExamplesDetail::number(Mosaic::navigationFocus(ui));
                        Mosaic::text(ui, navigation);
                        Mosaic::String capture = "Pointer owner: ";
                        capture += ExamplesDetail::number(Mosaic::capturedPointerOwner(ui));
                        Mosaic::text(ui, capture);
                        Mosaic::String routing = "Capture pointer/keyboard/text: ";
                        routing += frame.inputCapture.pointer ? "1/" : "0/";
                        routing += frame.inputCapture.keyboard ? "1/" : "0/";
                        routing += frame.inputCapture.text ? "1" : "0";
                        Mosaic::text(ui, routing);
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

        if(m_showMetrics == true && (m_metricsOverlayFlags[0] || m_metricsOverlayFlags[1] || m_metricsOverlayFlags[2]))
        {
            Mosaic::LayoutOptions overlayLayout;
            overlayLayout.width = Mosaic::Dimension::fixed(1.f);
            overlayLayout.height = Mosaic::Dimension::fixed(1.f);
            Mosaic::Canvas overlay = Mosaic::canvas(ui, "Metrics overlays", overlayLayout);
            overlay.setLayer(Mosaic::CanvasLayer::Foreground);
            Mosaic::Rect overlayBounds;
            if(Mosaic::debugBounds(ui, overlay.id(), &overlayBounds) == false)
            {
                return;
            }

            Mosaic::Vec2 origin = {overlayBounds.x, overlayBounds.y};

            if(m_metricsOverlayFlags[1] && m_renderMesh != nullptr)
            {
                for(const Mosaic::RenderBatch & batch : m_renderMesh->batches)
                {
                    Mosaic::Rect bounds = {batch.bounds.x - origin.x, batch.bounds.y - origin.y, batch.bounds.width, batch.bounds.height};
                    Mosaic::Color color = Mosaic::Color::fromBytes(70, 215, 255, 150);
                    overlay.line({bounds.x, bounds.y}, {bounds.right(), bounds.y}, 1.f, color);
                    overlay.line({bounds.right(), bounds.y}, {bounds.right(), bounds.bottom()}, 1.f, color);
                    overlay.line({bounds.right(), bounds.bottom()}, {bounds.x, bounds.bottom()}, 1.f, color);
                    overlay.line({bounds.x, bounds.bottom()}, {bounds.x, bounds.y}, 1.f, color);
                }
            }

            for(const Mosaic::DebugInfo & node : Mosaic::getFrame(ui).debug)
            {
                bool selected = node.id == m_metricsSelectedNode;
                bool drawWindow = m_metricsOverlayFlags[0] && node.type == "Window";
                bool drawClip = m_metricsOverlayFlags[2] && selected;

                if(drawWindow == true)
                {
                    Mosaic::Rect bounds = {node.bounds.x - origin.x, node.bounds.y - origin.y, node.bounds.width, node.bounds.height};
                    overlay.line({bounds.x, bounds.y}, {bounds.right(), bounds.y}, 1.f, Mosaic::Color::fromBytes(255, 210, 40, 220));
                    overlay.line({bounds.right(), bounds.y}, {bounds.right(), bounds.bottom()}, 1.f, Mosaic::Color::fromBytes(255, 210, 40, 220));
                    overlay.line({bounds.right(), bounds.bottom()}, {bounds.x, bounds.bottom()}, 1.f, Mosaic::Color::fromBytes(255, 210, 40, 220));
                    overlay.line({bounds.x, bounds.bottom()}, {bounds.x, bounds.y}, 1.f, Mosaic::Color::fromBytes(255, 210, 40, 220));
                }

                if(drawClip == true)
                {
                    Mosaic::Rect clip = {node.clip.x - origin.x, node.clip.y - origin.y, node.clip.width, node.clip.height};
                    overlay.line({clip.x, clip.y}, {clip.right(), clip.y}, 2.f, Mosaic::Color::fromBytes(255, 80, 170, 230));
                    overlay.line({clip.right(), clip.y}, {clip.right(), clip.bottom()}, 2.f, Mosaic::Color::fromBytes(255, 80, 170, 230));
                    overlay.line({clip.right(), clip.bottom()}, {clip.x, clip.bottom()}, 2.f, Mosaic::Color::fromBytes(255, 80, 170, 230));
                    overlay.line({clip.x, clip.bottom()}, {clip.x, clip.y}, 2.f, Mosaic::Color::fromBytes(255, 80, 170, 230));
                }
            }
        }

        if(m_showDebugLog == true)
        {
            bool appendedEvents = Mosaic::getFrame(ui).events.empty() == false;
            constexpr size_t maximumDebugEvents = 4096;
            for(const Mosaic::EventTraceEntry & event : Mosaic::getFrame(ui).events)
            {
                Mosaic::StringView eventName = ExamplesDetail::eventName(event.type);
                Mosaic::String line(eventName.data(), eventName.size());
                line += " ";
                line += event.path;
                line += " @ ";
                line += ExamplesDetail::fixed(event.timestamp, 3);

                if(m_debugEventItems.size() < maximumDebugEvents)
                {
                    m_debugEventItems.push_back({event.type, std::move(line)});
                }
                else
                {
                    m_debugEventItems[m_debugEventHead] = {event.type, std::move(line)};
                    m_debugEventHead = (m_debugEventHead + 1) % maximumDebugEvents;
                }
            }

            Mosaic::WindowOptions options;
            options.open = &m_showDebugLog;
            options.initialBounds = {120.f, 90.f, 620.f, 430.f};
            auto window = Mosaic::window(ui, "Mosaic Debug Log", options);

            if(window.visible() == true)
            {
                {
                    auto actions = Mosaic::row(ui);

                    if(Mosaic::button(ui, "Clear").clicked() == true)
                    {
                        m_debugEventItems.clear();
                        m_debugEventHead = 0;
                    }

                    if(Mosaic::button(ui, "Copy").clicked() == true)
                    {
                        Mosaic::String contents;
                        for(size_t logical = 0; logical != m_debugEventItems.size(); ++logical)
                        {
                            size_t physical = (m_debugEventHead + logical) % m_debugEventItems.size();
                            size_t category = ExamplesDetail::eventCategory(m_debugEventItems[physical].type);

                            if(m_debugLogCategories[category] == false)
                            {
                                continue;
                            }

                            contents += m_debugEventItems[physical].text;
                            contents += "\n";
                        }
                        Mosaic::platform(ui).setClipboardText(contents);
                    }

                    Mosaic::checkbox(ui, "Auto-scroll", &m_debugLogAutoScroll);
                }
                {
                    auto categories = Mosaic::treeNode(ui, Mosaic::Key("debug log categories"), "Event categories", true);

                    if(categories.expanded() == true)
                    {
                        auto categoryRow = Mosaic::row(ui);
                        Mosaic::checkbox(ui, "Input / Focus", &m_debugLogCategories[0]);
                        Mosaic::checkbox(ui, "Editing", &m_debugLogCategories[1]);
                        Mosaic::checkbox(ui, "Drag / Drop", &m_debugLogCategories[2]);
                        Mosaic::checkbox(ui, "Popup", &m_debugLogCategories[3]);
                    }
                }
                Mosaic::separator(ui);
                m_debugVisibleEvents.clear();
                for(size_t logical = 0; logical != m_debugEventItems.size(); ++logical)
                {
                    size_t physical = (m_debugEventHead + logical) % m_debugEventItems.size();
                    size_t category = ExamplesDetail::eventCategory(m_debugEventItems[physical].type);

                    if(m_debugLogCategories[category])
                    {
                        m_debugVisibleEvents.push_back(logical);
                    }
                }
                Mosaic::LayoutOptions logLayout;
                logLayout.width = Mosaic::SizeRule::Fill;
                logLayout.height = Mosaic::SizeRule::Fill;
                auto log = Mosaic::scrollArea(ui, "Debug event entries", Mosaic::ScrollOptions{}, logLayout);
                float itemHeight = Mosaic::getTheme(ui).metrics.lineHeight;
                Mosaic::VisibleRange range;
                (void)Mosaic::beginListClipper(ui, m_debugVisibleEvents.size(), itemHeight, &range, log.id());
                for(size_t index = range.begin; index != range.end; ++index)
                {
                    auto item = Mosaic::scope(ui, Mosaic::Key(index));
                    size_t logical = m_debugVisibleEvents[index];
                    size_t physical = (m_debugEventHead + logical) % m_debugEventItems.size();
                    Mosaic::text(ui, m_debugEventItems[physical].text);
                }
                Mosaic::endListClipper(ui, range, m_debugVisibleEvents.size(), itemHeight);

                if(appendedEvents == true && m_debugLogAutoScroll == true)
                {
                    Mosaic::scrollToEnd(ui, log.id(), Mosaic::ScrollAxes::Vertical);
                }
            }
        }

        if(m_showIdStack == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showIdStack;
            options.initialBounds = {190.f, 110.f, 610.f, 430.f};
            auto window = Mosaic::window(ui, "Mosaic ID Stack Tool", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "Hover an item in the demo to inspect its parent-derived identifier path.");
                Mosaic::separator(ui);
                const Mosaic::DebugInfoVector & debug = Mosaic::getFrame(ui).debug;
                Mosaic::Id inspected = Mosaic::InvalidId;
                for(auto iterator = debug.rbegin(); iterator != debug.rend(); ++iterator)
                {
                    if(iterator->hovered == false && iterator->active == false && iterator->focused == false)
                    {
                        continue;
                    }

                    inspected = iterator->id;
                    break;
                }
                Mosaic::IdVector chain;
                while(inspected != Mosaic::InvalidId)
                {
                    chain.push_back(inspected);
                    auto found = std::find_if(debug.begin(), debug.end(),
                                                    [inspected](const Mosaic::DebugInfo & node)
                                                    {
                                                        return node.id == inspected;
                                                    });

                    if(found == debug.end())
                    {
                        break;
                    }

                    if(found->parentId == inspected)
                    {
                        break;
                    }

                    inspected = found->parentId;
                }
                std::reverse(chain.begin(), chain.end());
                for(size_t depth = 0; depth != chain.size(); ++depth)
                {
                    auto found = std::find_if(debug.begin(), debug.end(),
                                                    [&chain, depth](const Mosaic::DebugInfo & node)
                                                    {
                                                        return node.id == chain[depth];
                                                    });

                    if(found == debug.end())
                    {
                        continue;
                    }

                    Mosaic::String line(depth * 2, ' ');
                    line += found->type;

                    if(found->label.empty() == false)
                    {
                        line += " \"";
                        line += found->label;
                        line += "\"";
                    }

                    line += " [";
                    line += ExamplesDetail::number(found->id);
                    line += "]";
                    Mosaic::text(ui, line);
                }

                if(chain.empty() == false)
                {
                    auto found = std::find_if(debug.begin(), debug.end(),
                                                    [&chain](const Mosaic::DebugInfo & node)
                                                    {
                                                        auto returnedValue = node.id == chain.back();

                                                        return returnedValue;
                                                    });

                    if(found != debug.end())
                    {
                        Mosaic::separator(ui);
                        Mosaic::text(ui, found->path);
                    }
                }
            }
        }

        if(m_showItemPicker == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showItemPicker;
            options.initialBounds = {300.f, 140.f, 390.f, 180.f};
            options.dockable = false;
            auto window = Mosaic::window(ui, "Mosaic Item Picker", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "Arm the picker, then click an item to keep inspecting it after the pointer moves away.");

                if(Mosaic::button(ui, m_itemPickerArmed ? "Cancel picking" : "Pick item").clicked() == true)
                {
                    m_itemPickerArmed = !m_itemPickerArmed;
                }

                const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();

                if(m_itemPickerArmed == true && pointer != nullptr && pointer->isPressed() == true)
                {
                    for(auto iterator = Mosaic::getFrame(ui).debug.rbegin(); iterator != Mosaic::getFrame(ui).debug.rend(); ++iterator)
                    {
                        if(iterator->bounds.contains(pointer->position) == false)
                        {
                            continue;
                        }

                        if(iterator->clip.contains(pointer->position) == false)
                        {
                            continue;
                        }

                        m_itemPickerTarget = iterator->id;
                        m_metricsSelectedNode = iterator->id;
                        m_itemPickerArmed = false;
                        break;
                    }
                }

                for(const Mosaic::DebugInfo & node : Mosaic::getFrame(ui).debug)
                {
                    bool hoveredWithoutTarget = m_itemPickerTarget == Mosaic::InvalidId && node.hovered == true;

                    if(node.id != m_itemPickerTarget && hoveredWithoutTarget == false)
                    {
                        continue;
                    }

                    Mosaic::String value(node.type);
                    value += " \"";
                    value += node.label;
                    value += "\" ID=";
                    value += ExamplesDetail::number(node.id);
                    Mosaic::text(ui, value);
                    Mosaic::String source(node.file);
                    source += ":";
                    source += ExamplesDetail::number(node.line);
                    Mosaic::text(ui, source);
                    Mosaic::text(ui, node.path);
                    break;
                }
            }
        }

        if(m_showStyleEditor == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showStyleEditor;
            options.initialBounds = {210.f, 60.f, 520.f, 650.f};
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
                static bool exportOnlyModified = true;
                {
                    auto referenceActions = Mosaic::row(ui);

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
                        Mosaic::platform(ui).setClipboardText(output);
                    }
                    Mosaic::checkbox(ui, "Only Modified Colors", &exportOnlyModified);
                }

                constexpr Mosaic::Array<Mosaic::StringView, 4> editorTabs = {"Sizes", "Colors", "Fonts", "Rendering"};
                static int selectedTab = 0;
                Mosaic::tabs(ui, "Style editor tabs", &selectedTab, editorTabs);

                if(selectedTab == 0)
                {
                    Mosaic::separatorText(ui, "Main");
                    Mosaic::slider(ui, "Alpha", &m_styleAlpha, 0.2f, 1.f);
                    Mosaic::slider(ui, "Font size", &m_styleFontSize, 9.f, 32.f);
                    Mosaic::separatorText(ui, "Sizes");
                    Mosaic::slider(ui, "WindowPadding", &theme.metrics.padding, 0.f, 20.f);
                    Mosaic::slider(ui, "LayoutGap", &theme.metrics.gap, 0.f, 20.f);
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
                    float popupPaddingX = theme.metrics.popupPadding.left;
                    float popupPaddingY = theme.metrics.popupPadding.top;
                    Mosaic::slider(ui, "PopupPadding.x", &popupPaddingX, 0.f, 20.f);
                    Mosaic::slider(ui, "PopupPadding.y", &popupPaddingY, 0.f, 20.f);
                    theme.metrics.popupPadding.left = popupPaddingX;
                    theme.metrics.popupPadding.right = popupPaddingX;
                    theme.metrics.popupPadding.top = popupPaddingY;
                    theme.metrics.popupPadding.bottom = popupPaddingY;
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
                    Mosaic::slider(ui, "TabMinWidthBase", &theme.metrics.tabMinimumWidthBase, 1.f, 100.f);
                    Mosaic::slider(ui, "TabMinWidthShrink", &theme.metrics.tabMinimumWidthForShrink, 1.f, 100.f);
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
                    Mosaic::slider(ui, "TreeLinesSize", &theme.metrics.treeLinesSize, 0.f, 4.f);
                    Mosaic::separatorText(ui, "Windows");
                    Mosaic::slider(ui, "WindowTitleHeight", &theme.metrics.windowTitleHeight, 18.f, 48.f);
                    Mosaic::slider(ui, "WindowTitleAlign.x", &theme.metrics.windowTitleAlignment.x, 0.f, 1.f);
                    Mosaic::slider(ui, "WindowTitleAlign.y", &theme.metrics.windowTitleAlignment.y, 0.f, 1.f);
                    Mosaic::slider(ui, "WindowBorderHoverPadding", &theme.metrics.windowBorderHoverPadding, 1.f, 20.f);
                    Mosaic::separatorText(ui, "Widgets");
                    Mosaic::slider(ui, "ColorMarkerSize", &theme.metrics.colorMarkerSize, 0.f, 8.f);
                    Mosaic::slider(ui, "LogSliderDeadzone", &theme.metrics.logarithmicSliderDeadzone, 0.f, 20.f);
                    Mosaic::slider(ui, "ControlHeight", &theme.metrics.controlHeight, 16.f, 44.f);
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
                    Mosaic::slider(ui, "MinimumPopupWidth", &theme.metrics.minimumPopupWidth, 80.f, 360.f);
                    Mosaic::slider(ui, "HoverDelayNormal", &theme.behavior.tooltipHoverDelay, 0.f, 2.f);
                    Mosaic::slider(ui, "HoverDelayShort", &theme.behavior.tooltipStationaryDelay, 0.f, 2.f);
                    Mosaic::separatorText(ui, "Misc");
                    Mosaic::slider(ui, "MinimumControlWidth", &theme.metrics.minimumControlWidth, 40.f, 260.f);
                }
                else if(selectedTab == 1)
                {
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
                    for(const StyleColorEntry & entry : styleColors)
                    {
                        if(ExamplesDetail::containsIgnoreCase(entry.name, m_styleColorFilter) == false)
                        {
                            continue;
                        }

                        std::ptrdiff_t offset = reinterpret_cast<const char *>(entry.color) - reinterpret_cast<const char *>(&theme.colors);
                        auto * reference = reinterpret_cast<Mosaic::Color *>(reinterpret_cast<char *>(&m_styleReference.colors) + offset);
                        auto colorRow = Mosaic::row(ui);
                        Mosaic::colorEditorRgba(ui, entry.name, entry.color, colorOptions);

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
                    Mosaic::text(ui, "Fonts loaded by FontProvider:");
                    Mosaic::bulletText(ui, "DefaultFont — proportional UI font");
                    Mosaic::bulletText(ui, "MonospaceFont — dynamic values, code and diagnostics");
                    Mosaic::slider(ui, "FontScaleMain", &m_styleFontSize, 9.f, 32.f);
                    Mosaic::FontCacheMetrics metrics;
                    if(Mosaic::fontCacheMetrics(ui, &metrics) == false)
                    {
                        return;
                    }

                    Mosaic::String cache = "Resolved faces: ";
                    cache += ExamplesDetail::number(metrics.resolvedFaceCount);
                    cache += ", glyphs: ";
                    cache += ExamplesDetail::number(metrics.glyphCount);
                    cache += ", atlas pages: ";
                    cache += ExamplesDetail::number(metrics.atlasPageCount);
                    Mosaic::text(ui, cache);
                    Mosaic::text(ui, "Font glyphs are shaped by CoreText and cached in the Metal glyph atlas.");
                    HelloDemo::drawFontCacheInspector(ui);
                }
                else
                {
                    Mosaic::slider(ui, "Alpha", &m_styleAlpha, 0.2f, 1.f);
                    Mosaic::slider(ui, "Disabled Alpha", &theme.behavior.disabledAlpha, 0.f, 1.f);
                    Mosaic::checkbox(ui, "Hover feedback", &m_hoverEnabled);
                    Mosaic::checkbox(ui, "Animations", &m_animationsEnabled);
                    Mosaic::slider(ui, "Hover animation duration", &theme.behavior.hoverAnimationDuration, 0.f, 0.5f);
                    Mosaic::slider(ui, "Active animation duration", &theme.behavior.activeAnimationDuration, 0.f, 0.5f);
                    Mosaic::slider(ui, "Selection animation duration", &theme.behavior.selectionAnimationDuration, 0.f, 0.5f);
                    Mosaic::slider(ui, "Value animation duration", &theme.behavior.valueAnimationDuration, 0.f, 0.5f);
                    Mosaic::slider(ui, "Drag threshold", &theme.behavior.dragThreshold, 0.f, 12.f);
                    Mosaic::slider(ui, "Drag speed ratio", &theme.behavior.dragSpeedDefaultRatio, 0.0001f, 0.01f);
                    Mosaic::slider(ui, "Drag minimum-step ratio", &theme.behavior.dragSpeedMinimumStepRatio, 0.01f, 1.f);
                }

                m_demoTheme = theme;
            }
        }

        if(m_showAbout == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_showAbout;
            options.initialBounds = {260.f, 160.f, 560.f, 420.f};
            options.fitContentHeight = true;
            auto window = Mosaic::window(ui, "About Mosaic", options);

            if(window.visible() == true)
            {
                static bool showConfigInfo = false;
                Mosaic::text(ui, "Mosaic 0.1.0");
                Mosaic::separator(ui);
                Mosaic::text(ui, "This executable is the interactive Mosaic component catalog.");
                Mosaic::hyperlink(ui, "https://github.com/irov/Mosaic", "https://github.com/irov/Mosaic");
                Mosaic::text(ui, "Mosaic is an immediate-hybrid C++20 UI library with a C-like public API.");
                Mosaic::text(ui, "Developed by the Mosaic contributors.");
                Mosaic::text(ui, "Mosaic is licensed under the MIT License, see LICENSE for more information.");
                Mosaic::checkbox(ui, "Config/Build Information", &showConfigInfo);

                if(showConfigInfo == true)
                {
                    Mosaic::Configuration configuration;
                    if(Mosaic::getConfiguration(ui, &configuration) == false)
                    {
                        return;
                    }

                    Mosaic::Viewport viewport;
                    if(Mosaic::currentViewport(ui, &viewport) == false)
                    {
                        return;
                    }

                    const Mosaic::Frame & frame = Mosaic::getFrame(ui);
                    Mosaic::FontCacheMetrics fontMetrics;
                    if(Mosaic::fontCacheMetrics(ui, &fontMetrics) == false)
                    {
                        return;
                    }

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
                    information += "Mosaic 0.1.0\n";
                    information += "Platform backend: macOS\n";
                    information += "Renderer backend: Metal\n";
                    appendValue("C++ standard: ", __cplusplus);
                    appendValue("sizeof(size_t): ", sizeof(size_t));
                    appendValue("sizeof(uint32_t): ", sizeof(uint32_t));
                    appendValue("sizeof(Mosaic::Vertex): ", sizeof(Mosaic::Vertex));
                    appendValue("Viewport width: ", viewport.bounds.width);
                    appendValue("Viewport height: ", viewport.bounds.height);
                    appendValue("DPI scale: ", viewport.dpiScale);
                    information += "\nConfiguration\n";
                    appendFlag("Pointer input: ", configuration.pointerInput);
                    appendFlag("Keyboard input: ", configuration.keyboardInput);
                    appendFlag("Keyboard navigation: ", configuration.keyboardNavigation);
                    appendFlag("Docking: ", configuration.dockingEnabled);
                    appendFlag("Resize from edges: ", configuration.windowResizeFromEdges);
                    appendFlag("Cursor changes: ", configuration.cursorChanges);
                    information += "\nCurrent frame\n";
                    appendValue("Widgets: ", frame.metrics.widgetCount);
                    appendValue("Visible widgets: ", frame.metrics.visibleWidgetCount);
                    appendValue("Culled widgets: ", frame.metrics.culledWidgetCount);
                    appendValue("Draw commands: ", frame.metrics.drawCommandCount);
                    appendValue("Vertices: ", frame.metrics.vertexCount);
                    appendValue("Indices: ", frame.metrics.indexCount);
                    appendValue("Text cache entries: ", frame.metrics.textCacheEntryCount);
                    appendValue("Text cache hits: ", frame.metrics.textCacheHitCount);
                    appendValue("Text cache misses: ", frame.metrics.textCacheMissCount);
                    appendValue("Font faces: ", fontMetrics.resolvedFaceCount);
                    appendValue("Cached glyphs: ", fontMetrics.glyphCount);
                    appendValue("Atlas pages: ", fontMetrics.atlasPageCount);
                    appendValue("Atlas memory: ", fontMetrics.atlasMemory);

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
    //////////////////////////////////////////////////////////////////////////
} // namespace MosaicExample
