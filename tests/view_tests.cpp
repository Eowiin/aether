#include <catch2/catch_test_macros.hpp>
#include <algorithm>
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

    View<Position, Velocity> view(positions, velocities);

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

        View<Position, Velocity> view(positions, velocities);

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

        View<Position, Velocity> view(positions, velocities);

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

        View<Position, Velocity> view(positions, velocities);

        int calls = 0;

        view.each([&](entity_t, Position&, Velocity&) {
            ++calls;
        });

        CHECK(calls == 0);
    }

    SECTION("One pool empty") {
        positions.emplace(e1, Position{1, 2});

        View<Position, Velocity> view(positions, velocities);

        int calls = 0;

        view.each([&](entity_t, Position&, Velocity&) {
            ++calls;
        });

        CHECK(calls == 0);
    }

    SECTION("Both pools empty") {
        View<Position, Velocity> view(positions, velocities);

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

    View<Position, Velocity> view(positions, velocities);

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

    View<Position, Velocity> view(positions, velocities);

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

    View<Velocity, Position> view(velocities, positions);

    int calls = 0;

    view.each([&](entity_t entity, Velocity& vel, Position& pos) {
        CHECK(entity == e1);
        CHECK(&vel == velocities.try_get(e1));
        CHECK(&pos == positions.try_get(e1));
        ++calls;
    });

    CHECK(calls == 1);
}
