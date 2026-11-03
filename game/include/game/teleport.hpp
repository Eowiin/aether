#ifndef TELEPORT_HPP
#define TELEPORT_HPP

#include "ecs/registry.hpp"
#include "game/components/vec2.hpp"

void teleport_entity(Registry& registry, entity_t entity, Vec2 new_pos);

#endif
