#ifndef VIEW_HPP
#define VIEW_HPP

#include <functional>
#include <typeindex>
#include <vector>
#include <tuple>
#include "icomponent_pool.hpp"
#include "component_pool.hpp"
#include "entity.hpp"

template<typename... Components>
class View {
    private:
        std::tuple<ComponentPool<Components>*...> pools;

        IComponentPool* get_smallest();
        bool has(const entity_t &entity);

    public:
        View(ComponentPool<Components>&... pools);
        template<typename Callback>
        void each(Callback&& callback);
};

template<typename... Components>
View<Components...>::View(ComponentPool<Components>&... pools) : pools(&pools...) {}

template<typename... Components>
IComponentPool* View<Components...>::get_smallest() {
    IComponentPool* smallest = nullptr;

    std::apply([&](auto*... pool) {
        ((smallest == nullptr || pool->size() < smallest->size() ? smallest = pool : smallest), ...);
    }, this->pools);
    return smallest;
}

template<typename... Components>
bool View<Components...>::has(const entity_t &entity) {
    return (std::get<ComponentPool<Components>*>(this->pools)->contains(entity) && ...);
}

template<typename... Components>
template<typename Callback>
void View<Components...>::each(Callback&& callback) {
    const auto* smallest = this->get_smallest();

    if (smallest) {
        for (std::size_t i = 0; i < smallest->size(); i++) {
            const entity_t entity = smallest->entity_at(i);
            if (this->has(entity)) {
                std::invoke(callback, entity, std::get<ComponentPool<Components>*>(this->pools)->get(entity)...);
            }
        }
    }
}

#endif
