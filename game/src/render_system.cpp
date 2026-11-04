#include "raylib.h"
#include "game/render_system.hpp"
#include "game/components/position.hpp"
#include "game/components/previous_position.hpp"
#include "game/components/rotation.hpp"
#include <cmath>

Vec2 calculate_interpolation(const PreviousPosition& previous_position, const Position& position, float alpha) {
    return Vec2 {
        previous_position.value.x + (position.value.x - previous_position.value.x) * alpha,
        previous_position.value.y + (position.value.y - previous_position.value.y) * alpha,
    };
}

// Le grand côté suit l'axe local x : à angle 0, la voiture regarde vers la droite
constexpr float CAR_LENGTH = 40.0f;
constexpr float CAR_WIDTH = 20.0f;

void render_system(const Registry& registry, float alpha) {
    registry.view<PreviousPosition, Position, Rotation>().each(
        [alpha](entity_t, const PreviousPosition& previous_position, const Position& position,
                const Rotation& rotation) {
            const Vec2 pos = calculate_interpolation(previous_position, position, alpha);

            // Origine au centre du rectangle : il pivote sur lui-même autour de pos.
            // DrawRectanglePro attend des degrés, Rotation stocke des radians.
            DrawRectanglePro({pos.x, pos.y, CAR_LENGTH, CAR_WIDTH},
                             {CAR_LENGTH / 2.0f, CAR_WIDTH / 2.0f},
                             rotation.angle * RAD2DEG, VIOLET);

            // Repère d'avant : sans lui, 0° et 180° (ou 90° et 270°) sont indiscernables
            const Vector2 front = {pos.x + std::cos(rotation.angle) * CAR_LENGTH / 2.0f,
                                   pos.y + std::sin(rotation.angle) * CAR_LENGTH / 2.0f};
            DrawCircleV(front, 5.0f, ORANGE);
        });
}
