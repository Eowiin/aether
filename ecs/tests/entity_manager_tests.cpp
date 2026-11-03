#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <vector>
#include "ecs/entity_manager.hpp"

// Accès aux détails internes que les tests ne peuvent pas atteindre autrement
struct EntityManagerTestAccess {
    static std::size_t free_list_capacity(const EntityManager& m) { return m.free_list.capacity(); }
    static std::size_t entity_count(const EntityManager& m) { return m.entities.size(); }
    static void set_generation(EntityManager& m, uint32_t index, uint32_t generation) {
        m.entities[index].generation = generation;
    }
};

TEST_CASE("Create entity") {
    EntityManager manager;

    entity_t e = manager.createEntity();

    CHECK(e.index == 0);
    CHECK(e.generation == 0);
    CHECK(manager.isAlive(e));
}

TEST_CASE("Create multiple entities") {
    EntityManager manager;

    entity_t e1 = manager.createEntity();
    entity_t e2 = manager.createEntity();

    CHECK(e1.id() != e2.id());
}

TEST_CASE("Destroy entity") {
    EntityManager manager;

    entity_t e = manager.createEntity();

    manager.destroyEntity(e);

    CHECK_FALSE(manager.isAlive(e));
}

TEST_CASE("Reuse destroyed entity") {
    EntityManager manager;

    entity_t old = manager.createEntity();
    manager.destroyEntity(old);

    entity_t current = manager.createEntity();

    CHECK(current.index == old.index);
    CHECK(current.generation == old.generation + 1);

    CHECK_FALSE(manager.isAlive(old));
    CHECK(manager.isAlive(current));
}

// 1. Détruire deux fois la même entité
TEST_CASE("Double destruction") {
    EntityManager manager;

    entity_t e = manager.createEntity();

    manager.destroyEntity(e);
    manager.destroyEntity(e);

    entity_t reused = manager.createEntity();
    entity_t next = manager.createEntity();

    CHECK(reused.index == e.index);
    CHECK(reused.generation == e.generation + 1);

    // L'index ne doit pas avoir été ajouté
    // deux fois à la free list.
    CHECK(next.index != reused.index);

    CHECK(manager.isAlive(reused));
    CHECK(manager.isAlive(next));
}

// 2. Créer 100 entités
TEST_CASE("Create 100 entities") {
    EntityManager manager;
    std::vector<entity_t> entities;

    for (uint32_t i = 0; i < 100; i++) {
        entity_t e = manager.createEntity();

        CHECK(e.index == i);
        CHECK(e.generation == 0);
        CHECK(manager.isAlive(e));

        entities.push_back(e);
    }

    for (const entity_t& e : entities) {
        CHECK(manager.isAlive(e));
    }
}

// Créer 10 000 entités, en détruire la moitié, puis les recréer.
TEST_CASE("Create destroy and recreate 10000 entities") {
    constexpr uint32_t entity_count = 10'000;
    constexpr uint32_t destroy_count = entity_count / 2;

    EntityManager manager;
    std::vector<entity_t> entities;
    entities.reserve(entity_count);

    for (uint32_t i = 0; i < entity_count; ++i) {
        entities.push_back(manager.createEntity());
    }

    for (uint32_t i = 0; i < entity_count; ++i) {
        CHECK(manager.isAlive(entities[i]));
    }

    for (uint32_t i = 0; i < destroy_count; ++i) {
        manager.destroyEntity(entities[i]);
        CHECK_FALSE(manager.isAlive(entities[i]));
    }

    for (uint32_t i = 0; i < entity_count - destroy_count; ++i) {
        CHECK(manager.isAlive(entities[destroy_count + i]));
    }

    std::vector<entity_t> recreated_entities;
    recreated_entities.reserve(destroy_count);

    for (uint32_t i = 0; i < destroy_count; ++i) {
        recreated_entities.push_back(manager.createEntity());
    }

    for (uint32_t i = 0; i < destroy_count; ++i) {
        CHECK(recreated_entities[i].index == entities[destroy_count - 1 - i].index);
        CHECK(recreated_entities[i].generation == entities[destroy_count - 1 - i].generation + 1);
        CHECK_FALSE(manager.isAlive(entities[i]));
        CHECK(manager.isAlive(recreated_entities[i]));
    }
}

// 3. Détruire et recréer dans un ordre différent
TEST_CASE("Reuse entities in LIFO order") {
    EntityManager manager;

    entity_t a = manager.createEntity();
    entity_t b = manager.createEntity();
    entity_t c = manager.createEntity();
    entity_t d = manager.createEntity();

    // Destruction dans un ordre différent
    manager.destroyEntity(c);
    manager.destroyEntity(a);
    manager.destroyEntity(b);

    entity_t r1 = manager.createEntity();
    entity_t r2 = manager.createEntity();
    entity_t r3 = manager.createEntity();

    // La deque fonctionne en FIFO
    CHECK(r1.index == b.index);
    CHECK(r2.index == a.index);
    CHECK(r3.index == c.index);

    CHECK(r1.generation == b.generation + 1);
    CHECK(r2.generation == a.generation + 1);
    CHECK(r3.generation == c.generation + 1);

    CHECK(manager.isAlive(d));
    CHECK(manager.isAlive(r1));
    CHECK(manager.isAlive(r2));
    CHECK(manager.isAlive(r3));
}

// 4. Détruire une ancienne entité
TEST_CASE("Destroy stale entity") {
    EntityManager manager;

    entity_t old = manager.createEntity();

    manager.destroyEntity(old);

    entity_t current = manager.createEntity();

    REQUIRE(current.index == old.index);
    REQUIRE(current.generation != old.generation);

    // Tentative de destruction avec l'ancien ID
    manager.destroyEntity(old);

    // La nouvelle entité doit rester vivante
    CHECK(manager.isAlive(current));
    CHECK_FALSE(manager.isAlive(old));

    // L'index de current ne doit pas
    // avoir été remis dans la free list.
    entity_t next = manager.createEntity();

    CHECK(next.index != current.index);
    CHECK(manager.isAlive(next));
}

// 5. Identifiant avec un index inexistant
TEST_CASE("Invalid entity_t index") {
    EntityManager manager;

    entity_t valid = manager.createEntity();
    entity_t invalid{9999, 0};

    CHECK_FALSE(manager.isAlive(invalid));

    // Ne doit pas provoquer de crash
    manager.destroyEntity(invalid);

    // L'entité valide ne doit pas être affectée
    CHECK(manager.isAlive(valid));

    entity_t next = manager.createEntity();

    CHECK(next.index == 1);
    CHECK(manager.isAlive(next));
}

TEST_CASE("An entity with an exhausted generation is never recycled") {
    EntityManager manager;

    entity_t first = manager.createEntity();
    entity_t other = manager.createEntity();

    manager.destroyEntity(first);

    // Atteindre 2^32 recyclages est impraticable : on place la génération de
    // l'index libre juste avant la limite
    EntityManagerTestAccess::set_generation(manager, first.index, UINT32_MAX - 1);

    entity_t last = manager.createEntity();
    REQUIRE(last.index == first.index);
    REQUIRE(last.generation == UINT32_MAX);
    CHECK(manager.isAlive(last));

    manager.destroyEntity(last);
    CHECK_FALSE(manager.isAlive(last));

    // L'index est retiré : ni recyclé avec une génération qui reboucle à 0,
    // ni ressuscité pour d'anciens handles
    for (int i = 0; i < 5; ++i) {
        entity_t e = manager.createEntity();

        CHECK(e.index != first.index);
        CHECK_FALSE(manager.isAlive(first));
        CHECK_FALSE(manager.isAlive(last));
        CHECK_FALSE(manager.isAlive(entity_t{first.index, 0}));
        CHECK(manager.isAlive(e));
    }
    CHECK(manager.isAlive(other));

    // Détruire deux fois l'entité retirée reste sans effet
    manager.destroyEntity(last);
    CHECK_FALSE(manager.isAlive(last));
}

TEST_CASE("Free list capacity always covers every index") {
    EntityManager manager;
    std::vector<entity_t> alive;

    // Suite déterministe mêlant créations, destructions et recyclages
    uint32_t state = 12345;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return state >> 16;
    };

    for (int step = 0; step < 5000; ++step) {
        if (alive.empty() || next() % 3 != 0) {
            alive.push_back(manager.createEntity());

            CHECK(EntityManagerTestAccess::free_list_capacity(manager) >=
                  EntityManagerTestAccess::entity_count(manager));
        } else {
            const std::size_t pick = next() % alive.size();
            const std::size_t capacity_before = EntityManagerTestAccess::free_list_capacity(manager);

            manager.destroyEntity(alive[pick]);

            // Une destruction ordinaire ne réalloue pas la free list
            CHECK(EntityManagerTestAccess::free_list_capacity(manager) == capacity_before);
            CHECK_FALSE(manager.isAlive(alive[pick]));
            alive.erase(alive.begin() + std::ptrdiff_t(pick));
        }
    }

    for (const entity_t& e : alive) {
        CHECK(manager.isAlive(e));
    }
}

TEST_CASE("EntityManager destruction cannot throw and copying is forbidden") {
    STATIC_CHECK(noexcept(std::declval<EntityManager&>().destroyEntity(entity_t{0, 0})));
    STATIC_CHECK(noexcept(std::declval<const EntityManager&>().isAlive(entity_t{0, 0})));
    STATIC_CHECK_FALSE(std::is_copy_constructible_v<EntityManager>);
    STATIC_CHECK_FALSE(std::is_copy_assignable_v<EntityManager>);
    STATIC_CHECK(std::is_nothrow_move_constructible_v<EntityManager>);
    STATIC_CHECK(std::is_nothrow_move_assignable_v<EntityManager>);
}

TEST_CASE("A moved EntityManager keeps its entities and its capacity invariant") {
    EntityManager original;
    entity_t a = original.createEntity();
    entity_t b = original.createEntity();

    EntityManager moved(std::move(original));

    CHECK(moved.isAlive(a));
    CHECK(moved.isAlive(b));
    CHECK(EntityManagerTestAccess::free_list_capacity(moved) >=
          EntityManagerTestAccess::entity_count(moved));

    const std::size_t capacity_before = EntityManagerTestAccess::free_list_capacity(moved);
    moved.destroyEntity(a);
    moved.destroyEntity(b);
    CHECK(EntityManagerTestAccess::free_list_capacity(moved) == capacity_before);

    entity_t c = moved.createEntity();
    CHECK(c.index == b.index);
    CHECK(c.generation == b.generation + 1);
}
