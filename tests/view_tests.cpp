#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>
#include "registry.hpp"
#include "view.hpp"

struct Position {
    float x;
    float y;
};

struct Velocity {
    float dx;
    float dy;
};

TEST_CASE("View - Basic intersection") {
    Registry registry;

    auto e1 = registry.create();
    auto e2 = registry.create();
    auto e3 = registry.create();
    auto e4 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    positions.emplace(e1, Position{1, 2});
    positions.emplace(e2, Position{3, 4});
    positions.emplace(e3, Position{5, 6});

    velocities.emplace(e2, Velocity{1, 1});
    velocities.emplace(e3, Velocity{2, 2});
    velocities.emplace(e4, Velocity{3, 3});

    View<Position, Velocity> view(&positions, &velocities);

    std::vector<entity_t> visited;

    view.each([&](entity_t entity, Position& pos, Velocity& vel) {
        visited.push_back(entity);

        CHECK(&pos == positions.try_get(entity));
        CHECK(&vel == velocities.try_get(entity));
    });

    REQUIRE(visited.size() == 2);
    CHECK(std::count(visited.begin(), visited.end(), e2) == 1);
    CHECK(std::count(visited.begin(), visited.end(), e3) == 1);
}

TEST_CASE("View - Different pool sizes") {
    Registry registry;

    auto e1 = registry.create();
    auto e2 = registry.create();
    auto e3 = registry.create();
    auto e4 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    SECTION("Position pool is smaller") {
        positions.emplace(e1, Position{1, 2});
        positions.emplace(e2, Position{3, 4});

        velocities.emplace(e2, Velocity{1, 1});
        velocities.emplace(e3, Velocity{2, 2});
        velocities.emplace(e4, Velocity{3, 3});

        View<Position, Velocity> view(&positions, &velocities);

        std::vector<entity_t> visited;

        view.each([&](entity_t entity, Position&, Velocity&) {
            visited.push_back(entity);
        });

        REQUIRE(visited.size() == 1);
        CHECK(visited[0] == e2);
    }

    SECTION("Velocity pool is smaller") {
        positions.emplace(e1, Position{1, 2});
        positions.emplace(e2, Position{3, 4});
        positions.emplace(e3, Position{5, 6});

        velocities.emplace(e2, Velocity{1, 1});

        View<Position, Velocity> view(&positions, &velocities);

        std::vector<entity_t> visited;

        view.each([&](entity_t entity, Position&, Velocity&) {
            visited.push_back(entity);
        });

        REQUIRE(visited.size() == 1);
        CHECK(visited[0] == e2);
    }
}

TEST_CASE("View - Empty intersection") {
    Registry registry;

    auto e1 = registry.create();
    auto e2 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    SECTION("No common entities") {
        positions.emplace(e1, Position{1, 2});
        velocities.emplace(e2, Velocity{1, 1});

        View<Position, Velocity> view(&positions, &velocities);

        int calls = 0;

        view.each([&](entity_t, Position&, Velocity&) {
            ++calls;
        });

        CHECK(calls == 0);
    }

    SECTION("One pool empty") {
        positions.emplace(e1, Position{1, 2});

        View<Position, Velocity> view(&positions, &velocities);

        int calls = 0;

        view.each([&](entity_t, Position&, Velocity&) {
            ++calls;
        });

        CHECK(calls == 0);
    }

    SECTION("Both pools empty") {
        View<Position, Velocity> view(&positions, &velocities);

        int calls = 0;

        view.each([&](entity_t, Position&, Velocity&) {
            ++calls;
        });

        CHECK(calls == 0);
    }
}

TEST_CASE("View - Modify components through references") {
    Registry registry;
    auto e1 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    positions.emplace(e1, Position{10, 20});
    velocities.emplace(e1, Velocity{3, 5});

    View<Position, Velocity> view(&positions, &velocities);

    view.each([](entity_t, Position& pos, Velocity& vel) {
        pos.x += vel.dx;
        pos.y += vel.dy;
    });

    CHECK(positions.get(e1).x == 13);
    CHECK(positions.get(e1).y == 25);
}

TEST_CASE("View - Stale entity generations") {
    Registry registry;

    auto e1 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    positions.emplace(e1, Position{1, 2});

    registry.destroy(e1);

    auto e2 = registry.create();

    REQUIRE(e1.index == e2.index);
    REQUIRE(e1.generation != e2.generation);

    velocities.emplace(e2, Velocity{3, 4});

    View<Position, Velocity> view(&positions, &velocities);

    int calls = 0;

    view.each([&](entity_t, Position&, Velocity&) {
        ++calls;
    });

    CHECK(calls == 0);
}

TEST_CASE("View - Callback component order") {
    Registry registry;
    auto e1 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    positions.emplace(e1, Position{10, 20});
    velocities.emplace(e1, Velocity{2, 3});

    View<Velocity, Position> view(&velocities, &positions);

    int calls = 0;

    view.each([&](entity_t entity, Velocity& vel, Position& pos) {
        CHECK(entity == e1);
        CHECK(&vel == velocities.try_get(e1));
        CHECK(&pos == positions.try_get(e1));
        ++calls;
    });

    CHECK(calls == 1);
}

struct Health {
    int hp;
};

struct Resource {
    std::unique_ptr<int> value;
};

TEST_CASE("View - Three components intersection") {
    Registry registry;

    auto e1 = registry.create();
    auto e2 = registry.create();
    auto e3 = registry.create();
    auto e4 = registry.create();
    auto e5 = registry.create();
    auto e6 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;
    ComponentPool<Health> healths;

    // Positions et Velocities sont insérées pour que e4 précède e2 dans leur
    // tableau dense, Health dans l'ordre inverse : l'ordre de visite révèle
    // ainsi quel pool a servi de base à l'itération
    positions.emplace(e4, Position{7, 8});
    positions.emplace(e3, Position{5, 6});
    positions.emplace(e2, Position{3, 4});
    positions.emplace(e1, Position{1, 2});

    velocities.emplace(e5, Velocity{5, 5});
    velocities.emplace(e4, Velocity{4, 4});
    velocities.emplace(e2, Velocity{2, 2});
    velocities.emplace(e1, Velocity{1, 1});

    healths.emplace(e2, Health{20});
    healths.emplace(e3, Health{30});
    healths.emplace(e4, Health{40});
    healths.emplace(e5, Health{50});
    healths.emplace(e6, Health{60});

    View<Position, Velocity, Health> view(&positions, &velocities, &healths);

    std::vector<entity_t> visited;

    auto callback = [&](entity_t entity, Position& pos, Velocity& vel, Health& health) {
        visited.push_back(entity);

        CHECK(&pos == positions.try_get(entity));
        CHECK(&vel == velocities.try_get(entity));
        CHECK(&health == healths.try_get(entity));
    };

    SECTION("Callback runs exactly for e2 and e4") {
        view.each(callback);

        REQUIRE(visited.size() == 2);
        CHECK(std::count(visited.begin(), visited.end(), e2) == 1);
        CHECK(std::count(visited.begin(), visited.end(), e4) == 1);
    }

    SECTION("Arguments follow the View type order") {
        view.each([&](entity_t entity, Position& pos, Velocity& vel, Health& health) {
            if (entity == e2) {
                CHECK(pos.x == 3);
                CHECK(vel.dx == 2);
                CHECK(health.hp == 20);
            } else {
                CHECK(entity == e4);
                CHECK(pos.x == 7);
                CHECK(vel.dx == 4);
                CHECK(health.hp == 40);
            }
            health.hp -= 1;
        });

        CHECK(healths.get(e2).hp == 19);
        CHECK(healths.get(e4).hp == 39);
        CHECK(healths.get(e3).hp == 30);
    }

    SECTION("Iteration follows one of the smallest pools") {
        REQUIRE(positions.size() == 4);
        REQUIRE(velocities.size() == 4);
        REQUIRE(healths.size() == 5);

        view.each(callback);

        // Health (le plus grand) aurait donné e2 puis e4
        REQUIRE(visited.size() == 2);
        CHECK(visited[0] == e4);
        CHECK(visited[1] == e2);
    }

    SECTION("Iteration follows Health once it is strictly the smallest") {
        healths.erase(e6);
        healths.erase(e5);
        REQUIRE(healths.size() == 3);

        view.each(callback);

        REQUIRE(visited.size() == 2);
        CHECK(visited[0] == e2);
        CHECK(visited[1] == e4);
    }
}

TEST_CASE("View - Single component") {
    Registry registry;

    auto e1 = registry.create();
    auto e2 = registry.create();
    auto e3 = registry.create();

    ComponentPool<Position> positions;

    positions.emplace(e1, Position{1, 2});
    positions.emplace(e2, Position{3, 4});
    positions.emplace(e3, Position{5, 6});

    View<Position> view(&positions);

    std::vector<entity_t> visited;

    view.each([&](entity_t entity, Position& pos) {
        visited.push_back(entity);
        CHECK(&pos == positions.try_get(entity));
    });

    REQUIRE(visited.size() == positions.size());
    CHECK(std::count(visited.begin(), visited.end(), e1) == 1);
    CHECK(std::count(visited.begin(), visited.end(), e2) == 1);
    CHECK(std::count(visited.begin(), visited.end(), e3) == 1);
}

TEST_CASE("View - Reusable view reflects current pool state") {
    Registry registry;

    auto e1 = registry.create();
    auto e2 = registry.create();

    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;

    positions.emplace(e1, Position{1, 2});
    positions.emplace(e2, Position{3, 4});

    velocities.emplace(e1, Velocity{1, 1});

    View<Position, Velocity> view(&positions, &velocities);

    std::vector<entity_t> visited;
    auto callback = [&](entity_t entity, Position&, Velocity&) {
        visited.push_back(entity);
    };

    view.each(callback);

    REQUIRE(visited.size() == 1);
    CHECK(visited[0] == e1);

    velocities.emplace(e2, Velocity{2, 2});
    visited.clear();

    view.each(callback);

    REQUIRE(visited.size() == 2);
    CHECK(std::count(visited.begin(), visited.end(), e1) == 1);
    CHECK(std::count(visited.begin(), visited.end(), e2) == 1);
}

TEST_CASE("View - Non-copyable component") {
    static_assert(!std::is_copy_constructible_v<Resource>);

    Registry registry;
    auto e1 = registry.create();

    ComponentPool<Resource> resources;

    resources.emplace(e1, Resource{std::make_unique<int>(42)});

    const int* original = resources.get(e1).value.get();

    View<Resource> view(&resources);

    int calls = 0;

    view.each([&](entity_t entity, Resource& resource) {
        CHECK(entity == e1);
        CHECK(&resource == resources.try_get(e1));
        REQUIRE(resource.value != nullptr);
        CHECK(*resource.value == 42);
        *resource.value = 100;
        ++calls;
    });

    CHECK(calls == 1);
    CHECK(resources.get(e1).value.get() == original);
    CHECK(*resources.get(e1).value == 100);
}

template<typename... Components>
concept CanMakeView = requires { typename View<Components...>; };

static_assert(CanMakeView<Position>);
static_assert(CanMakeView<Position, Velocity>);
static_assert(!CanMakeView<>);
static_assert(!CanMakeView<Position, Position>);
static_assert(!CanMakeView<Position, Velocity, Position>);
static_assert(!CanMakeView<Position, const Position>);
static_assert(!CanMakeView<const Position>);
static_assert(!CanMakeView<Position&>);
