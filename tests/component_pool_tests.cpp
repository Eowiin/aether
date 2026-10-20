#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "component_pool.hpp"

struct Position {
    float x;
    float y;
};

// Composant dont la copie lève une exception quand throw_on_copy est activé
struct ThrowingComponent {
    int value;

    static inline bool throw_on_copy = false;

    explicit ThrowingComponent(int v) : value(v) {}

    ThrowingComponent(const ThrowingComponent& other) : value(other.value) {
        if (throw_on_copy) {
            throw std::runtime_error("Copy failed");
        }
    }

    ThrowingComponent& operator=(const ThrowingComponent&) = default;
};

TEST_CASE("New pool is empty") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};

    CHECK(pool.size() == 0);
    CHECK_FALSE(pool.contains(e));
}

TEST_CASE("Emplace increases size by one") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};
    pool.emplace(e, Position{1.0f, 2.0f});

    CHECK(pool.size() == 1);
}

TEST_CASE("Emplaced component keeps its values") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};
    pool.emplace(e, Position{1.5f, -3.0f});

    REQUIRE(pool.contains(e));

    Position p = pool.get(e);

    CHECK(p.x == 1.5f);
    CHECK(p.y == -3.0f);
}

TEST_CASE("Contains returns true for present entity") {
    ComponentPool<Position> pool;

    entity_t e{3, 0};
    pool.emplace(e, Position{0.0f, 0.0f});

    CHECK(pool.contains(e));
}

TEST_CASE("Contains returns false for absent entity") {
    ComponentPool<Position> pool;

    entity_t present{0, 0};
    entity_t absent_below{1, 0};
    entity_t absent_above{5, 0};

    pool.emplace(present, Position{0.0f, 0.0f});
    pool.emplace(entity_t{2, 0}, Position{0.0f, 0.0f});

    CHECK_FALSE(pool.contains(absent_below));
    CHECK_FALSE(pool.contains(absent_above));
}

TEST_CASE("Get returns the right component after several insertions") {
    ComponentPool<Position> pool;

    entity_t a{0, 0};
    entity_t b{1, 0};
    entity_t c{2, 0};

    pool.emplace(a, Position{1.0f, 10.0f});
    pool.emplace(b, Position{2.0f, 20.0f});
    pool.emplace(c, Position{3.0f, 30.0f});

    CHECK(pool.get(a).x == 1.0f);
    CHECK(pool.get(a).y == 10.0f);
    CHECK(pool.get(b).x == 2.0f);
    CHECK(pool.get(b).y == 20.0f);
    CHECK(pool.get(c).x == 3.0f);
    CHECK(pool.get(c).y == 30.0f);
}

TEST_CASE("Different entities do not share a component") {
    ComponentPool<Position> pool;

    // Insertion order differs from index order on purpose
    entity_t a{7, 0};
    entity_t b{2, 0};

    pool.emplace(a, Position{7.0f, 7.0f});
    pool.emplace(b, Position{2.0f, 2.0f});

    Position pa = pool.get(a);
    Position pb = pool.get(b);

    CHECK(pa.x == 7.0f);
    CHECK(pb.x == 2.0f);
    CHECK(pa.x != pb.x);
}

TEST_CASE("Duplicate insertion is rejected") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};

    pool.emplace(e, Position{1.0f, 1.0f});

    CHECK_THROWS_AS(pool.emplace(e, Position{9.0f, 9.0f}), std::logic_error);
    CHECK(pool.size() == 1);
    CHECK(pool.get(e).x == 1.0f);
    CHECK(pool.get(e).y == 1.0f);
}

TEST_CASE("Right index with wrong generation is rejected") {
    ComponentPool<Position> pool;

    entity_t current{4, 1};
    entity_t stale{4, 0};
    entity_t future{4, 2};

    pool.emplace(current, Position{0.0f, 0.0f});

    CHECK(pool.contains(current));
    CHECK_FALSE(pool.contains(stale));
    CHECK_FALSE(pool.contains(future));
}

TEST_CASE("Out of bounds index lookup is safe") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};
    entity_t just_past{1, 0};
    entity_t far_away{9999, 0};

    pool.emplace(e, Position{0.0f, 0.0f});

    CHECK_NOTHROW(pool.contains(just_past));
    CHECK_FALSE(pool.contains(just_past));
    CHECK_NOTHROW(pool.contains(far_away));
    CHECK_FALSE(pool.contains(far_away));

    ComponentPool<Position> empty;

    CHECK_NOTHROW(empty.contains(e));
    CHECK_FALSE(empty.contains(e));
}

TEST_CASE("Insertion on an index owned by another generation is rejected") {
    ComponentPool<Position> pool;

    entity_t old{3, 0};
    entity_t current{3, 1};

    pool.emplace(old, Position{1.0f, 1.0f});

    CHECK_THROWS_AS(pool.emplace(current, Position{2.0f, 2.0f}), std::logic_error);
    CHECK(pool.size() == 1);
    CHECK(pool.contains(old));
    CHECK_FALSE(pool.contains(current));
    CHECK(pool.get(old).x == 1.0f);
}

TEST_CASE("Many insertions keep every association correct") {
    constexpr uint32_t entity_count = 10'000;

    ComponentPool<Position> pool;
    std::vector<entity_t> entities;
    entities.reserve(entity_count);

    // Reverse order so the sparse array grows from the largest index first
    for (uint32_t i = entity_count; i-- > 0;) {
        entity_t e{i, i % 3};
        pool.emplace(e, Position{float(i), float(i) * 2.0f});
        entities.push_back(e);
    }

    REQUIRE(pool.size() == entity_count);

    for (entity_t& e : entities) {
        REQUIRE(pool.contains(e));

        Position p = pool.get(e);

        CHECK(p.x == float(e.index));
        CHECK(p.y == float(e.index) * 2.0f);
    }
}

TEST_CASE("Component can be modified through get()") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};
    pool.emplace(e, Position{1.0f, 2.0f});

    pool.get(e).x = 42.0f;

    CHECK(pool.get(e).x == 42.0f);
    CHECK(pool.get(e).y == 2.0f);
}

TEST_CASE("try_get returns a usable pointer for a present component") {
    ComponentPool<Position> pool;

    entity_t e{2, 0};
    pool.emplace(e, Position{1.0f, 2.0f});

    Position* p = pool.try_get(e);

    REQUIRE(p != nullptr);
    CHECK(p->x == 1.0f);

    p->x = 7.0f;

    CHECK(pool.get(e).x == 7.0f);
}

TEST_CASE("try_get returns nullptr for a stale generation") {
    ComponentPool<Position> pool;

    entity_t current{4, 1};
    entity_t stale{4, 0};

    pool.emplace(current, Position{0.0f, 0.0f});

    CHECK(pool.try_get(stale) == nullptr);
    CHECK(pool.try_get(current) != nullptr);
}

TEST_CASE("try_get returns nullptr for an absent index") {
    ComponentPool<Position> pool;

    entity_t present{5, 0};
    entity_t unoccupied{2, 0};
    entity_t past_end{6, 0};
    entity_t far_away{9999, 0};

    CHECK(pool.try_get(present) == nullptr);

    pool.emplace(present, Position{0.0f, 0.0f});

    // Index couvert par le tableau sparse mais jamais occupé
    CHECK(pool.try_get(unoccupied) == nullptr);
    CHECK(pool.try_get(past_end) == nullptr);
    CHECK(pool.try_get(far_away) == nullptr);
}

TEST_CASE("Const pool only gives read-only access") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};
    pool.emplace(e, Position{3.0f, 4.0f});

    const ComponentPool<Position>& cpool = pool;

    // Vérifié à la compilation : les surcharges const renvoient des types const
    STATIC_CHECK(std::is_same_v<decltype(cpool.get(e)), const Position&>);
    STATIC_CHECK(std::is_same_v<decltype(cpool.try_get(e)), const Position*>);

    CHECK(cpool.get(e).x == 3.0f);
    REQUIRE(cpool.try_get(e) != nullptr);
    CHECK(cpool.try_get(e)->y == 4.0f);
}

TEST_CASE("Failed insertion leaves the pool unchanged") {
    ComponentPool<ThrowingComponent> pool;

    entity_t first{0, 0};
    entity_t failed{3, 0};

    pool.emplace(first, ThrowingComponent{1});

    ThrowingComponent::throw_on_copy = true;
    CHECK_THROWS_AS(pool.emplace(failed, ThrowingComponent{2}), std::runtime_error);
    ThrowingComponent::throw_on_copy = false;

    CHECK(pool.size() == 1);
    CHECK_FALSE(pool.contains(failed));
    CHECK(pool.try_get(failed) == nullptr);
    REQUIRE(pool.contains(first));
    CHECK(pool.get(first).value == 1);

    // L'index doit rester libre : une nouvelle insertion doit réussir
    pool.emplace(failed, ThrowingComponent{2});

    CHECK(pool.size() == 2);
    CHECK(pool.get(failed).value == 2);
    CHECK(pool.get(first).value == 1);
}

TEST_CASE("get and try_get refer to the same component") {
    ComponentPool<Position> pool;

    std::vector<entity_t> entities{{4, 0}, {0, 1}, {9, 2}, {2, 0}};

    for (const entity_t& e : entities) {
        pool.emplace(e, Position{float(e.index), 0.0f});
    }

    for (const entity_t& e : entities) {
        Position* p = pool.try_get(e);

        REQUIRE(p != nullptr);
        CHECK(&pool.get(e) == p);
    }
}
