#include <catch2/catch_test_macros.hpp>
#include "game/components/position.hpp"
#include "game/components/previous_position.hpp"
#include "game/components/velocity.hpp"
#include "game/movement_system.hpp"
#include "game/position_snapshot_system.hpp"
#include "ecs/registry.hpp"
#include "game/teleport.hpp"

// Même formule que calculate_interpolation (render_system.cpp), qui dépend de
// raylib et n'est donc pas liée aux tests
static Vec2 interpolate(const PreviousPosition& previous_position, const Position& position, float alpha) {
    return Vec2 {
        previous_position.value.x + (position.value.x - previous_position.value.x) * alpha,
        previous_position.value.y + (position.value.y - previous_position.value.y) * alpha,
    };
}

TEST_CASE("teleport_entity - Position et PreviousPosition deviennent identiques") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, Vec2{1.0f, 2.0f});
    registry.emplace<PreviousPosition>(a, Vec2{0.0f, 0.0f});

    teleport_entity(registry, a, Vec2{100.0f, -50.0f});

    CHECK(registry.get<Position>(a).value.x == 100.0f);
    CHECK(registry.get<Position>(a).value.y == -50.0f);
    CHECK(registry.get<PreviousPosition>(a).value.x == 100.0f);
    CHECK(registry.get<PreviousPosition>(a).value.y == -50.0f);
}

TEST_CASE("teleport_entity - entité avec Position seule") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, Vec2{1.0f, 2.0f});

    teleport_entity(registry, a, Vec2{8.0f, 9.0f});

    CHECK(registry.get<Position>(a).value.x == 8.0f);
    CHECK(registry.get<Position>(a).value.y == 9.0f);
    // Aucun composant ajouté en douce
    CHECK_FALSE(registry.has<PreviousPosition>(a));
}

TEST_CASE("teleport_entity - PreviousPosition seule : aucune modification") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<PreviousPosition>(a, Vec2{1.0f, 2.0f});

    teleport_entity(registry, a, Vec2{8.0f, 9.0f});

    CHECK(registry.get<PreviousPosition>(a).value.x == 1.0f);
    CHECK(registry.get<PreviousPosition>(a).value.y == 2.0f);
    CHECK_FALSE(registry.has<Position>(a));
}

TEST_CASE("teleport_entity - entité détruite : rien n'est modifié, pas de crash") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, Vec2{1.0f, 2.0f});
    registry.emplace<PreviousPosition>(a, Vec2{1.0f, 2.0f});
    registry.destroy(a);

    // Un autre composant d'une entité vivante ne doit pas être touché, même si
    // l'emplacement de a est recyclé
    auto b = registry.create();
    registry.emplace<Position>(b, Vec2{5.0f, 5.0f});
    registry.emplace<PreviousPosition>(b, Vec2{5.0f, 5.0f});

    teleport_entity(registry, a, Vec2{8.0f, 9.0f});

    CHECK_FALSE(registry.has<Position>(a));
    CHECK_FALSE(registry.has<PreviousPosition>(a));
    CHECK(registry.get<Position>(b).value.x == 5.0f);
    CHECK(registry.get<PreviousPosition>(b).value.x == 5.0f);
}

TEST_CASE("teleport_entity - le rendu donne la nouvelle position pour tout alpha") {
    Registry registry;

    auto a = registry.create();
    registry.emplace<Position>(a, Vec2{0.0f, 0.0f});
    registry.emplace<PreviousPosition>(a, Vec2{0.0f, 0.0f});
    registry.emplace<Velocity>(a, Vec2{10.0f, 0.0f});

    // Un tick : snapshot puis mouvement, donc previous != position
    position_snapshot_system(registry);
    movement_system(registry, 1.0f);
    REQUIRE(registry.get<PreviousPosition>(a).value.x != registry.get<Position>(a).value.x);

    // Téléportation après le snapshot, avant le rendu
    teleport_entity(registry, a, Vec2{500.0f, 300.0f});

    for (float alpha : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        Vec2 rendered = interpolate(
            registry.get<PreviousPosition>(a), registry.get<Position>(a), alpha);
        CHECK(rendered.x == 500.0f);
        CHECK(rendered.y == 300.0f);
    }
}
