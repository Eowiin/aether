#ifndef MOVEMENT_SYSTEM_HPP
#define MOVEMENT_SYSTEM_HPP

#include "ecs/registry.hpp"

// Avance chaque entité possédant Position et Velocity de velocity * dt
// (dt en secondes). Aucune modification structurelle pendant le parcours.
void movement_system(Registry& registry, float dt);

#endif
