#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <cstdint>

struct entity_t {
    uint32_t index;
    uint32_t generation;

    uint64_t id() const {
        return (uint64_t(generation) << 32) | index;
    }

    bool operator==(const entity_t&) const = default;
};

#endif
