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
    src/entity_manager.cpp
    src/registry.cpp
)
target_include_directories(aether_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(aether_core PUBLIC cxx_std_20)
target_link_libraries(aether_core PRIVATE aether_warnings)

# Executable principal
add_executable(${PROJECT_NAME}
    src/main.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE aether_core aether_warnings)

# Benchmarks : compilés dans tous les modes, mais seules les mesures en
# Release (-O3 -DNDEBUG) sont représentatives
if(AETHER_BUILD_BENCHMARKS OR AETHER_BUILD_TESTS)
    include(FetchContent)
endif()

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
        tests/entity_manager_tests.cpp
        tests/component_pool_tests.cpp
        tests/registry_tests.cpp
        tests/view_tests.cpp
    )
    target_link_libraries(ecs_tests PRIVATE
        aether_core
        aether_warnings
        Catch2::Catch2WithMain
    )

    # Un test CTest par TEST_CASE
    enable_testing()
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    include(Catch)
    catch_discover_tests(ecs_tests)

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

    aether_add_compile_fail_test(const_view_mutation tests/compile_fail/const_view_mutation.cpp)
endif()
