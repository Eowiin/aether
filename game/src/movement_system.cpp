#include "game/movement_system.hpp"
#include "game/components/position.hpp"
#include "game/components/velocity.hpp"

void movement_system(Registry& registry, float dt) {
    registry.view<Position, Velocity>().each(
        [dt](entity_t, Position& position, const Velocity& velocity) {
            position.x += velocity.dx * dt;
            position.y += velocity.dy * dt;
        });
}
