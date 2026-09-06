#include "NativeSurface.hpp"

namespace Mosaic
{
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool nativeSurfaceVisible(const Context::Node & node) noexcept
        {
            if(node.visible == false)
            {
                return false;
            }

            if(node.clip.empty() == true)
            {
                return false;
            }

            if(node.bounds.empty() == true)
            {
                return false;
            }

            Rect intersection = Rect::intersection(node.bounds, node.clip);
            auto returnedValue = intersection.empty() == false;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        void fillNativeSurfaceResponse(Id id, const Context::NativeSurfaceState & state, NativeSurfaceResponse * const _out) noexcept
        {
            if(_out == nullptr)
            {
                return;
            }

            _out->id = id;
            _out->surfaceId = id;
            _out->handle = state.handle;
            _out->renderHandle = state.renderHandle;
            _out->bounds = state.bounds;
            _out->localBounds = {0.f, 0.f, state.bounds.width, state.bounds.height};
            _out->screenBounds = state.screenBounds;
            _out->dpiScale = state.dpiScale;
            _out->generation = state.generation;
            _out->created = state.createdThisFrame;
            _out->visible = state.visible;
            _out->focused = state.focused;
        }
        //////////////////////////////////////////////////////////////////////////
        void syncNativeSurfaces(Context * ui)
        {
            ui->visibleNativeSurfaceNodes.clear();

            for(auto & [id, surface] : ui->nativeSurfaces)
            {
                (void)id;
                surface.createdThisFrame = false;

                if(surface.lastFrame != ui->frame.number && surface.visible == true)
                {
                    (void)ui->platform->showNativeSurface(surface.handle, false);
                    surface.visible = false;
                }
            }

            for(size_t index = 1; index != ui->nodes.size(); ++index)
            {
                Context::Node & node = ui->nodes[index];

                if(node.kind != NodeKind::NativeSurface)
                {
                    continue;
                }

                if(node.nativeSurfacePayload == nullptr)
                {
                    continue;
                }

                const NativeSurfaceOptions & options = node.nativeSurfacePayload->options;
                Context::NativeSurfaceState & surface = ui->nativeSurfaces[node.id];
                surface.createdThisFrame = false;
                surface.lastFrame = ui->frame.number;
                NativeSurfaceHandle parent = options.parent == nullptr ? ui->viewport.nativeHandle : options.parent;
                bool surfaceChanged = false;

                if(surface.handle == nullptr)
                {
                    NativeSurfaceDescription description;
                    description.id = node.id;
                    description.kind = options.kind;
                    description.parent = parent;
                    description.bounds = node.bounds;
                    description.applicationTag = options.applicationTag;
                    description.visible = false;
                    description.focusable = options.focusable;
                    description.forwardInput = options.forwardInput;
                    NativeSurfaceHandle handle = nullptr;

                    if(ui->platform->createNativeSurface(description, &handle) == false || handle == nullptr)
                    {
                        ui->frame.diagnostics.emplace_back("Platform backend could not create a native surface");

                        continue;
                    }

                    surface.handle = handle;
                    surface.parent = parent;
                    surface.bounds = node.bounds;
                    surface.generation += 1;
                    surface.createdThisFrame = true;
                }

                NativeSurfaceInputCallback inputCallback = options.forwardInput == true ? options.inputCallback : nullptr;
                void * inputUserData = options.forwardInput == true ? options.inputUserData : nullptr;

                if(surface.inputCallback != inputCallback || surface.inputUserData != inputUserData)
                {
                    if(ui->platform->setNativeSurfaceInputCallback(surface.handle, inputCallback, inputUserData) == false && options.forwardInput == true)
                    {
                        ui->frame.diagnostics.emplace_back("Platform backend could not configure native surface input forwarding");
                    }
                    else
                    {
                        surface.inputCallback = inputCallback;
                        surface.inputUserData = inputUserData;
                    }
                }

                if(surface.parent != parent)
                {
                    if(ui->platform->setNativeSurfaceParent(surface.handle, parent) == false)
                    {
                        ui->frame.diagnostics.emplace_back("Platform backend could not reparent a native surface");
                    }
                    else
                    {
                        surface.parent = parent;
                        surfaceChanged = true;
                    }
                }

                if(surface.bounds != node.bounds)
                {
                    if(ui->platform->setNativeSurfaceBounds(surface.handle, node.bounds) == false)
                    {
                        ui->frame.diagnostics.emplace_back("Platform backend could not resize a native surface");
                    }
                    else
                    {
                        surface.bounds = node.bounds;
                        surfaceChanged = true;
                    }
                }

                bool visible = options.visible == true && Detail::nativeSurfaceVisible(node) == true;

                if(surface.visible != visible)
                {
                    if(ui->platform->showNativeSurface(surface.handle, visible) == false)
                    {
                        ui->frame.diagnostics.emplace_back("Platform backend could not change native surface visibility");
                    }
                    else
                    {
                        surface.visible = visible;
                    }
                }

                if(options.requestFocus == true && visible == true)
                {
                    (void)ui->platform->focusNativeSurface(surface.handle);
                }

                float dpiScale = ui->viewport.dpiScale;
                (void)ui->platform->nativeSurfaceDpiScale(surface.handle, &dpiScale);
                Rect screenBounds = surface.bounds;
                (void)ui->platform->nativeSurfaceScreenBounds(surface.handle, &screenBounds);
                bool focused = false;
                (void)ui->platform->nativeSurfaceFocused(surface.handle, &focused);
                NativeSurfaceHandle renderHandle = surface.handle;
                (void)ui->platform->nativeSurfaceRenderHandle(surface.handle, &renderHandle);

                if(surface.dpiScale != dpiScale)
                {
                    surfaceChanged = true;
                }

                if(surface.renderHandle != nullptr && surface.renderHandle != renderHandle)
                {
                    surfaceChanged = true;
                }

                if(surface.createdThisFrame == false && surfaceChanged == true)
                {
                    surface.generation += 1;
                }

                surface.renderHandle = renderHandle;
                surface.screenBounds = screenBounds;
                surface.dpiScale = dpiScale;
                surface.renderTarget = options.renderTarget;
                surface.kind = options.kind;
                surface.applicationTag = options.applicationTag;
                surface.focused = focused;

                if(visible == true)
                {
                    ui->visibleNativeSurfaceNodes.push_back(index);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        void destroyNativeSurfaces(Context * ui) noexcept
        {
            if(ui == nullptr)
            {
                return;
            }

            for(auto & [id, surface] : ui->nativeSurfaces)
            {
                (void)id;

                if(surface.handle != nullptr)
                {
                    (void)ui->platform->destroyNativeSurface(surface.handle);
                    surface.handle = nullptr;
                    surface.renderHandle = nullptr;
                }
            }

            ui->nativeSurfaces.clear();
        }
        //////////////////////////////////////////////////////////////////////////
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    Scope nativeSurface(Context * ui, const Key & key, StringView label, const NativeSurfaceOptions & surfaceOptions, const LayoutOptions & layout, NativeSurfaceResponse * const _out, const SourceLocation & location)
    {
        size_t nodeIndex = ui->addNode(Detail::NodeKind::NativeSurface, key, label, layout, location, SemanticRole::Group, surfaceOptions.focusable);
        Context::Node & node = ui->nodes[nodeIndex];
        node.nativeSurfacePayload->options = surfaceOptions;
        Response response = ui->interact(nodeIndex, surfaceOptions.focusable);
        node.response = response;

        if(response.pressed() == true && surfaceOptions.focusable == true)
        {
            node.nativeSurfacePayload->options.requestFocus = true;
        }

        auto iterator = ui->nativeSurfaces.find(node.id);

        if(iterator != ui->nativeSurfaces.end())
        {
            Detail::fillNativeSurfaceResponse(node.id, iterator->second, _out);
        }
        else if(_out != nullptr)
        {
            *_out = {};
            _out->id = node.id;
            _out->surfaceId = node.id;
        }

        uint64_t token = ui->pushScope(nodeIndex, ui->currentStyle, ui->currentDisabled);

        return {ui, token, node.id, true};
    }
    //////////////////////////////////////////////////////////////////////////
    bool nativeSurfaceResponse(const Context * ui, Id id, NativeSurfaceResponse * const _out) noexcept
    {
        if(ui == nullptr || _out == nullptr)
        {
            return false;
        }

        auto iterator = ui->nativeSurfaces.find(id);

        if(iterator == ui->nativeSurfaces.end())
        {
            return false;
        }

        Detail::fillNativeSurfaceResponse(id, iterator->second, _out);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool releaseNativeSurface(Context * ui, Id id) noexcept
    {
        if(ui == nullptr)
        {
            return false;
        }

        auto iterator = ui->nativeSurfaces.find(id);

        if(iterator == ui->nativeSurfaces.end())
        {
            return false;
        }

        Context::NativeSurfaceState & surface = iterator->second;

        if(surface.handle != nullptr && ui->platform->destroyNativeSurface(surface.handle) == false)
        {
            return false;
        }

        ui->nativeSurfaces.erase(iterator);

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
