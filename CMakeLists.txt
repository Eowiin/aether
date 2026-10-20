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

if(AETHER_BUILD_TESTS)
    include(FetchContent)

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
endif()
