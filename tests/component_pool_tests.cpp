#include <catch2/catch_test_macros.hpp>
#include <memory>
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

TEST_CASE("Erasing the only component empties the pool") {
    ComponentPool<Position> pool;

    entity_t e{0, 0};
    pool.emplace(e, Position{1.0f, 2.0f});

    CHECK(pool.erase(e));
    CHECK(pool.size() == 0);
    CHECK_FALSE(pool.contains(e));
    CHECK(pool.try_get(e) == nullptr);
}

TEST_CASE("Erasing the last component keeps the others intact") {
    ComponentPool<Position> pool;

    entity_t a{0, 0};
    entity_t b{1, 0};
    entity_t c{2, 0};

    pool.emplace(a, Position{1.0f, 10.0f});
    pool.emplace(b, Position{2.0f, 20.0f});
    pool.emplace(c, Position{3.0f, 30.0f});

    CHECK(pool.erase(c));
    CHECK(pool.size() == 2);
    CHECK_FALSE(pool.contains(c));

    REQUIRE(pool.contains(a));
    REQUIRE(pool.contains(b));
    CHECK(pool.get(a).x == 1.0f);
    CHECK(pool.get(a).y == 10.0f);
    CHECK(pool.get(b).x == 2.0f);
    CHECK(pool.get(b).y == 20.0f);
}

TEST_CASE("Erasing a middle component keeps the others intact") {
    ComponentPool<Position> pool;

    entity_t a{0, 0};
    entity_t b{1, 0};
    entity_t c{2, 0};

    pool.emplace(a, Position{1.0f, 10.0f});
    pool.emplace(b, Position{2.0f, 20.0f});
    pool.emplace(c, Position{3.0f, 30.0f});

    CHECK(pool.erase(b));
    CHECK(pool.size() == 2);
    CHECK_FALSE(pool.contains(b));

    REQUIRE(pool.contains(a));
    REQUIRE(pool.contains(c));
    CHECK(pool.get(a).x == 1.0f);
    CHECK(pool.get(c).x == 3.0f);
    CHECK(pool.get(c).y == 30.0f);

    // Réutilise la case libérée en fin de tableau dense : le composant
    // déplacé doit rester accessible et distinct du nouveau.
    entity_t d{3, 0};
    pool.emplace(d, Position{4.0f, 40.0f});

    REQUIRE(pool.contains(c));
    REQUIRE(pool.contains(d));
    CHECK(pool.get(c).x == 3.0f);
    CHECK(pool.get(d).x == 4.0f);
    CHECK(&pool.get(c) != &pool.get(d));
}

TEST_CASE("Erasing an absent component returns false") {
    ComponentPool<Position> pool;

    entity_t present{2, 0};
    entity_t unoccupied{0, 0};
    entity_t far_away{9999, 0};

    CHECK_FALSE(pool.erase(present));

    pool.emplace(present, Position{1.0f, 1.0f});

    CHECK_FALSE(pool.erase(unoccupied));
    CHECK_FALSE(pool.erase(far_away));
    CHECK(pool.size() == 1);
    REQUIRE(pool.contains(present));
    CHECK(pool.get(present).x == 1.0f);
}

TEST_CASE("Erasing with a wrong generation is rejected") {
    ComponentPool<Position> pool;

    entity_t current{4, 1};
    entity_t stale{4, 0};
    entity_t future{4, 2};

    pool.emplace(current, Position{5.0f, 5.0f});

    CHECK_FALSE(pool.erase(stale));
    CHECK_FALSE(pool.erase(future));
    CHECK(pool.size() == 1);
    REQUIRE(pool.contains(current));
    CHECK(pool.get(current).x == 5.0f);
}

TEST_CASE("Erasing the same entity twice does not corrupt the pool") {
    ComponentPool<Position> pool;

    entity_t a{0, 0};
    entity_t b{1, 0};
    entity_t c{2, 0};

    pool.emplace(a, Position{1.0f, 0.0f});
    pool.emplace(b, Position{2.0f, 0.0f});
    pool.emplace(c, Position{3.0f, 0.0f});

    CHECK(pool.erase(a));
    CHECK_FALSE(pool.erase(a));
    CHECK(pool.size() == 2);

    REQUIRE(pool.contains(b));
    REQUIRE(pool.contains(c));
    CHECK(pool.get(b).x == 2.0f);
    CHECK(pool.get(c).x == 3.0f);
}

TEST_CASE("Erased index can be reused with a new generation") {
    ComponentPool<Position> pool;

    entity_t old{3, 0};
    entity_t other{5, 0};
    entity_t recycled{3, 1};

    pool.emplace(old, Position{1.0f, 1.0f});
    pool.emplace(other, Position{2.0f, 2.0f});

    REQUIRE(pool.erase(old));

    CHECK_NOTHROW(pool.emplace(recycled, Position{9.0f, 9.0f}));
    CHECK(pool.size() == 2);
    CHECK_FALSE(pool.contains(old));
    CHECK(pool.try_get(old) == nullptr);
    CHECK_FALSE(pool.erase(old));

    REQUIRE(pool.contains(recycled));
    REQUIRE(pool.contains(other));
    CHECK(pool.get(recycled).x == 9.0f);
    CHECK(pool.get(other).x == 2.0f);
}

TEST_CASE("Successive insertions and erasures keep every association correct") {
    constexpr uint32_t entity_count = 1'000;

    ComponentPool<Position> pool;
    std::vector<entity_t> alive;

    auto check_all = [&]() {
        REQUIRE(pool.size() == alive.size());
        for (const entity_t& e : alive) {
            REQUIRE(pool.contains(e));
            CHECK(pool.get(e).x == float(e.index));
            CHECK(pool.get(e).y == float(e.generation));
        }
    };

    for (uint32_t i = 0; i < entity_count; ++i) {
        entity_t e{i, 0};
        pool.emplace(e, Position{float(i), 0.0f});
        alive.push_back(e);
    }
    check_all();

    // Supprime une entité sur trois, en partant du début pour forcer
    // des déplacements depuis la fin du tableau dense.
    std::vector<entity_t> erased;
    std::vector<entity_t> kept;
    for (const entity_t& e : alive) {
        if (e.index % 3 == 0) {
            REQUIRE(pool.erase(e));
            erased.push_back(e);
        } else {
            kept.push_back(e);
        }
    }
    alive = kept;
    check_all();

    for (const entity_t& e : erased) {
        CHECK_FALSE(pool.contains(e));
    }

    // Réinsère les index libérés avec une nouvelle génération.
    for (const entity_t& e : erased) {
        entity_t recycled{e.index, e.generation + 1};
        pool.emplace(recycled, Position{float(recycled.index), float(recycled.generation)});
        alive.push_back(recycled);
    }
    check_all();

    for (const entity_t& e : erased) {
        CHECK_FALSE(pool.contains(e));
    }

    // Vide entièrement le pool dans un ordre mélangé.
    for (size_t i = 0; i < alive.size(); i += 2) {
        REQUIRE(pool.erase(alive[i]));
    }
    for (size_t i = 1; i < alive.size(); i += 2) {
        REQUIRE(pool.erase(alive[i]));
    }
    CHECK(pool.size() == 0);
    for (const entity_t& e : alive) {
        CHECK_FALSE(pool.contains(e));
    }
}

struct Tracked {
    int value;

    static inline int copies = 0;
    static inline int move_constructions = 0;
    static inline int move_assignments = 0;
    static inline int destructions = 0;
    static inline std::vector<int> destroyed_values;

    static void reset() {
        copies = 0;
        move_constructions = 0;
        move_assignments = 0;
        destructions = 0;
        destroyed_values.clear();
    }

    explicit Tracked(int v) : value(v) {}

    Tracked(const Tracked& other) : value(other.value) { ++copies; }

    Tracked(Tracked&& other) noexcept : value(other.value) {
        other.value = -1;
        ++move_constructions;
    }

    Tracked& operator=(const Tracked& other) {
        value = other.value;
        ++copies;
        return *this;
    }

    Tracked& operator=(Tracked&& other) noexcept {
        value = other.value;
        other.value = -1;
        ++move_assignments;
        return *this;
    }

    ~Tracked() {
        ++destructions;
        destroyed_values.push_back(value);
    }
};

TEST_CASE("Erase moves the last component and destroys only its moved-from shell") {
    STATIC_CHECK(std::is_nothrow_move_assignable_v<Tracked>);

    ComponentPool<Tracked> pool;

    entity_t a{0, 0};
    entity_t b{1, 0};
    entity_t c{2, 0};

    pool.emplace(a, Tracked{1});
    pool.emplace(b, Tracked{2});
    pool.emplace(c, Tracked{3});

    SECTION("erasing a middle component") {
        Tracked::reset();

        REQUIRE(pool.erase(b));

        CHECK(Tracked::copies == 0);
        CHECK(Tracked::move_constructions == 0);
        CHECK(Tracked::move_assignments == 1);
        CHECK(Tracked::destructions == 1);
        REQUIRE(Tracked::destroyed_values.size() == 1);
        CHECK(Tracked::destroyed_values[0] == -1);

        REQUIRE(pool.contains(a));
        REQUIRE(pool.contains(c));
        CHECK(pool.get(a).value == 1);
        CHECK(pool.get(c).value == 3);
    }

    SECTION("erasing the last component") {
        Tracked::reset();

        REQUIRE(pool.erase(c));

        CHECK(Tracked::copies == 0);
        CHECK(Tracked::move_constructions == 0);
        CHECK(Tracked::move_assignments == 0);
        CHECK(Tracked::destructions == 1);
        REQUIRE(Tracked::destroyed_values.size() == 1);
        CHECK(Tracked::destroyed_values[0] == 3);
    }

    SECTION("erasing an absent component touches nothing") {
        Tracked::reset();

        CHECK_FALSE(pool.erase(entity_t{1, 1}));

        CHECK(Tracked::copies == 0);
        CHECK(Tracked::move_assignments == 0);
        CHECK(Tracked::destructions == 0);
    }
}

// Composant qui compte ses constructions par copie et par déplacement
struct CountingComponent {
    int value;

    static inline int copy_constructions = 0;
    static inline int move_constructions = 0;

    static void reset() {
        copy_constructions = 0;
        move_constructions = 0;
    }

    explicit CountingComponent(int v) : value(v) {}

    CountingComponent(const CountingComponent& other) : value(other.value) {
        ++copy_constructions;
    }

    CountingComponent(CountingComponent&& other) noexcept : value(other.value) {
        other.value = -1;
        ++move_constructions;
    }

    CountingComponent& operator=(const CountingComponent&) = default;
    CountingComponent& operator=(CountingComponent&&) noexcept = default;
};

TEST_CASE("Inserting a temporary never copies the component") {
    ComponentPool<CountingComponent> pool;
    entity_t e{0, 0};

    CountingComponent::reset();
    pool.emplace(e, CountingComponent{42});

    // Le temporaire est déplacé une seule fois dans le tableau dense.
    CHECK(CountingComponent::copy_constructions == 0);
    CHECK(CountingComponent::move_constructions == 1);

    REQUIRE(pool.contains(e));
    CHECK(pool.get(e).value == 42);
}

TEST_CASE("Inserting an existing object through std::move never copies it") {
    ComponentPool<CountingComponent> pool;
    entity_t e{0, 0};

    CountingComponent source{7};

    CountingComponent::reset();
    pool.emplace(e, std::move(source));

    // emplace transmet l'argument tel quel : un seul déplacement vers le tableau dense.
    CHECK(CountingComponent::copy_constructions == 0);
    CHECK(CountingComponent::move_constructions == 1);

    CHECK(source.value == -1);
    REQUIRE(pool.contains(e));
    CHECK(pool.get(e).value == 7);
}

TEST_CASE("Inserting an existing object by lvalue copies it once") {
    ComponentPool<CountingComponent> pool;
    entity_t e{0, 0};

    CountingComponent source{7};

    CountingComponent::reset();
    pool.emplace(e, source);

    // Une seule copie directement dans le tableau dense, aucun déplacement.
    CHECK(CountingComponent::copy_constructions == 1);
    CHECK(CountingComponent::move_constructions == 0);

    CHECK(source.value == 7);
    REQUIRE(pool.contains(e));
    CHECK(pool.get(e).value == 7);
}

struct Pair {
    int a;
    int b;

    static inline int copy_constructions = 0;
    static inline int move_constructions = 0;

    Pair(int a_, int b_) : a(a_), b(b_) {}
    Pair(const Pair& o) : a(o.a), b(o.b) { ++copy_constructions; }
    Pair(Pair&& o) noexcept : a(o.a), b(o.b) { ++move_constructions; }
    Pair& operator=(Pair&&) noexcept = default;
};

TEST_CASE("Emplace with constructor arguments builds in place") {
    ComponentPool<Pair> pool;
    entity_t e{0, 0};

    Pair::copy_constructions = 0;
    Pair::move_constructions = 0;
    pool.emplace(e, 3, 4);

    // Le tableau dense peut se réallouer mais ici il est vide : aucune copie ni déplacement.
    CHECK(Pair::copy_constructions == 0);
    CHECK(Pair::move_constructions == 0);
    REQUIRE(pool.contains(e));
    CHECK(pool.get(e).a == 3);
    CHECK(pool.get(e).b == 4);
}

TEST_CASE("Emplace without arguments value-initializes the component") {
    ComponentPool<Position> pool;
    entity_t e{0, 0};

    pool.emplace(e);

    REQUIRE(pool.contains(e));
    CHECK(pool.get(e).x == 0.0f);
    CHECK(pool.get(e).y == 0.0f);
}

TEST_CASE("Copied pool is independent from the original") {
    ComponentPool<Position> original;
    entity_t a{0, 0};
    entity_t b{5, 1};

    original.emplace(a, Position{1.0f, 2.0f});
    original.emplace(b, Position{3.0f, 4.0f});

    ComponentPool<Position> copy(original);

    REQUIRE(copy.size() == 2);
    REQUIRE(copy.contains(a));
    REQUIRE(copy.contains(b));
    CHECK(copy.get(a).x == 1.0f);
    CHECK(copy.get(b).y == 4.0f);
    CHECK(&copy.get(a) != &original.get(a));

    copy.get(a).x = 99.0f;
    CHECK(copy.erase(b));

    CHECK(original.get(a).x == 1.0f);
    CHECK(original.contains(b));
    CHECK(original.size() == 2);
}

TEST_CASE("Copy assignment replaces the previous content") {
    ComponentPool<Position> source;
    ComponentPool<Position> target;
    entity_t a{1, 0};
    entity_t old{7, 0};

    source.emplace(a, Position{1.0f, 1.0f});
    target.emplace(old, Position{9.0f, 9.0f});

    target = source;

    CHECK(target.size() == 1);
    CHECK(target.contains(a));
    CHECK_FALSE(target.contains(old));
    CHECK(source.size() == 1);
    CHECK(source.contains(a));
}

TEST_CASE("Moved pool transfers its components without copying them") {
    static_assert(std::is_nothrow_move_constructible_v<ComponentPool<CountingComponent>>);

    ComponentPool<CountingComponent> original;
    entity_t a{0, 0};
    entity_t b{3, 0};

    original.emplace(a, 10);
    original.emplace(b, 20);

    CountingComponent::reset();
    ComponentPool<CountingComponent> moved(std::move(original));

    CHECK(CountingComponent::copy_constructions == 0);
    CHECK(CountingComponent::move_constructions == 0);

    REQUIRE(moved.size() == 2);
    REQUIRE(moved.contains(a));
    REQUIRE(moved.contains(b));
    CHECK(moved.get(a).value == 10);
    CHECK(moved.get(b).value == 20);
}

TEST_CASE("Move assignment transfers the content and leaves a usable source") {
    ComponentPool<Position> source;
    ComponentPool<Position> target;
    entity_t a{2, 0};
    entity_t old{4, 0};

    source.emplace(a, Position{5.0f, 6.0f});
    target.emplace(old, Position{0.0f, 0.0f});

    target = std::move(source);

    CHECK(target.size() == 1);
    CHECK(target.contains(a));
    CHECK_FALSE(target.contains(old));
    CHECK(target.get(a).x == 5.0f);

    // La source déplacée doit rester valide et réutilisable
    source.emplace(old, Position{1.0f, 1.0f});
    CHECK(source.contains(old));
}

// Composant non copiable : ne peut exister que par construction directe ou déplacement
struct OwningComponent {
    std::unique_ptr<int> ptr;

    explicit OwningComponent(int v) : ptr(std::make_unique<int>(v)) {}
    explicit OwningComponent(std::unique_ptr<int> p) : ptr(std::move(p)) {}
};

TEST_CASE("Emplace constructs a move-only component in place") {
    STATIC_CHECK_FALSE(std::is_copy_constructible_v<OwningComponent>);

    ComponentPool<OwningComponent> pool;
    entity_t a{0, 0};
    entity_t b{1, 0};

    SECTION("from constructor arguments") {
        pool.emplace(a, 42);

        REQUIRE(pool.contains(a));
        REQUIRE(pool.get(a).ptr != nullptr);
        CHECK(*pool.get(a).ptr == 42);
    }

    SECTION("from a moved unique_ptr") {
        auto raw = std::make_unique<int>(7);
        int* address = raw.get();

        pool.emplace(a, std::move(raw));

        CHECK(raw == nullptr);
        REQUIRE(pool.contains(a));
        CHECK(pool.get(a).ptr.get() == address);
        CHECK(*pool.get(a).ptr == 7);
    }

    SECTION("pointers survive reallocation and erase") {
        pool.emplace(a, 1);
        pool.emplace(b, 2);
        for (uint32_t i = 2; i < 100; ++i) {
            pool.emplace(entity_t{i, 0}, int(i));
        }

        REQUIRE(pool.erase(a));

        REQUIRE(pool.contains(b));
        CHECK(*pool.get(b).ptr == 2);
        CHECK(*pool.get(entity_t{99, 0}).ptr == 99);
    }
}
