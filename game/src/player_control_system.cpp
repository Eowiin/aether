#include "game/player_control_system.hpp"
#include "game/components/player_input.hpp"
#include "game/components/rotation.hpp"
#include "game/components/velocity.hpp"
#include "game/macros.hpp"
#include <cmath>

void player_control_system(Registry& registry, float dt) {
    registry.view<PlayerInput, Rotation, Velocity>().each(
        [dt](entity_t, const PlayerInput& input, Rotation& rotation, Velocity& velocity) {
            rotation.angle += input.steering * PLAYER_ANGULAR_SPEED * dt;

            // Vecteur unitaire vers l'avant du véhicule : pas besoin de normaliser
            const Vec2 forward = {std::cos(rotation.angle), std::sin(rotation.angle)};
            // Inertie : on ajoute la variation de vitesse (a * dt) à la vitesse existante
            const float delta_speed = input.throttle * PLAYER_ACCELERATION * dt;
            velocity.value.x += delta_speed * forward.x;
            velocity.value.y += delta_speed * forward.y;
        });
}
