#ifndef COMPONENT_POOL_HPP
#define COMPONENT_POOL_HPP

#include <vector>
#include <stdexcept>
#include "entity.hpp"

template<typename Component>
class ComponentPool
{
private:
    std::vector<uint32_t> sparse;
    std::vector<entity_t> dense_entities;
    std::vector<Component> dense_components;
public:
    ComponentPool(/* args */);
    ~ComponentPool();
    void emplace(entity_t entity, Component comp);
    bool contains(entity_t entity) const;
    Component& get(entity_t entity);
    const Component& get(entity_t entity) const;

    Component* try_get(entity_t entity);
    const Component* try_get(entity_t entity) const;
    size_t size() const;
};

template<typename Component>
ComponentPool<Component>::ComponentPool(/* args */)
{
}

template<typename Component>
ComponentPool<Component>::~ComponentPool()
{
}

template<typename Component>
void ComponentPool<Component>::emplace(entity_t entity, Component comp) {
    if (entity.index == UINT32_MAX) {
        throw std::out_of_range("Invalid entity");
    }
    if (entity.index < this->sparse.size() && this->sparse.at(entity.index) != UINT32_MAX) {
        throw std::logic_error("Component already exists");
    }

    const size_t pos = this->dense_entities.size();

    if (pos >= UINT32_MAX) {
        throw std::overflow_error("Pool is full");
    }

    try {
        this->dense_components.push_back(comp);
        this->dense_entities.push_back(entity);
        if (entity.index >= this->sparse.size()) {
            this->sparse.resize(entity.index + 1, UINT32_MAX);
        }
        this->sparse[entity.index] = this->dense_components.size() - 1;
    } catch (...) {
        if (this->dense_components.size() > pos) {
            this->dense_components.pop_back();
        }
        if (this->dense_entities.size() > pos) {
            this->dense_entities.pop_back();
        }
        throw;
    }
}

template<typename Component>
bool ComponentPool<Component>::contains(entity_t entity) const {
    if (entity.index >= this->sparse.size()) { 
        return false;
    }
    const uint32_t pos = this->sparse[entity.index];

    return pos != UINT32_MAX && this->dense_entities[pos] == entity;
}

template<typename Component>
Component& ComponentPool<Component>::get(entity_t entity) {
    assert(contains(entity) && "Component does not exist");
    return this->dense_components[this->sparse[entity.index]];
}

template<typename Component>
const Component& ComponentPool<Component>::get(entity_t entity) const {
    assert(contains(entity) && "Component does not exist");
    return this->dense_components[this->sparse[entity.index]];
}

template<typename Component>
Component* ComponentPool<Component>::try_get(entity_t entity) {
    if (this->contains(entity)) {
        return &this->dense_components[this->sparse[entity.index]];
    }
    return nullptr;
}

template<typename Component>
const Component* ComponentPool<Component>::try_get(entity_t entity) const {
    if (this->contains(entity)) {
        return &this->dense_components[this->sparse[entity.index]];
    }
    return nullptr;
}

template<typename Component>
size_t ComponentPool<Component>::size() const {
    return this->dense_components.size();
}

#endif
