#include "Draw.hpp"
#include "DrawCommand.hpp"
#include "FrameRenderData.hpp"
#include "Mosaic/Platform.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>

#if defined(MOSAIC_HAS_IROV_GRAPHICS)
extern "C"
{
#include <graphics/graphics.h>
}

#if GP_API_VERSION < 5U
#error Mosaic requires Graphics API version 5 or newer
#endif
#endif

namespace Mosaic
{
    namespace Detail
    {
        struct AlignedLine
        {
            Vec2 first;
            Vec2 second;
            float thickness = 1.f;
        };
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float snapDrawToPixel(float value, float scale) noexcept
        {
            float resolvedScale = std::max(1.f, scale);
            auto returnedValue = std::round(value * resolvedScale) / resolvedScale;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] Rect pixelAlignedRect(const Rect & rectangle, float scale) noexcept
        {
            float left = Detail::snapDrawToPixel(rectangle.x, scale);
            float top = Detail::snapDrawToPixel(rectangle.y, scale);
            float right = Detail::snapDrawToPixel(rectangle.right(), scale);
            float bottom = Detail::snapDrawToPixel(rectangle.bottom(), scale);
            Rect result = {left, top, std::max(0.f, right - left), std::max(0.f, bottom - top)};

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] float strokeCenter(float value, float scale, bool oddThickness) noexcept
        {
            float physical = value * scale;
            auto returnedValue = (oddThickness ? std::floor(physical) + 0.5f : std::round(physical)) / scale;

            return returnedValue;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] AlignedLine pixelAlignedLine(const Vec2 & first, const Vec2 & second, float thickness, float scale) noexcept
        {
            float resolvedScale = std::max(1.f, scale);
            float physicalThickness = std::max(1.f, std::round(thickness * resolvedScale));
            float logicalThickness = physicalThickness / resolvedScale;
            bool oddThickness = static_cast<uint32_t>(physicalThickness) % 2U != 0;

            AlignedLine result = {first, second, logicalThickness};

            if(std::abs(first.y - second.y) <= 0.0001f)
            {
                result.first.x = Detail::snapDrawToPixel(first.x, resolvedScale);
                result.second.x = Detail::snapDrawToPixel(second.x, resolvedScale);
                result.first.y = Detail::strokeCenter(first.y, resolvedScale, oddThickness);
                result.second.y = result.first.y;
            }
            else if(std::abs(first.x - second.x) <= 0.0001f)
            {
                result.first.y = Detail::snapDrawToPixel(first.y, resolvedScale);
                result.second.y = Detail::snapDrawToPixel(second.y, resolvedScale);
                result.first.x = Detail::strokeCenter(first.x, resolvedScale, oddThickness);
                result.second.x = result.first.x;
            }

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
#if defined(MOSAIC_HAS_IROV_GRAPHICS)
        struct GraphicsBridgeState
        {
            explicit GraphicsBridgeState(Allocator & value) : allocator(&value), renderData(&value), batches(StlAllocator<gp_render_batch_t>(value))
            {
            }

            Allocator * allocator = nullptr;
            gp_canvas_t * canvas = nullptr;
            RenderMesh renderData;
            Vector<gp_render_batch_t> batches;
        };

        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] gp_color_t gpColor(const Color & color) noexcept
        {
            return {color.r, color.g, color.b, color.a};
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] gp_fill_spread_t gpFillSpread(FillSpread spread) noexcept
        {
            switch(spread)
            {
            case FillSpread::Repeat:
                return GP_FILL_SPREAD_REPEAT;
            case FillSpread::Mirror:
                return GP_FILL_SPREAD_MIRROR;
            case FillSpread::Clamp:
            default:
                return GP_FILL_SPREAD_CLAMP;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        bool addGpFillStops(gp_canvas_t * canvas, const Fill & source)
        {
            if(source.stopCount > FillStopCapacity)
            {
                return false;
            }

            uint32_t stopCount = source.stopCount;

            if(stopCount == 0)
            {
                return false;
            }

            for(uint32_t index = 0; index != stopCount; ++index)
            {
                const FillStop & stop = source.stops[index];
                const Color & color = stop.color;

                if(gp_add_fill_stop(canvas, stop.offset, color.r, color.g, color.b, color.a) != GP_SUCCESSFUL)
                {
                    return false;
                }
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        bool setGpFill(gp_canvas_t * canvas, const Fill & source)
        {
            if(source.type == FillType::Solid)
            {
                gp_result_t fillResult = gp_set_fill_color(canvas, source.color.r, source.color.g, source.color.b, source.color.a);
                bool successful = fillResult == GP_SUCCESSFUL;

                return successful;
            }

            gp_fill_spread_t spread = Detail::gpFillSpread(source.spread);
            gp_result_t result = GP_FAILURE;
            switch(source.type)
            {
            case FillType::Linear:
                result = gp_set_fill_linear(canvas, source.from.x, source.from.y, source.to.x, source.to.y, spread);
                break;
            case FillType::Radial:
                result = gp_set_fill_radial(canvas, source.from.x, source.from.y, source.to.x, spread);
                break;
            case FillType::Conic:
                result = gp_set_fill_conic(canvas, source.from.x, source.from.y, source.to.x, spread);
                break;
            case FillType::Solid:
                break;
            }

            if(result != GP_SUCCESSFUL)
            {
                return false;
            }

            bool stopsAdded = Detail::addGpFillStops(canvas, source);

            return stopsAdded;
        }
        //////////////////////////////////////////////////////////////////////////
        bool recordGpBox(gp_canvas_t * canvas, const BoxDrawCommand & command, float pixelScale, float inheritedFeather)
        {
            Rect bounds = Detail::pixelAlignedRect(command.bounds, pixelScale);

            if(gp_push_state(canvas) != GP_SUCCESSFUL)
            {
                return false;
            }

            const Color & border = command.style.borderColor;
            const Shadow & shadow = command.style.shadow;
            uint8_t quality = command.style.cornerQuality == 0 ? 4 : command.style.cornerQuality;
            bool recorded = Detail::setGpFill(canvas, command.style.fill);

            if(recorded == true)
            {
                gp_result_t borderResult = gp_set_border(canvas, command.style.borderWidth, border.r, border.g, border.b, border.a);
                recorded = borderResult == GP_SUCCESSFUL;
            }

            if(recorded == true)
            {
                float feather = std::max(command.style.feather, inheritedFeather);
                gp_result_t featherResult = gp_set_feather(canvas, feather);
                recorded = featherResult == GP_SUCCESSFUL;
            }

            if(recorded == true)
            {
                gp_result_t shadowResult = gp_set_shadow(canvas, shadow.offset.x, shadow.offset.y, shadow.spread, shadow.blur, shadow.color.r, shadow.color.g, shadow.color.b, shadow.color.a);
                recorded = shadowResult == GP_SUCCESSFUL;
            }

            if(recorded == true)
            {
                gp_result_t qualityResult = gp_set_rect_quality(canvas, quality);
                recorded = qualityResult == GP_SUCCESSFUL;
            }

            if(recorded == true)
            {
                const CornerRadii & radii = command.style.radii;
                gp_result_t boxResult = gp_box(canvas, bounds.x, bounds.y, bounds.width, bounds.height, radii.topLeft, radii.topRight, radii.bottomRight, radii.bottomLeft);
                recorded = boxResult == GP_SUCCESSFUL;
            }

            gp_result_t popResult = gp_pop_state(canvas);
            bool successful = recorded == true && popResult == GP_SUCCESSFUL;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        bool recordGpRects(gp_canvas_t * canvas, RectInstanceSpan instances, float pixelScale)
        {
            if(instances.empty() == true)
            {
                return true;
            }

            if(gp_push_state(canvas) != GP_SUCCESSFUL)
            {
                return false;
            }

            if(gp_begin_rects(canvas, instances.size()) != GP_SUCCESSFUL)
            {
                gp_pop_state(canvas);

                return false;
            }

            bool recorded = true;
            for(const RectInstance & instance : instances)
            {
                Rect bounds = Detail::pixelAlignedRect(instance.bounds, pixelScale);
                const Color & color = instance.color;
                gp_result_t colorResult = gp_set_color(canvas, color.r, color.g, color.b, color.a);

                if(colorResult != GP_SUCCESSFUL)
                {
                    recorded = false;
                    break;
                }

                gp_result_t rectResult = gp_add_rect(canvas, bounds.x, bounds.y, bounds.width, bounds.height);

                if(rectResult != GP_SUCCESSFUL)
                {
                    recorded = false;
                    break;
                }
            }

            gp_result_t endResult = gp_end_rects(canvas);
            gp_result_t popResult = gp_pop_state(canvas);
            bool successful = recorded == true && endResult == GP_SUCCESSFUL && popResult == GP_SUCCESSFUL;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        bool addGpQuadVertex(gp_canvas_t * canvas, const Vec2 & position, const Color & color, const Vec2 & uv)
        {
            gp_result_t colorResult = gp_set_color(canvas, color.r, color.g, color.b, color.a);

            if(colorResult != GP_SUCCESSFUL)
            {
                return false;
            }

            gp_result_t uvResult = gp_set_uv(canvas, uv.x, uv.y);

            if(uvResult != GP_SUCCESSFUL)
            {
                return false;
            }

            gp_result_t vertexResult = gp_add_quad_vertex(canvas, position.x, position.y);
            bool successful = vertexResult == GP_SUCCESSFUL;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        bool recordGpQuads(gp_canvas_t * canvas, QuadInstanceSpan instances)
        {
            if(instances.empty() == true)
            {
                return true;
            }

            if(gp_push_state(canvas) != GP_SUCCESSFUL)
            {
                return false;
            }

            if(gp_begin_quads(canvas, instances.size()) != GP_SUCCESSFUL)
            {
                gp_pop_state(canvas);

                return false;
            }

            bool recorded = true;
            for(const QuadInstance & quad : instances)
            {
                for(const Vertex & vertex : quad.vertices)
                {
                    bool vertexAdded = Detail::addGpQuadVertex(canvas, vertex.position, vertex.color, vertex.uv);

                    if(vertexAdded == false)
                    {
                        recorded = false;
                        break;
                    }
                }

                if(recorded == false)
                {
                    break;
                }
            }

            gp_result_t endResult = gp_end_quads(canvas);
            gp_result_t popResult = gp_pop_state(canvas);
            bool successful = recorded == true && endResult == GP_SUCCESSFUL && popResult == GP_SUCCESSFUL;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        bool recordGpGradient(gp_canvas_t * canvas, const GradientDrawCommand & gradient)
        {
            const Rect & bounds = gradient.bounds;
            const Color & topLeft = gradient.colors[0];
            const Color & topRight = gradient.colors[1];
            const Color & bottomRight = gradient.colors[2];
            const Color & bottomLeft = gradient.colors[3];
            gp_result_t result = gp_multicolor_rect(canvas, bounds.x, bounds.y, bounds.width, bounds.height, topLeft.r, topLeft.g, topLeft.b, topLeft.a, topRight.r, topRight.g, topRight.b, topRight.a, bottomRight.r, bottomRight.g, bottomRight.b, bottomRight.a, bottomLeft.r, bottomLeft.g, bottomLeft.b, bottomLeft.a);
            bool successful = result == GP_SUCCESSFUL;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        bool recordGpBezier(gp_canvas_t * canvas, Vec2Span points, bool cubic)
        {
            gp_result_t result = GP_FAILURE;

            if(cubic == true)
            {
                result = gp_cubic_bezier(canvas, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y);
            }
            else
            {
                result = gp_quadratic_bezier(canvas, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y);
            }

            bool successful = result == GP_SUCCESSFUL;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        bool recordGpPath(gp_canvas_t * canvas, const DrawCommandStorage & storage, const PathDrawCommand & path, float inheritedPenumbra)
        {
            Vec2Span points = storage.pointSpan(path.points);
            ColoredPointSpan coloredPoints = storage.coloredPointSpan(path.coloredPoints);
            ColorSpan colors = storage.colorSpan(path.colors);
            gp_result_t colorResult = gp_set_color(canvas, path.color.r, path.color.g, path.color.b, path.color.a);

            if(colorResult != GP_SUCCESSFUL)
            {
                return false;
            }

            float thickness = std::max(path.thickness, 0.01f);
            gp_result_t thicknessResult = gp_set_thickness(canvas, thickness);

            if(thicknessResult != GP_SUCCESSFUL)
            {
                return false;
            }

            float penumbra = std::clamp(inheritedPenumbra, 0.f, thickness * 0.5f);
            gp_result_t penumbraResult = gp_set_penumbra(canvas, penumbra);

            if(penumbraResult != GP_SUCCESSFUL)
            {
                return false;
            }

            if(path.segments != 0)
            {
                gp_uint8_t quality = static_cast<gp_uint8_t>(std::clamp(path.segments, 1U, 255U));
                gp_result_t curveQualityResult = gp_set_curve_quality(canvas, quality);

                if(curveQualityResult != GP_SUCCESSFUL)
                {
                    return false;
                }

                gp_result_t ellipseQualityResult = gp_set_ellipse_quality(canvas, quality);

                if(ellipseQualityResult != GP_SUCCESSFUL)
                {
                    return false;
                }
            }

            bool graphicsFill = path.filled == true && path.shape != PathShape::GradientRing;

            if(graphicsFill == true)
            {
                gp_result_t beginFillResult = gp_begin_fill(canvas);

                if(beginFillResult != GP_SUCCESSFUL)
                {
                    return false;
                }
            }

            gp_result_t primitiveResult = GP_FAILURE;
            switch(path.shape)
            {
            case PathShape::Circle:
                primitiveResult = gp_circle(canvas, path.center.x, path.center.y, path.radii.x);
                break;
            case PathShape::Arc:
                primitiveResult = gp_arc(canvas, path.center.x, path.center.y, path.radii.x, path.startAngle, path.endAngle);
                break;
            case PathShape::Ellipse:
                primitiveResult = std::abs(path.rotation) <= 0.0001f ? gp_ellipse(canvas, path.center.x, path.center.y, path.radii.x, path.radii.y) : gp_superellipse(canvas, path.center.x, path.center.y, path.radii.x, path.radii.y, 2.f, path.rotation);
                break;
            case PathShape::RegularPolygon:
                primitiveResult = path.segments < 3 ? GP_FAILURE : gp_regular_polygon(canvas, path.center.x, path.center.y, path.radii.x, path.segments, path.rotation);
                break;
            case PathShape::Polygon:
                if(coloredPoints.size() >= 3)
                {
                    static_assert(sizeof(Color) == sizeof(gp_color_t));
                    gp_path_layout_t layout = {GP_PATH_ATTRIBUTE_NONE, offsetof(ColoredPoint, color), GP_PATH_ATTRIBUTE_NONE};
                    primitiveResult = gp_polyline(canvas, coloredPoints.data(), coloredPoints.size(), sizeof(ColoredPoint), &layout, GP_TRUE);
                }
                else if(points.size() >= 3)
                {
                    primitiveResult = gp_polyline(canvas, points.data(), points.size(), sizeof(Vec2), nullptr, GP_TRUE);
                }

                break;
            case PathShape::GradientRing:
                if(colors.empty() == false)
                {
                    constexpr float tau = 6.28318530718f;
                    size_t sourceCount = colors.size();
                    size_t colorCount = sourceCount;
                    gp_uint8_t quality = static_cast<gp_uint8_t>(path.segments == 0 ? 96U : std::clamp(path.segments, 3U, 255U));

                    if(gp_push_state(canvas) != GP_SUCCESSFUL)
                    {
                        primitiveResult = GP_FAILURE;
                        break;
                    }

                    gp_result_t conicResult = gp_set_fill_conic(canvas, path.center.x, path.center.y, path.rotation, GP_FILL_SPREAD_REPEAT);
                    bool recorded = conicResult == GP_SUCCESSFUL;
                    for(size_t index = 0; recorded == true && index != colorCount; ++index)
                    {
                        size_t sourceIndex = index * sourceCount / colorCount;
                        const Color & color = colors[sourceIndex];
                        float offset = static_cast<float>(index) / static_cast<float>(colorCount);
                        gp_result_t stopResult = gp_add_fill_stop(canvas, offset, color.r, color.g, color.b, color.a);
                        recorded = stopResult == GP_SUCCESSFUL;
                    }
                    const Color & first = colors.front();

                    if(recorded == true)
                    {
                        gp_result_t lastStopResult = gp_add_fill_stop(canvas, 1.f, first.r, first.g, first.b, first.a);
                        recorded = lastStopResult == GP_SUCCESSFUL;
                    }

                    if(recorded == true)
                    {
                        gp_result_t featherResult = gp_set_feather(canvas, 0.75f);
                        recorded = featherResult == GP_SUCCESSFUL;
                    }

                    if(recorded == true)
                    {
                        gp_result_t qualityResult = gp_set_rect_quality(canvas, quality);
                        recorded = qualityResult == GP_SUCCESSFUL;
                    }

                    if(recorded == true)
                    {
                        gp_result_t ringResult = gp_ring(canvas, path.center.x, path.center.y, path.radii.x, path.radii.y, path.rotation, path.rotation + tau);
                        recorded = ringResult == GP_SUCCESSFUL;
                    }

                    primitiveResult = recorded ? GP_SUCCESSFUL : GP_FAILURE;

                    if(gp_pop_state(canvas) != GP_SUCCESSFUL)
                    {
                        primitiveResult = GP_FAILURE;
                    }
                }

                break;
            case PathShape::QuadraticBezier:
                if(points.size() == 3)
                {
                    bool recorded = Detail::recordGpBezier(canvas, points, false);
                    primitiveResult = recorded == true ? GP_SUCCESSFUL : GP_FAILURE;
                }

                break;
            case PathShape::CubicBezier:
                if(points.size() == 4)
                {
                    bool recorded = Detail::recordGpBezier(canvas, points, true);
                    primitiveResult = recorded == true ? GP_SUCCESSFUL : GP_FAILURE;
                }

                break;
            }

            bool endFilled = true;

            if(graphicsFill == true)
            {
                gp_result_t endFillResult = gp_end_fill(canvas);
                endFilled = endFillResult == GP_SUCCESSFUL;
            }

            bool successful = primitiveResult == GP_SUCCESSFUL && endFilled == true;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        bool recordGpCommand(GraphicsBridgeState & state, const DrawCommandStorage & storage, const DrawCommand & command, const RenderState * renderState, float pixelScale)
        {
            gp_canvas_t * canvas = state.canvas;

            if(command.type == DrawCommandType::PushTransform)
            {
                if(gp_push_state(canvas) != GP_SUCCESSFUL)
                {
                    return false;
                }

                float translationX = 0.f;
                float translationY = 0.f;
                float axisXX = 1.f;
                float axisXY = 0.f;
                float axisYX = 0.f;
                float axisYY = 1.f;
                gp_result_t getResult = gp_get_transform(canvas, &translationX, &translationY, &axisXX, &axisXY, &axisYX, &axisYY);

                if(getResult != GP_SUCCESSFUL)
                {
                    gp_pop_state(canvas);

                    return false;
                }

                const Transform2D & relative = command.payload.transform.transform;
                float composedTranslationX = translationX + axisXX * relative.translation.x + axisYX * relative.translation.y;
                float composedTranslationY = translationY + axisXY * relative.translation.x + axisYY * relative.translation.y;
                float composedAxisXX = axisXX * relative.axisX.x + axisYX * relative.axisX.y;
                float composedAxisXY = axisXY * relative.axisX.x + axisYY * relative.axisX.y;
                float composedAxisYX = axisXX * relative.axisY.x + axisYX * relative.axisY.y;
                float composedAxisYY = axisXY * relative.axisY.x + axisYY * relative.axisY.y;
                gp_result_t transformResult = gp_set_transform(canvas, composedTranslationX, composedTranslationY, composedAxisXX, composedAxisXY, composedAxisYX, composedAxisYY);

                if(transformResult != GP_SUCCESSFUL)
                {
                    gp_pop_state(canvas);

                    return false;
                }

                gp_stroke_scale_t scale = command.payload.transform.strokeScale == StrokeScale::Screen ? GP_STROKE_SCALE_SCREEN : GP_STROKE_SCALE_WORLD;
                gp_result_t scaleResult = gp_set_stroke_scale(canvas, scale);

                if(scaleResult != GP_SUCCESSFUL)
                {
                    gp_pop_state(canvas);

                    return false;
                }

                return true;
            }

            if(command.type == DrawCommandType::PopTransform)
            {
                gp_result_t result = gp_pop_state(canvas);

                return result == GP_SUCCESSFUL;
            }

            if(command.type == DrawCommandType::PushClip)
            {
                return true;
            }

            if(command.type == DrawCommandType::PopClip)
            {
                return true;
            }

            if(gp_push_state(canvas) != GP_SUCCESSFUL)
            {
                return false;
            }

            float fillFeather = 0.f;
            float linePenumbra = 0.f;

            if(renderState != nullptr)
            {
                bool featheredFill = command.type == DrawCommandType::Box || command.type == DrawCommandType::RoundedRect || command.type == DrawCommandType::Path;

                if(featheredFill == true)
                {
                    fillFeather = std::max(0.f, renderState->fillFeather);
                }

                linePenumbra = std::max(0.f, renderState->linePenumbra);
                gp_result_t featherResult = gp_set_feather(canvas, fillFeather);

                if(featherResult != GP_SUCCESSFUL)
                {
                    gp_pop_state(canvas);

                    return false;
                }

                if(renderState->curveQuality != 0)
                {
                    gp_result_t curveResult = gp_set_curve_quality(canvas, renderState->curveQuality);

                    if(curveResult != GP_SUCCESSFUL)
                    {
                        gp_pop_state(canvas);

                        return false;
                    }
                }

                if(renderState->ellipseQuality != 0)
                {
                    gp_result_t ellipseResult = gp_set_ellipse_quality(canvas, renderState->ellipseQuality);

                    if(ellipseResult != GP_SUCCESSFUL)
                    {
                        gp_pop_state(canvas);

                        return false;
                    }
                }

                if(renderState->rectangleQuality != 0)
                {
                    gp_result_t rectangleResult = gp_set_rect_quality(canvas, renderState->rectangleQuality);

                    if(rectangleResult != GP_SUCCESSFUL)
                    {
                        gp_pop_state(canvas);

                        return false;
                    }
                }

                if(command.type == DrawCommandType::Path && command.payload.path.segments == 0)
                {
                    const PathDrawCommand & path = command.payload.path;

                    if(path.shape == PathShape::Circle || path.shape == PathShape::Arc || path.shape == PathShape::Ellipse || path.shape == PathShape::GradientRing)
                    {
                        float radius = std::max(path.radii.x, path.radii.y);
                        gp_uint8_t segmentCount = renderState->ellipseQuality;
                        gp_result_t segmentResult = gp_calculate_circle_segment_count(radius, renderState->circleTessellationMaximumError, &segmentCount);

                        if(segmentResult == GP_SUCCESSFUL)
                        {
                            gp_result_t qualityResult = gp_set_ellipse_quality(canvas, segmentCount);

                            if(qualityResult != GP_SUCCESSFUL)
                            {
                                gp_pop_state(canvas);

                                return false;
                            }
                        }
                    }
                    else if(path.shape == PathShape::QuadraticBezier || path.shape == PathShape::CubicBezier)
                    {
                        Vec2Span points = storage.pointSpan(path.points);
                        float controlPolygonLength = 0.f;
                        for(size_t index = 1; index != points.size(); ++index)
                        {
                            Vec2 delta = points[index] - points[index - 1];
                            controlPolygonLength += std::sqrt(delta.x * delta.x + delta.y * delta.y);
                        }

                        gp_uint8_t segmentCount = renderState->curveQuality;
                        gp_result_t segmentResult = gp_calculate_curve_segment_count(controlPolygonLength, renderState->curveTessellationMaximumError, &segmentCount);

                        if(segmentResult == GP_SUCCESSFUL)
                        {
                            gp_result_t qualityResult = gp_set_curve_quality(canvas, segmentCount);

                            if(qualityResult != GP_SUCCESSFUL)
                            {
                                gp_pop_state(canvas);

                                return false;
                            }
                        }
                    }
                }
            }

            bool result = false;
            switch(command.type)
            {
            case DrawCommandType::Rect:
            {
                const RectDrawCommand & rectangleCommand = command.payload.rectangle;
                Rect rectangle = Detail::pixelAlignedRect(rectangleCommand.bounds, pixelScale);
                gp_result_t colorResult = gp_set_color(canvas, rectangleCommand.color.r, rectangleCommand.color.g, rectangleCommand.color.b, rectangleCommand.color.a);
                gp_result_t uvResult = colorResult == GP_SUCCESSFUL ? gp_set_uv_offset(canvas, rectangleCommand.uv.x, rectangleCommand.uv.y, rectangleCommand.uv.width, rectangleCommand.uv.height) : GP_FAILURE;
                gp_result_t beginFillResult = uvResult == GP_SUCCESSFUL ? gp_begin_fill(canvas) : GP_FAILURE;

                if(beginFillResult == GP_SUCCESSFUL)
                {
                    gp_result_t primitiveResult = gp_rect(canvas, rectangle.x, rectangle.y, rectangle.width, rectangle.height);
                    gp_result_t endResult = gp_end_fill(canvas);
                    result = primitiveResult == GP_SUCCESSFUL && endResult == GP_SUCCESSFUL;
                }

                break;
            }
            case DrawCommandType::RectBatch:
                result = Detail::recordGpRects(canvas, storage.rectangleSpan(command.payload.rectBatch.instances), pixelScale);
                break;
            case DrawCommandType::QuadBatch:
                result = Detail::recordGpQuads(canvas, storage.quadSpan(command.payload.quadBatch.instances));
                break;
            case DrawCommandType::Box:
                result = Detail::recordGpBox(canvas, command.payload.box, pixelScale, fillFeather);
                break;
            case DrawCommandType::RoundedRect:
            {
                const RectDrawCommand & rectangleCommand = command.payload.rectangle;
                Rect rectangle = Detail::pixelAlignedRect(rectangleCommand.bounds, pixelScale);
                gp_result_t colorResult = gp_set_color(canvas, rectangleCommand.color.r, rectangleCommand.color.g, rectangleCommand.color.b, rectangleCommand.color.a);

                if(colorResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_result_t uvResult = gp_set_uv_offset(canvas, rectangleCommand.uv.x, rectangleCommand.uv.y, rectangleCommand.uv.width, rectangleCommand.uv.height);

                if(uvResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_result_t beginFillResult = gp_begin_fill(canvas);

                if(beginFillResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_result_t primitiveResult = gp_rounded_rect(canvas, rectangle.x, rectangle.y, rectangle.width, rectangle.height, rectangleCommand.radius);
                gp_result_t endFillResult = gp_end_fill(canvas);
                result = primitiveResult == GP_SUCCESSFUL && endFillResult == GP_SUCCESSFUL;
                break;
            }
            case DrawCommandType::Gradient:
                result = Detail::recordGpGradient(canvas, command.payload.gradient);
                break;
            case DrawCommandType::Line:
            {
                Detail::AlignedLine line = Detail::pixelAlignedLine(command.payload.line.first, command.payload.line.second, command.payload.line.thickness, pixelScale);

                if(line.first == line.second)
                {
                    result = true;
                    break;
                }

                Array<Vec2, 2> points = {line.first, line.second};
                const Color & color = command.payload.line.color;
                gp_result_t colorResult = gp_set_color(canvas, color.r, color.g, color.b, color.a);

                if(colorResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_result_t thicknessResult = gp_set_thickness(canvas, line.thickness);

                if(thicknessResult != GP_SUCCESSFUL)
                {
                    break;
                }

                float penumbra = std::clamp(linePenumbra, 0.f, line.thickness * 0.5f);
                gp_result_t penumbraResult = gp_set_penumbra(canvas, penumbra);

                if(penumbraResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_result_t lineResult = gp_polyline(canvas, points.data(), points.size(), sizeof(Vec2), nullptr, GP_FALSE);

                if(lineResult == GP_SUCCESSFUL)
                {
                    result = true;
                }

                break;
            }
            case DrawCommandType::Polyline:
            {
                const PolylineDrawCommand & polyline = command.payload.polyline;
                Vec2Span points = storage.pointSpan(polyline.points);
                gp_result_t colorResult = gp_set_color(canvas, polyline.color.r, polyline.color.g, polyline.color.b, polyline.color.a);

                if(colorResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_result_t thicknessResult = gp_set_thickness(canvas, polyline.thickness);

                if(thicknessResult != GP_SUCCESSFUL)
                {
                    break;
                }

                float penumbra = std::clamp(linePenumbra, 0.f, polyline.thickness * 0.5f);
                gp_result_t penumbraResult = gp_set_penumbra(canvas, penumbra);

                if(penumbraResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_bool_t closed = polyline.closed == true ? GP_TRUE : GP_FALSE;
                gp_result_t lineResult = gp_polyline(canvas, points.data(), points.size(), sizeof(Vec2), nullptr, closed);

                if(lineResult == GP_SUCCESSFUL)
                {
                    result = true;
                }

                break;
            }
            case DrawCommandType::Path:
                result = Detail::recordGpPath(canvas, storage, command.payload.path, linePenumbra);
                break;
            case DrawCommandType::Image:
            {
                const RectDrawCommand & rectangle = command.payload.rectangle;
                gp_result_t colorResult = gp_set_color(canvas, rectangle.color.r, rectangle.color.g, rectangle.color.b, rectangle.color.a);
                gp_result_t uvResult = colorResult == GP_SUCCESSFUL ? gp_set_uv_offset(canvas, rectangle.uv.x, rectangle.uv.y, rectangle.uv.width, rectangle.uv.height) : GP_FAILURE;
                gp_result_t beginFillResult = uvResult == GP_SUCCESSFUL ? gp_begin_fill(canvas) : GP_FAILURE;

                if(beginFillResult == GP_SUCCESSFUL)
                {
                    gp_result_t primitiveResult = gp_rect(canvas, rectangle.bounds.x, rectangle.bounds.y, rectangle.bounds.width, rectangle.bounds.height);
                    gp_result_t endResult = gp_end_fill(canvas);
                    result = primitiveResult == GP_SUCCESSFUL && endResult == GP_SUCCESSFUL;
                }

                break;
            }
            case DrawCommandType::CustomGeometry:
            {
                VertexSpan vertices = storage.vertexSpan(command.payload.custom.vertices);
                IndexSpan indices = storage.indexSpan(command.payload.custom.indices);

                if(vertices.empty() == true)
                {
                    result = true;
                    break;
                }

                if(indices.empty() == true)
                {
                    result = true;
                    break;
                }

                if(vertices.size() > std::numeric_limits<uint32_t>::max())
                {
                    break;
                }

                if(indices.size() > std::numeric_limits<uint32_t>::max())
                {
                    break;
                }

                gp_result_t geometryResult = gp_geometry(canvas, static_cast<uint32_t>(vertices.size()), vertices.data(), offsetof(Vertex, position), sizeof(Vertex), vertices.data(), offsetof(Vertex, color), sizeof(Vertex), vertices.data(), offsetof(Vertex, uv), sizeof(Vertex), static_cast<uint32_t>(indices.size()), indices.data(), 0, sizeof(uint32_t), 0.f, 0.f, 1.f, 0.f, 0.f, 1.f);
                result = geometryResult == GP_SUCCESSFUL;
                break;
            }
            case DrawCommandType::TextGeometry:
            {
                const TextGeometryDrawCommand & textGeometry = command.payload.textGeometry;

                if(textGeometry.rectangles.empty() == true)
                {
                    result = true;
                    break;
                }

                if(textGeometry.rectangles.size() > std::numeric_limits<uint32_t>::max())
                {
                    break;
                }

                gp_result_t colorResult = gp_set_color(canvas, textGeometry.tint.r, textGeometry.tint.g, textGeometry.tint.b, textGeometry.tint.a);

                if(colorResult != GP_SUCCESSFUL)
                {
                    break;
                }

                const TexturedRectInstance * rectangles = textGeometry.rectangles.data();
                gp_result_t rectanglesResult = gp_textured_rects(canvas, static_cast<uint32_t>(textGeometry.rectangles.size()), rectangles, offsetof(TexturedRectInstance, bounds), sizeof(TexturedRectInstance), rectangles, offsetof(TexturedRectInstance, uv), sizeof(TexturedRectInstance), rectangles, offsetof(TexturedRectInstance, color), sizeof(TexturedRectInstance), textGeometry.translation.x, textGeometry.translation.y, textGeometry.axisX.x, textGeometry.axisX.y, textGeometry.axisY.x, textGeometry.axisY.y);
                result = rectanglesResult == GP_SUCCESSFUL;
                break;
            }
            case DrawCommandType::NineSlice:
            {
                const NineSliceDrawCommand & nineSlice = command.payload.nineSlice;
                const NineSliceOptions & options = nineSlice.options;
                gp_result_t colorResult = gp_set_color(canvas, options.tint.r, options.tint.g, options.tint.b, options.tint.a);

                if(colorResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_nine_slice_mode_t edgeMode = options.edges == NineSliceMode::Tile ? GP_NINE_SLICE_TILE : GP_NINE_SLICE_STRETCH;
                gp_nine_slice_mode_t centerMode = options.center == NineSliceMode::Tile ? GP_NINE_SLICE_TILE : GP_NINE_SLICE_STRETCH;
                gp_result_t primitiveResult = gp_nine_slice(canvas, nineSlice.bounds.x, nineSlice.bounds.y, nineSlice.bounds.width, nineSlice.bounds.height, options.sourceSize.x, options.sourceSize.y, options.uv.x, options.uv.y, options.uv.width, options.uv.height, options.left, options.top, options.right, options.bottom, edgeMode, centerMode);
                result = primitiveResult == GP_SUCCESSFUL;
                break;
            }
            case DrawCommandType::Grid:
            {
                const GridDrawCommand & grid = command.payload.grid;
                const GridStyle & style = grid.style;
                gp_stroke_scale_t scale = style.strokeScale == StrokeScale::Screen ? GP_STROKE_SCALE_SCREEN : GP_STROKE_SCALE_WORLD;
                gp_result_t scaleResult = gp_set_stroke_scale(canvas, scale);

                if(scaleResult != GP_SUCCESSFUL)
                {
                    break;
                }

                gp_result_t primitiveResult;

                if(style.bounded == true)
                {
                    primitiveResult = gp_grid_bounded(canvas, grid.bounds.x, grid.bounds.y, grid.bounds.width, grid.bounds.height, style.range.x, style.range.y, style.range.width, style.range.height, style.origin.x, style.origin.y, style.axisX.x, style.axisX.y, style.axisY.x, style.axisY.y, style.minorSpacing, style.majorInterval, style.minorThickness, style.majorThickness, style.minorColor.r, style.minorColor.g, style.minorColor.b, style.minorColor.a, style.majorColor.r, style.majorColor.g, style.majorColor.b, style.majorColor.a);
                }
                else
                {
                    primitiveResult = gp_grid(canvas, grid.bounds.x, grid.bounds.y, grid.bounds.width, grid.bounds.height, style.origin.x, style.origin.y, style.axisX.x, style.axisX.y, style.axisY.x, style.axisY.y, style.minorSpacing, style.majorInterval, style.minorThickness, style.majorThickness, style.minorColor.r, style.minorColor.g, style.minorColor.b, style.minorColor.a, style.majorColor.r, style.majorColor.g, style.majorColor.b, style.majorColor.a);
                }

                result = primitiveResult == GP_SUCCESSFUL;
                break;
            }
            case DrawCommandType::PushTransform:
            case DrawCommandType::PopTransform:
            case DrawCommandType::PushClip:
            case DrawCommandType::PopClip:
                break;
            }
            gp_result_t popResult = gp_pop_state(canvas);
            bool successful = result == true && popResult == GP_SUCCESSFUL;

            return successful;
        }
        //////////////////////////////////////////////////////////////////////////
        [[nodiscard]] bool flushGpSegment(GraphicsBridgeState & state, RenderMesh & output, uint64_t renderKey)
        {
            gp_mesh_t mesh = {};
            gp_result_t sizeResult = gp_calculate_mesh_size(state.canvas, &mesh);

            if(sizeResult != GP_SUCCESSFUL)
            {
                return false;
            }

            size_t vertexOffset = output.vertices.size();
            size_t indexOffset = output.indices.size();
            size_t maximumMeshSize = std::numeric_limits<uint32_t>::max();

            if(vertexOffset > maximumMeshSize)
            {
                return false;
            }

            if(indexOffset > maximumMeshSize)
            {
                return false;
            }

            size_t availableVertexCount = maximumMeshSize - vertexOffset;

            if(mesh.vertex_count > availableVertexCount)
            {
                return false;
            }

            size_t availableIndexCount = maximumMeshSize - indexOffset;

            if(mesh.index_count > availableIndexCount)
            {
                return false;
            }

            output.vertices.resize(vertexOffset + mesh.vertex_count);
            output.indices.resize(indexOffset + mesh.index_count);
            state.batches.resize(mesh.batch_count);

            mesh.positions_buffer = output.vertices.data();
            mesh.positions_offset = vertexOffset * sizeof(RenderVertex) + offsetof(RenderVertex, position);
            mesh.positions_stride = sizeof(RenderVertex);
            mesh.colors_buffer = output.vertices.data();
            mesh.colors_offset = vertexOffset * sizeof(RenderVertex) + offsetof(RenderVertex, color);
            mesh.colors_stride = sizeof(RenderVertex);
            mesh.uv_buffer = output.vertices.data();
            mesh.uv_offset = vertexOffset * sizeof(RenderVertex) + offsetof(RenderVertex, uv);
            mesh.uv_stride = sizeof(RenderVertex);
            mesh.indices_buffer = output.indices.data();
            mesh.indices_offset = indexOffset * sizeof(uint32_t);
            mesh.indices_stride = sizeof(uint32_t);
            mesh.batches_buffer = state.batches.data();
            mesh.batches_offset = 0;
            mesh.batches_stride = sizeof(gp_render_batch_t);

            gp_result_t flushResult = gp_flush(state.canvas, &mesh);

            if(flushResult != GP_SUCCESSFUL)
            {
                output.vertices.resize(vertexOffset);
                output.indices.resize(indexOffset);

                return false;
            }

            output.batches.reserve(output.batches.size() + state.batches.size());

            for(const gp_render_batch_t & batch : state.batches)
            {
                Rect bounds = {batch.bounds.minimum_x, batch.bounds.minimum_y, batch.bounds.maximum_x - batch.bounds.minimum_x, batch.bounds.maximum_y - batch.bounds.minimum_y};
                RenderBatch renderBatch = {static_cast<uint32_t>(vertexOffset) + batch.vertex_offset, batch.vertex_count, static_cast<uint32_t>(indexOffset) + batch.index_offset, batch.index_count, renderKey, bounds};
                output.batches.push_back(renderBatch);
            }

            return true;
        }
#endif
    } // namespace Detail
    //////////////////////////////////////////////////////////////////////////
    void RenderMesh::clear() noexcept
    {
        vertices.clear();
        indices.clear();
        batches.clear();
        renderStates.clear();
        metrics = {};
    }
    //////////////////////////////////////////////////////////////////////////
    GraphicsBridge::GraphicsBridge()
    {
    }
    //////////////////////////////////////////////////////////////////////////
    GraphicsBridge::~GraphicsBridge()
    {
        assert(m_state == nullptr && "GraphicsBridge::finalize not called");
    }
    //////////////////////////////////////////////////////////////////////////
    bool GraphicsBridge::initialize(gp_graphics_t * graphics, Allocator * allocator)
    {
#if defined(MOSAIC_HAS_IROV_GRAPHICS)
        if(graphics == nullptr)
        {
            m_lastError = "Graphics object is null";

            return false;
        }

        uint32_t apiVersion = gp_get_api_version();

        if(apiVersion < GP_API_VERSION)
        {
            m_lastError = "Graphics runtime API is older than the headers used to build Mosaic";

            return false;
        }

        Allocator & resolvedAllocator = allocator == nullptr ? defaultAllocator() : *allocator;
        void * memory = resolvedAllocator.allocate(sizeof(Detail::GraphicsBridgeState), alignof(Detail::GraphicsBridgeState));

        if(memory == nullptr)
        {
            m_lastError = "Graphics bridge state allocation failed";

            return false;
        }

        auto * state = ::new(memory) Detail::GraphicsBridgeState(resolvedAllocator);

        gp_result_t createResult = gp_canvas_create(graphics, &state->canvas);

        if(createResult != GP_SUCCESSFUL)
        {
            state->~GraphicsBridgeState();
            resolvedAllocator.deallocate(state, sizeof(Detail::GraphicsBridgeState), alignof(Detail::GraphicsBridgeState));
            m_lastError = "Graphics canvas creation failed";

            return false;
        }

        m_state = state;

        return true;
#else
        (void)graphics;
        (void)allocator;

        return true;
#endif
    }
    //////////////////////////////////////////////////////////////////////////
    void GraphicsBridge::finalize()
    {
#if defined(MOSAIC_HAS_IROV_GRAPHICS)
        if(m_state != nullptr)
        {
            Detail::GraphicsBridgeState & state = *m_state;

            if(state.canvas != nullptr)
            {
                gp_canvas_destroy(state.canvas);
            }

            Allocator & allocator = *state.allocator;
            Detail::AllocatorReference keepAllocatorAlive(allocator);
            state.~GraphicsBridgeState();
            allocator.deallocate(&state, sizeof(Detail::GraphicsBridgeState), alignof(Detail::GraphicsBridgeState));

            m_state = nullptr;
        }
#endif
    }
    //////////////////////////////////////////////////////////////////////////
    bool GraphicsBridge::build(const Frame & frame, RenderMesh * const _out, size_t viewportIndex) const
    {
        bool result = buildWithPlatform(frame, _out, nullptr, viewportIndex);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool GraphicsBridge::build(const Frame & frame, RenderMesh * const _out, PlatformAdapter & platform, size_t viewportIndex) const
    {
        bool result = buildWithPlatform(frame, _out, &platform, viewportIndex);

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    bool GraphicsBridge::prepare(const Frame & frame, size_t viewportIndex) const
    {
#if defined(MOSAIC_HAS_IROV_GRAPHICS)
        if(m_state == nullptr)
        {
            m_lastError = "Graphics bridge is not initialized";

            return false;
        }

        bool result = buildWithPlatform(frame, &m_state->renderData, nullptr, viewportIndex);

        return result;
#else
        (void)frame;
        (void)viewportIndex;
        m_lastError = "Graphics support is not available";

        return false;
#endif
    }
    //////////////////////////////////////////////////////////////////////////
    bool GraphicsBridge::prepare(const Frame & frame, PlatformAdapter & platform, size_t viewportIndex) const
    {
#if defined(MOSAIC_HAS_IROV_GRAPHICS)
        if(m_state == nullptr)
        {
            m_lastError = "Graphics bridge is not initialized";

            return false;
        }

        bool result = buildWithPlatform(frame, &m_state->renderData, &platform, viewportIndex);

        return result;
#else
        (void)frame;
        (void)platform;
        (void)viewportIndex;
        m_lastError = "Graphics support is not available";

        return false;
#endif
    }
    //////////////////////////////////////////////////////////////////////////
    const RenderMesh * GraphicsBridge::renderData() const noexcept
    {
#if defined(MOSAIC_HAS_IROV_GRAPHICS)
        if(m_state == nullptr)
        {
            return nullptr;
        }

        return &m_state->renderData;
#else
        return nullptr;
#endif
    }
    //////////////////////////////////////////////////////////////////////////
    StringView GraphicsBridge::lastError() const noexcept
    {
        return m_lastError;
    }
    //////////////////////////////////////////////////////////////////////////
    bool GraphicsBridge::buildWithPlatform(const Frame & frame, RenderMesh * const _out, PlatformAdapter * platform, size_t viewportIndex) const
    {
        m_lastError = {};

        if(_out == nullptr)
        {
            m_lastError = "Render mesh output is null";

            return false;
        }

        RenderMesh & output = *_out;
        double buildStarted = platform == nullptr ? 0.0 : platform->monotonicTime();
        output.vertices.clear();
        output.indices.clear();
        output.batches.clear();
        output.metrics = {};

        if(viewportIndex >= frame.viewports.size())
        {
            output.renderStates.clear();
            m_lastError = "Frame viewport index is out of range";

            return false;
        }

        const FrameViewport & viewport = frame.viewports[viewportIndex];
        const DrawCommandVector * drawCommands = Detail::FrameViewportAccess::commands(viewport);
        const DrawCommandStorage * drawStorage = Detail::FrameViewportAccess::storage(viewport);

        if(drawCommands == nullptr)
        {
            output.renderStates.clear();
            m_lastError = "Frame viewport has no render data";

            return false;
        }

        if(drawStorage == nullptr)
        {
            output.renderStates.clear();
            m_lastError = "Frame viewport has no draw storage";

            return false;
        }

        if(output.renderStates != frame.renderStates)
        {
            output.renderStates.assign(frame.renderStates.begin(), frame.renderStates.end());
        }
#if defined(MOSAIC_HAS_IROV_GRAPHICS)
        float pixelScale = std::max(1.f, viewport.dpiScale);
        Detail::GraphicsBridgeState * state = m_state;

        if(state == nullptr)
        {
            output.clear();
            m_lastError = "Graphics bridge is not initialized";

            return false;
        }

        gp_canvas_t * canvas = state->canvas;
        gp_result_t clearResult = gp_canvas_clear(canvas);

        if(clearResult != GP_SUCCESSFUL)
        {
            output.clear();
            m_lastError = "Graphics canvas clear failed";

            return false;
        }

        gp_result_t penumbraResult = gp_set_penumbra(canvas, 0.f);

        if(penumbraResult != GP_SUCCESSFUL)
        {
            output.clear();
            m_lastError = "Graphics default state setup failed";

            return false;
        }

        gp_result_t viewportTransformResult = gp_set_transform(canvas, -viewport.bounds.x, -viewport.bounds.y, 1.f, 0.f, 0.f, 1.f);

        if(viewportTransformResult != GP_SUCCESSFUL)
        {
            output.clear();
            m_lastError = "Graphics viewport transform setup failed";

            return false;
        }

        bool hasSegment = false;
        uint64_t renderKey = 0;

        for(const DrawCommand & command : *drawCommands)
        {
            if(hasSegment == true && renderKey != command.renderKey)
            {
                if(Detail::flushGpSegment(*state, output, renderKey) == false)
                {
                    output.clear();
                    m_lastError = "Graphics segment flush failed";

                    return false;
                }

                hasSegment = false;
            }

            if(hasSegment == false)
            {
                renderKey = command.renderKey;
                hasSegment = true;
            }

            const RenderState * renderState = command.renderKey == 0 || command.renderKey > frame.renderStates.size() ? nullptr : &frame.renderStates[static_cast<size_t>(command.renderKey - 1)];
            bool commandRecorded = Detail::recordGpCommand(*state, *drawStorage, command, renderState, pixelScale);

            if(commandRecorded == false)
            {
                output.clear();
                m_lastError = "Graphics rejected a Mosaic draw command";

                return false;
            }
        }

        if(hasSegment == true && Detail::flushGpSegment(*state, output, renderKey) == false)
        {
            output.clear();
            m_lastError = "Graphics final segment flush failed";

            return false;
        }

        if(platform != nullptr)
        {
            double buildFinished = platform->monotonicTime();
            output.metrics.buildMilliseconds = (buildFinished - buildStarted) * 1000.0;
        }

        return true;
#else
        for(const DrawCommand & command : *drawCommands)
        {
            bool result = true;

            switch(command.type)
            {
            case DrawCommandType::Rect:
            case DrawCommandType::RectBatch:
            case DrawCommandType::QuadBatch:
            case DrawCommandType::Box:
            case DrawCommandType::RoundedRect:
            case DrawCommandType::Gradient:
            case DrawCommandType::Line:
            case DrawCommandType::Polyline:
            case DrawCommandType::Image:
            case DrawCommandType::Path:
            case DrawCommandType::NineSlice:
            case DrawCommandType::Grid:
                result = false;
                break;
            case DrawCommandType::CustomGeometry:
                result = false;
                break;
            case DrawCommandType::TextGeometry:
                result = false;
                break;
            case DrawCommandType::PushTransform:
            case DrawCommandType::PopTransform:
            case DrawCommandType::PushClip:
            case DrawCommandType::PopClip:
                break;
            }

            if(result == false)
            {
                output.clear();
                m_lastError = "Graphics support is required for this draw command";

                return false;
            }
        }

        if(platform != nullptr)
        {
            double buildFinished = platform->monotonicTime();
            output.metrics.buildMilliseconds = (buildFinished - buildStarted) * 1000.0;
        }

        return true;
#endif
    }
    //////////////////////////////////////////////////////////////////////////
} // namespace Mosaic
