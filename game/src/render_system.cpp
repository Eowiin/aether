#include "raylib.h"
#include "game/render_system.hpp"
#include "game/components/position.hpp"
#include "game/components/previous_position.hpp"

Vec2 calculate_interpolation(const PreviousPosition& previous_position, const Position& position, float alpha) {
    return Vec2 {
        previous_position.value.x + (position.value.x - previous_position.value.x) * alpha,
        previous_position.value.y + (position.value.y - previous_position.value.y) * alpha,
    };
}

void render_system(const Registry& registry, float alpha) {
    registry.view<PreviousPosition, Position>().each([alpha](entity_t, const PreviousPosition& previous_position, const Position& position) {
        Vec2 pos = calculate_interpolation(previous_position, position, alpha);
        DrawCircleV({pos.x, pos.y}, 20.0, VIOLET);
    });
}
