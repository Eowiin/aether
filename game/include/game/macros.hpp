#ifndef MACROS_HPP
#define MACROS_HPP

#include <numbers>

constexpr float FIXED_DT = 1.0f / 60.0f;
constexpr float PLAYER_ACCELERATION = 10.0f;
constexpr float PLAYER_DECELERATION = 5.0f;  // unités/s², résistance passive quand throttle = 0
constexpr float PLAYER_ANGULAR_SPEED = std::numbers::pi_v<float>;  // rad/s, un demi-tour par seconde
constexpr int MAX_TICKS_PER_FRAME = 5;

#endif
