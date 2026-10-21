#include "entity_manager.hpp"

EntityManager::EntityManager(/* args */)
{
}

EntityManager::~EntityManager()
{
}

entity_t EntityManager::createEntity() {
    if (!this->free_list.empty()) {
        uint32_t index = this->free_list.back();

        this->free_list.pop_back();
        this->entities[index] = entity_t{
            index,
            this->entities[index].generation + 1
        };
        this->alive[index] = true;
        return this->entities[index];
    } else {
        if (this->entities.size() >= UINT32_MAX) {
            throw std::overflow_error("No entity indices are available");
        }

        const std::size_t needed = this->entities.size() + 1;

        if (this->free_list.capacity() < needed) {
            const std::size_t cap = this->free_list.capacity();
            constexpr std::size_t limit = UINT32_MAX;

            const std::size_t grown = cap > limit / 2
                ? limit
                : std::max<std::size_t>(1, cap * 2);

            this->free_list.reserve(std::max(needed, grown));
        }

        this->entities.push_back(entity_t{
            static_cast<uint32_t>(this->entities.size()),
            0
        });
        try {
            this->alive.push_back(true);
        } catch (...) {
            this->entities.pop_back();
            throw;
        }
        return this->entities.back();
    }
}

void EntityManager::destroyEntity(entity_t entity) {
    if (this->isAlive(entity)) {
        this->alive[entity.index] = false;
        if (this->entities[entity.index].generation <= UINT32_MAX) {
            this->free_list.push_back(entity.index);
        }
    }
}

bool EntityManager::isAlive(entity_t entity) const {
    return entity.index < this->entities.size()
    && this->alive.at(entity.index)
    && this->entities[entity.index].generation == entity.generation;
}
