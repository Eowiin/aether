// Comparaison aether / EnTT sur View::each(), sans group.
// À compiler en Release (./build.sh release) puis lancer build/Release/entt_bench
// [reference|distributions|trois]. Sans argument, les trois sections sont lancées :
//   reference      scénario de référence, trois genres de passes
//   distributions  expérience A : intersection faible / importante / équilibrée,
//                  identifiants regroupés puis dispersés
//   trois          expérience B : View<Position, Velocity, Health>
//
// Les deux ECS vivent dans des unités de compilation séparées (aether_bench.cpp
// et entt_bench.cpp) : ce fichier ne voit aucun des deux, il pilote seulement
// les mesures à travers l'interface Engine et affiche les résultats.
//
// Le travail n'est jamais supprimé par l'optimiseur : chaque passe écrit dans
// les pools, clobber_memory() force ces écritures après chaque passe, et une
// somme de contrôle calculée hors mesure est affichée à la fin.
// L'ordre de mesure alterne entre les deux ECS, d'un tour à l'autre et d'un
// échantillon à l'autre.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "benchmark_common.hpp"

using namespace bench;

namespace {

const char* kind_name(Kind kind) {
    switch (kind) {
    case Kind::construction: return "Construction de la View";
    case Kind::iteration: return "Parcours each()";
    case Kind::full: return "Construction + each()";
    }
    return "";
}

// Pools de la View, dans l'ordre de Components...
const char* const pool_names[] = {"Position", "Velocity", "Health"};

// Valeur attendue après `passes` passes pour une entité présente dans tous les pools
void expected_values(uint32_t id, std::size_t passes, Position& position, Velocity& velocity, Health& health) {
    position = initial<Position>(id);
    velocity = initial<Velocity>(id);
    health = initial<Health>(id);
    for (std::size_t p = 0; p < passes; ++p) {
        integrate_one(position, velocity, health);
    }
}

void check_world(const Scenario& scenario, const Layout& layout, const Engine& engine, const char* name,
                 std::size_t passes) {
    const bool three = scenario.component_count() == 3;
    for (uint32_t id = 0; id < layout.entity_count; ++id) {
        const Position* position = engine.try_position(id);
        const Health* health = engine.try_health(id);
        if ((position != nullptr) != bool(layout.present[0][id]) ||
            (health != nullptr) != (three && layout.present[2][id])) {
            fail(name, "présence de composant inattendue");
        }

        bool in_all = true;
        for (uint32_t k = 0; k < scenario.component_count(); ++k) {
            in_all = in_all && layout.present[k][id];
        }

        Position expected_position = initial<Position>(id);
        Health expected_health = initial<Health>(id);
        if (in_all) {
            Velocity velocity;
            expected_values(id, passes, expected_position, velocity, expected_health);
            if (!three) {
                // Sans Health, seuls Position et Velocity évoluent
                expected_health = initial<Health>(id);
            }
        }
        if (position != nullptr && (position->x != expected_position.x || position->y != expected_position.y)) {
            fail(name, "position différente du résultat attendu");
        }
        if (health != nullptr && health->value != expected_health.value) {
            fail(name, "santé différente du résultat attendu");
        }
    }
}

// Pools dont l'ordre de stockage dense donne cet ordre de visite : l'ordre
// des identifiants du pool k, filtré sur les entités présentes dans tous les
// pools. Plusieurs pools peuvent donner le même ordre (dispositions regroupées).
std::vector<uint32_t> expected_order(const Scenario& scenario, const Layout& layout, uint32_t driver) {
    std::vector<uint32_t> order;
    for (uint32_t id : layout.ids[driver]) {
        bool in_all = true;
        for (uint32_t k = 0; k < scenario.component_count(); ++k) {
            in_all = in_all && layout.present[k][id];
        }
        if (in_all) {
            order.push_back(id);
        }
    }
    return order;
}

// Pool(s) parcouru(s) par la bibliothèque, déduit de l'ordre de visite : un
// seul nom, ou plusieurs séparés par « / » quand l'ordre ne permet pas de
// trancher. Échoue si le pool parcouru n'est pas un des plus petits.
std::string detect_driver(const Scenario& scenario, const Layout& layout, Engine& engine, const char* name,
                          bool& reversed) {
    std::vector<uint32_t> observed = engine.visit_order();
    if (observed.size() != layout.match_count) {
        fail(name, "each() ne visite pas toutes les correspondances");
    }

    std::size_t smallest = layout.ids[0].size();
    for (uint32_t k = 1; k < scenario.component_count(); ++k) {
        smallest = std::min(smallest, layout.ids[k].size());
    }

    // Un pool peut être parcouru de la fin vers le début (c'est le cas d'EnTT)
    std::string drivers;
    for (bool backwards : {false, true}) {
        for (uint32_t k = 0; k < scenario.component_count(); ++k) {
            std::vector<uint32_t> expected = expected_order(scenario, layout, k);
            if (backwards) {
                std::reverse(expected.begin(), expected.end());
            }
            if (expected != observed) {
                continue;
            }
            if (layout.ids[k].size() != smallest) {
                fail(name, "la View parcourt un pool qui n'est pas le plus petit");
            }
            drivers += (drivers.empty() ? "" : "/") + std::string(pool_names[k]);
        }
        if (!drivers.empty()) {
            reversed = backwards;
            return drivers;
        }
    }
    fail(name, "l'ordre de visite ne correspond à aucun pool, ni à l'endroit ni à l'envers");
}

struct Verified {
    std::string aether_driver;
    std::string entt_driver;
    bool aether_reversed = false;
    bool entt_reversed = false;
};

// Mêmes données, mêmes correspondances et mêmes calculs des deux côtés, et
// pool parcouru identifié pour chaque bibliothèque
Verified verify(const Scenario& scenario, const Layout& layout) {
    for (Kind kind : {Kind::iteration, Kind::full}) {
        auto aether = make_aether_engine(scenario, layout);
        auto entt_engine = make_entt_engine(scenario, layout);

        if (aether->run(kind, 1) != layout.match_count) {
            fail("aether", "nombre d'entités traitées inattendu");
        }
        if (entt_engine->run(kind, 1) != layout.match_count) {
            fail("EnTT", "nombre d'entités traitées inattendu");
        }
        check_world(scenario, layout, *aether, "aether", 1);
        check_world(scenario, layout, *entt_engine, "EnTT", 1);
    }

    auto aether = make_aether_engine(scenario, layout);
    auto entt_engine = make_entt_engine(scenario, layout);
    aether->run(Kind::construction, 1);
    entt_engine->run(Kind::construction, 1);
    check_world(scenario, layout, *aether, "aether", 0);
    check_world(scenario, layout, *entt_engine, "EnTT", 0);

    // Le pool choisi ne dépend que des tailles, pas de la disposition. Dans une
    // disposition regroupée tous les pools sont triés par identifiant et
    // donnent le même ordre de visite : on identifie donc le pool sur la même
    // scène en disposition dispersée, où chaque pool a son propre ordre.
    Scenario probe = scenario;
    probe.scattered = true;
    const Layout probe_layout = make_layout(probe);
    auto probe_aether = make_aether_engine(probe, probe_layout);
    auto probe_entt = make_entt_engine(probe, probe_layout);
    Verified verified;
    verified.aether_driver = detect_driver(probe, probe_layout, *probe_aether, "aether", verified.aether_reversed);
    verified.entt_driver = detect_driver(probe, probe_layout, *probe_entt, "EnTT", verified.entt_reversed);
    // Les deux bibliothèques doivent pouvoir parcourir le même pool, sinon la
    // comparaison ne mesure pas la même chose
    if (verified.aether_driver != verified.entt_driver) {
        fail("vérification", "aether et EnTT ne parcourent pas le même pool");
    }
    return verified;
}

// ns par passe
double sample(Engine& engine, Kind kind, int pass_count) {
    using clock = std::chrono::steady_clock;

    const auto start = clock::now();
    engine.run(kind, pass_count);
    const auto end = clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count() / pass_count;
}

struct Result {
    std::vector<double> aether;
    std::vector<double> entt;
};

Result measure(Kind kind, const Scenario& scenario, const Layout& layout, double& total) {
    const int pass_count = kind == Kind::construction ? construction_passes_per_sample : passes_per_sample;
    auto aether = make_aether_engine(scenario, layout);
    auto entt_engine = make_entt_engine(scenario, layout);

    Result result;
    result.aether.reserve(round_count * samples_per_round);
    result.entt.reserve(round_count * samples_per_round);

    for (int round = 0; round < round_count; ++round) {
        const bool aether_first = round % 2 == 0;
        if (aether_first) {
            aether->run(kind, warmup_passes);
            entt_engine->run(kind, warmup_passes);
        } else {
            entt_engine->run(kind, warmup_passes);
            aether->run(kind, warmup_passes);
        }

        for (int s = 0; s < samples_per_round; ++s) {
            if ((s % 2 == 0) == aether_first) {
                result.aether.push_back(sample(*aether, kind, pass_count));
                result.entt.push_back(sample(*entt_engine, kind, pass_count));
            } else {
                result.entt.push_back(sample(*entt_engine, kind, pass_count));
                result.aether.push_back(sample(*aether, kind, pass_count));
            }
        }
    }

    for (uint32_t i = 0; i < layout.entity_count; ++i) {
        for (const Engine* engine : {aether.get(), entt_engine.get()}) {
            if (const Position* p = engine->try_position(i)) {
                total += double(p->x) + double(p->y);
            }
            if (const Health* h = engine->try_health(i)) {
                total += double(h->value);
            }
        }
    }
    return result;
}

double median(std::vector<double> samples) {
    auto middle = samples.begin() + samples.size() / 2;
    std::nth_element(samples.begin(), middle, samples.end());
    return *middle;
}

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

void print_row(const char* library, const std::vector<double>& samples, Kind kind, double ratio,
               std::size_t candidates, std::size_t matches) {
    const auto [min_it, max_it] = std::minmax_element(samples.begin(), samples.end());
    const double med = median(samples);
    print_padded(library, 10);
    if (kind == Kind::construction) {
        std::printf(" %12.2f %12.2f %12.2f %12s %12s %12.2f\n", med, *min_it, *max_it, "-", "-", ratio);
    } else {
        std::printf(" %12.2f %12.2f %12.2f %12.3f %12.3f %12.2f\n",
                    med / 1000.0, *min_it / 1000.0, *max_it / 1000.0,
                    med / double(candidates), med / double(matches), ratio);
    }
}

// Scénario de référence : Velocity sur [90 000, 110 000), intersection sur [90 000, 100 000)
constexpr Scenario reference_scenario = two_components("Référence", 100'000, 20'000, 10'000, false);

// Expérience A
constexpr Scenario distribution_scenarios[] = {
    two_components("A — Intersection faible", 100'000, 20'000, 1'000, false),
    two_components("B — Intersection importante", 100'000, 80'000, 70'000, false),
    two_components("C — Pools équilibrés", 100'000, 100'000, 50'000, false),
    two_components("A — Intersection faible", 100'000, 20'000, 1'000, true),
    two_components("B — Intersection importante", 100'000, 80'000, 70'000, true),
    two_components("C — Pools équilibrés", 100'000, 100'000, 50'000, true),
};

// Expérience B : Position et Velocity identiques d'un scénario à l'autre, puis
// Health plus petit (parcouru) ou plus grand (vérifié) que Velocity
constexpr Scenario three_scenarios[] = {
    two_components("2 composants (P, V)", 100'000, 50'000, 40'000, false),
    Scenario{"3 composants, Health 20 000", 100'000, 50'000, 20'000, 60'000, 95'000, false},
    Scenario{"3 composants, Health 80 000", 100'000, 50'000, 80'000, 60'000, 70'000, false},
    two_components("2 composants (P, V)", 100'000, 50'000, 40'000, true),
    Scenario{"3 composants, Health 20 000", 100'000, 50'000, 20'000, 60'000, 95'000, true},
    Scenario{"3 composants, Health 80 000", 100'000, 50'000, 80'000, 60'000, 70'000, true},
};

void print_run_info() {
    std::printf("aether contre EnTT %s (View, pas de group)\n", entt_version());
#if defined(__VERSION__)
    std::printf("Compilateur : %s\n", __VERSION__);
#endif
    std::printf("%d tours × %d échantillons, %d passes de chauffe par tour ; "
                "%d passes par échantillon (%d pour la construction seule)\n\n",
                round_count, samples_per_round, warmup_passes, passes_per_sample, construction_passes_per_sample);
}

void run_reference(double& total) {
    const Layout layout = make_layout(reference_scenario);
    const Verified verified = verify(reference_scenario, layout);

    std::printf("== Scénario de référence ==\n");
    std::printf("%u entités, %u Position, %u Velocity, %zu communes, regroupées\n",
                layout.entity_count, reference_scenario.position_count, reference_scenario.velocity_count,
                layout.match_count);
    std::printf("Résultats vérifiés avant mesure : %zu entités traitées, positions identiques ; "
                "pool parcouru : %s %s (aether), %s %s (EnTT)\n\n",
                layout.match_count,
                verified.aether_driver.c_str(), verified.aether_reversed ? "à l'envers" : "à l'endroit",
                verified.entt_driver.c_str(), verified.entt_reversed ? "à l'envers" : "à l'endroit");

    for (Kind kind : {Kind::construction, Kind::iteration, Kind::full}) {
        const Result result = measure(kind, reference_scenario, layout, total);
        const double aether_median = median(result.aether);
        const double entt_median = median(result.entt);

        std::printf("%s\n", kind_name(kind));
        if (kind == Kind::construction) {
            const char* headers[] = {"médiane ns", "min ns", "max ns", "", "", "× aether"};
            print_headers("ECS", 10, headers, std::size(headers));
        } else {
            const char* headers[] = {"médiane µs", "min µs", "max µs", "ns/candidat", "ns/traitée", "× aether"};
            print_headers("ECS", 10, headers, std::size(headers));
        }
        print_row("aether", result.aether, kind, 1.0, layout.ids[0].size() < layout.ids[1].size() ? layout.ids[0].size() : layout.ids[1].size(), layout.match_count);
        print_row("EnTT", result.entt, kind, entt_median / aether_median, layout.ids[0].size() < layout.ids[1].size() ? layout.ids[0].size() : layout.ids[1].size(), layout.match_count);
        std::printf("\n");
    }
}

// Parcours each() seul, un tableau pour une liste de scénarios
void run_table(const char* title, const Scenario* scenarios, std::size_t count, double& total) {
    std::printf("== %s ==\n", title);
    std::printf("Parcours each() ; médianes ; × aether = temps EnTT / temps aether (plus de 1 : aether plus rapide)\n\n");

    const char* headers[] = {"disposition", "Position", "Velocity", "Health", "communes", "aether µs", "EnTT µs",
                             "× aether", "ns/cand aeth", "ns/cand EnTT", "ns/trt aeth", "parcouru"};
    print_headers("Scénario", 30, headers, std::size(headers));

    for (std::size_t n = 0; n < count; ++n) {
        const Scenario& scenario = scenarios[n];
        const Layout layout = make_layout(scenario);
        const Verified verified = verify(scenario, layout);
        const Result result = measure(Kind::iteration, scenario, layout, total);

        std::size_t candidates = layout.ids[0].size();
        for (uint32_t k = 1; k < scenario.component_count(); ++k) {
            candidates = std::min(candidates, layout.ids[k].size());
        }
        const double aether_median = median(result.aether);
        const double entt_median = median(result.entt);

        print_padded(scenario.name, 30);
        print_padded(scenario.scattered ? "dispersée" : "regroupée", 12);
        std::printf(" %12u %12u %12s %12zu %12.2f %12.2f %12.2f %12.3f %12.3f %12.3f  %s\n",
                    scenario.position_count, scenario.velocity_count,
                    scenario.health_count > 0 ? std::to_string(scenario.health_count).c_str() : "-",
                    layout.match_count, aether_median / 1000.0, entt_median / 1000.0, entt_median / aether_median,
                    aether_median / double(candidates), entt_median / double(candidates),
                    aether_median / double(layout.match_count), verified.aether_driver.c_str());
    }
    std::printf("\n");
}

}  // namespace

int main(int argc, char** argv) {
#ifndef NDEBUG
    std::fprintf(stderr, "ATTENTION : build sans NDEBUG, les assert d'aether et d'EnTT faussent les mesures. "
                         "Utiliser ./build.sh release.\n\n");
#endif

    const char* section = argc > 1 ? argv[1] : "";
    const bool all = section[0] == '\0';

    print_run_info();

    double total = 0.0;
    if (all || std::strcmp(section, "reference") == 0) {
        run_reference(total);
    }
    if (all || std::strcmp(section, "distributions") == 0) {
        run_table("Expérience A : distributions", distribution_scenarios, std::size(distribution_scenarios), total);
    }
    if (all || std::strcmp(section, "trois") == 0) {
        run_table("Expérience B : trois composants", three_scenarios, std::size(three_scenarios), total);
    }

    std::printf("somme de contrôle : %.6e\n", total);
    return 0;
}
