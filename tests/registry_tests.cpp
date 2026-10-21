#include <catch2/catch_test_macros.hpp>
#include <string>
#include <type_traits>
#include <stdexcept>
#if !defined(NDEBUG) && (defined(__unix__) || defined(__APPLE__))
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#define AETHER_DEATH_TESTS 1
#endif
#include "registry.hpp"

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
