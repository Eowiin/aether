#include "game/player_control_system.hpp"
#include "game/components/player_input.hpp"
#include "game/components/rotation.hpp"
#include "game/components/velocity.hpp"
#include "game/macros.hpp"
#include "game/velocity_decomposition.hpp"
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

            // Adhérence : on atténue uniquement la vitesse latérale (v · r), de
            // PLAYER_GRIP * dt au plus, sans jamais changer son signe. La vitesse
            // longitudinale (v · f) est conservée telle quelle.
            const VelocityComponents v = decompose_velocity(velocity.value, rotation.angle);
            const float grip_drop = PLAYER_GRIP * dt;
            const float lateral = std::fabs(v.lateral) <= grip_drop
                                      ? 0.0f
                                      : v.lateral - std::copysign(grip_drop, v.lateral);
            velocity.value = recompose_velocity({v.longitudinal, lateral}, rotation.angle);

            // Décélération passive : opposée à la vitesse, bornée pour ne jamais l'inverser
            if (input.throttle == 0.0f) {
                const float speed = std::sqrt(velocity.value.x * velocity.value.x +
                                              velocity.value.y * velocity.value.y);
                const float drop = PLAYER_DECELERATION * dt;
                if (speed <= drop) {
                    velocity.value = Vec2{0.0f, 0.0f};
                } else {
                    const float scale = (speed - drop) / speed;
                    velocity.value.x *= scale;
                    velocity.value.y *= scale;
                }
            }
        });
}
