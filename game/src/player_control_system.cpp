#include "game/player_control_system.hpp"
#include "game/components/player_input.hpp"
#include "game/components/velocity.hpp"
#include "game/macros.hpp"
#include <cmath>

Vec2 normalize(PlayerInput input) {
    const float length = std::sqrt(input.horizontal * input.horizontal +
                                   input.vertical * input.vertical);
    if (length == 0.0f) {
        return Vec2{0.0f, 0.0f};
    }
    return Vec2{input.horizontal / length, input.vertical / length};
}

void player_control_system(Registry& registry) {
    registry.view<PlayerInput, Velocity>().each(
        [](entity_t, const PlayerInput& input, Velocity& velocity) {
            const Vec2 direction = normalize(input);
            velocity.value = {PLAYER_SPEED * direction.x, PLAYER_SPEED * direction.y};
        });
}
