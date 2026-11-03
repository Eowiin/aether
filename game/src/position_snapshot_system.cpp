#include "game/position_snapshot_system.hpp"
#include "game/components/position.hpp"
#include "game/components/previous_position.hpp"

void position_snapshot_system(Registry& registry) {
    registry.view<PreviousPosition, Position>().each(
        [](entity_t, PreviousPosition& previous_position, const Position& position) {
            previous_position.value = position.value;
        });
}
