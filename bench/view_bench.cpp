// Benchmark du parcours de View, indépendant de Catch2.
// À compiler en Release (./build.sh release) puis lancer build/Release/view_bench.
//
// Seul le parcours est mesuré : les pools sont remplis avant les mesures.
// Le travail n'est jamais supprimé par l'optimiseur : chaque passe écrit dans
// les pools, clobber_memory() force ces écritures après chaque passe, et une
// somme de contrôle calculée hors mesure est affichée à la fin.
//
// Chaque méthode a son propre jeu de données (identique aux autres), et les
// mesures sont réparties en plusieurs tours dont l'ordre change, pour qu'aucune
// méthode ne profite systématiquement de l'état laissé par la précédente.
//
// Le premier tableau compare toutes les méthodes sur le scénario de référence.
// Le second compare seulement l'intersection manuelle et View sur plusieurs
// scénarios, avec des identifiants regroupés puis dispersés.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <random>
#include <vector>
#include "component_pool.hpp"
#include "icomponent_pool.hpp"
#include "view.hpp"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace {

struct Position {
    float x;
    float y;
};

struct Velocity {
    float x;
    float y;
};

// Disposition regroupée : Position sur [0, P), Velocity sur [P - I, P - I + V),
// les deux pools remplis dans l'ordre croissant des identifiants.
// Disposition dispersée : les mêmes effectifs, mais les identifiants communs
// et exclusifs sont tirés au hasard (graine fixe) parmi [0, P + V - I), et
// chaque pool est rempli dans un ordre mélangé indépendamment de l'autre.
struct Scenario {
    const char* name;
    uint32_t position_count;
    uint32_t velocity_count;
    uint32_t intersection_count;
    bool scattered;
};

// Velocity sur [90 000, 110 000) → intersection sur [90 000, 100 000)
constexpr Scenario reference_scenario{"Référence", 100'000, 20'000, 10'000, false};

constexpr Scenario comparison_scenarios[] = {
    {"Référence", 100'000, 20'000, 10'000, false},
    {"A — Intersection faible", 100'000, 20'000, 1'000, false},
    {"B — Intersection importante", 100'000, 80'000, 70'000, false},
    {"C — Pools équilibrés", 100'000, 100'000, 50'000, false},
    {"Référence", 100'000, 20'000, 10'000, true},
    {"A — Intersection faible", 100'000, 20'000, 1'000, true},
    {"B — Intersection importante", 100'000, 80'000, 70'000, true},
    {"C — Pools équilibrés", 100'000, 100'000, 50'000, true},
};

constexpr uint32_t shuffle_seed = 20'260'425;

constexpr float dt = 1.0f / 60.0f;

constexpr int round_count = 3;
constexpr int warmup_passes = 50;            // par méthode et par tour
constexpr int samples_per_round = 51;        // 153 échantillons au total : médiane réelle
constexpr int passes_per_sample = 100;

// Barrière de compilation : le compilateur doit supposer que toute la mémoire
// peut être lue ici, donc effectuer les écritures de la passe précédente.
// Ne coûte aucune instruction et n'est appelée qu'entre deux passes.
inline void clobber_memory() {
#if defined(_MSC_VER)
    _ReadWriteBarrier();
#else
    asm volatile("" : : : "memory");
#endif
}

// Rend un pointeur opaque pour l'optimiseur : il ne peut plus déduire vers
// quel objet il pointe.
template<typename T>
T* hide_dynamic_type(T* ptr) {
#if defined(_MSC_VER)
    T* volatile opaque = ptr;
    return opaque;
#else
    asm volatile("" : "+r"(ptr));
    return ptr;
#endif
}

[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "ÉCHEC : %s\n", message);
    std::abort();
}

struct Pools {
    ComponentPool<Position> positions;
    ComponentPool<Velocity> velocities;
};

// Fisher-Yates avec std::mt19937, dont la suite est fixée par la norme :
// std::shuffle et std::uniform_int_distribution dépendent de la bibliothèque
// standard, ce mélange donne le même résultat partout
void shuffle(std::vector<uint32_t>& ids, std::mt19937& rng) {
    for (std::size_t i = ids.size(); i > 1; --i) {
        const std::size_t j = std::size_t((uint64_t(rng()) * i) >> 32);
        std::swap(ids[i - 1], ids[j]);
    }
}

std::unique_ptr<Pools> make_pools(const Scenario& scenario) {
    const uint32_t p = scenario.position_count;
    const uint32_t v = scenario.velocity_count;
    const uint32_t in = scenario.intersection_count;

    std::vector<uint32_t> position_ids(p);
    std::vector<uint32_t> velocity_ids(v);
    if (!scenario.scattered) {
        std::iota(position_ids.begin(), position_ids.end(), 0u);
        std::iota(velocity_ids.begin(), velocity_ids.end(), p - in);
    } else {
        // ids[0, in) communs, ids[in, p) Position seule, ids[p, p + v - in) Velocity seule
        std::mt19937 rng(shuffle_seed);
        std::vector<uint32_t> ids(p + v - in);
        std::iota(ids.begin(), ids.end(), 0u);
        shuffle(ids, rng);

        std::copy(ids.begin(), ids.begin() + p, position_ids.begin());
        std::copy(ids.begin(), ids.begin() + in, velocity_ids.begin());
        std::copy(ids.begin() + p, ids.end(), velocity_ids.begin() + in);
        shuffle(position_ids, rng);
        shuffle(velocity_ids, rng);
    }

    auto pools = std::make_unique<Pools>();
    for (uint32_t i : position_ids) {
        pools->positions.emplace(entity_t{i, 0}, Position{float(i), -float(i)});
    }
    for (uint32_t i : velocity_ids) {
        pools->velocities.emplace(entity_t{i, 0}, Velocity{float(i % 7) + 1.0f, float(i % 13) - 6.0f});
    }
    if (pools->positions.size() != p || pools->velocities.size() != v) {
        fail("tailles des pools inattendues");
    }
    return pools;
}

// --- Méthodes mesurées : chacune renvoie le nombre d'entités traitées ---

// Référence : parcours du stockage dense, sans sparse ni intersection
std::size_t direct_dense(Pools& pools) {
    std::span<Position> components = pools.positions.components();
    for (Position& p : components) {
        p.x += dt;
        p.y += dt;
    }
    return components.size();
}

// Même travail que direct_dense, mais en repassant par sparse via get()
std::size_t direct_sparse(Pools& pools) {
    ComponentPool<Position>& positions = pools.positions;
    const std::size_t size = positions.size();
    for (std::size_t i = 0; i < size; ++i) {
        Position& p = positions.get(positions.entity_at(i));
        p.x += dt;
        p.y += dt;
    }
    return size;
}

// Parcours du plus petit pool (Velocity) et test de contains() sur l'autre
std::size_t manual_intersection(Pools& pools) {
    std::size_t processed = 0;
    const std::size_t size = pools.velocities.size();
    for (std::size_t i = 0; i < size; ++i) {
        const entity_t entity = pools.velocities.entity_at(i);
        if (pools.positions.contains(entity)) {
            Position& p = pools.positions.get(entity);
            const Velocity& v = pools.velocities.get(entity);
            p.x += v.x * dt;
            p.y += v.y * dt;
            ++processed;
        }
    }
    return processed;
}

// Expérience 1 : manual_intersection + la vérification inutile que fait
// View::has() sur le pool candidat (même ordre : Position puis Velocity)
std::size_t manual_redundant(Pools& pools) {
    std::size_t processed = 0;
    const std::size_t size = pools.velocities.size();
    for (std::size_t i = 0; i < size; ++i) {
        const entity_t entity = pools.velocities.entity_at(i);
        if (pools.positions.contains(entity) && pools.velocities.contains(entity)) {
            Position& p = pools.positions.get(entity);
            const Velocity& v = pools.velocities.get(entity);
            p.x += v.x * dt;
            p.y += v.y * dt;
            ++processed;
        }
    }
    return processed;
}

// Contrôle : reproduit à la main tout ce que fait View::each() — pool candidat
// sauté par comparaison d'adresse, size() lu une fois. Le pointeur est rendu
// opaque pour que l'optimiseur ne résolve pas ces comparaisons, comme dans View
// où le pool candidat n'est connu qu'à l'exécution. Si ce temps rejoint celui
// de View, l'écart est expliqué.
std::size_t manual_like_view(Pools& pools) {
    const ComponentPool<Velocity>* candidates = hide_dynamic_type<const ComponentPool<Velocity>>(&pools.velocities);
    std::size_t processed = 0;
    // View::has() compare les pools via leur interface commune
    const IComponentPool* candidate_pool = candidates;
    const std::size_t size = candidates->size();
    for (std::size_t i = 0; i < size; ++i) {
        const entity_t entity = candidates->entity_at(i);
        if ((&pools.positions == candidate_pool || pools.positions.contains(entity))
            && (&pools.velocities == candidate_pool || pools.velocities.contains(entity))) {
            Position& p = pools.positions.get(entity);
            const Velocity& v = pools.velocities.get(entity);
            p.x += v.x * dt;
            p.y += v.y * dt;
            ++processed;
        }
    }
    return processed;
}

std::size_t view_each(Pools& pools) {
    std::size_t processed = 0;
    View<false, Position, Velocity> view(&pools.positions, &pools.velocities);
    view.each([&](entity_t, Position& p, const Velocity& v) {
        p.x += v.x * dt;
        p.y += v.y * dt;
        ++processed;
    });
    return processed;
}

using Pass = std::size_t (*)(Pools&);

struct Benchmark {
    const char* name;
    const Scenario* scenario;
    Pass pass;
    std::size_t candidates;        // entités examinées par passe
    std::size_t expected;          // entités traitées par passe
    Pass reference;                // méthode qui doit produire les mêmes positions
    std::unique_ptr<Pools> pools;  // jeu de données propre à cette méthode
    std::vector<double> samples;   // ns par passe
};

// --- Vérifications (hors mesure) ---

bool same_positions(const ComponentPool<Position>& a, const ComponentPool<Position>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const entity_t entity = a.entity_at(i);
        if (!b.contains(entity)) {
            return false;
        }
        // Mêmes calculs dans le même ordre : égalité exacte attendue
        const Position& pa = a.get(entity);
        const Position& pb = b.get(entity);
        if (pa.x != pb.x || pa.y != pb.y) {
            return false;
        }
    }
    return true;
}

// Une passe sur des pools neufs : bon nombre d'entités, mêmes positions que la référence
void verify(const Benchmark& benchmark) {
    auto tested = make_pools(*benchmark.scenario);
    auto expected = make_pools(*benchmark.scenario);

    if (benchmark.pass(*tested) != benchmark.expected) {
        std::fprintf(stderr, "%s : ", benchmark.name);
        fail("nombre d'entités traitées inattendu");
    }
    benchmark.reference(*expected);
    if (!same_positions(tested->positions, expected->positions)) {
        std::fprintf(stderr, "%s : ", benchmark.name);
        fail("positions différentes de la méthode de référence");
    }
}

// --- Mesure ---

void measure_round(Benchmark& benchmark) {
    using clock = std::chrono::steady_clock;
    Pools& pools = *benchmark.pools;

    for (int i = 0; i < warmup_passes; ++i) {
        benchmark.pass(pools);
        clobber_memory();
    }

    for (int s = 0; s < samples_per_round; ++s) {
        const auto start = clock::now();
        for (int i = 0; i < passes_per_sample; ++i) {
            benchmark.pass(pools);
            clobber_memory();
        }
        const auto end = clock::now();
        benchmark.samples.push_back(std::chrono::duration<double, std::nano>(end - start).count() / passes_per_sample);
    }
}

// Vérifie chaque méthode, lui donne son jeu de données, puis mesure le groupe
// en plusieurs tours : chaque tour démarre à une méthode différente
void measure_all(std::vector<Benchmark>& benchmarks) {
    for (Benchmark& benchmark : benchmarks) {
        verify(benchmark);
        benchmark.pools = make_pools(*benchmark.scenario);
        benchmark.samples.reserve(round_count * samples_per_round);
    }

    const std::size_t count = benchmarks.size();
    for (int round = 0; round < round_count; ++round) {
        const std::size_t offset = std::size_t(round) * count / round_count;
        for (std::size_t i = 0; i < count; ++i) {
            measure_round(benchmarks[(offset + i) % count]);
        }
    }
}

double median(std::vector<double> samples) {
    auto middle = samples.begin() + samples.size() / 2;
    std::nth_element(samples.begin(), middle, samples.end());
    return *middle;
}

double checksum(const ComponentPool<Position>& positions) {
    double sum = 0.0;
    for (const Position& p : positions.components()) {
        sum += double(p.x) + double(p.y);
    }
    return sum;
}

// printf aligne en octets : on complète selon le nombre de caractères UTF-8
int utf8_length(const char* text) {
    int chars = 0;
    for (const char* c = text; *c; ++c) {
        chars += (static_cast<unsigned char>(*c) & 0xC0) != 0x80;
    }
    return chars;
}

void print_padded(const char* text, int width) {
    std::printf("%s%*s", text, std::max(0, width - utf8_length(text)), "");
}

void print_headers(const char* first, int first_width, const char* const* headers, std::size_t count) {
    print_padded(first, first_width);
    for (std::size_t i = 0; i < count; ++i) {
        std::printf(" %*s%s", std::max(0, 12 - utf8_length(headers[i])), "", headers[i]);
    }
    std::printf("\n");
}

}  // namespace

int main() {
#ifndef NDEBUG
    std::fprintf(stderr, "ATTENTION : build sans NDEBUG, les assert de get() faussent les mesures. "
                         "Utiliser ./build.sh release.\n\n");
#endif

    const Scenario& ref = reference_scenario;
    std::vector<Benchmark> benchmarks;
    benchmarks.push_back({"Parcours direct (span)", &ref, direct_dense, ref.position_count, ref.position_count, direct_dense, {}, {}});
    benchmarks.push_back({"Parcours direct (sparse)", &ref, direct_sparse, ref.position_count, ref.position_count, direct_dense, {}, {}});
    benchmarks.push_back({"Intersection manuelle", &ref, manual_intersection, ref.velocity_count, ref.intersection_count, manual_intersection, {}, {}});
    benchmarks.push_back({"Manuelle + vérif. redondante", &ref, manual_redundant, ref.velocity_count, ref.intersection_count, manual_intersection, {}, {}});
    benchmarks.push_back({"Manuelle comme View", &ref, manual_like_view, ref.velocity_count, ref.intersection_count, manual_intersection, {}, {}});
    benchmarks.push_back({"View each()", &ref, view_each, ref.velocity_count, ref.intersection_count, manual_intersection, {}, {}});

    measure_all(benchmarks);

    std::printf("%d tours × %d échantillons de %d passes, %d passes de chauffe par tour\n\n",
                round_count, samples_per_round, passes_per_sample, warmup_passes);

    std::printf("Scénario de référence (%u Position, %u Velocity, %u communes, regroupées)\n\n",
                ref.position_count, ref.velocity_count, ref.intersection_count);
    const double manual_median = median(benchmarks[2].samples);
    const char* headers[] = {"médiane µs", "min µs", "max µs", "ns/candidat", "ns/traitée", "traitées", "× manuelle"};
    print_headers("Méthode", 30, headers, std::size(headers));

    double total = 0.0;
    for (const Benchmark& b : benchmarks) {
        const auto [min_it, max_it] = std::minmax_element(b.samples.begin(), b.samples.end());
        const double med = median(b.samples);
        print_padded(b.name, 30);
        std::printf(" %12.2f %12.2f %12.2f %12.3f %12.3f %12zu",
                    med / 1000.0, *min_it / 1000.0, *max_it / 1000.0,
                    med / double(b.candidates), med / double(b.expected), b.expected);
        if (b.candidates != b.expected) {
            std::printf(" %12.2f", med / manual_median);
        }
        std::printf("\n");
        total += checksum(b.pools->positions);
    }

    // Intersection manuelle contre View, mesurées ensemble scénario par scénario.
    // Velocity n'est jamais plus grand que Position : les deux méthodes
    // examinent min(P, V) candidats (à égalité, View parcourt Position et
    // l'intersection manuelle Velocity)
    std::printf("\nIntersection manuelle contre View (médianes)\n\n");
    const char* comparison_headers[] = {"disposition", "Position", "Velocity", "communes",
                                        "manuelle µs", "View µs", "View/man.",
                                        "ns/cand man", "ns/cand View", "ns/trt man", "ns/trt View"};
    print_headers("Scénario", 30, comparison_headers, std::size(comparison_headers));

    for (const Scenario& scenario : comparison_scenarios) {
        const std::size_t candidates = std::min(scenario.position_count, scenario.velocity_count);
        std::vector<Benchmark> pair;
        pair.push_back({"Intersection manuelle", &scenario, manual_intersection, candidates, scenario.intersection_count, manual_intersection, {}, {}});
        pair.push_back({"View each()", &scenario, view_each, candidates, scenario.intersection_count, manual_intersection, {}, {}});
        measure_all(pair);

        const double manual = median(pair[0].samples);
        const double view = median(pair[1].samples);
        // « regroupée » et « dispersée » ont un é sur deux octets : 13 octets = 12 caractères
        print_padded(scenario.name, 30);
        std::printf(" %13s %12u %12u %12u %12.2f %12.2f %12.2f %12.3f %12.3f %12.3f %12.3f\n",
                    scenario.scattered ? "dispersée" : "regroupée",
                    scenario.position_count, scenario.velocity_count, scenario.intersection_count,
                    manual / 1000.0, view / 1000.0, view / manual,
                    manual / double(candidates), view / double(candidates),
                    manual / double(scenario.intersection_count), view / double(scenario.intersection_count));
        for (const Benchmark& b : pair) {
            total += checksum(b.pools->positions);
        }
    }

    std::printf("\nsomme de contrôle : %.6e\n", total);
    return 0;
}
