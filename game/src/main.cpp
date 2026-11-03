#include "raylib.h"
#include <algorithm>
#include "game/movement_system.hpp"
#include "game/position_snapshot_system.hpp"
#include "game/render_system.hpp"
#include "game/components/position.hpp"
#include "game/components/velocity.hpp"
#include "game/components/previous_position.hpp"
#include "game/macros.hpp"

int main() {
    float accumulator = 0.0f;
    Registry registry;

    entity_t entity = registry.create();
    registry.emplace<Position>(entity, Vec2{50.0, 50.0});
    registry.emplace<PreviousPosition>(entity, Vec2{50.0, 50.0});
    registry.emplace<Velocity>(entity, Vec2{15.0, 20.0});


    InitWindow(800, 450, "aether");
    while (!WindowShouldClose()) {
        accumulator += std::min(GetFrameTime(), 0.25f);  // cap anti « spirale de la mort »
        while (accumulator >= FIXED_DT) {
            position_snapshot_system(registry);
            movement_system(registry, FIXED_DT);
            accumulator -= FIXED_DT;
        }
        BeginDrawing();
        ClearBackground(RAYWHITE);
        render_system(registry, accumulator / FIXED_DT);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
