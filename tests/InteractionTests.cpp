#include <Mosaic/Mosaic.hpp>
#include "Context.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;
    void check(bool condition, const char * message)
    {
        if(!condition)
        {
            std::fprintf(stderr, "FAIL: %s\n", message);
            ++failures;
        }
    }

    Mosaic::Input pointerInput(Mosaic::Vec2 position, uint8_t down = 0, uint8_t pressed = 0, uint8_t released = 0, Mosaic::Vec2 delta = {})
    {
        Mosaic::Input input;
        auto & pointer = input.pointers.emplace_back();
        pointer.position = position;
        pointer.delta = delta;
        pointer.down = down;
        pointer.pressed = pressed;
        pointer.released = released;
        pointer.clickCount = pressed ? 1 : 0;
        return input;
    }

    struct Controls
    {
        Mosaic::Context * ui = Mosaic::newContext();
        Mosaic::Response button, checkbox, dial, integer;
        bool checked = false;
        bool disabled = false;
        float degrees = 35.f;
        int32_t count = -47;
        int clicks = 0;
        double clock = 0;
        ~Controls() { Mosaic::deleteContext(ui); }

        void frame(Mosaic::Input input = {})
        {
            clock += input.deltaTime;
            input.timestamp = clock;
            Mosaic::Viewport viewport;
            viewport.bounds = viewport.workArea = {0, 0, 320, 240};
            Mosaic::beginFrame(ui, input, viewport);
            {
                auto disabledScope = Mosaic::disabledScope(ui, disabled);
                button = Mosaic::button(ui, "Action");
                clicks += button.clicked();
                checkbox = Mosaic::checkbox(ui, "Visible", &checked);
                dial = Mosaic::angleDial(ui, Mosaic::Key("Rotation"), "Rotation", &degrees);
                Mosaic::SliderOptions options;
                options.width = Mosaic::Dimension::fixed(92.f);
                options.dragSpeed = 1;
                integer = Mosaic::dragValue(ui, "Count", &count, int32_t{-100}, int32_t{100}, options);
            }
            (void)Mosaic::endFrame(ui);
        }

        Mosaic::Vec2 center(Mosaic::Id id) const
        {
            Mosaic::Rect bounds;
            check(Mosaic::debugBounds(ui, id, &bounds), "interactive control has bounds");
            return {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
        }
    };

    void buttonFeedback()
    {
        Controls controls;
        controls.frame();
        auto point = controls.center(controls.button.id);
        controls.frame(pointerInput(point));
        check(controls.button.hovered(), "compact button is hoverable");
        auto state = controls.ui->findState(controls.button.id);
        check(state && state->hoverVisual > 0 && state->hoverVisual < 1, "hover transitions rather than jumping");
        controls.frame(pointerInput(point, 1, 1));
        check(controls.button.active() && controls.clicks == 0, "press gives feedback before release activation");
        controls.frame(pointerInput(point, 0, 0, 1));
        check(controls.clicks == 1, "release triggers exactly one action");
        state = controls.ui->findState(controls.button.id);
        check(state && state->activationVisual > 0, "release confirmation is visible");
        for(int frame = 0; frame != 30; ++frame) controls.frame(pointerInput({300, 220}));
        state = controls.ui->findState(controls.button.id);
        check(controls.clicks == 1 && state && state->activationVisual == 0, "confirmation fades without repeating the action");
        auto checkbox = controls.center(controls.checkbox.id);
        controls.frame(pointerInput(checkbox, 1, 1));
        controls.frame(pointerInput(checkbox, 0, 0, 1));
        check(controls.checked, "small checkbox keeps its full interactive row");
    }

    void inputPolicies()
    {
        Controls controls;
        auto theme = Mosaic::Theme::dark();
        theme.behavior.animationsEnabled = false;
        Mosaic::setTheme(controls.ui, theme);
        controls.frame();
        auto point = controls.center(controls.button.id);
        controls.frame(pointerInput(point, 1, 1));
        controls.frame(pointerInput(point, 0, 0, 1));
        const auto * state = controls.ui->findState(controls.button.id);
        check(controls.clicks == 1 && state && state->activationVisual == 0 && state->hoverVisual == 1, "reduced motion retains immediate interactive states without a pulse");
        controls.disabled = true;
        controls.frame(pointerInput(point, 1, 1));
        controls.frame(pointerInput(point, 0, 0, 1));
        check(controls.clicks == 1 && !controls.button.active(), "disabled control cannot activate");
        controls.disabled = false;
        theme.behavior.hoverEnabled = false;
        Mosaic::setTheme(controls.ui, theme);
        controls.frame(pointerInput(point));
        check(!controls.button.hovered(), "touch policy suppresses hover");
        controls.frame(pointerInput(point, 1, 1));
        controls.frame(pointerInput(point, 0, 0, 1));
        check(controls.clicks == 2, "touch policy preserves activation");
    }

    void angleEditing()
    {
        Controls controls;
        controls.frame();
        auto point = controls.center(controls.dial.id);
        controls.frame(pointerInput(point, 1, 1));
        auto moved = Mosaic::Vec2{point.x + 24, point.y};
        controls.frame(pointerInput(moved, 1, 0, 0, {24, 0}));
        check(controls.degrees > 35 && controls.dial.changed(), "angle dial scrubs the actual value");
        auto escape = pointerInput(moved, 1);
        escape.keyboard.push_back({.key = Mosaic::KeyCode::Escape, .pressed = true, .modifiers = {}});
        controls.frame(escape);
        check(std::abs(controls.degrees - 35) < 0.01f && controls.dial.canceled(), "Escape rolls back an angle drag");
        controls.frame(pointerInput(moved, 0, 0, 1));
        controls.frame(pointerInput(point, 1, 1));
        controls.frame(pointerInput(moved, 1, 0, 0, {24, 0}));
        controls.frame(pointerInput(moved, 0, 0, 1));
        check(controls.degrees > 35 && controls.dial.committed(), "releasing the angle dial commits the edit");
    }

    void numericInput()
    {
        Controls controls;
        controls.frame();
        auto point = controls.center(controls.integer.id);
        Mosaic::Rect bounds;
        (void)Mosaic::debugBounds(controls.ui, controls.integer.id, &bounds);
        check(std::abs(bounds.width - 92.f) < 0.1f, "numeric controls respect the requested compact width");
        controls.frame(pointerInput(point, 1, 1));
        controls.frame(pointerInput({point.x + 24, point.y}, 1, 0, 0, {24, 0}));
        check(controls.count != -47, "integer control scrubs signed values");
        auto escape = pointerInput(point, 1);
        escape.keyboard.push_back({.key = Mosaic::KeyCode::Escape, .pressed = true, .modifiers = {}});
        controls.frame(escape);
        check(controls.count == -47 && controls.integer.canceled(), "integer drag cancellation restores the exact value");
        controls.frame(pointerInput(point, 0, 0, 1));
        point = controls.center(controls.dial.id);
        auto doubleClick = pointerInput(point, 1, 1);
        doubleClick.pointers.front().clickCount = 2;
        controls.frame(doubleClick);
        controls.frame(pointerInput(point, 0, 0, 1));
        Mosaic::Input typed;
        typed.text.emplace_back("90");
        controls.frame(typed);
        Mosaic::Input enter;
        enter.keyboard.push_back({.key = Mosaic::KeyCode::Enter, .pressed = true, .modifiers = {}});
        controls.frame(enter);
        check(std::abs(controls.degrees - 90.f) < 0.01f, "angle accepts a typed value after double-click");
    }

    void compactTabs()
    {
        Mosaic::Context * ui = Mosaic::newContext();
        constexpr Mosaic::Array<Mosaic::StringView, 4> labels = {"Object", "Material", "Import", "Widgets"};
        Mosaic::Array<Mosaic::Rect, 4> bounds;
        int selected = 0;
        auto frame = [&](Mosaic::Input input)
        {
            Mosaic::beginFrame(ui, input);
            {
                Mosaic::LayoutOptions layout;
                layout.width = Mosaic::Dimension::fixed(210.f);
                auto column = Mosaic::column(ui, layout);
                Mosaic::TabsOptions options;
                options.fittingPolicy = Mosaic::TabFittingPolicy::Shrink;
                Mosaic::tabs(ui, "Inspector", &selected, labels, options);
            }
            (void)Mosaic::endFrame(ui);
            for(const auto & node : ui->nodes)
            {
                if(node.kind != Mosaic::Detail::NodeKind::Tab) continue;
                for(size_t index = 0; index != labels.size(); ++index)
                {
                    if(node.label == labels[index]) bounds[index] = node.bounds;
                }
            }
        };
        frame({});
        frame({});
        for(size_t index = 0; index != bounds.size(); ++index)
        {
            check(bounds[index].width > 20.f && bounds[index].right() <= bounds[0].x + 210.1f, "every compact tab fits inside its panel");
            if(index > 0) check(bounds[index].x >= bounds[index - 1].right(), "compact tabs do not overlap");
        }
        Mosaic::Vec2 point = {bounds.back().x + bounds.back().width * 0.5f, bounds.back().y + bounds.back().height * 0.5f};
        frame(pointerInput(point, 1, 1));
        frame(pointerInput(point, 0, 0, 1));
        check(selected == 3, "last compact tab remains selectable");
        Mosaic::deleteContext(ui);
    }

    void timelineEditing()
    {
        Mosaic::Context * ui = Mosaic::newContext();
        Mosaic::TimelineState state;
        state.trackWidth = 80;
        Mosaic::TimelineTrack track;
        track.id = 1;
        track.label = "Rotation";
        Mosaic::TimelineKeyframe key;
        key.id = 2;
        key.track = 1;
        key.time = 2.0;
        Mosaic::TimelineResponse edit;
        Mosaic::Response response;
        Mosaic::LayoutOptions layout;
        layout.width = Mosaic::Dimension::fixed(400);
        layout.height = Mosaic::Dimension::fixed(140);
        double time = 0;
        auto frame = [&](Mosaic::Input input)
        {
            input.timestamp = time += 1.0 / 60.0;
            Mosaic::beginFrame(ui, input);
            response = Mosaic::timeline(ui, Mosaic::Key("Timeline"), "Timeline", {&track, 1}, {&key, 1}, &state, &edit, layout);
            (void)Mosaic::endFrame(ui);
        };
        frame({});
        frame({});
        Mosaic::Rect bounds;
        check(Mosaic::debugBounds(ui, response.id, &bounds), "timeline has bounds");
        Mosaic::Vec2 point = {bounds.x + 80 + (bounds.width - 80) * 0.2f, bounds.y + Mosaic::getTheme(ui).metrics.controlHeight + state.trackHeight * 0.5f};
        frame(pointerInput(point, 1, 1));
        auto moved = Mosaic::Vec2{point.x + 32, point.y};
        frame(pointerInput(moved, 1, 0, 0, {32, 0}));
        check(edit.item == key.id && edit.keyframesChanged && edit.time > key.time, "timeline diamond can be dragged");
        frame(pointerInput(moved, 0, 0, 1));
        check(edit.item == key.id && edit.phase == Mosaic::EditorTransactionPhase::Commit, "timeline drag reports a commit");
        auto ruler = Mosaic::Vec2{bounds.x + 240, bounds.y + 8};
        frame(pointerInput(ruler, 1, 1));
        check(edit.playheadChanged && state.playhead > 0, "timeline ruler scrubs the playhead");
        frame(pointerInput(ruler, 0, 0, 1));
        Mosaic::deleteContext(ui);
    }
}

int main()
{
    buttonFeedback();
    inputPolicies();
    angleEditing();
    numericInput();
    compactTabs();
    timelineEditing();
    return failures == 0 ? 0 : 1;
}
