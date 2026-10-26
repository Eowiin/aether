// Définitions communes aux deux côtés de la comparaison aether / EnTT :
// composants, paramètres du scénario, protocole de mesure et interface Engine.
//
// Ce fichier ne contient aucun en-tête d'ECS. Chaque implémentation
// (aether_bench.cpp, entt_bench.cpp) compile sa propre boucle chaude dans son
// unité de compilation : each() et son callback y sont visibles de bout en
// bout, donc inlinables par GCC comme dans un vrai programme. Seul run(), appelé
// une fois par échantillon, traverse la frontière entre unités : il exécute
// toutes les passes d'un échantillon, donc son appel est amorti et ne remplace
// pas un problème d'inlining par un autre.
#ifndef BENCHMARK_COMMON_HPP
#define BENCHMARK_COMMON_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <random>
#include <type_traits>
#include <vector>

namespace bench {

struct Position {
    float x;
    float y;
};

struct Velocity {
    float x;
    float y;
};

struct Health {
    float value;
};

// --- Scénarios ---
// Position sur [0, P), Velocity sur [velocity_first, velocity_first + V) et,
// si health_count > 0, Health sur [health_first, health_first + H). Les
// identifiants d'entités sont créés dans l'ordre [0, entity_count).
//
// Disposition regroupée : chaque pool est rempli dans l'ordre croissant des
// identifiants. Disposition dispersée : les identifiants sont permutés au
// hasard (graine fixe) puis chaque pool est rempli dans un ordre mélangé
// indépendamment des autres. Les effectifs de chaque intersection sont les
// mêmes dans les deux dispositions.
struct Scenario {
    const char* name;
    uint32_t position_count;
    uint32_t velocity_count;
    uint32_t health_count;      // 0 : View<Position, Velocity> ; sinon View<Position, Velocity, Health>
    uint32_t velocity_first;
    uint32_t health_first;
    bool scattered;

    uint32_t component_count() const { return health_count > 0 ? 3 : 2; }
};

// Deux composants : Velocity sur [P - I, P - I + V), donc I entités communes
constexpr Scenario two_components(const char* name, uint32_t p, uint32_t v, uint32_t common, bool scattered) {
    return Scenario{name, p, v, 0, p - common, 0, scattered};
}

constexpr uint32_t shuffle_seed = 20'260'425;

// Fisher-Yates avec std::mt19937, dont la suite est fixée par la norme :
// std::shuffle et std::uniform_int_distribution dépendent de la bibliothèque
// standard, ce mélange donne le même résultat partout
inline void shuffle(std::vector<uint32_t>& ids, std::mt19937& rng) {
    for (std::size_t i = ids.size(); i > 1; --i) {
        const std::size_t j = std::size_t((uint64_t(rng()) * i) >> 32);
        std::swap(ids[i - 1], ids[j]);
    }
}

// Contenu des pools d'un scénario, calculé une fois et donné aux deux ECS :
// ids[k] liste les entités du pool k (0 Position, 1 Velocity, 2 Health) dans
// l'ordre où il faut les y ajouter, donc dans l'ordre de leur stockage dense
struct Layout {
    uint32_t entity_count = 0;
    std::array<std::vector<uint32_t>, 3> ids;
    std::array<std::vector<char>, 3> present;   // present[k][id] : l'entité a le composant k
    std::size_t match_count = 0;                // entités présentes dans tous les pools de la View
};

inline Layout make_layout(const Scenario& scenario) {
    Layout layout;
    const uint32_t first[3] = {0, scenario.velocity_first, scenario.health_first};
    const uint32_t count[3] = {scenario.position_count, scenario.velocity_count, scenario.health_count};
    const uint32_t pools = scenario.component_count();

    for (uint32_t k = 0; k < pools; ++k) {
        layout.ids[k].resize(count[k]);
        std::iota(layout.ids[k].begin(), layout.ids[k].end(), first[k]);
        layout.entity_count = std::max(layout.entity_count, first[k] + count[k]);
    }

    if (scenario.scattered) {
        std::mt19937 rng(shuffle_seed);
        std::vector<uint32_t> permutation(layout.entity_count);
        std::iota(permutation.begin(), permutation.end(), 0u);
        shuffle(permutation, rng);
        for (uint32_t k = 0; k < pools; ++k) {
            for (uint32_t& id : layout.ids[k]) {
                id = permutation[id];
            }
            shuffle(layout.ids[k], rng);
        }
    }

    for (uint32_t k = 0; k < pools; ++k) {
        layout.present[k].assign(layout.entity_count, 0);
        for (uint32_t id : layout.ids[k]) {
            layout.present[k][id] = 1;
        }
    }
    for (uint32_t id = 0; id < layout.entity_count; ++id) {
        bool in_all = true;
        for (uint32_t k = 0; k < pools; ++k) {
            in_all = in_all && layout.present[k][id];
        }
        layout.match_count += in_all;
    }
    return layout;
}

constexpr float dt = 1.0f / 60.0f;

template<typename Component>
inline Component initial(uint32_t i) {
    if constexpr (std::is_same_v<Component, Position>) {
        return Position{float(i), -float(i)};
    } else if constexpr (std::is_same_v<Component, Velocity>) {
        return Velocity{float(i % 7) + 1.0f, float(i % 13) - 6.0f};
    } else {
        return Health{100.0f + float(i % 5)};
    }
}

// Le calcul par entité, identique des deux côtés
inline void integrate_one(Position& p, const Velocity& v) {
    p.x += v.x * dt;
    p.y += v.y * dt;
}

inline void integrate_one(Position& p, const Velocity& v, Health& h) {
    integrate_one(p, v);
    h.value -= dt;
}

// --- Protocole de mesure ---
constexpr int round_count = 3;
constexpr int warmup_passes = 50;                 // par ECS et par tour
constexpr int samples_per_round = 51;             // 153 échantillons au total : médiane réelle
constexpr int passes_per_sample = 100;
constexpr int construction_passes_per_sample = 10'000;

enum class Kind { construction, iteration, full };

// Barrière de compilation : le compilateur doit supposer que toute la mémoire
// peut être lue ici, donc effectuer les écritures de la passe précédente.
// Ne coûte aucune instruction.
inline void clobber_memory() {
    asm volatile("" : : : "memory");
}

// Rend un objet opaque pour l'optimiseur, pour qu'il ne supprime pas une View
// construite mais jamais parcourue
template<typename T>
inline void escape(T& value) {
    asm volatile("" : : "r"(&value) : "memory");
}

[[noreturn]] inline void fail(const char* context, const char* message) {
    std::fprintf(stderr, "ÉCHEC (%s) : %s\n", context, message);
    std::abort();
}

// Un monde (registry + entités + View) et les passes qu'on peut y mesurer
class Engine {
    public:
        virtual ~Engine() = default;

        // Exécute `passes` passes du genre demandé. La boucle sur les passes
        // est dans l'implémentation, à côté de each() et de son callback.
        // Renvoie le nombre d'entités traitées par la dernière passe.
        virtual std::size_t run(Kind kind, int passes) = 0;

        // Indices des entités dans l'ordre où each() les visite (sans rien
        // modifier). Cet ordre est celui du stockage dense du pool parcouru :
        // il permet de savoir quel pool la View a choisi.
        virtual std::vector<uint32_t> visit_order() = 0;

        // nullptr si l'entité d'indice i n'a pas le composant
        virtual const Position* try_position(uint32_t i) const = 0;
        virtual const Health* try_health(uint32_t i) const = 0;
};

std::unique_ptr<Engine> make_aether_engine(const Scenario& scenario, const Layout& layout);
std::unique_ptr<Engine> make_entt_engine(const Scenario& scenario, const Layout& layout);

// Version d'EnTT, pour l'affichage
const char* entt_version();

}  // namespace bench

#endif
