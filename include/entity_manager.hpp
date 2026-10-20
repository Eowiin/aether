#ifndef ENTITY_MANAGER_HPP
#define ENTITY_MANAGER_HPP

#include <vector>
#include <deque>
#include <limits>
#include <stdexcept>
#include "entity.hpp"

class EntityManager {
    private:
        std::vector<entity_t> entities;
        std::vector<uint32_t> free_list;
        std::vector<bool> alive;
    public:
        EntityManager(/* args */);
        ~EntityManager();
        entity_t createEntity();
        void destroyEntity(entity_t entity);
        bool isAlive(const entity_t &entity) const;
};

#endif
