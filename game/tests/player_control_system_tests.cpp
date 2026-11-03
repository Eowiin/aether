#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "game/components/player_input.hpp"
#include "game/components/position.hpp"
#include "game/components/velocity.hpp"
#include "game/macros.hpp"
#include "game/player_control_system.hpp"
#include "ecs/registry.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("PlayerControlSystem - seule l'entité avec PlayerInput est pilotée") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, Vec2{0.0f, 0.0f});
    registry.emplace<Velocity>(a, Vec2{0.0f, 0.0f});
    registry.emplace<PlayerInput>(a, PlayerInput{1.0f, 0.0f});

    auto b = registry.create();
    registry.emplace<Position>(b, Vec2{0.0f, 0.0f});
    registry.emplace<Velocity>(b, Vec2{3.0f, -4.0f});

    player_control_system(registry);

    CHECK(registry.get<Velocity>(a).value.x == PLAYER_SPEED);
    CHECK(registry.get<Velocity>(a).value.y == 0.0f);

    CHECK(registry.get<Velocity>(b).value.x == 3.0f);
    CHECK(registry.get<Velocity>(b).value.y == -4.0f);
}

TEST_CASE("PlayerControlSystem - la diagonale est normalisée") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Velocity>(a, Vec2{0.0f, 0.0f});
    registry.emplace<PlayerInput>(a, PlayerInput{1.0f, -1.0f});

    player_control_system(registry);

    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK_THAT(v.x * v.x + v.y * v.y,
               WithinAbs(PLAYER_SPEED * PLAYER_SPEED, 1e-4));
    CHECK(v.x > 0.0f);
    CHECK(v.y < 0.0f);
}

TEST_CASE("PlayerControlSystem - entrée nulle : l'entité s'arrête") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Velocity>(a, Vec2{7.0f, 7.0f});
    registry.emplace<PlayerInput>(a, PlayerInput{0.0f, 0.0f});

    player_control_system(registry);

    CHECK(registry.get<Velocity>(a).value.x == 0.0f);
    CHECK(registry.get<Velocity>(a).value.y == 0.0f);
}

TEST_CASE("PlayerControlSystem - PlayerInput sans Velocity, registre vide") {
    Registry registry;
    player_control_system(registry);

    auto a = registry.create();
    registry.emplace<PlayerInput>(a, PlayerInput{1.0f, 1.0f});
    player_control_system(registry);

    CHECK_FALSE(registry.has<Velocity>(a));
}
