#ifndef PLAYER_INPUT_HPP
#define PLAYER_INPUT_HPP

// Commande du joueur, exprimée dans le repère du véhicule (pas de l'écran)
struct PlayerInput {
    float throttle;  // +1 avance, -1 recule
    float steering;  // +1 tourne à droite (sens horaire à l'écran), -1 à gauche
};

#endif
