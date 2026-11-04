#ifndef VELOCITY_DECOMPOSITION_HPP
#define VELOCITY_DECOMPOSITION_HPP

#include <cmath>
#include "game/components/vec2.hpp"

// Décomposition d'une vitesse dans le repère du véhicule (angle θ).
//   avant f = (cos θ, sin θ),  côté r = (-sin θ, cos θ)
//   longitudinal = v · f,      latéral = v · r
// Fonction pure : ne modifie aucune vitesse.
struct VelocityComponents {
    float longitudinal;
    float lateral;
};

inline VelocityComponents decompose_velocity(Vec2 velocity, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return {velocity.x * c + velocity.y * s, -velocity.x * s + velocity.y * c};
}

inline Vec2 recompose_velocity(VelocityComponents velocity, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);

    return {velocity.longitudinal * c - velocity.lateral * s, velocity.longitudinal * s + velocity.lateral * c};
}

#endif
