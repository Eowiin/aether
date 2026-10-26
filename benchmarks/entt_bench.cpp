#include <utility>
#include <vector>
#include <entt/entity/registry.hpp>
#include <entt/config/version.h>
#include "benchmark_common.hpp"

namespace bench {

namespace {

template<typename... Components>
class EnttEngine final : public Engine {
    private:
        using EnttView = decltype(std::declval<entt::registry&>().view<Components...>());
        static constexpr bool has_health = sizeof...(Components) == 3;

        entt::registry registry;
        std::vector<entt::entity> entities;
        EnttView view;

        static std::size_t integrate(EnttView& v) {
            std::size_t processed = 0;
            if constexpr (has_health) {
                v.each([&](entt::entity, Position& p, const Velocity& vel, Health& h) {
                    integrate_one(p, vel, h);
                    ++processed;
                });
            } else {
                v.each([&](entt::entity, Position& p, const Velocity& vel) {
                    integrate_one(p, vel);
                    ++processed;
                });
            }
            return processed;
        }

        template<typename Component>
        void fill(const std::vector<uint32_t>& ids) {
            for (uint32_t id : ids) {
                registry.emplace<Component>(entities[id], initial<Component>(id));
            }
        }

        // Remplit les pools puis crée la View : elle est déclarée après
        // registry et entities, donc initialisée en dernier
        EnttView populate(const Layout& layout) {
            entities.reserve(layout.entity_count);
            for (uint32_t i = 0; i < layout.entity_count; ++i) {
                entities.push_back(registry.create());
                if (entt::to_entity(entities.back()) != i) {
                    fail("EnTT", "identifiants d'entités non séquentiels");
                }
            }
            [&]<std::size_t... Is>(std::index_sequence<Is...>) {
                (fill<Components>(layout.ids[Is]), ...);
            }(std::index_sequence_for<Components...>{});
            return registry.view<Components...>();
        }

    public:
        explicit EnttEngine(const Layout& layout) : view(populate(layout)) {}

        std::size_t run(Kind kind, int passes) override {
            std::size_t processed = 0;
            switch (kind) {
            case Kind::construction:
                for (int i = 0; i < passes; ++i) {
                    EnttView v = registry.view<Components...>();
                    escape(v);
                    clobber_memory();
                }
                break;
            case Kind::iteration:
            case Kind::full: {
                // Un seul site d'appel à each() pour les deux genres, comme
                // dans aether_bench.cpp
                EnttView v = view;
                const bool rebuild = kind == Kind::full;
                for (int i = 0; i < passes; ++i) {
                    if (rebuild) {
                        v = registry.view<Components...>();
                    }
                    processed = integrate(v);
                    clobber_memory();
                }
                break;
            }
            }
            return processed;
        }

        std::vector<uint32_t> visit_order() override {
            std::vector<uint32_t> order;
            for (entt::entity entity : view) {
                order.push_back(entt::to_entity(entity));
            }
            return order;
        }

        const Position* try_position(uint32_t i) const override {
            return registry.try_get<Position>(entities[i]);
        }

        const Health* try_health(uint32_t i) const override {
            if constexpr (has_health) {
                return registry.try_get<Health>(entities[i]);
            } else {
                (void)i;
                return nullptr;
            }
        }
};

}  // namespace

std::unique_ptr<Engine> make_entt_engine(const Scenario& scenario, const Layout& layout) {
    if (scenario.health_count > 0) {
        return std::make_unique<EnttEngine<Position, Velocity, Health>>(layout);
    }
    return std::make_unique<EnttEngine<Position, Velocity>>(layout);
}

const char* entt_version() {
    return ENTT_VERSION;
}

}  // namespace bench
