#ifndef COMPONENT_POOL_HPP
#define COMPONENT_POOL_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <cassert>
#include "icomponent_pool.hpp"
#include "entity.hpp"

// Exception-safety / type requirements:
//  - erase() requires a noexcept move assignment and destructor.
//  - emplace() requires a noexcept move constructor, or a copy constructor:
//    std::vector may relocate elements when it grows, and a throwing move of a
//    non-copyable type would leave the pool unable to roll back (no strong
//    guarantee). Copyable types keep the strong guarantee via copy relocation.
//    These constraints may be refined during the optimisation phase.
template<typename Component>
class ComponentPool final : public IComponentPool
{
private:
    std::vector<uint32_t> sparse;
    std::vector<entity_t> dense_entities;
    std::vector<Component> dense_components;
public:
    ComponentPool() = default;
    template<typename... Args>
    void emplace(entity_t entity, Args&&... args);
    bool contains(entity_t entity) const noexcept override;
    Component& get(entity_t entity) noexcept;
    const Component& get(entity_t entity) const noexcept;
    Component* try_get(entity_t entity) noexcept;
    const Component* try_get(entity_t entity) const noexcept;
    std::size_t size() const noexcept override;
    bool erase(entity_t entity) noexcept override;
};

template<typename Component>
template<typename... Args>
void ComponentPool<Component>::emplace(entity_t entity, Args&&... args) {
    static_assert(std::is_nothrow_move_constructible_v<Component> ||
                  std::is_copy_constructible_v<Component>,
                  "emplace() requires a noexcept move constructor or a copy constructor");
    if (entity.index == UINT32_MAX) {
        throw std::out_of_range("Invalid entity");
    }
    if (entity.index < this->sparse.size() && this->sparse.at(entity.index) != UINT32_MAX) {
        throw std::logic_error("Component already exists");
    }

    const std::size_t pos = this->dense_entities.size();

    if (pos >= UINT32_MAX) {
        throw std::overflow_error("Pool is full");
    }

    try {
        this->dense_components.emplace_back(std::forward<Args>(args)...);
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
bool ComponentPool<Component>::contains(entity_t entity) const noexcept {
    if (entity.index >= this->sparse.size()) { 
        return false;
    }
    const uint32_t pos = this->sparse[entity.index];

    return pos != UINT32_MAX && this->dense_entities[pos] == entity;
}

template<typename Component>
Component& ComponentPool<Component>::get(entity_t entity) noexcept {
    assert(this->contains(entity) && "Component does not exist");
    return this->dense_components[this->sparse[entity.index]];
}

template<typename Component>
const Component& ComponentPool<Component>::get(entity_t entity) const noexcept {
    assert(this->contains(entity) && "Component does not exist");
    return this->dense_components[this->sparse[entity.index]];
}

template<typename Component>
Component* ComponentPool<Component>::try_get(entity_t entity) noexcept {
    if (this->contains(entity)) {
        return &this->dense_components[this->sparse[entity.index]];
    }
    return nullptr;
}

template<typename Component>
const Component* ComponentPool<Component>::try_get(entity_t entity) const noexcept {
    if (this->contains(entity)) {
        return &this->dense_components[this->sparse[entity.index]];
    }
    return nullptr;
}

template<typename Component>
std::size_t ComponentPool<Component>::size() const noexcept {
    return this->dense_components.size();
}

template<typename Component>
bool ComponentPool<Component>::erase(entity_t entity) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<Component>,
                  "erase() requires a noexcept move assignment");
    static_assert(std::is_nothrow_destructible_v<Component>,
                  "erase() requires a noexcept destructor");

    if (!this->contains(entity)) {
        return false;
    }
    const uint32_t pos = this->sparse[entity.index];
    const uint32_t pos_back = static_cast<uint32_t>(this->dense_entities.size() - 1);

    if (pos != pos_back) {
        const entity_t back = this->dense_entities[pos_back];
        this->dense_components[pos] = std::move(this->dense_components[pos_back]);
        this->dense_entities[pos] = back;
        this->sparse[back.index] = pos;
    }

    this->sparse[entity.index] = UINT32_MAX;
    this->dense_components.pop_back();
    this->dense_entities.pop_back();
    return true;
}

#endif
