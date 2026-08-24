#pragma once

#include "EditorPersistence.hpp"

namespace MosaicExample
{
    class FakeEditor
    {
    public:
        FakeEditor(Mosaic::TextureHandle checkerTexture, EditorPersistence & persistence);

        void configure(Mosaic::Context * ui);
        void draw(Mosaic::Context * ui, const Mosaic::Vec2 & workspaceSize);
        void updateProfiler(const Mosaic::Frame & frame, const Mosaic::RenderMesh & mesh);

    private:
        using TransformValues = Mosaic::Array<float, 3>;
        using FrameHistory = Mosaic::Array<float, 96>;
        using EntityNames = Mosaic::Array<Mosaic::StringView, 10>;
        using InspectorTabs = Mosaic::Array<Mosaic::StringView, 4>;
        using BottomTabs = Mosaic::Array<Mosaic::StringView, 3>;
        using ToolNames = Mosaic::Array<Mosaic::StringView, 4>;
        using AssetNames = Mosaic::Array<Mosaic::StringView, 8>;
        using ViewportEntityIndices = Mosaic::Array<size_t, 4>;
        using ViewportRects = Mosaic::Array<Mosaic::Rect, 4>;

        struct EntityState
        {
            Mosaic::String name;
            TransformValues position = {};
            TransformValues rotation = {};
            Mosaic::Color color = Mosaic::Color::fromBytes(74, 126, 148);
            bool visible = true;
        };

        using EntityStateArray = Mosaic::Array<EntityState, 10>;

        struct PanelState
        {
            bool visible = true;
            bool collapsed = false;
        };

        struct WorkspaceLayout
        {
            Mosaic::Rect hierarchy;
            Mosaic::Rect properties;
            Mosaic::Rect viewport;
            Mosaic::Rect inspector;
            Mosaic::Rect console;
            Mosaic::Rect contentBrowser;
            float width = 0.f;
            float height = 0.f;
        };

        struct EventLogEntry
        {
            size_t sequence = 0;
            Mosaic::EventType type = Mosaic::EventType::PointerDown;
            Mosaic::Id id = Mosaic::InvalidId;
            Mosaic::String path;
        };

        struct GeneratedCheckbox
        {
            bool checked = false;
        };

        using EventLog = Mosaic::Vector<EventLogEntry>;
        using GeneratedCheckboxVector = Mosaic::Vector<GeneratedCheckbox>;

        void updateLayout(const Mosaic::Vec2 & workspaceSize) noexcept;
        void drawTopBar(Mosaic::Context * ui);
        void drawSceneColumn(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & hierarchy, Mosaic::Id & properties);
        void drawCenterColumn(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & viewport, Mosaic::Id & console);
        void drawInspectorColumn(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & inspector, Mosaic::Id & contentBrowser);
        void drawEditorColumns(Mosaic::Context * ui, const Mosaic::LayoutOptions & layout, Mosaic::Id & viewport, Mosaic::Id & inspector, Mosaic::Id & console, Mosaic::Id & contentBrowser);
        [[nodiscard]] Mosaic::Id drawHierarchy(Mosaic::Context * ui);
        [[nodiscard]] Mosaic::Id drawProperties(Mosaic::Context * ui);
        [[nodiscard]] Mosaic::Id drawViewport(Mosaic::Context * ui);
        [[nodiscard]] Mosaic::Id drawInspector(Mosaic::Context * ui);
        void drawGeneratedWidgets(Mosaic::Context * ui);
        [[nodiscard]] Mosaic::Id drawConsole(Mosaic::Context * ui);
        [[nodiscard]] Mosaic::Id drawContentBrowser(Mosaic::Context * ui);
        void drawTransientWindows(Mosaic::Context * ui);
        void initializeDockModel(Mosaic::Context * ui, Mosaic::Id hierarchy, Mosaic::Id properties, Mosaic::Id viewport, Mosaic::Id inspector, Mosaic::Id console, Mosaic::Id contentBrowser);
        [[nodiscard]] static Mosaic::Id entityId(size_t index) noexcept;
        [[nodiscard]] EntityState & selectedEntity() noexcept;
        void selectEntity(size_t index, Mosaic::StringView source);

    private:
        EditorPersistence & m_persistence;
        Mosaic::SelectionModel m_selection{Mosaic::SelectionMode::Single};
        Mosaic::SelectionModel m_profilerSelection{Mosaic::SelectionMode::Multiple};
        EntityStateArray m_entities;
        Mosaic::TextureHandle m_checkerTexture = 0;
        Mosaic::String m_projectName = "Orbital Workshop";
        Mosaic::String m_filter;
        Mosaic::String m_assetFilter;
        Mosaic::String m_command;
        Mosaic::String m_notes = "Material compilation completed without errors.";
        Mosaic::String m_status = "Editor ready";
        Mosaic::String m_buttonCountText = "4";
        Mosaic::String m_checkboxCountText = "4";
        Mosaic::String m_disabledText = "Locked value";
        Mosaic::String m_readOnlyText = "Asset metadata is generated";
        Mosaic::String m_warningText = "Unsaved material name";
        Mosaic::String m_errorText = "";
        Mosaic::Color m_materialTint = Mosaic::Color::fromBytes(52, 156, 235);
        FrameHistory m_frameHistory = {};
        Mosaic::FrameMetrics m_frameMetrics;
        WorkspaceLayout m_layout;
        PanelState m_hierarchyPanel;
        PanelState m_propertiesPanel;
        PanelState m_inspectorPanel;
        PanelState m_consolePanel;
        PanelState m_contentBrowserPanel;
        EventLog m_eventLog;
        size_t m_eventLogHead = 0;
        GeneratedCheckboxVector m_generatedCheckboxes;
        size_t m_nextEventSequence = 1;
        size_t m_generatedButtonClicks = 0;
        size_t m_semanticNodeCount = 0;
        uint32_t m_generatedButtonCount = 4;
        uint32_t m_generatedCheckboxCount = 4;
        int32_t m_gridSize = 16;
        uint64_t m_entityBudget = 250000;
        float m_exposure = 1.2f;
        double m_cameraSpeed = 4.5;
        float m_importProgress = 0.72f;
        float m_disabledSlider = 0.62f;
        float m_leftColumnRatio = 0.19f;
        float m_centerColumnRatio = 0.74f;
        float m_hierarchyRatio = 0.55f;
        float m_viewportRatio = 0.70f;
        float m_inspectorRatio = 0.70f;
        int m_selectedTool = 1;
        int m_selectedEntity = 2;
        int m_inspectorTab = 0;
        int m_bottomTab = 0;
        int m_selectedAsset = 2;
        int m_radioChoice = 1;
        int m_customComboChoice = 0;
        bool m_disabledCheckbox = true;
        bool m_disabledToggle = true;
        bool m_snapToGrid = true;
        bool m_showGrid = true;
        bool m_simulating = false;
        bool m_paused = false;
        bool m_liveReload = true;
        bool m_animations = true;
        bool m_touchMode = false;
        bool m_modalOpen = false;
        bool m_dockModelInitialized = false;
        Mosaic::Id m_addComponentOwner = Mosaic::InvalidId;
    };
} // namespace MosaicExample
