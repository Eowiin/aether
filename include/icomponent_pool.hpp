#ifndef ICOMPONENT_POOL_HPP
#define ICOMPONENT_POOL_HPP

#include "entity.hpp"

class IComponentPool {
    public:
        virtual ~IComponentPool() = default;
        virtual bool erase(entity_t entity) noexcept = 0;
        virtual bool contains(entity_t entity) const noexcept = 0;
        virtual std::size_t size() const noexcept = 0;
};

#endif
