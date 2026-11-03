#ifndef PLAYER_CONTROL_SYSTEM_HPP
#define PLAYER_CONTROL_SYSTEM_HPP

#include "ecs/registry.hpp"

// Traduit PlayerInput en Velocity pour les entités qui possèdent les deux
void player_control_system(Registry& registry);

#endif
