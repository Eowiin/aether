#include <catch2/catch_test_macros.hpp>
#include "game/components/position.hpp"
#include "game/components/velocity.hpp"
#include "game/movement_system.hpp"
#include "ecs/registry.hpp"

TEST_CASE("MovementSystem - Position + Velocity, Position seule, Velocity seule") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, 0.0f, 0.0f);
    registry.emplace<Velocity>(a, 2.0f, -4.0f);

    auto b = registry.create();
    registry.emplace<Position>(b, 5.0f, 7.0f);

    auto c = registry.create();
    registry.emplace<Velocity>(c, 3.0f, 3.0f);

    // Valeurs exactes en float : la comparaison directe est sûre
    movement_system(registry, 0.5f);

    // A : position actualisée
    CHECK(registry.get<Position>(a).x == 1.0f);
    CHECK(registry.get<Position>(a).y == -2.0f);
    CHECK(registry.get<Velocity>(a).dx == 2.0f);
    CHECK(registry.get<Velocity>(a).dy == -4.0f);

    // B : inchangée
    CHECK(registry.get<Position>(b).x == 5.0f);
    CHECK(registry.get<Position>(b).y == 7.0f);

    // C : ignorée, et le système ne lui a pas ajouté de Position
    CHECK(registry.get<Velocity>(c).dx == 3.0f);
    CHECK(registry.get<Velocity>(c).dy == 3.0f);
    CHECK_FALSE(registry.has<Position>(c));
}

TEST_CASE("MovementSystem - Les mouvements s'accumulent") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, 0.0f, 0.0f);
    registry.emplace<Velocity>(a, 2.0f, -4.0f);

    auto b = registry.create();
    registry.emplace<Position>(b, 5.0f, 7.0f);

    movement_system(registry, 0.5f);
    movement_system(registry, 0.5f);

    CHECK(registry.get<Position>(a).x == 2.0f);
    CHECK(registry.get<Position>(a).y == -4.0f);

    CHECK(registry.get<Position>(b).x == 5.0f);
    CHECK(registry.get<Position>(b).y == 7.0f);
}

TEST_CASE("MovementSystem - dt nul ou registre vide") {
    Registry registry;
    movement_system(registry, 0.5f);

    auto a = registry.create();
    registry.emplace<Position>(a, 1.0f, 2.0f);
    registry.emplace<Velocity>(a, 2.0f, -4.0f);

    movement_system(registry, 0.0f);

    CHECK(registry.get<Position>(a).x == 1.0f);
    CHECK(registry.get<Position>(a).y == 2.0f);
}
