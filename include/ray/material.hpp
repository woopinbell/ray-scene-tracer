#pragma once

#include "ray/math.hpp"

namespace ray {

struct Material {
    Color albedo;

    Material();
    explicit Material(const Color& color);
};

}  // namespace ray
