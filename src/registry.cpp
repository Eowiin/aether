#include "registry.hpp"

void Registry::destroy(entity_t entity) {
    if (this->e_manager.isAlive(entity)) {
        for (auto& [type, p] : this->pools) {
            p->erase(entity);
        }
        e_manager.destroyEntity(entity);
    }
}

entity_t Registry::create() {
    return this->e_manager.createEntity();
}
