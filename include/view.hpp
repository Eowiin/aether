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
        // Précondition : entity provient du stockage dense de candidates
        // (candidates->entity_at(i)). Sa présence dans ce pool n'est pas
        // revérifiée : appeler has() avec une autre entité peut renvoyer true
        // pour un composant absent, et get() serait alors indéfini.
        bool has(const entity_t &entity, const IComponentPool* candidates) const;

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
bool View<IsConst, Components...>::has(const entity_t &entity, const IComponentPool* candidates) const {
    return std::apply([&](auto*... pool) {
        // L'entité vient du pool candidat : inutile d'y vérifier sa présence
        return ((pool && (pool == candidates || pool->contains(entity))) && ...);
    }, this->pools);
}

template<bool IsConst, typename... Components> requires ViewComponents<Components...>
template<typename Callback>
void View<IsConst, Components...>::each(Callback&& callback) const {
    const auto* smallest = this->get_smallest();

    if (smallest) {
        // Parcours instancié pour chaque type de pool : candidates est un
        // ComponentPool<T>* concret (final), donc size() et entity_at() sont
        // appelés sans passer par la vtable
        auto iterate = [&](auto* candidates) {
            const std::size_t size = candidates->size();

            for (std::size_t i = 0; i < size; i++) {
                const entity_t entity = candidates->entity_at(i);
                if (this->has(entity, candidates)) {
                    std::invoke(callback, entity, std::get<PoolPtr<Components>>(this->pools)->get(entity)...);
                }
            }
        };

        // Retrouve à l'exécution le pool typé qui correspond à smallest
        std::apply([&](auto*... pool) {
            ((pool == smallest ? (iterate(pool), true) : false) || ...);
        }, this->pools);
    }
}

#endif
