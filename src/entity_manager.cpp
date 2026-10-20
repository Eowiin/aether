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

        this->entities.push_back(entity_t{
            (uint32_t) this->entities.size(),
            0
        });
        this->alive.push_back(true);
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
