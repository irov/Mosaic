#include "FakeEditor.hpp"
#include "GraphicsObject.hpp"
#include <Mosaic/GraphicsBridge.hpp>

#include <algorithm>
#include <cstdio>

int main()
{
    Mosaic::NullPlatformAdapter platform;
    MosaicExample::EditorPersistence persistence(platform, {});
    MosaicExample::FakeEditor editor(0, persistence);
    Mosaic::Context * ui = Mosaic::newContext(&platform);
    editor.configure(ui);
    gp_graphics_t * graphicsObject = MosaicExample::createGraphics();
    if(graphicsObject == nullptr)
    {
        std::fprintf(stderr, "Graphics object creation failed\n");
        Mosaic::deleteContext(ui);
        return 1;
    }
    Mosaic::GraphicsBridge graphics;
    if(!graphics.initialize(graphicsObject))
    {
        auto error = graphics.lastError();
        std::fprintf(stderr, "Graphics bridge initialize failed: %.*s\n", static_cast<int>(error.size()), error.data());
        gp_graphics_destroy(graphicsObject);
        Mosaic::deleteContext(ui);
        return 1;
    }
    int failures = 0;
    for(const Mosaic::Vec2 size : {Mosaic::Vec2{1440, 860}, Mosaic::Vec2{1000, 650}})
    {
        for(int index = 0; index != 4; ++index)
        {
            Mosaic::Viewport viewport;
            viewport.bounds = viewport.workArea = {0, 0, size.x, size.y};
            Mosaic::beginFrame(ui, {}, viewport);
            Mosaic::setFrameCaptureOptions(ui, {.semantics = true, .debug = true, .metrics = true});
            editor.draw(ui, size);
            const auto & frame = Mosaic::endFrame(ui);
            if(!graphics.prepare(frame, platform))
            {
                auto error = graphics.lastError();
                std::fprintf(stderr, "Editor render failed: %.*s\n", static_cast<int>(error.size()), error.data());
                ++failures;
            }
            if(!frame.diagnostics.empty())
            {
                for(const auto & message : frame.diagnostics) std::fprintf(stderr, "%s\n", message.c_str());
                ++failures;
            }
            if(index == 3)
            {
                float viewportWidth = 0;
                float inspectorWidth = 0;
                for(const auto & node : frame.debug)
                {
                    if(node.label == "Viewport - Perspective") viewportWidth = node.bounds.width;
                    if(node.label == "Inspector") inspectorWidth = node.bounds.width;
                }
                if(viewportWidth <= inspectorWidth || viewportWidth < size.x * 0.5f)
                {
                    std::fprintf(stderr, "Editor viewport should own most of the workspace: viewport %.0f, inspector %.0f\n", viewportWidth, inspectorWidth);
                    ++failures;
                }
            }
        }
    }
    graphics.finalize();
    gp_graphics_destroy(graphicsObject);
    Mosaic::deleteContext(ui);
    return failures == 0 ? 0 : 1;
}
