#include "types/burst/burst.hpp"

#include <cassert>
#include <stdexcept>

Burst::Burst(BurstType type, int length)
{
    this->type = type;
    this->length = length;
}

void Burst::update_time(int delta_t)
{
    this->length -= delta_t;
}
