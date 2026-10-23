// Test négatif de compilation : hors de la suite Catch2, piloté par CTest.
// Sans AETHER_EXPECT_COMPILE_ERROR, ce fichier doit compiler (contrôle) ; avec,
// il doit échouer. Le contrôle garantit que l'échec vient bien de la mutation
// et non d'une erreur sans rapport (include manquant, faute de frappe...).
#include "registry.hpp"

struct Position {
    float x;
    float y;
};

void mutate_through_const_view(Registry& registry) {
    const Registry& readonly = registry;

    auto view = readonly.view<Position>();

#ifdef AETHER_EXPECT_COMPILE_ERROR
    view.each([](entity_t, Position& pos) {
        pos.x = 100;
    });
#else
    view.each([](entity_t, const Position& pos) {
        (void)pos.x;
    });
#endif
}
