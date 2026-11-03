#include "game/movement_system.hpp"
#include "game/components/position.hpp"
#include "game/components/velocity.hpp"

void movement_system(Registry& registry, float dt) {
    registry.view<Position, Velocity>().each(
        [dt](entity_t, Position& position, const Velocity& velocity) {
            position.value.x += velocity.value.x * dt;
            position.value.y += velocity.value.y * dt;
        });
}
