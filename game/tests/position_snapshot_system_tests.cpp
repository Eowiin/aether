#include <catch2/catch_test_macros.hpp>
#include "game/components/position.hpp"
#include "game/components/previous_position.hpp"
#include "game/components/velocity.hpp"
#include "game/movement_system.hpp"
#include "game/position_snapshot_system.hpp"
#include "ecs/registry.hpp"

TEST_CASE("PositionSnapshotSystem - copie Position dans PreviousPosition") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, Vec2{3.0f, -1.0f});
    registry.emplace<PreviousPosition>(a, Vec2{0.0f, 0.0f});

    // B n'a pas PreviousPosition : ignorée, et rien ne lui est ajouté
    auto b = registry.create();
    registry.emplace<Position>(b, Vec2{5.0f, 7.0f});

    // C n'a pas Position : sa PreviousPosition reste intacte
    auto c = registry.create();
    registry.emplace<PreviousPosition>(c, Vec2{9.0f, 9.0f});

    position_snapshot_system(registry);

    CHECK(registry.get<PreviousPosition>(a).value.x == 3.0f);
    CHECK(registry.get<PreviousPosition>(a).value.y == -1.0f);
    CHECK(registry.get<Position>(a).value.x == 3.0f);
    CHECK(registry.get<Position>(a).value.y == -1.0f);

    CHECK(registry.get<Position>(b).value.x == 5.0f);
    CHECK_FALSE(registry.has<PreviousPosition>(b));

    CHECK(registry.get<PreviousPosition>(c).value.x == 9.0f);
    CHECK(registry.get<PreviousPosition>(c).value.y == 9.0f);
}

TEST_CASE("PositionSnapshotSystem - plusieurs ticks, avec et sans PreviousPosition") {
    Registry registry;

    // Vitesse 20 et dt 0.5 : 10 unités par tick, exactes en float
    const float dt = 0.5f;

    auto a = registry.create();
    registry.emplace<Position>(a, Vec2{50.0f, 50.0f});
    registry.emplace<PreviousPosition>(a, Vec2{50.0f, 50.0f});
    registry.emplace<Velocity>(a, Vec2{20.0f, 0.0f});

    // B n'a pas de PreviousPosition : elle doit se déplacer normalement
    auto b = registry.create();
    registry.emplace<Position>(b, Vec2{50.0f, 50.0f});
    registry.emplace<Velocity>(b, Vec2{20.0f, 0.0f});

    CHECK(registry.get<PreviousPosition>(a).value.x == 50.0f);
    CHECK(registry.get<Position>(a).value.x == 50.0f);

    // Tick 1
    position_snapshot_system(registry);
    CHECK(registry.get<PreviousPosition>(a).value.x == 50.0f);
    CHECK(registry.get<Position>(a).value.x == 50.0f);

    movement_system(registry, dt);
    CHECK(registry.get<PreviousPosition>(a).value.x == 50.0f);
    CHECK(registry.get<Position>(a).value.x == 60.0f);
    CHECK(registry.get<Position>(b).value.x == 60.0f);

    // Tick 2
    position_snapshot_system(registry);
    CHECK(registry.get<PreviousPosition>(a).value.x == 60.0f);
    CHECK(registry.get<Position>(a).value.x == 60.0f);

    movement_system(registry, dt);
    CHECK(registry.get<PreviousPosition>(a).value.x == 60.0f);
    CHECK(registry.get<Position>(a).value.x == 70.0f);
    CHECK(registry.get<Position>(b).value.x == 70.0f);

    // L'axe y n'a pas bougé et B n'a toujours pas de PreviousPosition
    CHECK(registry.get<Position>(a).value.y == 50.0f);
    CHECK(registry.get<Position>(b).value.y == 50.0f);
    CHECK_FALSE(registry.has<PreviousPosition>(b));
}

TEST_CASE("PositionSnapshotSystem - registre vide") {
    Registry registry;
    position_snapshot_system(registry);
}
