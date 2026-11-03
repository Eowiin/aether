#ifndef ENTITY_MANAGER_HPP
#define ENTITY_MANAGER_HPP

#include <vector>
#include <deque>
#include <stdexcept>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include "ecs/entity.hpp"

// Capacity invariant: free_list.capacity() >= entities.size() after every
// createEntity(), and entities.size() == alive.size() at all times. Every
// index appears at most once in free_list and only while its entity is dead,
// so destroyEntity() always has room for push_back() and never allocates:
// it is noexcept. createEntity() does all its allocating (reserve) before it
// modifies anything.
//
// The invariant is about capacity, which copying a vector does not preserve,
// so copying is deleted. Moving transfers the buffers and keeps it; a
// moved-from manager must only be destroyed or assigned to.
class EntityManager {
    private:
        friend struct EntityManagerTestAccess;

        std::vector<entity_t> entities;
        std::vector<uint32_t> free_list;
        std::vector<bool> alive;
    public:
        EntityManager() = default;
        ~EntityManager() = default;
        EntityManager(const EntityManager&) = delete;
        EntityManager& operator=(const EntityManager&) = delete;
        EntityManager(EntityManager&&) noexcept = default;
        EntityManager& operator=(EntityManager&&) noexcept = default;
        entity_t createEntity();
        void destroyEntity(entity_t entity) noexcept;
        bool isAlive(entity_t entity) const noexcept;
};

#endif
