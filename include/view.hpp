#ifndef VIEW_HPP
#define VIEW_HPP

#include <functional>
#include <typeindex>
#include <vector>
#include <tuple>
#include <type_traits>
#include "icomponent_pool.hpp"
#include "component_pool.hpp"
#include "entity.hpp"

template<typename... Ts>
struct are_unique : std::true_type {};

template<typename T, typename... Rest>
struct are_unique<T, Rest...> : std::bool_constant<(!std::is_same_v<T, Rest> && ...) && are_unique<Rest...>::value> {};

// V1 : au moins un composant, aucun type const/volatile/référence, aucun doublon
// (std::get<T>() exige des types uniques)
template<typename... Components>
concept ViewComponents = sizeof...(Components) > 0
    && (std::is_same_v<Components, std::remove_cvref_t<Components>> && ...)
    && are_unique<Components...>::value;

template<typename... Components> requires ViewComponents<Components...>
class View {
    private:
        std::tuple<ComponentPool<Components>*...> pools;

        IComponentPool* get_smallest();
        bool has(const entity_t &entity);

    public:
        View(ComponentPool<Components>*... pools);
        template<typename Callback>
        void each(Callback&& callback);
};

template<typename... Components> requires ViewComponents<Components...>
View<Components...>::View(ComponentPool<Components>*... pools) : pools(pools...) {}

template<typename... Components> requires ViewComponents<Components...>
IComponentPool* View<Components...>::get_smallest() {
    return std::apply([](auto*... pool) -> IComponentPool* {
        // Un seul pool absent suffit à rendre l'intersection vide
        if ((!pool || ...)) {
            return nullptr;
        }

        IComponentPool* smallest = nullptr;
        ((smallest == nullptr || pool->size() < smallest->size() ? smallest = pool : smallest), ...);
        return smallest;
    }, this->pools);
}

template<typename... Components> requires ViewComponents<Components...>
bool View<Components...>::has(const entity_t &entity) {
    return std::apply([&](auto*... pool) {
        return ((pool && pool->contains(entity)) && ...);
    }, this->pools);
}

template<typename... Components> requires ViewComponents<Components...>
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
