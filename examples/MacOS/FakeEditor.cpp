#include "FakeEditor.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace MosaicExample
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::WindowOptions panel(const Mosaic::Rect & bounds, bool * open = nullptr, bool * collapsed = nullptr) noexcept
        {
            Mosaic::WindowOptions options;
            options.initialBounds = bounds;
            options.open = open;
            options.collapsed = collapsed;
            options.movable = false;
            options.resizable = false;

            return options;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Theme editorTheme() noexcept
        {
            Mosaic::Theme theme = Mosaic::Theme::dark();

            return theme;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Theme textTheme(const Mosaic::Theme & base, const Mosaic::Color & color) noexcept
        {
            Mosaic::Theme theme = base;
            theme.colors.text = color;

            return theme;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::StringView eventTypeName(Mosaic::EventType type) noexcept
        {
            switch(type)
            {
            case Mosaic::EventType::PointerDown:
                return "Pointer down";
            case Mosaic::EventType::PointerUp:
                return "Pointer up";
            case Mosaic::EventType::FocusChanged:
                return "Focus";
            case Mosaic::EventType::BeginEdit:
                return "Begin edit";
            case Mosaic::EventType::Change:
                return "Change";
            case Mosaic::EventType::Commit:
                return "Commit";
            case Mosaic::EventType::Cancel:
                return "Cancel";
            case Mosaic::EventType::DragBegin:
                return "Drag begin";
            case Mosaic::EventType::Drop:
                return "Drop";
            case Mosaic::EventType::PopupOpen:
                return "Popup open";
            case Mosaic::EventType::PopupClose:
                return "Popup close";
            }

            return "Event";
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Theme listItemTheme(const Mosaic::Theme & base) noexcept
        {
            Mosaic::Theme theme = base;
            theme.colors.button = base.colors.background;
            theme.metrics.frameBorderSize = 0.f;
            theme.metrics.buttonTextAlignment.x = 0.f;

            return theme;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Theme menuTheme(const Mosaic::Theme & base, bool selected = false) noexcept
        {
            Mosaic::Theme theme = base;
            theme.colors.button = selected ? base.colors.selection : base.colors.menu;
            theme.metrics.frameBorderSize = 0.f;

            return theme;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Theme runTheme(const Mosaic::Theme & base, bool running) noexcept
        {
            Mosaic::Theme theme = base;
            theme.colors.button = running ? Mosaic::Color::fromBytes(74, 39, 39) : base.colors.button;
            theme.colors.buttonHovered = running ? Mosaic::Color::fromBytes(96, 48, 48) : base.colors.buttonHovered;
            theme.colors.text = running ? base.colors.error : base.colors.textLink;

            return theme;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::LayoutOptions panelContent() noexcept
        {
            Mosaic::LayoutOptions layout;
            layout.width = Mosaic::SizeRule::Fill;
            layout.height = Mosaic::SizeRule::Fill;
            layout.padding = Mosaic::EdgeInsets(5.f, 3.f);
            layout.gap = 2.f;

            return layout;
        }
        //////////////////////////////////////////////////////////////////////////
        void transformRow(Mosaic::Context * ui, Mosaic::StringView label, Mosaic::FloatSpan values, float minimum, float maximum)
        {
            Mosaic::Vec2 available;
            bool stacked = Mosaic::contentRegionAvailable(ui, &available) && available.x < 220.f;
            auto propertyScope = Mosaic::scope(ui, Mosaic::Key(label));
            Mosaic::Theme theme = Mosaic::getTheme(ui);
            theme.metrics.minimumControlWidth = 28.f;
            auto propertyStyle = Mosaic::styleScope(ui, theme);
            Mosaic::LayoutOptions rowLayout;
            rowLayout.width = Mosaic::SizeRule::Fill;
            rowLayout.gap = 2.f;
            auto row = stacked ? Mosaic::column(ui, rowLayout) : Mosaic::row(ui, rowLayout);
            Mosaic::TextOptions labelOptions;
            labelOptions.layout.width = Mosaic::Dimension::fixed(62.f);
            Mosaic::text(ui, label, labelOptions);
            Mosaic::SliderOptions options;
            options.minimum = minimum;
            options.maximum = maximum;
            options.precision = stacked ? 1 : 2;
            options.step = 0.01;
            options.labelPlacement = Mosaic::LabelPlacement::Hidden;
            options.colorMarkers = true;
            Mosaic::dragFloatVector(ui, label, values, options);
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
        [[nodiscard]] Mosaic::String metric(Mosaic::StringView name, size_t value)
        {
            Mosaic::String result(name);
            result += ": ";
            result += Detail::number(value);

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void updateWidgetCount(Mosaic::String & text, uint32_t & value)
        {
            if(text.empty() == true)
            {
                return;
            }

            uint32_t parsed = 0;
            const char * begin = text.data();
            const char * end = begin + text.size();
            auto result = std::from_chars(begin, end, parsed);

            if(result.ec != std::errc{})
            {
                return;
            }

            if(result.ptr != end)
            {
                return;
            }

            constexpr uint32_t maximumWidgetCount = 64;
            value = std::min(parsed, maximumWidgetCount);

            if(parsed > maximumWidgetCount)
            {
                text = Detail::number(maximumWidgetCount);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void fillQuad(Mosaic::Canvas & canvas, const Mosaic::Vec2 & first, const Mosaic::Vec2 & second, const Mosaic::Vec2 & third, const Mosaic::Vec2 & fourth, const Mosaic::Color & color)
        {
            Mosaic::Vec2Quad points = {first, second, third, fourth};
            canvas.convexPolygon(points, color);
        }

        struct ViewportPoint
        {
            float x = 0.f;
            float y = 0.f;
            float z = 0.f;
        };

        using ViewportPointArray = Mosaic::Array<ViewportPoint, 8>;
        using ViewportProjectedPointArray = Mosaic::Array<Mosaic::Vec2, 8>;
        using ViewportFace = Mosaic::Array<size_t, 4>;
        using ViewportFaceArray = Mosaic::Array<ViewportFace, 6>;
        using ViewportFaceOrder = Mosaic::Array<size_t, 6>;
        using ViewportFaceColors = Mosaic::Array<Mosaic::Color, 6>;
        using ViewportRotation = Mosaic::Array<float, 3>;
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] ViewportPoint rotateViewportPoint(const ViewportPoint & point, const ViewportRotation & rotation) noexcept
        {
            constexpr float degreesToRadians = 0.01745329252f;
            float sineX = std::sin(rotation[0] * degreesToRadians);
            float cosineX = std::cos(rotation[0] * degreesToRadians);
            float sineY = std::sin(rotation[1] * degreesToRadians);
            float cosineY = std::cos(rotation[1] * degreesToRadians);
            float sineZ = std::sin(rotation[2] * degreesToRadians);
            float cosineZ = std::cos(rotation[2] * degreesToRadians);

            ViewportPoint aroundX = {point.x, point.y * cosineX - point.z * sineX, point.y * sineX + point.z * cosineX};
            ViewportPoint aroundY = {aroundX.x * cosineY + aroundX.z * sineY, aroundX.y, -aroundX.x * sineY + aroundX.z * cosineY};

            return {aroundY.x * cosineZ - aroundY.y * sineZ, aroundY.x * sineZ + aroundY.y * cosineZ, aroundY.z};
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Vec2 projectViewportPoint(const ViewportPoint & point, const Mosaic::Vec2 & center) noexcept
        {
            return {center.x + point.x + point.z * 0.55f, center.y + point.y - point.z * 0.42f};
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Mosaic::Color shadeBlockColor(const Mosaic::Color & color, float factor) noexcept
        {
            Mosaic::Color result = {std::clamp(color.r * factor, 0.f, 1.f), std::clamp(color.g * factor, 0.f, 1.f), std::clamp(color.b * factor, 0.f, 1.f), color.a};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        void drawBlock(Mosaic::Canvas & canvas, const Mosaic::Vec2 & origin, const Mosaic::Vec2 & size, const ViewportRotation & rotation, const Mosaic::Color & front, bool selected)
        {
            float halfWidth = size.x * 0.5f;
            float halfHeight = size.y * 0.5f;
            float halfDepth = std::max(8.f, std::min(size.x, size.y) * 0.22f);
            Mosaic::Vec2 frontCenter = {origin.x + halfWidth, origin.y + halfHeight};
            Mosaic::Vec2 projectionCenter = {frontCenter.x + halfDepth * 0.55f, frontCenter.y - halfDepth * 0.42f};

            ViewportPointArray points = {{{-halfWidth, -halfHeight, -halfDepth}, {halfWidth, -halfHeight, -halfDepth}, {halfWidth, halfHeight, -halfDepth}, {-halfWidth, halfHeight, -halfDepth}, {-halfWidth, -halfHeight, halfDepth}, {halfWidth, -halfHeight, halfDepth}, {halfWidth, halfHeight, halfDepth}, {-halfWidth, halfHeight, halfDepth}}};
            ViewportPointArray rotatedPoints;
            ViewportProjectedPointArray projectedPoints;
            for(size_t index = 0; index != points.size(); ++index)
            {
                rotatedPoints[index] = Detail::rotateViewportPoint(points[index], rotation);
                projectedPoints[index] = Detail::projectViewportPoint(rotatedPoints[index], projectionCenter);
            }

            constexpr ViewportFaceArray faces = {{{{0, 1, 2, 3}}, {{5, 4, 7, 6}}, {{4, 5, 1, 0}}, {{1, 5, 6, 2}}, {{3, 2, 6, 7}}, {{4, 0, 3, 7}}}};
            ViewportFaceColors faceColors = {{front, Detail::shadeBlockColor(front, 0.48f), Detail::shadeBlockColor(front, 1.22f), Detail::shadeBlockColor(front, 0.66f), Detail::shadeBlockColor(front, 0.54f), Detail::shadeBlockColor(front, 0.82f)}};
            ViewportFaceOrder faceOrder = {0, 1, 2, 3, 4, 5};
            auto faceDepth = [&rotatedPoints, &faces](size_t faceIndex) noexcept
            {
                float depth = 0.f;
                for(size_t vertex : faces[faceIndex])
                {
                    const ViewportPoint & point = rotatedPoints[vertex];
                    depth += point.z - point.x * 0.55f + point.y * 0.42f;
                }

                return depth * 0.25f;
            };
            std::sort(faceOrder.begin(), faceOrder.end(),
                      [&faceDepth](size_t first, size_t second)
                      {
                          auto returnedValue = faceDepth(first) > faceDepth(second);

                          return returnedValue;
                      });

            for(size_t faceIndex : faceOrder)
            {
                const ViewportFace & face = faces[faceIndex];
                Detail::fillQuad(canvas, projectedPoints[face[0]], projectedPoints[face[1]], projectedPoints[face[2]], projectedPoints[face[3]], faceColors[faceIndex]);
            }

            if(selected == true)
            {
                Mosaic::Color selection = Mosaic::Color::fromBytes(72, 174, 255);
                for(size_t orderIndex = faceOrder.size() - 3; orderIndex != faceOrder.size(); ++orderIndex)
                {
                    const ViewportFace & face = faces[faceOrder[orderIndex]];
                    for(size_t edge = 0; edge != face.size(); ++edge)
                    {
                        canvas.line(projectedPoints[face[edge]], projectedPoints[face[(edge + 1) % face.size()]], 1.5f, selection);
                    }
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void drawPerspectiveGrid(Mosaic::Canvas & canvas, float width, float height)
        {
            float centerX = width * 0.5f;
            float horizon = height * 0.23f;
            Mosaic::Color minor = Mosaic::Color::fromBytes(35, 49, 52);
            Mosaic::Color major = Mosaic::Color::fromBytes(48, 68, 72);

            for(int column = -7; column <= 7; ++column)
            {
                float bottomX = centerX + static_cast<float>(column) * width * 0.105f;
                float horizonX = centerX + static_cast<float>(column) * width * 0.008f;
                canvas.line({horizonX, horizon}, {bottomX, height}, column % 4 == 0 ? 1.25f : 1.f, column % 4 == 0 ? major : minor);
            }

            for(int row = 0; row != 9; ++row)
            {
                float ratio = static_cast<float>(row) / 8.f;
                float y = horizon + ratio * ratio * (height - horizon);
                canvas.line({0.f, y}, {width, y}, row % 4 == 0 ? 1.25f : 1.f, row % 4 == 0 ? major : minor);
            }
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    FakeEditor::FakeEditor(Mosaic::TextureHandle checkerTexture, EditorPersistence & persistence) : m_persistence(persistence), m_checkerTexture(checkerTexture)
    {
        constexpr EntityNames names = {"Main Camera", "Sun Light", "Player Rig", "Drone A", "Drone B", "Cargo Platform", "Nav Mesh", "Reflection Probe", "FX Volume", "Audio Listener"};
        for(size_t index = 0; index != names.size(); ++index)
        {
            m_entities[index].name = names[index];
        }

        m_entities[0].position = {0.f, 5.f, -8.f};
        m_entities[0].rotation = {18.f, 0.f, 0.f};
        m_entities[1].position = {2.f, 6.f, -1.f};
        m_entities[1].color = Mosaic::Color::fromBytes(226, 174, 53);
        m_entities[2].position = {0.f, 1.25f, -3.5f};
        m_entities[2].rotation = {0.f, 35.f, 0.f};
        m_entities[2].color = Mosaic::Color::fromBytes(48, 112, 132);
        m_entities[3].position = {-5.f, 0.f, 1.f};
        m_entities[3].color = Mosaic::Color::fromBytes(64, 92, 108);
        m_entities[4].position = {5.f, 0.f, 1.f};
        m_entities[4].color = Mosaic::Color::fromBytes(116, 82, 54);
        m_entities[5].position = {9.f, 0.f, 2.f};
        m_entities[5].color = Mosaic::Color::fromBytes(72, 104, 68);

        m_timeline.visibleEnd = 8.0;
        m_timeline.workEnd = 8.0;
        m_timeline.loopEnd = 8.0;
        m_timeline.playhead = 2.0;
        m_timeline.trackWidth = 124.f;
        m_timeline.trackHeight = 20.f;
        for(size_t index = 0; index != m_motionKeys.size(); ++index)
        {
            m_motionKeys[index].id = 100 + index;
            m_motionKeys[index].track = index / 3 + 1;
            m_motionKeys[index].time = static_cast<double>(index % 3) * 3.0 + 0.5;
        }
        m_selection.select(FakeEditor::entityId(2));
        m_assetSelection.select(Mosaic::combineId(Mosaic::hashBytes("FakeEditor Asset"), 3));
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::Id FakeEditor::entityId(size_t index) noexcept
    {
        auto returnedValue = Mosaic::combineId(Mosaic::hashBytes("FakeEditor Entity"), static_cast<Mosaic::Id>(index + 1));

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    FakeEditor::EntityState & FakeEditor::selectedEntity() noexcept
    {
        size_t index = std::min(static_cast<size_t>(std::max(0, m_selectedEntity)), m_entities.size() - 1);

        return m_entities[index];
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::selectEntity(size_t index, Mosaic::StringView source)
    {
        if(index >= m_entities.size())
        {
            return;
        }

        m_selectedEntity = static_cast<int>(index);
        m_selection.select(FakeEditor::entityId(index));
        m_status = "Selected ";
        m_status += m_entities[index].name;
        m_status += " in ";
        m_status += source;
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::configure(Mosaic::Context * ui)
    {
        // These are editor commands, not widget hotkeys. The host chooses the macOS primary
        // modifier mapping and Mosaic only matches the immutable frame input against the registry.
        Mosaic::setTheme(ui, Detail::editorTheme());
        Mosaic::shortcuts(ui).bind("Save Layout", Mosaic::Shortcut::primary(Mosaic::KeyCode::S));
        Mosaic::shortcuts(ui).bind("Toggle Simulation", Mosaic::Shortcut::primary(Mosaic::KeyCode::Enter));
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::draw(Mosaic::Context * ui, const Mosaic::Vec2 & workspaceSize)
    {
        // This one function is the fake editor frame. Only domain state is retained by FakeEditor;
        // all controls, panel trees, draw commands and hit regions are rebuilt by Mosaic each tick.
        updateLayout(workspaceSize);
        Mosaic::setDockArea(ui, {0.f, 31.f, m_layout.width, std::max(0.f, m_layout.height - 31.f)});

        // The fake editor exposes both interaction policies. Touch mode disables visual hover
        // without changing hit testing; reduced motion snaps every visual state immediately.
        Mosaic::Theme theme = Mosaic::getTheme(ui);
        theme.behavior.hoverEnabled = m_touchMode == false;
        theme.behavior.animationsEnabled = m_animations;
        theme.behavior.rectangleTessellationQuality = m_roundedRectangleQuality;
        theme.metrics.gap = m_layoutGap;
        theme.metrics.popupPadding = Mosaic::EdgeInsets(m_popupPadding);
        theme.metrics.minimumPopupWidth = m_minimumPopupWidth;
        theme.metrics.windowTitleHeight = m_windowTitleHeight;
        theme.metrics.controlHeight = m_controlHeight;
        theme.metrics.minimumControlWidth = m_minimumControlWidth;
        Mosaic::setTheme(ui, theme);

        if(Mosaic::shortcuts(ui).triggered("Save Layout") == true)
        {
            bool saved = Mosaic::serialize(ui, m_persistence);
            m_status = saved == true ? "Layout saved with Command-S" : "Layout serialization failed";
        }

        if(Mosaic::shortcuts(ui).triggered("Toggle Simulation") == true)
        {
            m_simulating = m_simulating == false;
        }

        if(m_simulating && m_paused == false)
        {
            m_timeline.playhead = std::fmod(m_timeline.playhead + Mosaic::input(ui).deltaTime, m_timeline.workEnd);
            applyTimeline();
        }
        drawTopBar(ui);

        Mosaic::LayoutOptions workspaceLayout;
        workspaceLayout.width = Mosaic::SizeRule::Fill;
        workspaceLayout.height = Mosaic::SizeRule::Fill;

        Mosaic::Id hierarchy = Mosaic::InvalidId;
        Mosaic::Id properties = Mosaic::InvalidId;
        Mosaic::Id viewport = Mosaic::InvalidId;
        Mosaic::Id inspector = Mosaic::InvalidId;
        Mosaic::Id console = Mosaic::InvalidId;
        Mosaic::Id contentBrowser = Mosaic::InvalidId;

        if(m_dockModelInitialized == true)
        {
            // Once the first frame has provided stable window ids, DockModel becomes the source
            // of panel geometry. The windows are submitted at the root so reparenting, collapse
            // and close never leave a stale split-container hole behind.
            if(m_hierarchyPanel.visible == true)
            {
                hierarchy = drawHierarchy(ui);
            }

            if(m_propertiesPanel.visible == true)
            {
                properties = drawProperties(ui);
            }

            viewport = drawViewport(ui);

            if(m_inspectorPanel.visible == true)
            {
                inspector = drawInspector(ui);
            }

            if(m_consolePanel.visible == true)
            {
                console = drawConsole(ui);
            }

            if(m_contentBrowserPanel.visible == true)
            {
                contentBrowser = drawContentBrowser(ui);
            }
        }
        else
        {
            // The bootstrap frame uses the same deterministic split as the persisted default.
            // Its only extra job is to establish global window ids for the dock tree below.
            auto workspaceLayer = Mosaic::column(ui, workspaceLayout);
            bool transientOpen = m_modalOpen;
            auto workspaceInteraction = Mosaic::interactionScope(ui, transientOpen == false);

            // The editor workspace is a real nested split tree. Closed panels are not represented
            // by empty placeholders: when a whole side column disappears, the center editor owns
            // the released space in the same frame.
            bool sceneColumnVisible = m_hierarchyPanel.visible || m_propertiesPanel.visible;
            {
                if(sceneColumnVisible == true)
                {
                    Mosaic::SplitOptions primaryColumns;
                    primaryColumns.minimumFirst = 210.f;
                    primaryColumns.minimumSecond = 620.f;
                    // Column boundaries represent different structural levels and move freely.
                    primaryColumns.snapIndex = 0;
                    auto workspace = Mosaic::split(ui, "Workspace columns", Mosaic::Orientation::Horizontal, &m_leftColumnRatio, primaryColumns, workspaceLayout);
                    {
                        drawSceneColumn(ui, workspaceLayout, hierarchy, properties);
                    }
                    {
                        drawEditorColumns(ui, workspaceLayout, viewport, inspector, console, contentBrowser);
                    }
                }
                else
                {
                    drawEditorColumns(ui, workspaceLayout, viewport, inspector, console, contentBrowser);
                }
            }
        }

        initializeDockModel(ui, hierarchy, properties, viewport, inspector, console, contentBrowser);
        drawTransientWindows(ui);
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawSceneColumn(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & hierarchy, Mosaic::Id & properties)
    {
        if(m_hierarchyPanel.visible == true && m_propertiesPanel.visible == true)
        {
            Mosaic::SplitOptions rows;
            rows.minimumFirst = 180.f;
            rows.minimumSecond = 180.f;
            // All editor section rows use group 1, so nearby headers settle on one level.
            rows.snapIndex = 1;
            auto column = Mosaic::split(ui, "Scene column", Mosaic::Orientation::Vertical, &m_hierarchyRatio, rows, layout);
            hierarchy = drawHierarchy(ui);
            properties = drawProperties(ui);
        }
        else if(m_hierarchyPanel.visible == true)
        {
            hierarchy = drawHierarchy(ui);
        }
        else if(m_propertiesPanel.visible == true)
        {
            properties = drawProperties(ui);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawCenterColumn(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & viewport, Mosaic::Id & console)
    {
        if(m_consolePanel.visible == true)
        {
            Mosaic::SplitOptions rows;
            rows.minimumFirst = 280.f;
            rows.minimumSecond = 180.f;
            rows.snapIndex = 1;
            auto column = Mosaic::split(ui, "Viewport column", Mosaic::Orientation::Vertical, &m_viewportRatio, rows, layout);
            viewport = drawViewport(ui);
            console = drawConsole(ui);
        }
        else
        {
            viewport = drawViewport(ui);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawInspectorColumn(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & inspector, Mosaic::Id & contentBrowser)
    {
        if(m_inspectorPanel.visible == true && m_contentBrowserPanel.visible == true)
        {
            Mosaic::SplitOptions rows;
            rows.minimumFirst = 260.f;
            rows.minimumSecond = 180.f;
            rows.snapIndex = 1;
            auto column = Mosaic::split(ui, "Inspector column", Mosaic::Orientation::Vertical, &m_inspectorRatio, rows, layout);
            inspector = drawInspector(ui);
            contentBrowser = drawContentBrowser(ui);
        }
        else if(m_inspectorPanel.visible == true)
        {
            inspector = drawInspector(ui);
        }
        else if(m_contentBrowserPanel.visible == true)
        {
            contentBrowser = drawContentBrowser(ui);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawEditorColumns(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & viewport, Mosaic::Id & inspector, Mosaic::Id & console, Mosaic::Id & contentBrowser)
    {
        bool inspectorColumnVisible = m_inspectorPanel.visible || m_contentBrowserPanel.visible;

        if(inspectorColumnVisible == true)
        {
            Mosaic::SplitOptions columns;
            columns.minimumFirst = 360.f;
            columns.minimumSecond = 240.f;
            columns.snapIndex = 0;
            auto editorColumns = Mosaic::split(ui, "Editor columns", Mosaic::Orientation::Horizontal, &m_centerColumnRatio, columns, layout);
            drawCenterColumn(ui, layout, viewport, console);
            drawInspectorColumn(ui, layout, inspector, contentBrowser);
        }
        else
        {
            drawCenterColumn(ui, layout, viewport, console);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::updateLayout(const Mosaic::Vec2 & workspaceSize) noexcept
    {
        // These values mirror the nested split tree below. The panel rectangles are only used by
        // the fake scene drawing; Mosaic remains the authority that arranges the actual windows.
        constexpr float top = 31.f;
        constexpr float splitter = 4.f;
        constexpr float collapsedHeight = 24.f;
        float width = std::max(workspaceSize.x, 900.f);
        float height = std::max(workspaceSize.y, 620.f);
        float workspaceHeight = height - top;

        bool sceneColumnVisible = m_hierarchyPanel.visible || m_propertiesPanel.visible;
        bool inspectorColumnVisible = m_inspectorPanel.visible || m_contentBrowserPanel.visible;

        float leftWidth = 0.f;
        float editorWidth = width;

        if(sceneColumnVisible == true)
        {
            float available = width - splitter;
            leftWidth = std::clamp(available * m_leftColumnRatio, 210.f, available - 620.f);
            editorWidth = available - leftWidth;
        }

        float centerWidth = editorWidth;
        float rightWidth = 0.f;

        if(inspectorColumnVisible == true)
        {
            float available = editorWidth - splitter;
            centerWidth = std::clamp(available * m_centerColumnRatio, 360.f, available - 240.f);
            rightWidth = available - centerWidth;
        }

        float sceneRowsHeight = workspaceHeight - splitter;
        float hierarchyHeight = workspaceHeight;
        float propertiesHeight = workspaceHeight;

        if(m_hierarchyPanel.visible == true && m_propertiesPanel.visible == true)
        {
            if(m_hierarchyPanel.collapsed == true && m_propertiesPanel.collapsed == true)
            {
                hierarchyHeight = collapsedHeight;
                propertiesHeight = collapsedHeight;
            }
            else if(m_hierarchyPanel.collapsed == true)
            {
                hierarchyHeight = collapsedHeight;
                propertiesHeight = sceneRowsHeight - collapsedHeight;
            }
            else if(m_propertiesPanel.collapsed == true)
            {
                hierarchyHeight = sceneRowsHeight - collapsedHeight;
                propertiesHeight = collapsedHeight;
            }
            else
            {
                hierarchyHeight = std::clamp(sceneRowsHeight * m_hierarchyRatio, 180.f, sceneRowsHeight - 180.f);
                propertiesHeight = sceneRowsHeight - hierarchyHeight;
            }
        }

        float viewportHeight = workspaceHeight;
        float consoleHeight = workspaceHeight;

        if(m_consolePanel.visible == true)
        {
            float available = workspaceHeight - splitter;

            if(m_consolePanel.collapsed == true)
            {
                consoleHeight = collapsedHeight;
                viewportHeight = available - collapsedHeight;
            }
            else
            {
                viewportHeight = std::clamp(available * m_viewportRatio, 280.f, available - 180.f);
                consoleHeight = available - viewportHeight;
            }
        }

        float inspectorRowsHeight = workspaceHeight - splitter;
        float inspectorHeight = workspaceHeight;
        float browserHeight = workspaceHeight;

        if(m_inspectorPanel.visible == true && m_contentBrowserPanel.visible == true)
        {
            if(m_inspectorPanel.collapsed == true && m_contentBrowserPanel.collapsed == true)
            {
                inspectorHeight = collapsedHeight;
                browserHeight = collapsedHeight;
            }
            else if(m_inspectorPanel.collapsed == true)
            {
                inspectorHeight = collapsedHeight;
                browserHeight = inspectorRowsHeight - collapsedHeight;
            }
            else if(m_contentBrowserPanel.collapsed == true)
            {
                inspectorHeight = inspectorRowsHeight - collapsedHeight;
                browserHeight = collapsedHeight;
            }
            else
            {
                inspectorHeight = std::clamp(inspectorRowsHeight * m_inspectorRatio, 260.f, inspectorRowsHeight - 180.f);
                browserHeight = inspectorRowsHeight - inspectorHeight;
            }
        }

        float centerX = sceneColumnVisible ? leftWidth + splitter : 0.f;
        float rightX = centerX + centerWidth + (inspectorColumnVisible ? splitter : 0.f);

        m_layout.width = width;
        m_layout.height = height;
        m_layout.hierarchy = {0.f, top, leftWidth, hierarchyHeight};
        bool scenePanelsCollapsed = m_hierarchyPanel.visible && m_propertiesPanel.visible == true && m_hierarchyPanel.collapsed == true && m_propertiesPanel.collapsed;
        float propertiesY = scenePanelsCollapsed ? top + workspaceHeight - propertiesHeight : top + hierarchyHeight + splitter;
        bool inspectorPanelsCollapsed = m_inspectorPanel.visible && m_contentBrowserPanel.visible == true && m_inspectorPanel.collapsed == true && m_contentBrowserPanel.collapsed;
        float contentBrowserY = inspectorPanelsCollapsed ? top + workspaceHeight - browserHeight : top + inspectorHeight + splitter;

        m_layout.properties = {0.f, propertiesY, leftWidth, propertiesHeight};
        m_layout.viewport = {centerX, top, centerWidth, viewportHeight};
        m_layout.console = {centerX, top + viewportHeight + splitter, centerWidth, consoleHeight};
        m_layout.inspector = {rightX, top, rightWidth, inspectorHeight};
        m_layout.contentBrowser = {rightX, contentBrowserY, rightWidth, browserHeight};
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::updateProfiler(const Mosaic::Frame & frame, const Mosaic::RenderMesh & mesh)
    {
        // Backend numbers are captured after endFrame(), then shown on the next editor frame.
        m_frameMetrics = frame.metrics;
        m_frameMetrics.vertexCount = mesh.vertices.size();
        m_frameMetrics.indexCount = mesh.indices.size();
        m_frameMetrics.batchCount = mesh.batches.size();
        m_frameMetrics.meshMilliseconds = mesh.metrics.buildMilliseconds;
        m_semanticNodeCount = frame.semantics.size();

        for(size_t index = 1; index != m_frameHistory.size(); ++index)
        {
            m_frameHistory[index - 1] = m_frameHistory[index];
        }
        m_frameHistory.back() = static_cast<float>(frame.metrics.buildMilliseconds + frame.metrics.layoutMilliseconds + frame.metrics.emitMilliseconds + mesh.metrics.buildMilliseconds);

        constexpr size_t maximumEventCount = 512;

        if(m_eventLog.capacity() < maximumEventCount)
        {
            m_eventLog.reserve(maximumEventCount);
        }

        for(const Mosaic::EventTraceEntry & event : frame.events)
        {
            EventLogEntry entry = {m_nextEventSequence++, event.type, event.id, event.path};

            if(m_eventLog.size() < maximumEventCount)
            {
                m_eventLog.push_back(std::move(entry));
            }
            else
            {
                m_eventLog[m_eventLogHead] = std::move(entry);
                m_eventLogHead = (m_eventLogHead + 1) % maximumEventCount;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawTopBar(Mosaic::Context * ui)
    {
        Mosaic::LayoutOptions backgroundLayout;
        backgroundLayout.width = Mosaic::SizeRule::Fill;
        backgroundLayout.height = Mosaic::Dimension::fixed(34.f);
        auto chrome = Mosaic::overlay(ui, backgroundLayout);
        {
            Mosaic::LayoutOptions canvasLayout;
            canvasLayout.width = Mosaic::SizeRule::Fill;
            canvasLayout.height = Mosaic::SizeRule::Fill;
            Mosaic::Canvas background = Mosaic::canvas(ui, "Editor chrome", canvasLayout);
            background.rect({0.f, 0.f, m_layout.width, 28.f}, Mosaic::getTheme(ui).colors.menuBarBackground);
            background.line({0.f, 27.f}, {m_layout.width, 27.f}, 1.f, Mosaic::getTheme(ui).colors.borderStrong);
        }

        // A real editor usually keeps project commands in a permanent top strip. The File item
        // remains the owner of its dropdown, so the same control stays highlighted and can close
        // the menu instead of producing a second floating window title.
        Mosaic::LayoutOptions barLayout;
        barLayout.width = Mosaic::SizeRule::Fill;
        barLayout.height = Mosaic::SizeRule::Fill;
        barLayout.padding = Mosaic::EdgeInsets(6.f, 3.f);
        auto bar = Mosaic::row(ui, barLayout);
        {
            auto fileMenu = Mosaic::menu(ui, "File");

            if(fileMenu.expanded() == true)
            {
                Mosaic::MenuItemOptions save;
                save.shortcut = "Command-S";

                if(Mosaic::menuItem(ui, "Save Layout", save).clicked() == true)
                {
                    bool saved = Mosaic::serialize(ui, m_persistence);
                    m_status = saved == true ? "Layout saved" : "Layout serialization failed";
                }

                if(Mosaic::menuItem(ui, "Load Layout").clicked() == true)
                {
                    m_status = m_persistence.load() && Mosaic::deserialize(ui, m_persistence) ? "Layout loaded" : "No saved layout found";
                }

                if(Mosaic::menuItem(ui, "Reset Workspace").clicked() == true)
                {
                    Mosaic::clearDockSpace(ui, 1);
                    m_dockModelInitialized = false;
                    m_hierarchyPanel = {};
                    m_propertiesPanel = {};
                    m_inspectorPanel = {};
                    m_consolePanel = {};
                    m_contentBrowserPanel = {};
                    m_status = "Workspace reset";
                }

                if(Mosaic::menuItem(ui, "Preferences...").clicked() == true)
                {
                    m_modalOpen = true;
                }
            }
        }
        {
            auto windowsMenu = Mosaic::menu(ui, "Windows");

            if(windowsMenu.expanded() == true)
            {
                Mosaic::MenuItemOptions item;
                item.closeOnActivate = false;
                item.checked = &m_hierarchyPanel.visible;
                Mosaic::menuItem(ui, "Scene Hierarchy", item);
                item.checked = &m_propertiesPanel.visible;
                Mosaic::menuItem(ui, "Node Properties", item);
                item.checked = &m_inspectorPanel.visible;
                Mosaic::menuItem(ui, "Inspector", item);
                item.checked = &m_consolePanel.visible;
                Mosaic::menuItem(ui, "Console / Profiler", item);
                item.checked = &m_contentBrowserPanel.visible;
                Mosaic::menuItem(ui, "Content Browser", item);
            }
        }
        {
            auto remaining = Mosaic::row(ui);
            {
                Mosaic::Theme flatMenu = Detail::menuTheme(Mosaic::getTheme(ui));
                auto menuStyle = Mosaic::styleScope(ui, flatMenu);
                auto menus = Mosaic::row(ui);
                {
                    auto editMenu = Mosaic::menu(ui, "Edit");

                    if(editMenu.expanded() == true)
                    {
                        Mosaic::MenuItemOptions undo;
                        undo.shortcut = "Command-Z";
                        undo.enabled = false;
                        Mosaic::menuItem(ui, "Undo", undo);
                        Mosaic::MenuItemOptions redo;
                        redo.shortcut = "Shift-Command-Z";
                        redo.enabled = false;
                        Mosaic::menuItem(ui, "Redo", redo);
                        Mosaic::separator(ui);

                        if(Mosaic::menuItem(ui, "Command Palette...").clicked() == true)
                        {
                            m_status = "Edit command palette opened";
                        }
                    }
                }
                {
                    auto viewMenu = Mosaic::menu(ui, "View");

                    if(viewMenu.expanded() == true)
                    {
                        Mosaic::MenuItemOptions profiler;
                        profiler.selected = m_bottomTab == 2;

                        if(Mosaic::menuItem(ui, "Profiler", profiler).clicked() == true)
                        {
                            m_bottomTab = 2;
                        }

                        Mosaic::MenuItemOptions events;
                        events.selected = m_bottomTab == 3;

                        if(Mosaic::menuItem(ui, "Events", events).clicked() == true)
                        {
                            m_bottomTab = 3;
                        }
                    }
                }
                {
                    auto entityMenu = Mosaic::menu(ui, "Entity");

                    if(entityMenu.expanded() == true)
                    {
                        auto createMenu = Mosaic::menu(ui, "Create");

                        if(createMenu.expanded() == true)
                        {
                            if(Mosaic::menuItem(ui, "Empty Entity").clicked() == true)
                            {
                                m_status = "Empty entity created";
                            }

                            if(Mosaic::menuItem(ui, "Light").clicked() == true)
                            {
                                m_status = "Light entity created";
                            }
                        }

                        if(Mosaic::menuItem(ui, "Frame Selected").clicked() == true)
                        {
                            m_status = "Entity framed from menu";
                        }
                    }
                }
                {
                    auto buildMenu = Mosaic::menu(ui, "Build");

                    if(buildMenu.expanded() == true && Mosaic::menuItem(ui, "Reimport Assets").clicked() == true)
                    {
                        m_importProgress = 0.f;
                    }
                }
            }
            Mosaic::spacer(ui, 8.f);
            {
                Mosaic::Theme actionTheme = Detail::runTheme(Mosaic::getTheme(ui), m_simulating);
                auto actionStyle = Mosaic::styleScope(ui, actionTheme);

                if(Mosaic::button(ui, m_simulating ? "Stop" : "Run").clicked() == true)
                {
                    m_simulating = m_simulating == false;
                }
            }
            Mosaic::toggle(ui, "Pause", &m_paused);
            Mosaic::checkbox(ui, "Snap", &m_snapToGrid);
            Mosaic::spacer(ui, 8.f);

            Mosaic::text(ui, m_projectName);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::Id FakeEditor::drawHierarchy(Mosaic::Context * ui)
    {
        auto window = Mosaic::window(ui, "Scene Hierarchy", Detail::panel(m_layout.hierarchy, &m_hierarchyPanel.visible, &m_hierarchyPanel.collapsed));

        if(window.visible() == false)
        {
            auto returnedValue = window.id();

            return returnedValue;
        }

        auto content = Mosaic::column(ui, Detail::panelContent());
        Mosaic::searchField(ui, "Search entities", &m_filter);
        constexpr Mosaic::Id sceneId = 0x579c0d45ff40ea55ULL;
        constexpr Mosaic::Id collectionsId = 0xe1c775d3eb5a3ccdULL;
        constexpr Mosaic::Id gameplayId = 0x98583fc0ec210acbULL;
        constexpr Mosaic::Id environmentId = 0xf993833f450a51dfULL;
        constexpr Mosaic::TypeId entityDragType = 0x341ddd60094ef68aULL;
        Mosaic::Array<Mosaic::Id, 10> entityIds;

        for(size_t index = 0; index != m_entities.size(); ++index)
        {
            entityIds[index] = FakeEditor::entityId(index);
        }

        Mosaic::TreeRowVector rows;
        Mosaic::IdVector orderedRows;
        Mosaic::Vector<int> entityRows;
        rows.reserve(m_entities.size() + 4);
        orderedRows.reserve(m_entities.size() + 4);
        entityRows.reserve(m_entities.size() + 4);
        Mosaic::TreeRow sceneRow;
        sceneRow.key = Mosaic::Key("World");
        sceneRow.item = sceneId;
        sceneRow.expanded = m_sceneExpanded;
        sceneRow.leadingIcon.semanticFallback = "◇";
        sceneRow.label = "World / DemoScene";
        rows.emplace_back(sceneRow);
        orderedRows.emplace_back(sceneId);
        entityRows.emplace_back(-2);

        if(m_sceneExpanded == true)
        {
            for(size_t index = 0; index != m_entities.size(); ++index)
            {
                const EntityState & entity = m_entities[index];

                if(m_filter.empty() == false && entity.name.find(m_filter) == Mosaic::String::npos)
                {
                    continue;
                }

                Mosaic::TreeRow entityRow;
                entityRow.key = Mosaic::Key(entityIds[index]);
                entityRow.item = entityIds[index];
                entityRow.depth = 1;
                entityRow.leaf = true;
                entityRow.selected = m_selection.selected(entityIds[index]);
                entityRow.renameActive = m_renamingEntity == static_cast<int>(index);
                entityRow.dragEnabled = true;
                entityRow.dropEnabled = true;
                entityRow.leadingIcon.semanticFallback = index == 0 ? "◉" : "◇";
                entityRow.label = entity.name;
                entityRow.renameValue = &m_entities[index].name;
                entityRow.dragType = entityDragType;
                entityRow.dragPayload = Mosaic::ByteSpan(reinterpret_cast<const std::byte *>(&entityIds[index]), sizeof(entityIds[index]));
                entityRow.acceptedDropType = entityDragType;
                rows.emplace_back(entityRow);
                orderedRows.emplace_back(entityIds[index]);
                entityRows.emplace_back(static_cast<int>(index));
            }
        }

        Mosaic::TreeRow collectionsRow;
        collectionsRow.key = Mosaic::Key("Collections");
        collectionsRow.item = collectionsId;
        collectionsRow.expanded = m_collectionsExpanded;
        collectionsRow.leadingIcon.semanticFallback = "▱";
        collectionsRow.label = "Collections";
        rows.emplace_back(collectionsRow);
        orderedRows.emplace_back(collectionsId);
        entityRows.emplace_back(-3);

        if(m_collectionsExpanded == true)
        {
            constexpr Mosaic::Array<Mosaic::StringView, 2> labels = {"Gameplay", "Environment"};
            constexpr Mosaic::Array<Mosaic::Id, 2> ids = {gameplayId, environmentId};

            for(size_t index = 0; index != labels.size(); ++index)
            {
                Mosaic::TreeRow collectionRow;
                collectionRow.key = Mosaic::Key(ids[index]);
                collectionRow.item = ids[index];
                collectionRow.depth = 1;
                collectionRow.leaf = true;
                collectionRow.leadingIcon.semanticFallback = "▱";
                collectionRow.label = labels[index];
                rows.emplace_back(collectionRow);
                orderedRows.emplace_back(ids[index]);
                entityRows.emplace_back(-1);
            }
        }

        Mosaic::TreeViewOptions treeOptions;
        treeOptions.layout.width = Mosaic::SizeRule::Fill;
        treeOptions.layout.height = Mosaic::SizeRule::Fill;
        treeOptions.anchor = &m_hierarchyAnchor;
        treeOptions.orderedItems = orderedRows;
        Mosaic::TreeViewResponse treeResponse;
        (void)Mosaic::treeView(ui, Mosaic::Key("Scene hierarchy tree"), "Scene hierarchy rows", rows, &m_selection, &treeResponse, treeOptions);

        for(const Mosaic::TreeViewRowResponse & rowResponse : treeResponse.rows)
        {
            int entityIndex = entityRows[rowResponse.index];

            if(entityIndex == -2 && rowResponse.row.toggleExpanded == true)
            {
                m_sceneExpanded = m_sceneExpanded == false;
            }

            if(entityIndex == -3 && rowResponse.row.toggleExpanded == true)
            {
                m_collectionsExpanded = m_collectionsExpanded == false;
            }

            if(entityIndex < 0)
            {
                continue;
            }

            if(rowResponse.row.response.clicked() == true)
            {
                selectEntity(static_cast<size_t>(entityIndex), "Scene Hierarchy");
            }

            if(rowResponse.row.beginRename == true)
            {
                m_renamingEntity = entityIndex;
            }

            if(rowResponse.row.renameCommitted == true || rowResponse.row.renameCancelled == true)
            {
                m_renamingEntity = -1;
            }

            if(rowResponse.row.dropped == true)
            {
                m_status = "Scene hierarchy drop requested";
            }
        }

        auto returnedValue = window.id();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::Id FakeEditor::drawProperties(Mosaic::Context * ui)
    {
        auto window = Mosaic::window(ui, "Node Properties", Detail::panel(m_layout.properties, &m_propertiesPanel.visible, &m_propertiesPanel.collapsed));

        if(window.visible() == false)
        {
            auto returnedValue = window.id();

            return returnedValue;
        }

        auto content = Mosaic::scrollArea(ui, "Properties content", Mosaic::Orientation::Vertical, Detail::panelContent());

        EntityState & entity = selectedEntity();
        Mosaic::text(ui, entity.name);
        Mosaic::property(ui, "Visible", &entity.visible);

        auto transform = Mosaic::collapsingHeader(ui, "Transform", true);

        if(transform.expanded() == true)
        {
            Detail::transformRow(ui, "Position", entity.position, -20.f, 20.f);
            Detail::transformRow(ui, "Rotation", entity.rotation, -180.f, 180.f);
        }

        auto rendering = Mosaic::collapsingHeader(ui, "Rendering", true);

        if(rendering.expanded() == true)
        {
            Mosaic::property(ui, "Grid", &m_gridSize, int32_t{4}, int32_t{64});
            Mosaic::property(ui, "Budget", &m_entityBudget, uint64_t{1000}, uint64_t{1000000});
            Mosaic::colorEditorRgba(ui, "Tint", &entity.color);
        }

        auto returnedValue = window.id();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::Id FakeEditor::drawViewport(Mosaic::Context * ui)
    {
        auto window = Mosaic::window(ui, "Viewport - Perspective", Detail::panel(m_layout.viewport));

        if(window.visible() == false)
        {
            auto returnedValue = window.id();

            return returnedValue;
        }

        auto content = Mosaic::column(ui, Detail::panelContent());

        {
            auto toolbar = Mosaic::row(ui);
            constexpr ToolNames tools = {"Select", "Move", "Rotate", "Scale"};
            constexpr ToolNames glyphs = {"↖", "✥", "↻", "□"};
            for(size_t index = 0; index != tools.size(); ++index)
            {
                auto toolScope = Mosaic::scope(ui, Mosaic::Key(index));
                Mosaic::ButtonOptions options;
                options.width = Mosaic::Dimension::fixed(24.f);
                options.height = Mosaic::Dimension::fixed(20.f);
                bool selected = m_selectedTool == static_cast<int>(index);
                Mosaic::Theme toolTheme = Mosaic::getTheme(ui);
                toolTheme.colors.button = selected ? toolTheme.colors.selection : toolTheme.colors.panel;
                toolTheme.colors.text = selected ? toolTheme.colors.accent : toolTheme.colors.text;
                toolTheme.metrics.frameBorderSize = 0.f;
                auto toolStyle = Mosaic::styleScope(ui, toolTheme);
                options.fillBackground = selected;
                auto response = Mosaic::button(ui, Mosaic::Key("Tool"), glyphs[index], options);
                if(response.clicked())
                {
                    m_selectedTool = static_cast<int>(index);
                }
                Mosaic::itemTooltip(ui, response, tools[index]);
            }
            Mosaic::checkbox(ui, "Grid", &m_showGrid);
            auto frame = Mosaic::smallButton(ui, "Fit");
            if(frame.clicked())
            {
                m_designSurfaceState.pan = {};
                m_designSurfaceState.zoom = 1.f;
                m_status = "Framed selection";
            }
            Mosaic::itemTooltip(ui, frame, "Frame selection · reset pan and zoom");
        }

        Mosaic::LayoutOptions canvasLayout;
        canvasLayout.width = Mosaic::SizeRule::Fill;
        canvasLayout.height = Mosaic::SizeRule::Fill;
        Mosaic::DesignSurfaceOptions surfaceOptions;
        surfaceOptions.layout = canvasLayout;
        surfaceOptions.selectionMode = Mosaic::DesignSelectionMode::None;
        surfaceOptions.drawGrid = false;
        Mosaic::DesignSurfaceResponse surfaceResponse;
        Mosaic::Canvas canvas = Mosaic::beginDesignSurface(ui, Mosaic::Key("World design surface"), "World viewport", &m_designSurfaceState, surfaceOptions, &surfaceResponse);

        // The fake editor submits scene-like primitives in local viewport coordinates. Mosaic
        // clips and translates them; Metal only receives the final indexed geometry and states.
        Mosaic::Rect viewportBounds;
        if(Mosaic::debugBounds(ui, window.id(), &viewportBounds) == false)
        {
            (void)Mosaic::endDesignSurface(&canvas);

            return window.id();
        }

        float width = std::max(1.f, viewportBounds.width - 12.f);
        float height = std::max(1.f, viewportBounds.height - 66.f);
        (void)canvas.setLayer(Mosaic::CanvasLayer::Background);
        canvas.rect({0.f, 0.f, width, height}, Mosaic::Color::fromBytes(12, 29, 34));
        canvas.rect({0.f, height * 0.23f, width, height * 0.77f}, Mosaic::Color::fromBytes(16, 25, 27));
        (void)canvas.setLayer(Mosaic::CanvasLayer::Local);

        if(m_showGrid == true)
        {
            Detail::drawPerspectiveGrid(canvas, width, height);
        }

        Detail::fillQuad(canvas, {width * 0.13f, height * 0.62f}, {width * 0.5f, height * 0.32f}, {width * 0.87f, height * 0.62f}, {width * 0.5f, height * 0.94f}, Mosaic::Color::fromBytes(24, 35, 37, 220));

        // These four blocks are the same editor entities shown by Scene Hierarchy. Their stable
        // entity IDs drive the hierarchy highlight, the properties source and the viewport outline.
        // Editing Position or Rotation in either inspector transforms the same block on the next
        // frame, just as a real editor would render the currently selected scene object.
        constexpr ViewportEntityIndices viewportEntities = {3, 2, 4, 5};
        ViewportRects blocks = {{{width * 0.27f, height * 0.58f, width * 0.10f, height * 0.14f}, {width * 0.41f, height * 0.52f, width * 0.13f, height * 0.18f}, {width * 0.60f, height * 0.61f, width * 0.095f, height * 0.12f}, {width * 0.74f, height * 0.54f, width * 0.08f, height * 0.22f}}};
        Mosaic::Vec2 vanishingPoint = {width * 0.5f, height * 0.23f};
        for(size_t index = 0; index != blocks.size(); ++index)
        {
            const EntityState & entity = m_entities[viewportEntities[index]];
            Mosaic::Rect & block = blocks[index];
            Mosaic::Vec2 center = {block.x + block.width * 0.5f, block.y + block.height * 0.5f};

            // Editor coordinates have separate visual meanings: X moves sideways, Y changes
            // elevation, and Z travels along the ground plane toward the perspective vanishing
            // point. Scaling the block with Z makes depth distinct even without a real 3D camera.
            center.x += entity.position[0] * width * 0.006f;
            center.y -= entity.position[1] * height * 0.006f;
            float depthScale = std::clamp(1.f - entity.position[2] * 0.018f, 0.68f, 1.32f);
            center = vanishingPoint + (center - vanishingPoint) * depthScale;
            block.width *= depthScale;
            block.height *= depthScale;
            block.x = center.x - block.width * 0.5f;
            block.y = center.y - block.height * 0.5f;
        }

        const Mosaic::PointerState * pointer = Mosaic::input(ui).primaryPointer();

        if(surfaceResponse.response.hovered() == true && pointer != nullptr && pointer->isPressed() == true)
        {
            Mosaic::Vec2 localPointer;
            if(canvas.localPointerPosition(&localPointer) == true)
            {
                localPointer = surfaceResponse.pointerWorld;

                for(size_t reverse = blocks.size(); reverse != 0; --reverse)
                {
                    size_t index = reverse - 1;
                    const EntityState & entity = m_entities[viewportEntities[index]];
                    const Mosaic::Rect & block = blocks[index];
                    Mosaic::Rect hit = {block.x, block.y - 14.f, block.width + 18.f, block.height + 14.f};

                    if(entity.visible == true && hit.contains(localPointer) == true)
                    {
                        selectEntity(viewportEntities[index], "Viewport");
                        break;
                    }
                }
            }
        }

        Mosaic::Rect selectedBlock;
        for(size_t index = 0; index != blocks.size(); ++index)
        {
            size_t entityIndex = viewportEntities[index];
            const EntityState & entity = m_entities[entityIndex];

            if(entity.visible == false)
            {
                continue;
            }

            bool selected = m_selection.selected(FakeEditor::entityId(entityIndex));
            Detail::drawBlock(canvas, {blocks[index].x, blocks[index].y}, {blocks[index].width, blocks[index].height}, entity.rotation, entity.color, selected);

            if(selected == true)
            {
                selectedBlock = blocks[index];
            }
        }

        if(selectedBlock.empty() == false)
        {
            Mosaic::Vec2 gizmo = {selectedBlock.x + selectedBlock.width * 0.5f, selectedBlock.y - 8.f};
            canvas.line(gizmo, {gizmo.x + width * 0.08f, gizmo.y}, 4.f, Mosaic::Color::fromBytes(238, 74, 71));
            canvas.line(gizmo, {gizmo.x, gizmo.y - height * 0.14f}, 4.f, Mosaic::Color::fromBytes(80, 210, 104));
            canvas.line(gizmo, {gizmo.x - width * 0.05f, gizmo.y + height * 0.07f}, 4.f, Mosaic::Color::fromBytes(72, 135, 245));
        }

        if(surfaceResponse.response.hovered() == true)
        {
            Mosaic::Vec2 pointerPosition;
            if(canvas.localPointerPosition(&pointerPosition) == true)
            {
                pointerPosition = surfaceResponse.pointerWorld;
                canvas.line({pointerPosition.x - 7.f, pointerPosition.y}, {pointerPosition.x + 7.f, pointerPosition.y}, 1.f, Mosaic::Color::fromBytes(245, 229, 116));
                canvas.line({pointerPosition.x, pointerPosition.y - 7.f}, {pointerPosition.x, pointerPosition.y + 7.f}, 1.f, Mosaic::Color::fromBytes(245, 229, 116));
            }
        }

        (void)Mosaic::endDesignSurface(&canvas);

        auto returnedValue = window.id();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::Id FakeEditor::drawInspector(Mosaic::Context * ui)
    {
        auto window = Mosaic::window(ui, "Inspector", Detail::panel(m_layout.inspector, &m_inspectorPanel.visible, &m_inspectorPanel.collapsed));

        if(window.visible() == false)
        {
            auto returnedValue = window.id();

            return returnedValue;
        }

        auto content = Mosaic::scrollArea(ui, "Inspector content", Mosaic::Orientation::Vertical, Detail::panelContent());

        constexpr InspectorTabs tabs = {"Object", "Material", "Import", "Widgets"};
        Mosaic::TabsOptions tabOptions;
        tabOptions.fittingPolicy = Mosaic::TabFittingPolicy::Shrink;
        Mosaic::tabs(ui, "Inspector tabs", &m_inspectorTab, tabs, tabOptions);
        Mosaic::separator(ui);

        if(m_inspectorTab == 0)
        {
            EntityState & entity = selectedEntity();
            Mosaic::property(ui, "Name", &entity.name);
            Mosaic::property(ui, "Enabled", &entity.visible);
            auto transform = Mosaic::collapsingHeader(ui, "Layer Transform", true);
            if(transform.expanded())
            {
                Detail::transformRow(ui, "Position", entity.position, -20.f, 20.f);
                Mosaic::property(ui, "Grid", &m_gridSize, int32_t{4}, int32_t{64});
                Detail::transformRow(ui, "Rotation", entity.rotation, -180.f, 180.f);
                Mosaic::property(ui, "Opacity", &entity.color.a, 0.f, 1.f);
                {
                    auto angleRow = Mosaic::row(ui);
                    Mosaic::TextOptions label;
                    label.layout.width = Mosaic::Dimension::fixed(88.f);
                    Mosaic::text(ui, "Y Rotation", label);
                    auto angle = Mosaic::angleDial(ui, Mosaic::Key("Y rotation dial"), "Y Rotation", &entity.rotation[1]);
                    Mosaic::itemTooltip(ui, angle, "Drag to rotate · double-click to type · Esc to cancel");
                    if(Mosaic::hyperlink(ui, "Reset").clicked())
                    {
                        entity.position = {};
                        entity.rotation = {};
                        entity.color.a = 1.f;
                    }
                }
            }
        }
        else if(m_inspectorTab == 1)
        {
            Mosaic::image(ui, m_checkerTexture, {92.f, 72.f});
            Mosaic::property(ui, "Exposure", &m_exposure, 0.f, 4.f);
            Mosaic::SliderOptions speedOptions;
            speedOptions.minimum = 0.1;
            speedOptions.maximum = 20.0;
            speedOptions.step = 0.1;
            Mosaic::slider(ui, "Camera Speed", &m_cameraSpeed, speedOptions);
            Mosaic::text(ui, "Base Color");
            Mosaic::colorEditorRgb(ui, "Material Tint", &m_materialTint);
        }
        else if(m_inspectorTab == 2)
        {
            Mosaic::progressBar(ui, m_importProgress, "Asset Import");
            Mosaic::inputMultiline(ui, "Import log", &m_notes);
            auto disabled = Mosaic::disabledScope(ui, m_importProgress < 1.f);
            Mosaic::button(ui, "Apply Imported Asset");
        }
        else
        {
            drawGeneratedWidgets(ui);
        }

        Mosaic::separator(ui);
        Mosaic::Response addComponent = Mosaic::button(ui, "Add Component...");

        if(addComponent.clicked() == true)
        {
            m_addComponentOwner = addComponent.id;
            Mosaic::PopupOptions popupOptions;
            popupOptions.owner = m_addComponentOwner;

            if(Mosaic::debugBounds(ui, addComponent.id, &popupOptions.anchor) == true)
            {
                popupOptions.placement = Mosaic::PopupPlacement::Below;
                popupOptions.minimumSize = {210.f, 0.f};
                Mosaic::openPopup(ui, Mosaic::Key("Add Component Popup"), popupOptions);
            }
        }

        if(Mosaic::button(ui, "Editor Preferences...").clicked() == true)
        {
            m_modalOpen = true;
        }

        auto returnedValue = window.id();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawGeneratedWidgets(Mosaic::Context * ui)
    {
        // This editor-owned widget lab demonstrates why repeated immediate widgets need stable
        // keys: every row has the same visible label, while Key(index) preserves its own input
        // and animation state from one frame to the next.
        Mosaic::text(ui, "Radio buttons");
        {
            // A tool mode is naturally a radio group: the editor owns one integer, while each
            // immediate radio only reports the click that selects its corresponding value.
            auto radioRow = Mosaic::row(ui);

            if(Mosaic::radioButton(ui, "Move", m_radioChoice == 0).clicked() == true)
            {
                m_radioChoice = 0;
            }

            if(Mosaic::radioButton(ui, "Rotate", m_radioChoice == 1).clicked() == true)
            {
                m_radioChoice = 1;
            }

            if(Mosaic::radioButton(ui, "Scale", m_radioChoice == 2).clicked() == true)
            {
                m_radioChoice = 2;
            }
        }

        Mosaic::text(ui, "Pointer policies");
        {
            // Tool authors can choose when a command fires and which pointer button owns it.
            static uint32_t pressCount = 0;
            static uint32_t doubleClickCount = 0;
            static uint32_t secondaryCount = 0;
            auto behaviorRow = Mosaic::row(ui);
            Mosaic::ButtonOptions pressOptions;
            pressOptions.pressPolicy = Mosaic::ButtonPressPolicy::Press;

            if(Mosaic::button(ui, Mosaic::Key("press policy button"), "Press", pressOptions).clicked() == true)
            {
                ++pressCount;
            }

            Mosaic::ButtonOptions doubleClickOptions;
            doubleClickOptions.pressPolicy = Mosaic::ButtonPressPolicy::DoubleClick;

            if(Mosaic::button(ui, Mosaic::Key("double click policy button"), "Double click", doubleClickOptions).clicked() == true)
            {
                ++doubleClickCount;
            }

            Mosaic::ButtonOptions secondaryOptions;
            secondaryOptions.pointerButton = Mosaic::PointerButton::Secondary;

            if(Mosaic::button(ui, Mosaic::Key("secondary button"), "Right click", secondaryOptions).clicked() == true)
            {
                ++secondaryCount;
            }

            Mosaic::String status = "press=";
            status += Detail::number(pressCount);
            status += ", double=";
            status += Detail::number(doubleClickCount);
            status += ", right=";
            status += Detail::number(secondaryCount);
            Mosaic::text(ui, status);
        }
        {
            // Invisible hit regions are useful for editor overlays and viewport gizmos.
            Mosaic::Response invisible = Mosaic::invisibleButton(ui, Mosaic::Key("invisible editor hit region"), {120.f, 18.f});
            Mosaic::itemTooltip(ui, invisible, "Invisible editor hit region");
        }

        Mosaic::text(ui, "Disabled widgets");
        {
            // The editor can disable an entire subtree while preserving the values it displays.
            // Disabled controls remain readable, but never capture focus, hover, or pointer input.
            auto disabled = Mosaic::disabledScope(ui);
            Mosaic::button(ui, "Rebuild unavailable");
            Mosaic::checkbox(ui, "Read-only option", &m_disabledCheckbox);
            Mosaic::radioButton(ui, "Selected read-only mode", true);
            Mosaic::toggle(ui, "Locked toggle", &m_disabledToggle);
            Mosaic::slider(ui, "Locked amount", &m_disabledSlider, 0.f, 1.f);
            Mosaic::inputText(ui, "Locked text", &m_disabledText);
        }

        Mosaic::text(ui, "Read-only and validation");
        {
            Mosaic::TextInputOptions readOnly;
            readOnly.readOnly = true;
            Mosaic::inputText(ui, "Generated metadata", &m_readOnlyText, readOnly);

            Mosaic::TextInputOptions warning;
            warning.validation = Mosaic::Validation::Warning;
            warning.validationMessage = "This material has local changes";
            Mosaic::inputText(ui, "Warning value", &m_warningText, warning);

            Mosaic::TextInputOptions error;
            error.validation = Mosaic::Validation::Error;
            error.validationMessage = "A project key is required";
            error.hint = "Required value";
            Mosaic::inputText(ui, "Invalid value", &m_errorText, error);
        }

        Mosaic::text(ui, "Custom combo content");
        constexpr Mosaic::Array<Mosaic::StringView, 3> renderModes = {"Lit", "Wireframe", "Normals"};
        auto renderMode = Mosaic::beginCombo(ui, "Render mode", renderModes[static_cast<size_t>(m_customComboChoice)]);

        if(renderMode.expanded() == true)
        {
            Mosaic::text(ui, "Viewport shading");
            Mosaic::separator(ui);
            for(size_t index = 0; index != renderModes.size(); ++index)
            {
                auto itemScope = Mosaic::scope(ui, Mosaic::Key(index));

                if(Mosaic::selectable(ui, Mosaic::Key("Mode"), renderModes[index], m_customComboChoice == static_cast<int>(index)).clicked() == true)
                {
                    m_customComboChoice = static_cast<int>(index);
                }
            }
        }

        {
            auto windowOptions = Mosaic::treeNode(ui, Mosaic::Key("window behavior laboratory"), "Window behavior laboratory");

            if(windowOptions.expanded() == true)
            {
                Mosaic::checkbox(ui, "Navigation inputs", &m_windowOptionsNavigationInputs);
                Mosaic::checkbox(ui, "Navigation focus", &m_windowOptionsNavigationFocus);
                Mosaic::checkbox(ui, "Pointer input", &m_windowOptionsPointerInput);
                Mosaic::checkbox(ui, "Always auto-resize", &m_windowOptionsAutoResize);
                Mosaic::checkbox(ui, "Always horizontal scrollbar", &m_windowOptionsHorizontalScrollbar);
                Mosaic::checkbox(ui, "Always vertical scrollbar", &m_windowOptionsVerticalScrollbar);

                if(Mosaic::button(ui, "Open window behavior lab").clicked() == true)
                {
                    m_windowOptionsLabOpen = true;
                }
            }
        }

        Mosaic::separator(ui);
        Mosaic::TextInputOptions countOptions;
        countOptions.numeric = true;

        Mosaic::text(ui, "Button count");

        if(Mosaic::inputText(ui, "Generated button count", &m_buttonCountText, countOptions).changed() == true)
        {
            Detail::updateWidgetCount(m_buttonCountText, m_generatedButtonCount);
        }

        Mosaic::text(ui, "Checkbox count");

        if(Mosaic::inputText(ui, "Generated checkbox count", &m_checkboxCountText, countOptions).changed() == true)
        {
            Detail::updateWidgetCount(m_checkboxCountText, m_generatedCheckboxCount);
        }

        Mosaic::String clickSummary = "Button clicks: ";
        clickSummary += Detail::number(m_generatedButtonClicks);
        Mosaic::text(ui, clickSummary);
        Mosaic::text(ui, "Maximum: 64 of each widget");
        Mosaic::separator(ui);

        {
            auto buttonGroup = Mosaic::scope(ui, Mosaic::Key("Generated Buttons"));
            auto buttonGrid = Mosaic::grid(ui, 2);
            for(uint32_t index = 0; index != m_generatedButtonCount; ++index)
            {
                if(Mosaic::button(ui, Mosaic::Key(index), "Button").clicked() == true)
                {
                    ++m_generatedButtonClicks;
                    m_status = "Generated Button #";
                    m_status += Detail::number(index + 1);
                    m_status += " clicked";
                }
            }
        }

        Mosaic::separator(ui);
        m_generatedCheckboxes.resize(m_generatedCheckboxCount);
        {
            auto checkboxGroup = Mosaic::scope(ui, Mosaic::Key("Generated Checkboxes"));
            auto checkboxColumn = Mosaic::column(ui);
            for(uint32_t index = 0; index != m_generatedCheckboxCount; ++index)
            {
                Mosaic::checkbox(ui, Mosaic::Key(index), "Checkbox", &m_generatedCheckboxes[index].checked);
            }
        }

        {
            auto extensions = Mosaic::treeNode(ui, Mosaic::Key("Mosaic style extensions"), "Mosaic style extensions");

            if(extensions.expanded() == true)
            {
                Mosaic::slider(ui, "Layout gap", &m_layoutGap, 0.f, 20.f);
                Mosaic::slider(ui, "Popup padding", &m_popupPadding, 0.f, 20.f);
                Mosaic::slider(ui, "Minimum popup width", &m_minimumPopupWidth, 80.f, 360.f);
                Mosaic::slider(ui, "Window title height", &m_windowTitleHeight, 18.f, 48.f);
                Mosaic::slider(ui, "Control height", &m_controlHeight, 16.f, 44.f);
                Mosaic::slider(ui, "Minimum control width", &m_minimumControlWidth, 40.f, 260.f);
                Mosaic::slider(ui, "Rounded rectangle quality", &m_roundedRectangleQuality, uint8_t{1}, uint8_t{64});
                Mosaic::Response tooltipPolicy = Mosaic::button(ui, "Tooltip without shared delay");
                Mosaic::ItemTooltipOptions tooltipOptions;
                tooltipOptions.delayPolicy = Mosaic::TooltipDelay::Short;
                tooltipOptions.sharedDelay = false;
                Mosaic::itemTooltip(ui, tooltipPolicy, "This Mosaic-specific example always pays its own hover delay.", tooltipOptions);
            }
        }

        drawFontDiagnostics(ui);
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawFontDiagnostics(Mosaic::Context * ui)
    {
        auto diagnostics = Mosaic::treeNode(ui, Mosaic::Key("font diagnostics"), "Font Cache Inspector");

        if(diagnostics.expanded() == false)
        {
            return;
        }

        Mosaic::FontCacheMetrics metrics;
        if(Mosaic::fontCacheMetrics(ui, &metrics) == false)
        {
            Mosaic::text(ui, "Font cache information is unavailable.");
        }
        else
        {
            Mosaic::String summary = "Fonts ";
            summary += Detail::number(metrics.fontCount);
            summary += ", faces ";
            summary += Detail::number(metrics.resolvedFaceCount);
            summary += ", glyphs ";
            summary += Detail::number(metrics.glyphCount);
            summary += ", atlas pages ";
            summary += Detail::number(metrics.atlasPageCount);
            summary += ", bytes ";
            summary += Detail::number(metrics.atlasMemory);
            Mosaic::text(ui, summary);

            const Mosaic::Frame & frame = Mosaic::getFrame(ui);
            Mosaic::String textCache = "Text cache: ";
            textCache += Detail::number(frame.metrics.textCacheEntryCount);
            textCache += " entries, ";
            textCache += Detail::number(frame.metrics.textCacheHitCount);
            textCache += " hits, ";
            textCache += Detail::number(frame.metrics.textCacheMissCount);
            textCache += " misses, ";
            textCache += Detail::number(frame.metrics.textCacheEvictionCount);
            textCache += " evictions, ";
            textCache += Detail::number(frame.metrics.textCacheMemory);
            textCache += " bytes";
            Mosaic::text(ui, textCache);

            Mosaic::String timings = "CPU ms: build ";
            timings += Detail::number(frame.metrics.buildMilliseconds);
            timings += ", layout ";
            timings += Detail::number(frame.metrics.layoutMilliseconds);
            timings += ", emit ";
            timings += Detail::number(frame.metrics.emitMilliseconds);
            Mosaic::text(ui, timings);

            Mosaic::FontCacheEntryVector fonts;
            if(Mosaic::fontCacheEntries(ui, &fonts) == true)
            {
                auto fontEntries = Mosaic::treeNode(ui, Mosaic::Key("font cache entries"), "Resolved font entries");

                if(fontEntries.expanded() == true)
                {
                    Mosaic::TableOptions options;
                    options.headers = true;
                    options.rowBackground = true;
                    options.bordersInnerHorizontal = true;
                    auto table = Mosaic::table(ui, "Font entries", 3, options);
                    Mosaic::tableSetupColumn(ui, 0, "Font");
                    Mosaic::tableSetupColumn(ui, 1, "Size");
                    Mosaic::tableSetupColumn(ui, 2, "Line height");
                    Mosaic::tableHeadersRow(ui);
                    for(size_t index = 0; index != fonts.size(); ++index)
                    {
                        const Mosaic::FontCacheEntry & entry = fonts[index];
                        Mosaic::tableNextRow(ui, Mosaic::Key(index));
                        (void)Mosaic::tableSetColumn(ui, 0);
                        Mosaic::text(ui, entry.font == Mosaic::MonospaceFont ? "Monospace" : "Default");
                        (void)Mosaic::tableSetColumn(ui, 1);
                        Mosaic::text(ui, Detail::number(entry.size));
                        (void)Mosaic::tableSetColumn(ui, 2);
                        Mosaic::text(ui, Detail::number(entry.metrics.lineHeight()));
                    }
                }
            }

            Mosaic::GlyphCacheEntryVector glyphs;
            if(Mosaic::glyphCacheEntries(ui, &glyphs) == true)
            {
                Mosaic::String glyphLabel = "Cached glyphs ";
                glyphLabel += Detail::number(glyphs.size());
                Mosaic::text(ui, glyphLabel);
            }
        }

        auto textBounds = Mosaic::treeNode(ui, Mosaic::Key("text bounds diagnostics"), "Text BB");

        if(textBounds.expanded() == true)
        {
            constexpr Mosaic::Array<Mosaic::StringView, 2> samples = {
                "The quick brown fox jumps over the lazy dog. This sample validates wrapped proportional text bounds.",
                "WWWW iiiii 0123456789 — cached geometry must preserve the exact measured bounds."};
            constexpr Mosaic::Array<float, 2> widths = {180.f, 240.f};

            for(size_t index = 0; index != samples.size(); ++index)
            {
                auto sampleScope = Mosaic::scope(ui, Mosaic::Key(index));
                Mosaic::TextOptions options;
                options.wordWrap = true;
                options.layout.width = Mosaic::Dimension::fixed(widths[index]);
                Mosaic::Response sample = Mosaic::text(ui, samples[index], options);
                Mosaic::Rect bounds;

                if(Mosaic::debugBounds(ui, sample.id, &bounds) == true)
                {
                    Mosaic::String measured = "Text BB: ";
                    measured += Detail::number(bounds.width);
                    measured += " x ";
                    measured += Detail::number(bounds.height);
                    Mosaic::text(ui, measured);
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::applyTimeline()
    {
        float * values[] = {&m_entities[2].position[0], &m_entities[2].rotation[1], &m_entities[2].color.a};
        for(size_t track = 0; track != 3; ++track)
        {
            size_t first = track * 3;
            Mosaic::Array<size_t, 3> order = {first, first + 1, first + 2};
            std::sort(order.begin(), order.end(), [this](size_t left, size_t right) { return m_motionKeys[left].time < m_motionKeys[right].time; });
            size_t left = order.front();
            size_t right = order.back();
            if(m_timeline.playhead <= m_motionKeys[left].time)
            {
                right = left;
            }
            else if(m_timeline.playhead >= m_motionKeys[right].time)
            {
                left = right;
            }
            else
            {
                for(size_t index = 1; index != order.size(); ++index)
                {
                    if(m_timeline.playhead <= m_motionKeys[order[index]].time)
                    {
                        left = order[index - 1];
                        right = order[index];
                        break;
                    }
                }
            }
            double span = m_motionKeys[right].time - m_motionKeys[left].time;
            float ratio = span > 0.000001 ? static_cast<float>((m_timeline.playhead - m_motionKeys[left].time) / span) : 0.f;
            *values[track] = m_motionValues[left] + (m_motionValues[right] - m_motionValues[left]) * ratio;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawTimeline(Mosaic::Context * ui)
    {
        {
            auto toolbar = Mosaic::row(ui);
            if(Mosaic::smallButton(ui, "|<").clicked())
            {
                m_timeline.playhead = 0.0;
                applyTimeline();
            }
            if(Mosaic::smallButton(ui, m_simulating ? "Stop" : "Play").clicked()) m_simulating = !m_simulating;
            Mosaic::SliderOptions timeOptions;
            timeOptions.minimum = 0.0;
            timeOptions.maximum = 8.0;
            timeOptions.step = 1.0 / 30.0;
            timeOptions.precision = 2;
            timeOptions.width = Mosaic::Dimension::fixed(74.f);
            if(Mosaic::dragValue(ui, "", &m_timeline.playhead, timeOptions).changed()) applyTimeline();
            Mosaic::text(ui, "s   /   30 fps");
            Mosaic::checkbox(ui, "Snap", &m_snapToGrid);
        }
        Mosaic::Array<Mosaic::TimelineTrack, 3> tracks;
        constexpr Mosaic::Array<Mosaic::StringView, 3> names = {"Position X", "Y Rotation", "Opacity"};
        for(size_t index = 0; index != tracks.size(); ++index)
        {
            tracks[index].id = index + 1;
            tracks[index].label = names[index];
        }
        Mosaic::TimelineOptions options;
        options.layout.width = Mosaic::SizeRule::Fill;
        options.layout.height = Mosaic::SizeRule::Fill;
        options.frameRate = 30.0;
        options.snapFrames = m_snapToGrid;
        options.allowDuplicate = false;
        options.allowScale = false;
        Mosaic::TimelineResponse edit;
        (void)Mosaic::timeline(ui, Mosaic::Key("Scene timeline"), "Player Rig animation", tracks, m_motionKeys, &m_timeline, options, &edit);
        if(edit.item != Mosaic::InvalidId)
        {
            for(Mosaic::TimelineKeyframe & key : m_motionKeys)
            {
                key.selected = key.id == edit.item;
                if(key.id == edit.item)
                {
                    key.time = std::clamp(edit.time, 0.0, 8.0);
                }
            }
            applyTimeline();
        }
        if(edit.playheadChanged) applyTimeline();
        if(edit.selectionFinished && edit.phase != Mosaic::EditorTransactionPhase::Cancel)
        {
            for(Mosaic::TimelineKeyframe & key : m_motionKeys)
            {
                size_t track = static_cast<size_t>(key.track - 1);
                key.selected = track >= edit.firstSelectedTrack && track <= edit.lastSelectedTrack && key.time >= edit.selectionBegin && key.time <= edit.selectionEnd;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::Id FakeEditor::drawConsole(Mosaic::Context * ui)
    {
        auto window = Mosaic::window(ui, "Console / Profiler", Detail::panel(m_layout.console, &m_consolePanel.visible, &m_consolePanel.collapsed));

        if(window.visible() == false)
        {
            auto returnedValue = window.id();

            return returnedValue;
        }

        auto content = Mosaic::column(ui, Detail::panelContent());

        constexpr BottomTabs tabs = {"Timeline", "Console", "Profiler", "Events"};
        Mosaic::tabs(ui, "Bottom panel", &m_bottomTab, tabs);

        if(m_bottomTab == 0)
        {
            drawTimeline(ui);
        }
        else if(m_bottomTab == 1)
        {
            Mosaic::LayoutOptions logLayout;
            logLayout.width = Mosaic::SizeRule::Fill;
            logLayout.height = Mosaic::Dimension::fixed(136.f);
            auto log = Mosaic::scrollArea(ui, "Editor log", Mosaic::Orientation::Vertical, logLayout);
            const Mosaic::Theme & base = Mosaic::getTheme(ui);
            Mosaic::text(ui, "[info]  Scene DemoScene loaded in 42 ms");
            Mosaic::text(ui, "[info]  Metal pipeline cache contains 3 variants");
            {
                auto warning = Mosaic::styleScope(ui, Detail::textTheme(base, base.colors.warning));
                Mosaic::text(ui, "[warn]  Drone B uses a preview material");
            }
            {
                auto success = Mosaic::styleScope(ui, Detail::textTheme(base, base.colors.success));
                Mosaic::text(ui, "[done]  CoreText glyph cache synchronized");
            }
            Mosaic::text(ui, m_status);
            Mosaic::text(ui, m_persistence.status());
            Mosaic::property(ui, "Command", &m_command);
        }
        else if(m_bottomTab == 2)
        {
            Mosaic::TableOptions tableOptions;
            tableOptions.resizable = true;
            tableOptions.reorderable = true;
            tableOptions.hideable = true;
            tableOptions.sortable = true;
            tableOptions.multiSort = true;
            tableOptions.rowSelection = true;
            tableOptions.scrollVertical = true;
            tableOptions.frozenRows = 1;
            Mosaic::LayoutOptions tableLayout;
            tableLayout.width = Mosaic::SizeRule::Fill;
            tableLayout.height = Mosaic::Dimension::fixed(116.f);
            auto metrics = Mosaic::table(ui, "Frame metrics", 3, tableOptions, tableLayout);
            Mosaic::TableColumnOptions nameColumn;
            nameColumn.sizing = Mosaic::TableSizing::Stretch;
            nameColumn.widthOrWeight = 1.6f;
            Mosaic::tableSetupColumn(ui, 0, "Metric", nameColumn);
            Mosaic::TableColumnOptions valueColumn;
            valueColumn.sizing = Mosaic::TableSizing::Fixed;
            valueColumn.widthOrWeight = 88.f;
            Mosaic::tableSetupColumn(ui, 1, "Value", valueColumn);
            Mosaic::tableSetupColumn(ui, 2, "Stage");
            Mosaic::tableHeadersRow(ui);
            constexpr Mosaic::Array<Mosaic::StringView, 8> metricNames = {"Widgets", "Visible widgets", "Commands", "Vertices", "Indices", "Batches", "Semantics", "Persistent states"};
            Mosaic::Array<size_t, 8> metricValues = {m_frameMetrics.widgetCount, m_frameMetrics.visibleWidgetCount, m_frameMetrics.drawCommandCount, m_frameMetrics.vertexCount, m_frameMetrics.indexCount, m_frameMetrics.batchCount, m_semanticNodeCount, m_frameMetrics.persistentStateCount};
            constexpr Mosaic::Array<Mosaic::StringView, 8> metricStages = {"Submit", "Layout", "Render", "Mesh", "Mesh", "Metal", "A11y", "State"};
            Mosaic::Array<Mosaic::Id, 8> metricIds;
            for(size_t index = 0; index != metricNames.size(); ++index)
            {
                metricIds[index] = Mosaic::hashBytes(metricNames[index]);
            }
            for(size_t index = 0; index != metricNames.size(); ++index)
            {
                // Profiler rows use the same SelectionModel contract as the hierarchy. Command
                // click toggles rows and Shift click extends from the current anchor.
                Mosaic::tableNextRow(ui, Mosaic::Key(index), &m_profilerSelection, metricIds[index], metricIds);
                (void)Mosaic::tableSetColumn(ui, 0);
                Mosaic::text(ui, metricNames[index]);
                (void)Mosaic::tableSetColumn(ui, 1);
                Mosaic::text(ui, Detail::number(metricValues[index]));
                (void)Mosaic::tableSetColumn(ui, 2);
                Mosaic::text(ui, metricStages[index]);
            }

            Mosaic::LayoutOptions graphLayout;
            graphLayout.width = Mosaic::SizeRule::Fill;
            graphLayout.height = Mosaic::Dimension::fixed(105.f);
            Mosaic::Canvas graph = Mosaic::canvas(ui, "CPU frame history", graphLayout);
            Mosaic::Rect windowBounds;
            if(Mosaic::debugBounds(ui, window.id(), &windowBounds) == false)
            {
                return window.id();
            }

            float graphWidth = std::max(1.f, windowBounds.width - 8.f);
            graph.rect({0.f, 0.f, graphWidth, 105.f}, Mosaic::Color::fromBytes(12, 17, 18));
            for(int line = 1; line != 4; ++line)
            {
                float y = static_cast<float>(line) * 26.f;
                graph.line({0.f, y}, {graphWidth, y}, 1.f, Mosaic::Color::fromBytes(44, 51, 53));
            }
            Mosaic::Vec2Vector points;
            points.reserve(m_frameHistory.size());
            for(size_t index = 0; index != m_frameHistory.size(); ++index)
            {
                float x = static_cast<float>(index) / static_cast<float>(m_frameHistory.size() - 1) * graphWidth;
                float y = 96.f - std::min(m_frameHistory[index] * 14.f, 90.f);
                points.push_back({x, y});
            }
            graph.polyline(points, 2.f, Mosaic::Color::fromBytes(176, 224, 72));
        }
        else
        {
            Mosaic::String popupStatus = "Popup stack: ";
            popupStatus += Detail::number(Mosaic::popupLevel(ui));
            popupStatus += Mosaic::isAnyPopupOpen(ui) == true ? " open" : " empty";
            Mosaic::text(ui, popupStatus);

            Mosaic::LayoutOptions eventLayout;
            eventLayout.width = Mosaic::SizeRule::Fill;
            eventLayout.height = Mosaic::Dimension::fixed(170.f);
            eventLayout.gap = 4.f;
            auto events = Mosaic::scrollArea(ui, "Event trace", Mosaic::Orientation::Vertical, eventLayout);
            float lineHeight = Mosaic::getTheme(ui).metrics.lineHeight;
            Mosaic::VisibleRange visible;
            (void)Mosaic::beginListClipper(ui, m_eventLog.size(), lineHeight, &visible, events.id());
            for(size_t index = visible.begin; index != visible.end; ++index)
            {
                size_t physical = (m_eventLogHead + index) % m_eventLog.size();
                const EventLogEntry & event = m_eventLog[physical];
                Mosaic::String line = "#";
                line += Detail::number(event.sequence);
                line += "  [";
                line += Detail::eventTypeName(event.type);
                line += "]  ";
                line += event.path;
                line += "  id=";
                line += Detail::number(event.id);
                Mosaic::text(ui, line);
            }
            Mosaic::endListClipper(ui, visible, m_eventLog.size(), lineHeight);

            if(m_eventLog.empty() == true)
            {
                Mosaic::text(ui, "Interact with the editor to populate this trace.");
            }
        }

        auto returnedValue = window.id();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    Mosaic::Id FakeEditor::drawContentBrowser(Mosaic::Context * ui)
    {
        auto window = Mosaic::window(ui, "Content Browser", Detail::panel(m_layout.contentBrowser, &m_contentBrowserPanel.visible, &m_contentBrowserPanel.collapsed));

        if(window.visible() == false)
        {
            auto returnedValue = window.id();

            return returnedValue;
        }

        auto content = Mosaic::column(ui, Detail::panelContent());

        Mosaic::searchField(ui, "Filter assets", &m_assetFilter);
        constexpr AssetNames assets = {"Materials/MetalBlue", "Materials/WarningStripe", "Meshes/Drone", "Meshes/CargoCrate", "Scenes/DemoScene", "Textures/Grid", "Shaders/EditorLit", "Audio/AmbientLoop"};
        constexpr AssetNames types = {"Material", "Material", "Mesh", "Mesh", "Scene", "Texture", "Shader", "Audio"};
        constexpr Mosaic::TypeId assetDragType = 0xa2c0f1d0b0912763ULL;
        Mosaic::SizeVector visibleAssets;

        for(size_t index = 0; index != assets.size(); ++index)
        {
            if(m_assetFilter.empty() == false && assets[index].find(m_assetFilter) == Mosaic::StringView::npos)
            {
                continue;
            }

            visibleAssets.emplace_back(index);
        }

        Mosaic::IdVector assetIds;
        assetIds.reserve(visibleAssets.size());

        for(size_t assetIndex : visibleAssets)
        {
            assetIds.emplace_back(Mosaic::combineId(Mosaic::hashBytes("FakeEditor Asset"), static_cast<Mosaic::Id>(assetIndex + 1)));
        }

        Mosaic::ResourceTileVector resources;
        resources.reserve(visibleAssets.size());

        for(size_t index = 0; index != visibleAssets.size(); ++index)
        {
            size_t assetIndex = visibleAssets[index];
            Mosaic::ResourceTile tile;
            tile.key = Mosaic::Key(assetIndex);
            tile.resource = assetIds[index];
            tile.thumbnail.texture = m_checkerTexture;
            tile.thumbnail.logicalSize = {14.f, 14.f};
            tile.thumbnail.semanticFallback = "R";
            tile.label = assets[assetIndex];
            tile.type = types[assetIndex];
            tile.selected = m_assetSelection.selected(tile.resource);
            tile.dragType = assetDragType;
            tile.dragPayload = Mosaic::ByteSpan(reinterpret_cast<const std::byte *>(&assetIds[index]), sizeof(assetIds[index]));
            resources.emplace_back(tile);
        }

        Mosaic::ResourceBrowserOptions browserOptions;
        browserOptions.layout.width = Mosaic::SizeRule::Fill;
        browserOptions.layout.height = Mosaic::SizeRule::Fill;
        browserOptions.anchor = &m_assetAnchor;
        browserOptions.orderedItems = assetIds;
        browserOptions.mode = Mosaic::ResourceBrowserMode::List;
        Mosaic::ResourceBrowserResponse browserResponse;
        (void)Mosaic::resourceBrowser(ui, Mosaic::Key("Content resources"), "Content resources", resources, &m_assetSelection, &browserResponse, browserOptions);

        for(const Mosaic::ResourceBrowserItemResponse & itemResponse : browserResponse.items)
        {
            if(itemResponse.item.response.clicked() == false && itemResponse.item.activated == false)
            {
                continue;
            }

            m_selectedAsset = static_cast<int>(visibleAssets[itemResponse.index]);
            m_status = "Selected resource ";
            m_status += assets[static_cast<size_t>(m_selectedAsset)];
        }

        {
            auto actions = Mosaic::row(ui);
            if(Mosaic::button(ui, "Import").clicked() == true)
            {
                m_inspectorTab = 2;
            }
        }
        auto returnedValue = window.id();

        return returnedValue;
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::drawTransientWindows(Mosaic::Context * ui)
    {
        if(m_windowOptionsLabOpen == true)
        {
            Mosaic::WindowOptions options;
            options.open = &m_windowOptionsLabOpen;
            options.initialBounds = {360.f, 180.f, 390.f, 260.f};
            options.navigationInputs = m_windowOptionsNavigationInputs;
            options.navigationFocus = m_windowOptionsNavigationFocus;
            options.pointerInput = m_windowOptionsPointerInput;
            options.alwaysAutoResize = m_windowOptionsAutoResize;
            options.scroll.axes = Mosaic::ScrollAxes::Both;
            options.scroll.alwaysHorizontalScrollbar = m_windowOptionsHorizontalScrollbar;
            options.scroll.alwaysVerticalScrollbar = m_windowOptionsVerticalScrollbar;
            auto window = Mosaic::window(ui, "Window behavior lab", options);

            if(window.visible() == true)
            {
                Mosaic::text(ui, "These Mosaic-specific window controls intentionally live in Fake Editor.");
                Mosaic::ButtonOptions wideButton;
                wideButton.width = Mosaic::Dimension::fixed(520.f);
                Mosaic::button(ui, Mosaic::Key("wide scrolling content"), "Wide content used to exercise horizontal scrolling", wideButton);

                for(size_t index = 0; index != 12; ++index)
                {
                    auto lineScope = Mosaic::scope(ui, Mosaic::Key(index));
                    Mosaic::String line = "Scrollable content line ";
                    line += Detail::number(index + 1);
                    Mosaic::text(ui, line);
                }
            }
        }

        if(m_addComponentOwner != Mosaic::InvalidId)
        {
            Mosaic::PopupOptions popupOptions;
            popupOptions.owner = m_addComponentOwner;
            popupOptions.minimumSize = {210.f, 0.f};
            auto popup = Mosaic::popup(ui, Mosaic::Key("Add Component Popup"), popupOptions);

            if(popup.visible() == true)
            {
                auto content = Mosaic::column(ui, Detail::panelContent());
                Mosaic::text(ui, "Components");

                if(Mosaic::menuItem(ui, "Mesh Renderer").clicked() == true)
                {
                    m_addComponentOwner = Mosaic::InvalidId;
                }

                if(Mosaic::menuItem(ui, "Physics Body").clicked() == true)
                {
                    m_addComponentOwner = Mosaic::InvalidId;
                }

                if(Mosaic::menuItem(ui, "Audio Source").clicked() == true)
                {
                    m_addComponentOwner = Mosaic::InvalidId;
                }

                if(Mosaic::menuItem(ui, "Script Component").clicked() == true)
                {
                    m_addComponentOwner = Mosaic::InvalidId;
                }
            }
            else if(Mosaic::isPopupOpen(ui, Mosaic::Key("Add Component Popup"), m_addComponentOwner) == false)
            {
                m_addComponentOwner = Mosaic::InvalidId;
            }
        }

        if(m_modalOpen == true)
        {
            auto modal = Mosaic::modal(ui, "Editor Preferences", &m_modalOpen, {470.f, 265.f});

            if(modal.visible() == true)
            {
                auto content = Mosaic::column(ui, Detail::panelContent());
                Mosaic::checkbox(ui, "Animate controls", &m_animations);
                Mosaic::checkbox(ui, "Touch input", &m_touchMode);
                Mosaic::checkbox(ui, "Live reload shaders", &m_liveReload);
                Mosaic::checkbox(ui, "Snap viewport tools", &m_snapToGrid);
                Mosaic::property(ui, "Project", &m_projectName);
                Mosaic::separator(ui);

                if(Mosaic::button(ui, "Close Preferences").clicked() == true)
                {
                    m_modalOpen = false;
                }
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void FakeEditor::initializeDockModel(Mosaic::Context * ui, Mosaic::Id hierarchy, Mosaic::Id properties, Mosaic::Id viewport, Mosaic::Id inspector, Mosaic::Id console, Mosaic::Id contentBrowser)
    {
        if(m_dockModelInitialized == true)
        {
            return;
        }

        // The editor persists this split hierarchy. Building it through the public C-like
        // docking API keeps the example independent from DockModel implementation details.
        constexpr uint32_t dockGroup = 1;
        Mosaic::clearDockSpace(ui, dockGroup);
        Mosaic::DockNodeId root = Mosaic::dockSpaceRoot(ui, dockGroup);
        bool initialized = Mosaic::dockWindow(ui, dockGroup, viewport, root);
        initialized = Mosaic::dockWindow(ui, dockGroup, hierarchy, root, Mosaic::DockPlacement::Left, 0.18f) && initialized;
        initialized = Mosaic::dockWindow(ui, dockGroup, properties, Mosaic::dockNodeForWindow(ui, dockGroup, hierarchy), Mosaic::DockPlacement::Bottom, 0.54f) && initialized;
        initialized = Mosaic::dockWindow(ui, dockGroup, inspector, Mosaic::dockNodeForWindow(ui, dockGroup, viewport), Mosaic::DockPlacement::Right, 0.74f) && initialized;
        initialized = Mosaic::dockWindow(ui, dockGroup, console, Mosaic::dockNodeForWindow(ui, dockGroup, viewport), Mosaic::DockPlacement::Bottom, 0.70f) && initialized;
        initialized = Mosaic::dockWindow(ui, dockGroup, contentBrowser, Mosaic::dockNodeForWindow(ui, dockGroup, inspector), Mosaic::DockPlacement::Bottom, 0.66f) && initialized;
        m_dockModelInitialized = initialized;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace MosaicExample
