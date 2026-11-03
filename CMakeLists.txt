cmake_minimum_required(VERSION 3.20)

project(aether LANGUAGES CXX)

# C++20 minimum : on peut monter plus haut avec -DCMAKE_CXX_STANDARD=23
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
elseif(CMAKE_CXX_STANDARD LESS 20 OR CMAKE_CXX_STANDARD EQUAL 98)
    message(FATAL_ERROR "aether requiert au minimum C++20 (CMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD})")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Utile pour clangd et les IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Debug par défaut si aucun type de build n'est précisé
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Type de build" FORCE)
endif()

option(AETHER_BUILD_TESTS "Compiler les tests" ON)
option(AETHER_BUILD_BENCHMARKS "Compiler les benchmarks" ON)
option(AETHER_WARNINGS_AS_ERRORS "Traiter les avertissements comme des erreurs" ON)

option(AETHER_SANITIZE "Activer AddressSanitizer et UBSan (GCC/Clang)" OFF)

# Les sanitizers s'appliquent à tout le build (Catch2 compris) pour éviter de
# mélanger du code instrumenté et non instrumenté
if(AETHER_SANITIZE)
    if(MSVC)
        message(FATAL_ERROR "AETHER_SANITIZE n'est pas pris en charge avec MSVC")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Avertissements partagés par toutes les cibles du projet
add_library(aether_warnings INTERFACE)
if(MSVC)
    target_compile_options(aether_warnings INTERFACE
        /W4 /permissive-
        $<$<BOOL:${AETHER_WARNINGS_AS_ERRORS}>:/WX>
    )
else()
    target_compile_options(aether_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        $<$<BOOL:${AETHER_WARNINGS_AS_ERRORS}>:-Werror>
    )
endif()

# Cœur de l'ECS, partagé entre l'exécutable et les tests
add_library(aether_core STATIC
    ecs/src/entity_manager.cpp
    ecs/src/registry.cpp
)
target_include_directories(aether_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ecs/include"
)
target_compile_features(aether_core PUBLIC cxx_std_20)
target_link_libraries(aether_core PRIVATE aether_warnings)

# Code propre au jeu (systèmes, composants) : dépend du cœur de l'ECS mais pas
# de raylib, donc testable sans fenêtre. aether_core reste réutilisable ailleurs.
add_library(aether_game STATIC
    game/src/movement_system.cpp
    game/src/position_snapshot_system.cpp
    game/src/teleport.cpp
)
target_include_directories(aether_game PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/game/include"
)
target_link_libraries(aether_game
    PUBLIC aether_core
    PRIVATE aether_warnings
)

include(FetchContent)

# Affiche la progression des téléchargements (git clone) au lieu de rester
# silencieux pendant plusieurs minutes au premier configure
set(FETCHCONTENT_QUIET OFF)

# Executable principal
add_executable(${PROJECT_NAME}
    game/src/main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE aether_game aether_warnings)

if(AETHER_BUILD_BENCHMARKS)
    add_executable(view_bench bench/view_bench.cpp)
    target_link_libraries(view_bench PRIVATE aether_core aether_warnings)

    # EnTT ne sert que de point de comparaison : ses en-têtes sont inclus en
    # SYSTEM pour que nos avertissements (-Werror) ne s'appliquent pas à eux
    FetchContent_Declare(
        EnTT
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v4.0.0
        GIT_SHALLOW TRUE
        SYSTEM
    )
    FetchContent_MakeAvailable(EnTT)

    # Même cible, donc même compilateur et mêmes options pour les deux ECS
    # Chaque ECS a sa propre unité de compilation (donc sa propre boucle chaude)
    # et EnTT n'est visible que de benchmarks/entt_bench.cpp
    add_executable(entt_bench
        benchmarks/benchmark_main.cpp
        benchmarks/aether_bench.cpp
        benchmarks/entt_bench.cpp
    )
    target_include_directories(entt_bench PRIVATE benchmarks)
    target_link_libraries(entt_bench PRIVATE aether_core aether_warnings EnTT::EnTT)
endif()

if(AETHER_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.16.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(ecs_tests
        ecs/tests/entity_manager_tests.cpp
        ecs/tests/component_pool_tests.cpp
        ecs/tests/registry_tests.cpp
        ecs/tests/view_tests.cpp
    )
    target_link_libraries(ecs_tests PRIVATE
        aether_core
        aether_warnings
        Catch2::Catch2WithMain
    )

    add_executable(game_tests
        game/tests/movement_system_tests.cpp
        game/tests/position_snapshot_system_tests.cpp
        game/tests/teleport_tests.cpp
    )
    target_link_libraries(game_tests PRIVATE
        aether_game
        aether_warnings
        Catch2::Catch2WithMain
    )

    # Un test CTest par TEST_CASE
    enable_testing()
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(ecs_tests)
    catch_discover_tests(game_tests)

    # Tests négatifs de compilation : exclus du build normal, CTest lance la
    # compilation et attend un échec. La variante de contrôle doit compiler.
    function(aether_add_compile_fail_test name source)
        add_library(${name}_ok OBJECT EXCLUDE_FROM_ALL ${source})
        target_link_libraries(${name}_ok PRIVATE aether_core aether_warnings)

        add_library(${name}_fail OBJECT EXCLUDE_FROM_ALL ${source})
        target_link_libraries(${name}_fail PRIVATE aether_core aether_warnings)
        target_compile_definitions(${name}_fail PRIVATE AETHER_EXPECT_COMPILE_ERROR)

        foreach(variant ok fail)
            add_test(NAME compile_fail.${name}.${variant}
                COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                        --target ${name}_${variant} --config $<CONFIG>
            )
        endforeach()
        set_tests_properties(compile_fail.${name}.fail PROPERTIES WILL_FAIL TRUE)
    endfunction()

    aether_add_compile_fail_test(const_view_mutation ecs/tests/compile_fail/const_view_mutation.cpp)
endif()
