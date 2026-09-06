# Mosaic

```sh
git clone git@github.com:irov/Mosaic.git
```

Mosaic is a C++20 immediate/hybrid UI library for desktop tools and interactive applications.
Application code describes the UI every frame; Mosaic builds a transient tree for layout, input,
semantics and drawing while retaining long-lived state under stable 64-bit `Id` values.

```cpp
#include <Mosaic/Mosaic.hpp>

Mosaic::Context * ui = Mosaic::newContext();
bool enabled = true;

Mosaic::beginFrame(ui, input, viewport);
{
    auto window = Mosaic::window(ui, "Settings");
    if(window.visible())
    {
        Mosaic::checkbox(ui, "Enabled", &enabled);

        if(Mosaic::button(ui, "Apply").clicked())
        {
            apply();
        }
    }
}
const Mosaic::Frame & frame = Mosaic::endFrame(ui);
Mosaic::GraphicsBridge bridge;
if(bridge.prepare(frame) == true)
{
    const Mosaic::RenderMesh * renderData = bridge.renderData();

    if(renderData != nullptr)
    {
        renderer.render(frame.viewports.front(), *renderData);
    }
}
Mosaic::deleteContext(ui);
```

## Implemented architecture

- Independent `Context` instances with an immutable per-frame input snapshot and a free-function API.
- Hierarchical stable IDs, RAII scopes, source-location debug metadata and collision diagnostics.
- A transient UI tree plus persistent focus, capture, editing, scrolling, expansion and window state.
- Row, column, grid, overlay, scroll, split, absolute and clip layouts using content/fixed/fill/percent dimensions.
- Pointer capture, keyboard focus traversal, shortcuts, UTF-8 text editing, clipboard, IME commit
  events and Begin/Change/Commit/Cancel responses.
- Core controls, property helpers, trees, lists, tabs, menus, popups, modals and extensible clipped canvases.
- Semantic and debug trees, event tracing, frame metrics, versioned persistence interfaces,
  selection and drag/drop models.
- Deterministic docking data model with tab merge/reorder, side splits, activation, undocking, validation and layout.
- Ordered draw lists containing geometry only, interned texture/clip/blend render states, 32-bit mesh
  indices and replaceable renderer/platform/font adapters.
- Headless execution, including geometry above 65,535 vertices.

Font lookup and shaping are supplied by `FontProvider`; Mosaic first estimates a text BB, culls the
transient tree, then shapes only text that can contribute to the frame and stores prepared glyph
quads instead of a text render command. Native windows, GPU upload, operating-system accessibility,
clipboard/IME, monotonic time, console output and filesystem access remain adapter responsibilities.
Mosaic Core does not read the system clock, open files or write to process streams directly. Mosaic
ships a null platform adapter for headless use.

`PlatformAdapter` is application-owned and must outlive every `Context` and `GraphicsBridge::prepare`
call that uses it. A custom backend implements clipboard, console, binary file read/write, user-data
path resolution, monotonic time, cursor, IME, native-window, monitor and accessibility callbacks.

`Mosaic.hpp` is the single C-like API header and only forward-declares `Context`. Its complete
non-PIMPL definition is private to the library in `src`, so applications only work with `Context *`.
Create a context with `newContext` and release it with `deleteContext`.

## Geometry pipeline

`GraphicsBridge` converts a frame draw list into `RenderMesh`:

```text
Immediate API -> Estimate BB -> Transient Tree -> Layout/Input/Culling
             -> Shape visible text -> Geometry-only DrawList
             -> irov/graphics -> 32-bit RenderMesh -> Renderer Adapter
```

By default Mosaic downloads, builds and installs the latest Graphics `origin/master` into its build
tree. A host may instead provide the Graphics include directories and libraries. `GraphicsBridge`
owns one persistent canvas, clears it once at the start of each mesh build and records all visible
primitives in render order. A render-state change flushes the recorded segment into Mosaic-owned
32-bit vertex/index buffers without clearing the logical canvas state. Images and custom geometry
are passed as borrowed streams; prepared glyph rectangles are expanded into vertices and indices by
Graphics. Canvas commands and tessellation scratch data come from the fixed arenas configured by
Graphics, so recording and tessellation do not allocate after canvas creation.

`GraphicsBridge::prepare` finalizes immutable render data for one viewport. `renderData()` returns a
read-only pointer owned by the bridge; it remains valid until the next `prepare` call or bridge
destruction. A renderer must consume it before that boundary. GPU backends copy it into their own
frame resources during `render()` and keep those resources alive until the corresponding GPU
completion signal. This matches an immediate UI draw-data lifetime without making Graphics own a
second geometry cache.

Renderers consume `RenderBatch::renderKey` to look up the full `RenderState` (texture, clip, blend,
render target and variant) in `RenderMesh::renderStates`. Glyph-atlas page states are resolved before
the frame reaches `GraphicsBridge`; the bridge only converts prepared draw geometry into the final
mesh.

Visible labels live in the transient frame tree and prepared text cache; editable values remain
application-owned `String` objects. Raw text is never stored in `DrawCommand` or deferred to the
renderer.

## Text cache

`FontProvider::revision()` identifies the lifetime of provider-owned font and glyph resources.
The macOS provider caches native `CTFont` objects and their metrics by font handle and quantized
size; a backing-scale change clears its font/glyph resources and advances the revision.

Each `Context` keeps up to 4,096 prepared text runs. The key contains the provider and its revision,
font handle, font size, line height, text bytes and editing attributes. A cached run owns its measured
size, shaping clusters, editable cursor offsets and logical glyph rectangles with texture/UV data. Nodes
reference that immutable geometry for the frame; position, color, clipping and render target remain
dynamic draw state. Changing the provider or its revision invalidates the cache before the next frame.

Fixed-height collections can use `beginListClipper` / `endListClipper`, and scrolling tables can
use `tableVisibleRows`. Both APIs submit only the visible range plus overscan while preserving the
full content extent for scrollbars. The final layout also BB-culls grid, absolute, overlay, table
and ordinary row/column descendants before text shaping and geometry generation.

## Interaction visuals

Widget hover, press, selection, focus and programmatic value changes are animated from state kept
under each stable `Id`; application code does not retain animation values. The timings and input
policy are part of `Theme::behavior`:

```cpp
Mosaic::Theme theme = Mosaic::Theme::dark();
theme.behavior.hoverEnabled = false;       // Touch UI: clicks still use the same hit regions.
theme.behavior.animationsEnabled = false;  // Reduced motion: visual state changes snap immediately.
Mosaic::setTheme(ui, theme);
```

Pointers reported as `PointerType::Touch` never produce a visual hover state even when hover is
enabled globally. Mouse and pen pointers retain hover feedback unless the theme disables it.

Interactive split containers keep their ratio in application state and capture the pointer only
inside the visible divider. Minimum sizes are expressed in logical pixels:

```cpp
float inspectorRatio = 0.72f;
Mosaic::SplitOptions splitOptions;
splitOptions.minimumFirst = 320.f;
splitOptions.minimumSecond = 220.f;

auto panels = Mosaic::split(ui, "Editor panels", Mosaic::Orientation::Horizontal,
    &inspectorRatio, splitOptions, fillLayout);
drawViewport(ui);
drawInspector(ui);
```

The platform adapter receives `CursorShape::ResizeHorizontal` or
`CursorShape::ResizeVertical`. The macOS backend maps those values to AppKit's left-right and
up-down resize cursors.

Windows use an explicit docking compatibility group. Group `0` always stays floating;
matching non-zero groups may form tabs or side splits, while different groups ignore each
other as docking targets:

```cpp
Mosaic::WindowOptions sceneOptions;
sceneOptions.dockGroup = 1;

Mosaic::WindowOptions toolOptions;
toolOptions.dockGroup = 2;

Mosaic::WindowOptions overlayOptions;
overlayOptions.dockGroup = 0;
```

## Memory allocation

All dynamic library storage uses the public aliases in namespace `Mosaic`: `Vector`, `String`,
`UnorderedMap`, `UnorderedSet` and `UniquePtr`. Non-owning/fixed-size standard vocabulary types are
exposed consistently as `StringView`, `Span` and `Array`.

Install a host allocator before constructing Mosaic objects or containers:

```cpp
class GameAllocator final : public Mosaic::Allocator
{
public:
    void * allocate(size_t size, size_t alignment) noexcept override;
    void deallocate(void * memory, size_t size, size_t alignment) noexcept override;
};

GameAllocator allocator;
Mosaic::setDefaultAllocator(&allocator);
Mosaic::Context * ui = Mosaic::newContext();
// ...
Mosaic::deleteContext(ui);
```

The allocator must outlive every object that was created from it. Passing `nullptr` to
`setDefaultAllocator` restores Mosaic's built-in system allocator.

Starting a frame does not change the application's default allocator. Nested library containers
inherit their owning container's allocator during construction. Internal allocation trackers stay
alive while copied containers still reference them, even after their original context is deleted;
the host allocator must still outlive those containers. A copied `Frame` owns its metadata, but its
viewport draw data remains borrowed and expires at the next `beginFrame` or context destruction.

## Stable identity

Visible text never participates in identity. Without an explicit `Key`, Mosaic combines the
parent ID, widget kind and static source location. This is convenient when a call site creates
exactly one item under a given parent:

```cpp
Mosaic::button(ui, "Save");
```

Changing `"Save"` to a localized or dynamically generated label therefore preserves the widget
state. Repeated calls from the same source location must be placed under a keyed parent scope or
receive their own stable application keys:

```cpp
for(const Item & item : items)
{
    auto itemScope = Mosaic::scope(ui, item.id);
    Mosaic::text(ui, item.name);
    Mosaic::button(ui, Mosaic::Key("delete"), localizedDelete);
}
```

If two nodes resolve to the same ID in one frame, `Frame::diagnostics` reports the ID, both tree
paths and both source locations. This makes a missing key visible without coupling identity back
to mutable display buffers.

## Custom canvas

```cpp
auto canvas = Mosaic::canvas(ui, "Graph", {
    .width = Mosaic::SizeRule::Fill,
    .height = Mosaic::SizeRule::Fill
});

bool recorded = canvas.line({0, 0}, {100, 80}, 2.f, {0.3f, 0.7f, 1.f, 1.f});
recorded = canvas.ellipse({140, 50}, {32, 18}, 0.25f, 2.f, {1.f, 0.7f, 0.2f, 1.f}) && recorded;
recorded = canvas.pushClip({0, 0, 220, 100}) && recorded;
(void)canvas.text({8, 8}, "Cached canvas text", {1.f, 1.f, 1.f, 1.f});
recorded = canvas.popClip() && recorded;
```

Canvas coordinates are local to its content rectangle and are clipped hierarchically. The
canvas supports circles, ellipses, quadratic/cubic curves, convex and concave fills, nested
clips, cached text and images. `custom()` accepts vertices plus `uint32_t` indices.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Useful options:

- `MOSAIC_BUILD_EXAMPLES`
- `MOSAIC_BUILD_TESTS`
- `MOSAIC_GRAPHICS_TARGET`
- `MOSAIC_GRAPHICS_SOURCE_DIR`
- `MOSAIC_GRAPHICS_INCLUDE`
- `MOSAIC_GRAPHICS_LIBS`
- `MOSAIC_GRAPHICS_DEPENDENCY`
- `MOSAIC_GRAPHICS_GIT_REPOSITORY`
- `MOSAIC_ENABLE_EXCEPTIONS`
- `MOSAIC_ENABLE_RTTI`
- `MOSAIC_WARNINGS_AS_ERRORS`

The default top-level build enables the examples. When no external Graphics mode is selected,
Mosaic downloads and builds the current `origin/master` revision through a separate
`ExternalProject`. Configure is intentionally connected so a clean build never silently reuses a
pinned or disconnected revision.

```sh
# Mosaic downloads Graphics itself.
cmake -S . -B build
```

An embedding host can supply an existing target:

```cmake
add_subdirectory(/path/to/graphics graphics-build)
set(MOSAIC_GRAPHICS_TARGET graphics CACHE STRING "" FORCE)
add_subdirectory(/path/to/Mosaic mosaic-build)
```

`MOSAIC_GRAPHICS_SOURCE_DIR` adds a source checkout directly. Legacy hosts may instead provide the
paired `MOSAIC_GRAPHICS_INCLUDE` and `MOSAIC_GRAPHICS_LIBS` values; when the library is produced by
a custom target, `MOSAIC_GRAPHICS_DEPENDENCY` makes that dependency explicit. These integration
modes are mutually exclusive. Mosaic never duplicates vector tessellation on its side. Mosaic's
own code supports `MOSAIC_ENABLE_EXCEPTIONS=OFF` and `MOSAIC_ENABLE_RTTI=OFF`.

Install and consume with CMake:

```sh
cmake --install build --prefix /your/prefix
```

```cmake
find_package(Mosaic 0.1 CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE Mosaic::Mosaic)
```

## Compact editor UI

The default theme uses neutral charcoal surfaces, 20-pixel controls, 24-pixel panel
headers and 1-pixel separators. Buttons, tabs and tree rows are flat; emphasis is
reserved for selection, focus and interaction. Numeric properties use aligned,
underlined scrub values and become input fields during typing. `propertyLabelWidth`
controls the shared label column. `SliderOptions::width` also applies to drag values.

`angleDial(ui, key, label, &degrees)` provides a compact angle control: horizontal
drag, Shift/Alt precision modifiers, double-click to type and Escape to cancel.
Buttons confirm activation with a brief tint, keyboard focus grows an underline,
and slider handles and timeline keys gain local hover/drag feedback. Setting
`Theme::behavior.animationsEnabled = false` removes transitions and confirmation
flashes while retaining all input and focus states.

The macOS editor includes a working three-channel animation timeline with playback,
playhead scrubbing, keyframe dragging and frame snapping. **File → Reset Workspace**
restores the default panel proportions. Launch with `--fresh-layout` to preview the
layout without reading or overwriting saved settings; saving remains explicit.
**File → Preferences** contains animation and touch-input settings.

Visual references: Adobe's [Properties panel](https://helpx.adobe.com/after-effects/using/properties-panel.html)
and [layer-property controls](https://helpx.adobe.com/after-effects/using/layer-properties.html),
including compact value columns, restrained panel tabs and timeline hierarchy.
All controls are rendered by Mosaic.

## Targets

- `Mosaic::Core` — context, identity, input, layout, widgets, docking, semantics, persistence,
  debugging, draw-list recording and the headless null adapter.
- `Mosaic::Graphics` — CPU mesh generation and renderer-facing batches.
- `Mosaic::Platform` — compatibility target exposing the platform interfaces and linking Core.
- `Mosaic::Mosaic` — convenience target linking all modules.

See [examples/Headless.cpp](examples/Headless.cpp) for a complete renderer-independent frame.

## macOS fake editor

On macOS, enabling examples also builds native Cocoa + Metal applications. Application UI and
platform/rendering adapters remain separate: the examples submit only Mosaic calls, while compact
adapters translate AppKit input/platform services, CoreText glyphs and the final mesh independently.

```sh
cmake -S . -B build -DMOSAIC_BUILD_EXAMPLES=ON
cmake --build build --target MosaicMacOSExample -j
open build/examples/MacOS/MosaicMacOSExample.app
```

The example is split by responsibility so it can be read as integration documentation:

- [`FakeEditor.cpp`](examples/MacOS/FakeEditor.cpp) contains the immediate-mode editor frame and
  comments explaining which state belongs to the fake editor and which work is delegated to Mosaic.
- [`MosaicView.mm`](examples/MacOS/MosaicView.mm) owns the frame lifecycle and translates mouse,
  keyboard, text and IME events into `Mosaic::Input`.
- [`MacOSPlatformAdapter.mm`](backends/MacOS/MacOSPlatformAdapter.mm) provides clipboard, console and
  filesystem callbacks, user-data paths, cursors, monitor information, accessibility metadata and
  native window operations.
- [`CoreTextFontProvider.mm`](backends/MacOS/CoreTextFontProvider.mm) shapes text and
  uploads cached glyph textures through the renderer adapter.
- [`MetalRendererAdapter.mm`](backends/Metal/MetalRendererAdapter.mm) owns shaders, texture handles,
  dynamic vertex/index buffers, render-state batches, scissor rectangles and presentation.
- [`EditorPersistence.cpp`](examples/MacOS/EditorPersistence.cpp) demonstrates versioned layout
  serialization through `PlatformAdapter` callbacks under the user's Application Support directory.

The editor includes windows, menus, tooltips, popups, a modal, text inputs, integral and floating
sliders, property and vector/color editors, selection, drag/drop, shortcuts, persistence, docking
model setup, a virtualized event trace, textures and custom canvas geometry. Its profiler shows the
mesh produced by the preceding frame, making the CPU-to-Metal boundary visible while you interact.
The visible workspace is a responsive dock-like editor layout built from nested interactive split
containers: hierarchy and properties stay on the left, the scene viewport owns the center,
inspector/content stay on the right, and console/profiler fills the lower strip. Every gutter can be
dragged independently and clamps both adjacent panels to editor-defined minimum sizes.

Implementation-only declarations are kept out of the installed include surface. `Context.hpp`,
`Draw.hpp` and `DrawList.hpp` live under `src`; applications use the forward-declared `Context *`,
the public frame/render types and the free functions from `Mosaic.hpp`.
