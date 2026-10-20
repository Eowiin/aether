cmake_minimum_required(VERSION 3.20)

project(aether LANGUAGES CXX)

# C++20 obligatoire
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Utile pour clangd et les IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
)

FetchContent_MakeAvailable(Catch2)

add_executable(ecs_tests
    tests/entity_manager_tests.cpp
    src/entity_manager.cpp
)

# Executable principal
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/entity_manager.cpp
)

# Dossier contenant les headers
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(ecs_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(ecs_tests PRIVATE
    Catch2::Catch2WithMain
)

# Avertissements selon le compilateur
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -Wpedantic)
endif()

enable_testing()
add_test(NAME ecs_tests COMMAND ecs_tests)
