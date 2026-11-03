#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>
#include <stdexcept>
#include <vector>
#include <algorithm>
#if !defined(NDEBUG) && (defined(__unix__) || defined(__APPLE__))
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#define AETHER_DEATH_TESTS 1
#endif
#include "ecs/registry.hpp"

struct Position {
    float x;
    float y;
};

struct Velocity {
    float dx;
    float dy;
};

struct Name {
    std::string value;
};

TEST_CASE("Registry creates distinct entities", "[registry]") {
    Registry reg;
    entity_t a = reg.create();
    entity_t b = reg.create();

    REQUIRE(a != b);
}

TEST_CASE("Registry emplace and get", "[registry]") {
    Registry reg;
    entity_t e = reg.create();

    reg.emplace<Position>(e, 1.0f, 2.0f);

    REQUIRE(reg.has<Position>(e));
    REQUIRE(reg.get<Position>(e).x == 1.0f);
    REQUIRE(reg.get<Position>(e).y == 2.0f);

    reg.get<Position>(e).x = 5.0f;
    REQUIRE(reg.get<Position>(e).x == 5.0f);
}

TEST_CASE("Registry forwards constructor arguments", "[registry]") {
    Registry reg;
    entity_t e = reg.create();

    reg.emplace<Name>(e, std::string("aether"));

    REQUIRE(reg.get<Name>(e).value == "aether");
}

TEST_CASE("Registry has returns false for unknown component type", "[registry]") {
    Registry reg;
    entity_t e = reg.create();

    REQUIRE_FALSE(reg.has<Position>(e));
}

TEST_CASE("Registry has returns false when entity lacks the component", "[registry]") {
    Registry reg;
    entity_t a = reg.create();
    entity_t b = reg.create();

    reg.emplace<Position>(a, 0.0f, 0.0f);

    REQUIRE(reg.has<Position>(a));
    REQUIRE_FALSE(reg.has<Position>(b));
}

TEST_CASE("Registry try_get", "[registry]") {
    Registry reg;
    entity_t a = reg.create();
    entity_t b = reg.create();

    SECTION("returns nullptr when the pool does not exist") {
        REQUIRE(reg.try_get<Position>(a) == nullptr);
    }

    SECTION("returns nullptr when the entity has no component") {
        reg.emplace<Position>(a, 1.0f, 1.0f);
        REQUIRE(reg.try_get<Position>(b) == nullptr);
    }

    SECTION("returns the component when present") {
        reg.emplace<Position>(a, 3.0f, 4.0f);
        Position* p = reg.try_get<Position>(a);
        REQUIRE(p != nullptr);
        REQUIRE(p->x == 3.0f);
    }
}

TEST_CASE("Registry const accessors", "[registry]") {
    Registry reg;
    entity_t e = reg.create();
    reg.emplace<Position>(e, 1.0f, 2.0f);

    const Registry& creg = reg;

    REQUIRE(creg.has<Position>(e));
    REQUIRE(creg.get<Position>(e).y == 2.0f);
    REQUIRE(creg.try_get<Position>(e) != nullptr);
    STATIC_CHECK(std::is_same_v<decltype(creg.get<Position>(e)), const Position&>);
    STATIC_CHECK(std::is_same_v<decltype(creg.try_get<Position>(e)), const Position*>);
    STATIC_CHECK(std::is_same_v<decltype(reg.get<Position>(e)), Position&>);
    STATIC_CHECK(std::is_same_v<decltype(reg.try_get<Position>(e)), Position*>);
    REQUIRE(creg.try_get<Velocity>(e) == nullptr);
    REQUIRE_FALSE(creg.has<Velocity>(e));
}

TEST_CASE("Registry keeps component types independent", "[registry]") {
    Registry reg;
    entity_t e = reg.create();

    reg.emplace<Position>(e, 1.0f, 2.0f);
    reg.emplace<Velocity>(e, 3.0f, 4.0f);

    REQUIRE(reg.get<Position>(e).x == 1.0f);
    REQUIRE(reg.get<Velocity>(e).dx == 3.0f);
}

TEST_CASE("Registry emplace on a dead entity throws", "[registry]") {
    Registry reg;
    entity_t e = reg.create();
    reg.destroy(e);

    REQUIRE_THROWS_AS(reg.emplace<Position>(e, 1.0f, 2.0f), std::invalid_argument);
    REQUIRE_FALSE(reg.has<Position>(e));
}

TEST_CASE("Registry emplace twice throws", "[registry]") {
    Registry reg;
    entity_t e = reg.create();
    reg.emplace<Position>(e, 1.0f, 2.0f);

    REQUIRE_THROWS_AS(reg.emplace<Position>(e, 3.0f, 4.0f), std::logic_error);
    REQUIRE(reg.get<Position>(e).x == 1.0f);
}

TEST_CASE("Registry destroy removes all components", "[registry]") {
    Registry reg;
    entity_t a = reg.create();
    entity_t b = reg.create();

    reg.emplace<Position>(a, 1.0f, 2.0f);
    reg.emplace<Velocity>(a, 3.0f, 4.0f);
    reg.emplace<Position>(b, 5.0f, 6.0f);

    reg.destroy(a);

    REQUIRE_FALSE(reg.has<Position>(a));
    REQUIRE_FALSE(reg.has<Velocity>(a));
    REQUIRE(reg.has<Position>(b));
    REQUIRE(reg.get<Position>(b).x == 5.0f);
}

TEST_CASE("Registry destroy is idempotent", "[registry]") {
    Registry reg;
    entity_t e = reg.create();
    reg.emplace<Position>(e, 1.0f, 2.0f);

    reg.destroy(e);
    REQUIRE_NOTHROW(reg.destroy(e));
}

TEST_CASE("Registry stale handle does not see recycled entity's components", "[registry]") {
    Registry reg;
    entity_t old_e = reg.create();
    reg.emplace<Position>(old_e, 1.0f, 2.0f);
    reg.destroy(old_e);

    entity_t new_e = reg.create();
    reg.emplace<Position>(new_e, 9.0f, 9.0f);

    REQUIRE(new_e.index == old_e.index);
    REQUIRE(new_e.generation != old_e.generation);
    REQUIRE_FALSE(reg.has<Position>(old_e));
    REQUIRE(reg.try_get<Position>(old_e) == nullptr);
    REQUIRE(reg.has<Position>(new_e));
}

#ifdef AETHER_DEATH_TESTS
// get<T>() has a precondition enforced by assert(): violating it aborts.
static bool aborts(void (*fn)()) {
    pid_t pid = fork();
    if (pid == 0) {
        std::signal(SIGABRT, SIG_DFL);
        freopen("/dev/null", "w", stderr);
        fn();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

TEST_CASE("Registry get aborts when the pool does not exist", "[registry][death]") {
    REQUIRE(aborts([] {
        Registry reg;
        entity_t e = reg.create();
        reg.get<Position>(e);
    }));
}

TEST_CASE("Registry get aborts when the entity lacks the component", "[registry][death]") {
    REQUIRE(aborts([] {
        Registry reg;
        entity_t a = reg.create();
        entity_t b = reg.create();
        reg.emplace<Position>(a, 0.0f, 0.0f);
        reg.get<Position>(b);
    }));
}
#endif

// Itère sur le pool le plus petit et garde les entités présentes dans l'autre
template<typename A, typename B>
static std::vector<entity_t> intersect(const ComponentPool<A>& a, const ComponentPool<B>& b) {
    if (b.size() < a.size()) {
        return intersect(b, a);
    }
    std::vector<entity_t> intersection;

    for (std::size_t i = 0; i < a.size(); i++) {
        entity_t entity = a.entity_at(i);
        if (b.contains(entity)) {
            intersection.push_back(entity);
        }
    }
    return intersection;
}

static bool contains_entity(const std::vector<entity_t>& entities, entity_t entity) {
    return std::find(entities.begin(), entities.end(), entity) != entities.end();
}

TEST_CASE("Intersection test for view", "[registry][view]") {
    Registry registry;

    entity_t e1 = registry.create();
    entity_t e2 = registry.create();
    entity_t e3 = registry.create();
    entity_t e4 = registry.create();
    entity_t e5 = registry.create();
    entity_t e6 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    SECTION("pools of equal size") {
        positions.emplace(e1, Position{1, 2});
        positions.emplace(e2, Position{3, 4});
        positions.emplace(e3, Position{5, 6});

        velocities.emplace(e2, Velocity{1, 1});
        velocities.emplace(e3, Velocity{2, 2});
        velocities.emplace(e4, Velocity{3, 3});

        std::vector<entity_t> intersection = intersect(positions, velocities);

        REQUIRE(intersection.size() == 2);
        REQUIRE(contains_entity(intersection, e2));
        REQUIRE(contains_entity(intersection, e3));
    }

    SECTION("first pool larger than the second") {
        positions.emplace(e1, Position{1, 2});
        positions.emplace(e2, Position{3, 4});
        positions.emplace(e3, Position{5, 6});
        positions.emplace(e4, Position{7, 8});
        positions.emplace(e5, Position{9, 10});

        velocities.emplace(e4, Velocity{1, 1});
        velocities.emplace(e6, Velocity{2, 2});

        std::vector<entity_t> intersection = intersect(positions, velocities);

        REQUIRE(intersection.size() == 1);
        REQUIRE(contains_entity(intersection, e4));
        REQUIRE(intersect(velocities, positions) == intersection);
    }

    SECTION("first pool smaller than the second") {
        positions.emplace(e2, Position{1, 2});
        positions.emplace(e5, Position{3, 4});

        velocities.emplace(e1, Velocity{1, 1});
        velocities.emplace(e2, Velocity{2, 2});
        velocities.emplace(e3, Velocity{3, 3});
        velocities.emplace(e4, Velocity{4, 4});
        velocities.emplace(e5, Velocity{5, 5});
        velocities.emplace(e6, Velocity{6, 6});

        std::vector<entity_t> intersection = intersect(positions, velocities);

        REQUIRE(intersection.size() == 2);
        REQUIRE(contains_entity(intersection, e2));
        REQUIRE(contains_entity(intersection, e5));
        REQUIRE(intersect(velocities, positions) == intersection);
    }

    SECTION("one empty pool") {
        positions.emplace(e1, Position{1, 2});
        positions.emplace(e2, Position{3, 4});

        REQUIRE(intersect(positions, velocities).empty());
        REQUIRE(intersect(velocities, positions).empty());
    }
}

TEST_CASE("Intersection distinguishes generations of a recycled index", "[registry][view]") {
    EntityManager manager;
    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    entity_t e1 = manager.createEntity();
    positions.emplace(e1, Position{1, 2});
    manager.destroyEntity(e1);

    entity_t e2 = manager.createEntity();
    REQUIRE(e2.index == e1.index);
    REQUIRE(e2.generation != e1.generation);

    velocities.emplace(e2, Velocity{3, 4});

    REQUIRE(intersect(positions, velocities).empty());
    REQUIRE(intersect(velocities, positions).empty());
}

// Composant dont le constructeur lève quand throw_on_construct est activé.
// Copie et déplacement implicites (noexcept) : il respecte les contraintes de
// type de ComponentPool, seul son constructeur d'origine peut échouer.
struct FragileComponent {
    int value;

    static inline bool throw_on_construct = false;

    explicit FragileComponent(int v) : value(v) {
        if (throw_on_construct) {
            throw std::runtime_error("Construction failed");
        }
    }
};

TEST_CASE("Registry stays usable after a failed first insertion in a new pool", "[registry]") {
    struct ThrowOnConstructGuard {
        ThrowOnConstructGuard() { FragileComponent::throw_on_construct = true; }
        ~ThrowOnConstructGuard() { FragileComponent::throw_on_construct = false; }
    };

    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<FragileComponent>);

    Registry reg;
    entity_t first = reg.create();
    entity_t second = reg.create();

    reg.emplace<Position>(first, 1.0f, 2.0f);

    {
        ThrowOnConstructGuard guard;
        CHECK_THROWS_AS(reg.emplace<FragileComponent>(second, 7), std::runtime_error);
    }

    // Le pool de FragileComponent a été créé puis est resté vide
    CHECK_FALSE(reg.has<FragileComponent>(second));
    CHECK(reg.try_get<FragileComponent>(second) == nullptr);
    CHECK_FALSE(reg.has<FragileComponent>(first));
    CHECK_FALSE(reg.has<Position>(second));

    REQUIRE(reg.has<Position>(first));
    CHECK(reg.get<Position>(first).x == 1.0f);
    CHECK(reg.get<Position>(first).y == 2.0f);

    // Exception désactivée : la même insertion réussit
    REQUIRE_NOTHROW(reg.emplace<FragileComponent>(second, 7));

    REQUIRE(reg.has<FragileComponent>(second));
    CHECK(reg.get<FragileComponent>(second).value == 7);
    CHECK_FALSE(reg.has<FragileComponent>(first));
    REQUIRE(reg.has<Position>(first));
    CHECK(reg.get<Position>(first).x == 1.0f);
    CHECK_FALSE(reg.has<Position>(second));

    // Aucune entité perdue : elles sont toujours vivantes et détruites proprement
    reg.destroy(second);
    CHECK_FALSE(reg.has<FragileComponent>(second));
    CHECK(reg.has<Position>(first));

    // Le Registry reste utilisable : une nouvelle entité reçoit ses composants
    entity_t third = reg.create();
    REQUIRE_NOTHROW(reg.emplace<FragileComponent>(third, 9));
    CHECK(reg.get<FragileComponent>(third).value == 9);
}

TEST_CASE("Registry destroys an entity completely and recycles its index cleanly", "[registry]") {
    Registry reg;
    entity_t a = reg.create();
    entity_t b = reg.create();
    entity_t c = reg.create();

    reg.emplace<Position>(a, 1.0f, 1.0f);
    reg.emplace<Velocity>(a, 2.0f, 2.0f);
    reg.emplace<Position>(b, 3.0f, 3.0f);
    reg.emplace<Velocity>(c, 4.0f, 4.0f);

    auto check_b_and_c_intact = [&]() {
        REQUIRE(reg.has<Position>(b));
        CHECK(reg.get<Position>(b).x == 3.0f);
        CHECK_FALSE(reg.has<Velocity>(b));

        REQUIRE(reg.has<Velocity>(c));
        CHECK(reg.get<Velocity>(c).dx == 4.0f);
        CHECK_FALSE(reg.has<Position>(c));
    };

    SECTION("destroying removes every component and leaves other entities intact") {
        reg.destroy(a);

        CHECK_FALSE(reg.has<Position>(a));
        CHECK_FALSE(reg.has<Velocity>(a));
        CHECK(reg.try_get<Position>(a) == nullptr);
        CHECK(reg.try_get<Velocity>(a) == nullptr);
        check_b_and_c_intact();
    }

    SECTION("destroying twice has no effect") {
        reg.destroy(a);
        CHECK_NOTHROW(reg.destroy(a));

        CHECK_FALSE(reg.has<Position>(a));
        CHECK_FALSE(reg.has<Velocity>(a));
        check_b_and_c_intact();

        // Le recyclage n'a pas été perturbé par la double destruction :
        // un seul index est libre, il est rendu une seule fois
        entity_t d = reg.create();
        entity_t e = reg.create();
        CHECK(d.index == a.index);
        CHECK(e.index != a.index);
        CHECK(e.index != b.index);
        CHECK(e.index != c.index);
    }

    SECTION("a recycled index gets a new generation and no old component") {
        reg.destroy(a);
        entity_t d = reg.create();

        CHECK(d.index == a.index);
        CHECK(d.generation != a.generation);
        CHECK(d != a);

        CHECK_FALSE(reg.has<Position>(d));
        CHECK_FALSE(reg.has<Velocity>(d));
        CHECK(reg.try_get<Position>(d) == nullptr);
        CHECK(reg.try_get<Velocity>(d) == nullptr);

        reg.emplace<Position>(d, 9.0f, 9.0f);
        reg.emplace<Velocity>(d, 8.0f, 8.0f);

        REQUIRE(reg.has<Position>(d));
        REQUIRE(reg.has<Velocity>(d));
        CHECK(reg.get<Position>(d).x == 9.0f);
        CHECK(reg.get<Velocity>(d).dx == 8.0f);

        // L'ancienne entité reste invalide malgré l'index partagé
        CHECK_FALSE(reg.has<Position>(a));
        CHECK_FALSE(reg.has<Velocity>(a));
        CHECK(reg.try_get<Position>(a) == nullptr);
        CHECK(reg.try_get<Velocity>(a) == nullptr);
        CHECK_THROWS_AS(reg.emplace<Position>(a, 0.0f, 0.0f), std::invalid_argument);
        CHECK(reg.get<Position>(d).x == 9.0f);

        check_b_and_c_intact();
    }
}
