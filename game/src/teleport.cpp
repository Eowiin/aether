#include "game/teleport.hpp"
#include "game/components/position.hpp"
#include "game/components/previous_position.hpp"

void teleport_entity(Registry& registry, entity_t entity, Vec2 new_pos) {
    Position* position = registry.try_get<Position>(entity);
    if (position == nullptr) {
        return;
    }
    position->value = new_pos;

    if (PreviousPosition* previous_position = registry.try_get<PreviousPosition>(entity)) {
        previous_position->value = new_pos;
    }
}
