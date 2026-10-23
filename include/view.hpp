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

// IsConst : la View ne donne accès qu'en lecture aux composants (pools const)
//
// Contrat V1 (restriction volontaire, pas une limite des ECS en général) :
//   Opération                                  | Pendant each() | Entre deux each()
//   Lire un composant                          | autorisé       | autorisé
//   Modifier la valeur d'un composant mutable  | autorisé       | autorisé
//   Ajouter un composant                       | interdit       | autorisé
//   Supprimer un composant                     | interdit       | autorisé
//   Créer ou détruire une entité               | interdit       | autorisé
//   Détruire le Registry                       | interdit       | invalide les Views associées
// Les opérations interdites peuvent réorganiser les pools en cours de parcours :
// comportement indéfini, non détecté.
template<bool IsConst, typename... Components> requires ViewComponents<Components...>
class View {
    private:
        template<typename Component>
        using PoolPtr = std::conditional_t<IsConst, const ComponentPool<Component>*, ComponentPool<Component>*>;
        std::tuple<PoolPtr<Components>...> pools;

        const IComponentPool* get_smallest() const;
        bool has(const entity_t &entity) const;

    public:
        View(PoolPtr<Components>... pools);
        template<typename Callback>
        void each(Callback&& callback) const;
};

template<bool IsConst, typename... Components> requires ViewComponents<Components...>
View<IsConst, Components...>::View(PoolPtr<Components>... pools) : pools(pools...) {}

template<bool IsConst, typename... Components> requires ViewComponents<Components...>
const IComponentPool* View<IsConst, Components...>::get_smallest() const {
    return std::apply([](auto*... pool) -> const IComponentPool* {
        // Un seul pool absent suffit à rendre l'intersection vide
        if ((!pool || ...)) {
            return nullptr;
        }

        const IComponentPool* smallest = nullptr;
        ((smallest == nullptr || pool->size() < smallest->size() ? smallest = pool : smallest), ...);
        return smallest;
    }, this->pools);
}

template<bool IsConst, typename... Components> requires ViewComponents<Components...>
bool View<IsConst, Components...>::has(const entity_t &entity) const {
    return std::apply([&](auto*... pool) {
        return ((pool && pool->contains(entity)) && ...);
    }, this->pools);
}

template<bool IsConst, typename... Components> requires ViewComponents<Components...>
template<typename Callback>
void View<IsConst, Components...>::each(Callback&& callback) const {
    const auto* smallest = this->get_smallest();

    if (smallest) {
        for (std::size_t i = 0; i < smallest->size(); i++) {
            const entity_t entity = smallest->entity_at(i);
            if (this->has(entity)) {
                std::invoke(callback, entity, std::get<PoolPtr<Components>>(this->pools)->get(entity)...);
            }
        }
    }
}

#endif
