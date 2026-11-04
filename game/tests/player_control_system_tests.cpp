#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <numbers>
#include "game/components/player_input.hpp"
#include "game/components/position.hpp"
#include "game/components/rotation.hpp"
#include "game/components/velocity.hpp"
#include "game/macros.hpp"
#include "game/player_control_system.hpp"
#include "ecs/registry.hpp"

using Catch::Matchers::WithinAbs;

namespace {
constexpr float PI = std::numbers::pi_v<float>;
constexpr float EPS = 1e-4f;
// Variation de vitesse produite par un tick à plein régime
constexpr float DV = PLAYER_ACCELERATION * FIXED_DT;

entity_t make_car(Registry& registry, float angle, PlayerInput input) {
    auto e = registry.create();
    registry.emplace<Velocity>(e, Vec2{0.0f, 0.0f});
    registry.emplace<Rotation>(e, angle);
    registry.emplace<PlayerInput>(e, input);
    return e;
}
}  // namespace

TEST_CASE("PlayerControlSystem - Z avance dans la direction du véhicule") {
    Registry registry;

    // throttle = 1, steering = 0 : l'angle ne bouge pas
    auto right = make_car(registry, 0.0f, PlayerInput{1.0f, 0.0f});
    auto down = make_car(registry, PI / 2.0f, PlayerInput{1.0f, 0.0f});
    auto left = make_car(registry, PI, PlayerInput{1.0f, 0.0f});
    auto up = make_car(registry, 3.0f * PI / 2.0f, PlayerInput{1.0f, 0.0f});

    player_control_system(registry, FIXED_DT);

    const Vec2 vr = registry.get<Velocity>(right).value;
    CHECK_THAT(vr.x, WithinAbs(DV, EPS));
    CHECK_THAT(vr.y, WithinAbs(0.0f, EPS));

    const Vec2 vd = registry.get<Velocity>(down).value;
    CHECK_THAT(vd.x, WithinAbs(0.0f, EPS));
    CHECK_THAT(vd.y, WithinAbs(DV, EPS));

    const Vec2 vl = registry.get<Velocity>(left).value;
    CHECK_THAT(vl.x, WithinAbs(-DV, EPS));
    CHECK_THAT(vl.y, WithinAbs(0.0f, EPS));

    const Vec2 vu = registry.get<Velocity>(up).value;
    CHECK_THAT(vu.x, WithinAbs(0.0f, EPS));
    CHECK_THAT(vu.y, WithinAbs(-DV, EPS));
}

TEST_CASE("PlayerControlSystem - l'accélération s'ajoute à la vitesse existante") {
    Registry registry;
    // Voiture tournée vers le bas (90°), qui glisse déjà vers la droite
    auto a = make_car(registry, PI / 2.0f, PlayerInput{1.0f, 0.0f});
    registry.get<Velocity>(a).value = Vec2{100.0f, 0.0f};

    // dt choisi pour que accélération * dt = 10 * 0,5 = 5 (scénario de l'exercice)
    const float dt = 5.0f / PLAYER_ACCELERATION;
    player_control_system(registry, dt);

    // La vitesse (100, 0) est purement latérale pour une voiture à 90° : seule
    // l'adhérence la réduit (de PLAYER_GRIP * dt), l'accélération s'y ajoute en y.
    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK_THAT(v.x, WithinAbs(100.0f, 1e-3));
    CHECK_THAT(v.y, WithinAbs(5.0f, 1e-3));
}

TEST_CASE("PlayerControlSystem - S recule à l'opposé de l'orientation") {
    Registry registry;
    auto a = make_car(registry, PI / 2.0f, PlayerInput{-1.0f, 0.0f});

    player_control_system(registry, FIXED_DT);

    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK_THAT(v.x, WithinAbs(0.0f, EPS));
    CHECK_THAT(v.y, WithinAbs(-DV, EPS));
    CHECK_THAT(registry.get<Rotation>(a).angle, WithinAbs(PI / 2.0f, EPS));
}

TEST_CASE("PlayerControlSystem - la variation de vitesse a toujours la même norme") {
    Registry registry;
    auto a = make_car(registry, 0.7f, PlayerInput{1.0f, 0.0f});

    player_control_system(registry, FIXED_DT);

    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK_THAT(v.x * v.x + v.y * v.y, WithinAbs(DV * DV, 1e-3));
}

TEST_CASE("PlayerControlSystem - D tourne à droite, Q à gauche, à vitesse angulaire fixe") {
    Registry registry;
    auto d = make_car(registry, 0.0f, PlayerInput{0.0f, 1.0f});
    auto q = make_car(registry, 0.0f, PlayerInput{0.0f, -1.0f});

    player_control_system(registry, FIXED_DT);

    CHECK_THAT(registry.get<Rotation>(d).angle, WithinAbs(PLAYER_ANGULAR_SPEED * FIXED_DT, EPS));
    CHECK_THAT(registry.get<Rotation>(q).angle, WithinAbs(-PLAYER_ANGULAR_SPEED * FIXED_DT, EPS));
}

TEST_CASE("PlayerControlSystem - une seconde de D donne PLAYER_ANGULAR_SPEED radians") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{0.0f, 1.0f});

    for (int i = 0; i < 60; ++i) {
        player_control_system(registry, FIXED_DT);
    }

    CHECK_THAT(registry.get<Rotation>(a).angle, WithinAbs(PLAYER_ANGULAR_SPEED, 1e-3));
}

TEST_CASE("PlayerControlSystem - la vitesse suit l'angle mis à jour dans le même tick") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{1.0f, 1.0f});

    player_control_system(registry, FIXED_DT);

    const float angle = registry.get<Rotation>(a).angle;
    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK(angle > 0.0f);
    CHECK_THAT(v.x, WithinAbs(DV * std::cos(angle), EPS));
    CHECK_THAT(v.y, WithinAbs(DV * std::sin(angle), EPS));
}

TEST_CASE("PlayerControlSystem - entrée nulle : ne tourne pas, la vitesse ne fait que décroître") {
    Registry registry;
    // Orientée selon la vitesse (45°) : l'adhérence n'intervient pas
    auto a = make_car(registry, PI / 4.0f, PlayerInput{0.0f, 0.0f});
    registry.get<Velocity>(a).value = Vec2{7.0f, 7.0f};

    player_control_system(registry, FIXED_DT);

    CHECK(registry.get<Rotation>(a).angle == PI / 4.0f);
    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK(v.x < 7.0f);
    CHECK_THAT(v.x, WithinAbs(v.y, EPS));  // direction conservée
}

TEST_CASE("PlayerControlSystem - accélérateur relâché : (10, 0) ralentit") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{0.0f, 0.0f});
    registry.get<Velocity>(a).value = Vec2{10.0f, 0.0f};

    player_control_system(registry, FIXED_DT);

    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK_THAT(v.x, WithinAbs(10.0f - PLAYER_DECELERATION * FIXED_DT, EPS));
    CHECK_THAT(v.y, WithinAbs(0.0f, EPS));
}

TEST_CASE("PlayerControlSystem - accélérateur relâché : (0, -10) diminue en norme, même direction") {
    Registry registry;
    auto a = make_car(registry, -PI / 2.0f, PlayerInput{0.0f, 0.0f});  // regarde vers le haut
    registry.get<Velocity>(a).value = Vec2{0.0f, -10.0f};

    player_control_system(registry, FIXED_DT);

    const Vec2 v = registry.get<Velocity>(a).value;
    CHECK_THAT(v.x, WithinAbs(0.0f, EPS));
    CHECK_THAT(v.y, WithinAbs(-(10.0f - PLAYER_DECELERATION * FIXED_DT), EPS));
}

TEST_CASE("PlayerControlSystem - vitesse nulle : reste nulle") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{0.0f, 0.0f});

    player_control_system(registry, FIXED_DT);

    CHECK(registry.get<Velocity>(a).value.x == 0.0f);
    CHECK(registry.get<Velocity>(a).value.y == 0.0f);
}

TEST_CASE("PlayerControlSystem - vitesse très faible : s'arrête sans repartir en sens inverse") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{0.0f, 0.0f});
    registry.get<Velocity>(a).value = Vec2{0.001f, -0.002f};

    // dt énorme : la décélération dépasse largement la vitesse
    player_control_system(registry, 10.0f);

    CHECK(registry.get<Velocity>(a).value.x == 0.0f);
    CHECK(registry.get<Velocity>(a).value.y == 0.0f);
}

TEST_CASE("PlayerControlSystem - relâcher Z finit par immobiliser la voiture") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{0.0f, 0.0f});
    registry.get<Velocity>(a).value = Vec2{10.0f, 0.0f};

    float previous = 10.0f;
    for (int i = 0; i < 200; ++i) {  // 10 / 5 = 2 s = 120 ticks
        player_control_system(registry, FIXED_DT);
        const float x = registry.get<Velocity>(a).value.x;
        CHECK(x >= 0.0f);
        CHECK(x <= previous);
        previous = x;
    }
    CHECK(registry.get<Velocity>(a).value.x == 0.0f);
    CHECK(registry.get<Velocity>(a).value.y == 0.0f);
}

TEST_CASE("PlayerControlSystem - la décélération ne s'applique pas quand Z est maintenu") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{1.0f, 0.0f});
    registry.get<Velocity>(a).value = Vec2{10.0f, 0.0f};

    player_control_system(registry, FIXED_DT);

    CHECK_THAT(registry.get<Velocity>(a).value.x, WithinAbs(10.0f + DV, EPS));
}

TEST_CASE("PlayerControlSystem - chaque entité suit sa propre commande") {
    Registry registry;
    auto a = make_car(registry, 0.0f, PlayerInput{1.0f, 0.0f});
    auto b = make_car(registry, PI, PlayerInput{0.0f, 1.0f});

    auto c = registry.create();
    registry.emplace<Velocity>(c, Vec2{5.0f, 2.0f});

    player_control_system(registry, FIXED_DT);

    CHECK_THAT(registry.get<Velocity>(a).value.x, WithinAbs(DV, EPS));
    CHECK(registry.get<Rotation>(a).angle == 0.0f);

    CHECK(registry.get<Velocity>(b).value.x == 0.0f);  // ne tourne que, pas d'accélération
    CHECK_THAT(registry.get<Rotation>(b).angle, WithinAbs(PI + PLAYER_ANGULAR_SPEED * FIXED_DT, EPS));

    CHECK(registry.get<Velocity>(c).value.x == 5.0f);
    CHECK(registry.get<Velocity>(c).value.y == 2.0f);
}

TEST_CASE("PlayerControlSystem - il faut PlayerInput, Rotation et Velocity") {
    Registry registry;
    player_control_system(registry, FIXED_DT);  // registre vide

    // Sans Rotation : pas de direction, l'entité n'est pas pilotée
    auto no_rotation = registry.create();
    registry.emplace<PlayerInput>(no_rotation, PlayerInput{1.0f, 1.0f});
    registry.emplace<Velocity>(no_rotation, Vec2{3.0f, -4.0f});

    // Sans Velocity : la rotation ne bouge pas non plus
    auto no_velocity = registry.create();
    registry.emplace<PlayerInput>(no_velocity, PlayerInput{1.0f, 1.0f});
    registry.emplace<Rotation>(no_velocity, 0.0f);

    player_control_system(registry, FIXED_DT);

    CHECK(registry.get<Velocity>(no_rotation).value.x == 3.0f);
    CHECK(registry.get<Velocity>(no_rotation).value.y == -4.0f);
    CHECK(registry.get<Rotation>(no_velocity).angle == 0.0f);
    CHECK_FALSE(registry.has<Velocity>(no_velocity));
}

TEST_CASE("PlayerControlSystem - décélération passive : drop = 2 (dt = 0,4)") {
    constexpr float dt = 0.4f;  // PLAYER_DECELERATION * dt = 2
    static_assert(PLAYER_DECELERATION == 5.0f);

    Registry registry;
    auto a = make_car(registry, std::atan2(4.0f, 3.0f), PlayerInput{0.0f, 0.0f});  // alignée sur la vitesse
    auto b = make_car(registry, 0.0f, PlayerInput{0.0f, 0.0f});
    auto c = make_car(registry, 0.0f, PlayerInput{0.0f, 0.0f});
    registry.get<Velocity>(a).value = Vec2{3.0f, 4.0f};  // norme 5 -> 3, même direction
    registry.get<Velocity>(b).value = Vec2{1.0f, 0.0f};  // norme 1 < drop -> arrêt
    registry.get<Velocity>(c).value = Vec2{0.0f, 0.0f};

    player_control_system(registry, dt);

    CHECK_THAT(registry.get<Velocity>(a).value.x, WithinAbs(1.8f, EPS));
    CHECK_THAT(registry.get<Velocity>(a).value.y, WithinAbs(2.4f, EPS));
    CHECK_THAT(registry.get<Velocity>(b).value.x, WithinAbs(0.0f, EPS));
    CHECK_THAT(registry.get<Velocity>(b).value.y, WithinAbs(0.0f, EPS));
    CHECK_THAT(registry.get<Velocity>(c).value.x, WithinAbs(0.0f, EPS));
    CHECK_THAT(registry.get<Velocity>(c).value.y, WithinAbs(0.0f, EPS));
}
