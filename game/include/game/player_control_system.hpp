#ifndef PLAYER_CONTROL_SYSTEM_HPP
#define PLAYER_CONTROL_SYSTEM_HPP

#include "ecs/registry.hpp"

// Fait tourner Rotation selon steering, puis ajoute à Velocity une variation
// throttle * PLAYER_ACCELERATION * dt dans la nouvelle direction du véhicule
// (cos θ, sin θ). La vitesse existante est conservée (inertie).
// Adhérence : la composante latérale de Velocity (v · r, r = (-sin θ, cos θ))
// diminue de PLAYER_GRIP * dt sans changer de signe ; la longitudinale est intacte.
// Quand throttle = 0, la norme de Velocity diminue de PLAYER_DECELERATION * dt
// (direction inchangée), sans jamais passer sous zéro ni s'inverser.
// Seules les entités qui possèdent PlayerInput, Rotation et Velocity sont pilotées.
void player_control_system(Registry& registry, float dt);

#endif
