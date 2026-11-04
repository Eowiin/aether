#ifndef PLAYER_CONTROL_SYSTEM_HPP
#define PLAYER_CONTROL_SYSTEM_HPP

#include "ecs/registry.hpp"

// Fait tourner Rotation selon steering, puis ajoute à Velocity une variation
// throttle * PLAYER_ACCELERATION * dt dans la nouvelle direction du véhicule
// (cos θ, sin θ). La vitesse existante est conservée (inertie).
// Seules les entités qui possèdent PlayerInput, Rotation et Velocity sont pilotées.
void player_control_system(Registry& registry, float dt);

#endif
