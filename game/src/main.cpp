#include "raylib.h"
#include <algorithm>
#include <cmath>
#include "game/movement_system.hpp"
#include "game/position_snapshot_system.hpp"
#include "game/render_system.hpp"
#include "game/player_control_system.hpp"
#include "game/components/position.hpp"
#include "game/components/velocity.hpp"
#include "game/components/previous_position.hpp"
#include "game/components/player_input.hpp"
#include "game/macros.hpp"

int main() {
    float accumulator = 0.0f;
    Registry registry;

    entity_t entity = registry.create();
    registry.emplace<Position>(entity, Vec2{50.0, 50.0});
    registry.emplace<PreviousPosition>(entity, Vec2{50.0, 50.0});
    registry.emplace<Velocity>(entity, Vec2{0.0, 0.0});
    registry.emplace<PlayerInput>(entity, PlayerInput{0.0f, 0.0f});


    InitWindow(800, 450, "aether");
    while (!WindowShouldClose()) {
        // Touches opposées : leurs contributions se compensent (Z+S → 0, Q+D → 0)
        PlayerInput& input = registry.get<PlayerInput>(entity);
        input.vertical = (IsKeyDown(KEY_S) ? 1.0f : 0.0f) - (IsKeyDown(KEY_Z) ? 1.0f : 0.0f);
        input.horizontal = (IsKeyDown(KEY_D) ? 1.0f : 0.0f) - (IsKeyDown(KEY_Q) ? 1.0f : 0.0f);
        accumulator += std::min(GetFrameTime(), 0.25f);  // cap anti « spirale de la mort »
        int ticks = 0;
        while (accumulator >= FIXED_DT && ticks < MAX_TICKS_PER_FRAME) {
            position_snapshot_system(registry);
            player_control_system(registry);
            movement_system(registry, FIXED_DT);
            accumulator -= FIXED_DT;
            ++ticks;
        }
        if (accumulator >= FIXED_DT) {
            accumulator = std::fmod(accumulator, FIXED_DT);
        }
        BeginDrawing();
        ClearBackground(RAYWHITE);
        render_system(registry, accumulator / FIXED_DT);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
