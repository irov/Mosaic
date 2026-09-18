#include <Mosaic/Mosaic.hpp>

#include "GraphicsObject.hpp"
//////////////////////////////////////////////////////////////////////////
int main()
{
    Mosaic::Context * ui = Mosaic::newContext();

    if(ui == nullptr)
    {
        return 1;
    }

    bool enabled = true;

    Mosaic::beginFrame(ui, {});
    {
        auto window = Mosaic::window(ui, "Settings");

        if(window.visible() == true)
        {
            Mosaic::checkbox(ui, "Enabled", &enabled);
            Mosaic::button(ui, "Apply");
        }
    }
    const Mosaic::Frame & frame = Mosaic::endFrame(ui);

    gp_graphics_t * graphics = MosaicExample::createGraphics();

    if(graphics == nullptr)
    {
        Mosaic::deleteContext(ui);

        return 1;
    }

    Mosaic::GraphicsBridge bridge;

    if(bridge.initialize(graphics) == false)
    {
        gp_graphics_destroy(graphics);
        Mosaic::deleteContext(ui);

        return 1;
    }

    if(bridge.prepare(frame) == false)
    {
        bridge.finalize();
        gp_graphics_destroy(graphics);
        Mosaic::deleteContext(ui);

        return 1;
    }

    const Mosaic::RenderMesh * renderData = bridge.renderData();
    bool geometryGenerated = renderData != nullptr && renderData->vertices.empty() == false;
    bridge.finalize();
    gp_graphics_destroy(graphics);
    Mosaic::deleteContext(ui);

    if(geometryGenerated == false)
    {
        return 1;
    }

    return 0;
}
