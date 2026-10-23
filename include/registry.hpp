#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <unordered_map>
#include <typeindex>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <utility>
#include <stdexcept>
#include "icomponent_pool.hpp"
#include "component_pool.hpp"
#include "entity_manager.hpp"
#include "view.hpp"

class Registry {
    private:
        std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools;
        EntityManager e_manager;

        template<typename Component>
        ComponentPool<Component>& assure_pool();
        template<typename Component>
        ComponentPool<Component>* find_pool();
        template<typename Component>
        const ComponentPool<Component>* find_pool() const;

    public:
        void destroy(entity_t entity);
        entity_t create();
        template<typename Component, typename... Args>
        void emplace(entity_t entity, Args&&... args);
        template<typename Component>
        Component& get(entity_t entity) noexcept;
        template<typename Component>
        const Component& get(entity_t entity) const noexcept;
        template<typename Component>
        Component* try_get(entity_t entity) noexcept;
        template<typename Component>
        const Component* try_get(entity_t entity) const noexcept;
        template<typename Component>
        bool has(entity_t entity) const noexcept;
        template<typename... Components>
            requires ViewComponents<Components...>
        View<Components...> view();
};

template<typename Component>
ComponentPool<Component>& Registry::assure_pool() {
    std::type_index index = typeid(Component);

    auto it = this->pools.find(index);
    if (it == this->pools.end()) {
        it = this->pools.emplace(index, std::make_unique<ComponentPool<Component>>()).first;
    }
    return static_cast<ComponentPool<Component>&>(*it->second);
}

template<typename Component>
ComponentPool<Component>* Registry::find_pool() {
    auto it = this->pools.find(typeid(Component));

    return it == this->pools.end() ? nullptr : static_cast<ComponentPool<Component>*>(it->second.get());
}

template<typename Component>
const ComponentPool<Component>* Registry::find_pool() const {
    auto it = this->pools.find(typeid(Component));

    return it == this->pools.end() ? nullptr : static_cast<const ComponentPool<Component>*>(it->second.get());
}

template<typename Component, typename... Args>
void Registry::emplace(entity_t entity, Args&&... args) {
    if (this->e_manager.isAlive(entity)) {
        this->assure_pool<Component>().emplace(entity, std::forward<Args>(args)...);
    } else {
        throw std::invalid_argument("Invalid entity");
    }
}

template<typename Component>
Component& Registry::get(entity_t entity) noexcept {
    assert(this->has<Component>(entity) && "Component does not exist");
    return this->find_pool<Component>()->get(entity);
}

template<typename Component>
const Component& Registry::get(entity_t entity) const noexcept {
    assert(this->has<Component>(entity) && "Component does not exist");
    return this->find_pool<Component>()->get(entity);
}

template<typename Component>
Component* Registry::try_get(entity_t entity) noexcept {
    auto* pool = this->find_pool<Component>();

    return pool != nullptr ? pool->try_get(entity) : nullptr;
}

template<typename Component>
const Component* Registry::try_get(entity_t entity) const noexcept {
    const auto* pool = this->find_pool<Component>();

    return pool != nullptr ? pool->try_get(entity) : nullptr;
}

template<typename Component>
bool Registry::has(entity_t entity) const noexcept {
    const auto* pool = this->find_pool<Component>();

    return pool != nullptr && pool->contains(entity);
}

template<typename... Components> requires ViewComponents<Components...>
View<Components...> Registry::view() {
    return View(this->find_pool<Components>()...);
}

#endif
